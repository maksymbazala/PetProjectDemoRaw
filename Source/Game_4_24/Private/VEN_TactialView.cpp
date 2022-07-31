// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_TactialView.h"
#include "VEN_PlayerUnit.h"
#include "VEN_EnemyUnit.h"
#include "VEN_InteractableNPC.h"
#include "VEN_BattleSystem.h"
#include "VEN_FreeFunctions.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstance.h"
#include "UObject/ConstructorHelpers.h"

using namespace vendetta;

UVEN_TactialView::UVEN_TactialView()
	: m_owner(nullptr)
	, m_hovered_actor(nullptr)
	, m_battle_system(nullptr)
	, m_movement_spline_actor(nullptr)
	, m_movement_spline_component(nullptr)
	, m_main_area_decal_actor(nullptr)
	, m_minor_area_decal_actors({})
	, m_main_area_decal_material_bow(nullptr)
	, m_main_area_decal_material_sword(nullptr)
	, m_main_area_decal_material_minor(nullptr)
	, m_is_allowed(false)
	, m_movement_spline_is_built(false)
	, m_hovered_location(FVector::ZeroVector)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> TacticalViewDecalMaterialBow(TEXT("MaterialInstance'/Game/Materials/TacticalView/MI_TacticalViewMainBow.MI_TacticalViewMainBow'"));
	m_main_area_decal_material_bow = TacticalViewDecalMaterialBow.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> TacticalViewDecalMaterialSword(TEXT("MaterialInstance'/Game/Materials/TacticalView/MI_TacticalViewMainSword.MI_TacticalViewMainSword'"));
	m_main_area_decal_material_sword = TacticalViewDecalMaterialSword.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> TacticalViewDecalMaterialMinor(TEXT("MaterialInstance'/Game/Materials/TacticalView/MI_TacticalViewMinor.MI_TacticalViewMinor'"));
	m_main_area_decal_material_minor = TacticalViewDecalMaterialMinor.Object;
}

void UVEN_TactialView::Initialize(AVEN_PlayerUnit* owner, UVEN_BattleSystem* battle_system)
{
	m_owner = owner;
	m_battle_system = battle_system;
	m_enemy_units_info.Empty();
	m_minor_area_decal_actors.Empty();
}

void UVEN_TactialView::OnRequestData(bool is_allowed, AActor* hovered_actor, FVector hovered_location)
{
	m_is_allowed = is_allowed;
	m_hovered_actor = hovered_actor;
	m_hovered_location = hovered_location;

	UpdateDecals();

	if (m_is_allowed)
	{
		GatherEnemyUnitsRelatedInfo();
		NotifyOwner();
	}
}

void UVEN_TactialView::GatherEnemyUnitsRelatedInfo()
{
	m_enemy_units_info.Empty();

	if (!m_battle_system || !m_battle_system->IsBattleInProgress())
	{
		//debug_log("UVEN_TactialView::GatherEnemyUnitsRelatedInfo. Battle System is corrupted", FColor::Red);
		return;
	}

	//gather alive enemy in battle
	TArray<AActor*> enemy_units_raw = m_battle_system->GetEnemyUnitsInBattle();
	TArray<AVEN_EnemyUnit*> enemy_units;

	for (const auto& enemy_unit_raw : enemy_units_raw)
		enemy_units.Add(Cast<AVEN_EnemyUnit>(enemy_unit_raw));

	if (!enemy_units.Num())
		return;

	for (const auto& enemy_unit : enemy_units)
	{
		if (!enemy_unit)
		{
			//debug_log("UVEN_TactialView::GatherEnemyUnitsRelatedInfo. Invalid enemy unit", FColor::Red);
			continue;
		}

		if (enemy_unit->IsDead())
			continue;

		m_enemy_units_info.Add({});
		auto& info = m_enemy_units_info[m_enemy_units_info.Num() - 1];

		info.location = enemy_unit->GetActorLocation();
		info.type = enemy_unit->GetUnitType();
		info.is_hovered = m_hovered_actor == enemy_unit;
		info.in_main_area = CanActorBeAttackedFromPoint(enemy_unit, m_owner->GetActorLocation());
		info.in_minor_area = CanActorBeAttackedFromPoint(enemy_unit, m_hovered_location + m_owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		info.hp_current = enemy_unit->GetHP();
		info.hp_total = enemy_unit->GetTotalHP();
		info.attack_power_min = enemy_unit->GetAttackPowerMin();
		info.attack_power_max = enemy_unit->GetAttackPowerMax();
		info.turn_points = enemy_unit->GetTurnPointsTotal();
		info.attack_price = enemy_unit->GetAttackPrice();
		info.owner_attack_price = m_owner->GetAttackPrice();
		info.owner_movement_price = GetOwnerMovementPriceToAttackPoint(enemy_unit);
	}
}

void UVEN_TactialView::UpdateDecals()
{
	if (!m_is_allowed)
	{
		if (m_main_area_decal_actor)
		{
			m_main_area_decal_actor->Destroy();
		}
		if (m_minor_area_decal_actors.Num())
		{
			for (const auto& actor : m_minor_area_decal_actors)
				actor->Destroy();
		}

		m_main_area_decal_actor = nullptr;
		m_minor_area_decal_actors = {};

		return;
	}

	const float DECAL_DEPTH = 500.f;
	const float DECAL_HEIGHT_OFFSET = 450.f;
	const float DECAL_DIAMETER_OWNER = m_owner->GetAttackRange() * 2;
	FVector owner_location = m_owner->GetActorLocation();
	FVector decal_actor_location = FVector(owner_location.X, owner_location.Y, (owner_location.Z - m_owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - DECAL_HEIGHT_OFFSET));

	//update main decal
	if (!m_main_area_decal_actor)
	{
		const auto& owner_attack_type = m_owner->GetAttackType();
		UMaterialInstance* main_material = nullptr;
		if (owner_attack_type == ATTACK_TYPE::MELEE)
			main_material = m_main_area_decal_material_sword;
		else if (owner_attack_type == ATTACK_TYPE::RANGE)
			main_material = m_main_area_decal_material_bow;

		if (!main_material)
			return;

		FActorSpawnParameters spawn_params;
		m_main_area_decal_actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), decal_actor_location, FRotator::ZeroRotator, spawn_params);

		if (!m_main_area_decal_actor)
			return;

		UDecalComponent* decal = NewObject<UDecalComponent>(m_main_area_decal_actor, "Tactical View Main Decal");
		if (decal)
		{
			decal->RegisterComponent();
			decal->SetMaterial(0, main_material);
			decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
			decal->DecalSize = FVector(DECAL_DEPTH, DECAL_DIAMETER_OWNER, DECAL_DIAMETER_OWNER);
		}

		m_main_area_decal_actor->SetRootComponent(decal);
		m_main_area_decal_actor->SetActorEnableCollision(false);
	}

	if (m_main_area_decal_actor)
		m_main_area_decal_actor->SetActorLocation(decal_actor_location);

	/*
		implement for minor area decal actor if needed
	*/

	//enemy units decals
	TArray<AActor*> enemy_units_raw = m_battle_system->GetEnemyUnitsInBattle();
	TArray<AVEN_EnemyUnit*> enemy_units;
	for (const auto& enemy_unit_raw : enemy_units_raw)
	{
		const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(enemy_unit_raw);
		if (enemy_unit_c && !enemy_unit_c->IsDead())
			enemy_units.Add(enemy_unit_c);
	}

	if (m_minor_area_decal_actors.Num() != enemy_units.Num())
	{
		for (const auto& actor : m_minor_area_decal_actors)
			actor->Destroy();
		m_minor_area_decal_actors = {};


		for (int i = 0; i < enemy_units.Num(); ++i)
		{
			const float DECAL_DIAMETER_ENEMY = enemy_units[i]->GetAttackRange() * 2;
			owner_location = enemy_units[i]->GetActorLocation();
			decal_actor_location = FVector(owner_location.X, owner_location.Y, (owner_location.Z - enemy_units[i]->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - DECAL_HEIGHT_OFFSET));

			if (!m_main_area_decal_material_minor)
				return;

			FActorSpawnParameters spawn_params_minor;
			auto enemy_decal_actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), decal_actor_location, FRotator::ZeroRotator, spawn_params_minor);
			if (!enemy_decal_actor)
				return;

			UDecalComponent* decal = NewObject<UDecalComponent>(enemy_decal_actor, "Tactical View Minor Decal");
			if (decal)
			{
				decal->RegisterComponent();
				decal->SetMaterial(0, m_main_area_decal_material_minor);
				decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
				decal->DecalSize = FVector(DECAL_DEPTH, DECAL_DIAMETER_ENEMY, DECAL_DIAMETER_ENEMY);
			}

			enemy_decal_actor->SetRootComponent(decal);
			enemy_decal_actor->SetActorEnableCollision(false);

			m_minor_area_decal_actors.Add(enemy_decal_actor);
		}
	}

	if (m_minor_area_decal_actors.Num() == enemy_units.Num())
	{
		for (int i = 0; i < enemy_units.Num(); ++i)
		{
			owner_location = enemy_units[i]->GetActorLocation();
			decal_actor_location = FVector(owner_location.X, owner_location.Y, (owner_location.Z - enemy_units[i]->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - DECAL_HEIGHT_OFFSET));

			m_minor_area_decal_actors[i]->SetActorLocation(decal_actor_location);
		}
	}
}

void UVEN_TactialView::NotifyOwner()
{
	const float custom_z = (m_owner->GetActorLocation().Z - m_owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector main_area_center_point = FVector(m_owner->GetActorLocation().X, m_owner->GetActorLocation().Y, custom_z);
	FVector minor_area_center_point = IsHoveredUnit() ? FVector::ZeroVector : FVector(m_hovered_location.X, m_hovered_location.Y, custom_z);

	m_owner->OnUpdateFromTacticalView(main_area_center_point, minor_area_center_point, m_enemy_units_info);
}

bool UVEN_TactialView::IsHoveredUnit()
{
	bool is_player_unit = Cast<AVEN_PlayerUnit>(m_hovered_actor) != nullptr;
	bool is_enemy_unit = Cast<AVEN_EnemyUnit>(m_hovered_actor) != nullptr;
	bool is_npc_unit = Cast<AVEN_InteractableNPC>(m_hovered_actor) != nullptr;

	return (is_player_unit || is_enemy_unit || is_npc_unit);
}

bool UVEN_TactialView::CanActorBeAttackedFromPoint(AActor* actor, FVector point) const
{
	FHitResult trace_hit_result;
	FVector trace_start_point = point;
	FVector trace_end_point = actor->GetActorLocation();
	FCollisionQueryParams trace_params;
	trace_params.AddIgnoredActor(m_owner);

	bool no_obstacles_on_attack_trace = true;
	bool valid_attack_range = true;

	if (m_owner->GetAttackRange() < calculate_distance(trace_start_point, trace_end_point, true))
		valid_attack_range = false;

	if (GetWorld()->LineTraceSingleByChannel(trace_hit_result, trace_start_point, trace_end_point, ECC_Visibility, trace_params))
	{
		if (trace_hit_result.GetActor() != actor)
			no_obstacles_on_attack_trace = false;
	}

	if (valid_attack_range && no_obstacles_on_attack_trace)
		return true;

	return false;
}

float UVEN_TactialView::GetOwnerMovementPriceToAttackPoint(AActor* actor)
{
	FVector target_location = actor->GetActorLocation();
	float movement_price = 0.f;

	//do nothing if in range
	if (calculate_distance(m_owner->GetActorLocation(), target_location, true) <= m_owner->GetAttackRange())
		return movement_price;

	BuildMovementSpline(target_location);
	FVector interaction_point_location = GetAttackPointLocation(actor);
	BuildMovementSpline(interaction_point_location);

	if (m_movement_spline_is_built)
		movement_price = m_movement_spline_component->GetSplineLength();

	DestroyMovementSpline();
	return movement_price;
}

void UVEN_TactialView::BuildMovementSpline(FVector destination_point)
{
	//destroy previous temporary spline if any
	DestroyMovementSpline();

	//calculate navigation path
	float half_capsule_height = 0.f;
	float capsule_radius = 0.f;
	m_owner->GetCapsuleComponent()->GetScaledCapsuleSize(capsule_radius, half_capsule_height);
	FVector start_movement_point = m_owner->GetCapsuleComponent()->GetComponentLocation();
	start_movement_point.Z -= half_capsule_height;

	UNavigationPath* movement_path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), start_movement_point, destination_point, m_owner);
	m_movement_spline_is_built = (movement_path->PathPoints.Num() > 0);

	if (!m_movement_spline_is_built)
		return;

	//create temporary spline based on path
	FActorSpawnParameters spawn_params;
	m_movement_spline_actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), start_movement_point, FRotator::ZeroRotator, spawn_params);
	m_movement_spline_component = NewObject<USplineComponent>(m_movement_spline_actor, "Movement Spline");
	m_movement_spline_component->RegisterComponent();
	m_movement_spline_component->RemoveSplinePoint(1);

	if (!m_movement_spline_component)
		return;

	m_movement_spline_actor->SetRootComponent(m_movement_spline_component);
	m_movement_spline_actor->SetActorLocation(start_movement_point);

	//setup created spline with points
	for (int i = 1; i < movement_path->PathPoints.Num(); ++i)
	{
		m_movement_spline_component->AddSplinePoint(movement_path->PathPoints[i], ESplineCoordinateSpace::World);
		m_movement_spline_component->SetSplinePointType(i, ESplinePointType::CurveClamped);
	}
}

void UVEN_TactialView::DestroyMovementSpline()
{
	m_movement_spline_is_built = false;

	if (m_movement_spline_actor)
		m_movement_spline_actor->Destroy();

	m_movement_spline_actor = nullptr;
	m_movement_spline_component = nullptr;
}

FVector UVEN_TactialView::GetAttackPointLocation(AActor* actor) const
{
	if (!m_movement_spline_component)
		return FVector::ZeroVector;

	const FVector target_location = actor->GetActorLocation();
	const int CHECKPOINTS_FREQUENCY_FACTOR = 50;
	const int CHECKPOINTS_COUNT = (int)(m_movement_spline_component->GetSplineLength() / CHECKPOINTS_FREQUENCY_FACTOR);

	for (int i = 0; i < CHECKPOINTS_COUNT; ++i)
	{
		const float time_at_current_checkpoint = (m_movement_spline_component->Duration / CHECKPOINTS_COUNT) * i;
		FVector checkpoint_location = m_movement_spline_component->GetLocationAtTime(time_at_current_checkpoint, ESplineCoordinateSpace::World);
		checkpoint_location.Z += m_owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		//target is too far from current checkpoint
		if (calculate_distance(checkpoint_location, target_location, true) > m_owner->GetAttackRange())
			continue;

		//trace if is interation range
		FHitResult trace_hit_result;
		FCollisionQueryParams trace_params;
		trace_params.AddIgnoredActor(m_owner);

		if (GetWorld()->LineTraceSingleByChannel(trace_hit_result, checkpoint_location, target_location, ECC_Visibility, trace_params))
		{
			if (trace_hit_result.GetActor() == actor)
				return checkpoint_location;
		}
	}

	return FVector::ZeroVector;
}