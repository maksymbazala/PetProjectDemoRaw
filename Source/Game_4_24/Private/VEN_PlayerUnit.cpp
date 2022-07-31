// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_PlayerUnit.h"
#include "VEN_InteractableItem.h"
#include "VEN_InteractableNPC.h"
#include "VEN_GameMode.h"
#include "VEN_MainController.h"
#include "VEN_EnemyUnit.h"
#include "VEN_Camera.h"
#include "VEN_BattleSystem.h"
#include "VEN_AnimInstance.h"
#include "VEN_CursorManager.h"
#include "VEN_TactialView.h"
#include "VEN_Types.h"
#include "VEN_FreeFunctions.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstance.h"

namespace
{
	constexpr const float MOVEMENT_MIN_DISTANCE = 100.f;
	constexpr const float HP_RECOVERY_PERCENT = .3f;
	constexpr const float HP_RECOVERY_SPEED = 0.9f;
	constexpr const float BATTLE_INITIATE_DISTANCE = 1000.f;

	const FName INTERACTABLE_OBJECT_TAG = "interactable_object";
}

using namespace vendetta;

AVEN_PlayerUnit::AVEN_PlayerUnit()
	: m_is_active(false)
	, m_movement_spline_actor_temporary(nullptr)
	, m_movement_spline_component_temporary(nullptr)
	, m_movement_spline_actor_fixed(nullptr)
	, m_movement_spline_component_fixed(nullptr)
	, m_movement_spline_mesh_enabled(nullptr)
	, m_movement_spline_mesh_disabled(nullptr)
	, m_movement_destination_point_actor(nullptr)
	, m_movement_destination_point_mesh(nullptr)
	, m_movement_destination_point_decal_material(nullptr)
	, m_focused_target(nullptr)
	, m_is_currently_moving(false)
	, m_temporary_movement_spline_is_built(false)
	, m_movement_spline_is_visualized(false)
	, m_spline_comleted_movement_distance(0.f)
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SplineEnabledMeshCompObject(TEXT("/Game/Meshes/MovementSpline/SM_MovementSplineEnabled.SM_MovementSplineEnabled"));
	m_movement_spline_mesh_enabled = SplineEnabledMeshCompObject.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SplineDisabledMeshCompObject(TEXT("/Game/Meshes/MovementSpline/SM_MovementSplineDisabled.SM_MovementSplineDisabled"));
	m_movement_spline_mesh_disabled = SplineDisabledMeshCompObject.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DestinationPointMeshCompObject(TEXT("/Game/Meshes/DestinationPoint/SM_DestinationPoint.SM_DestinationPoint"));
	m_movement_destination_point_mesh = DestinationPointMeshCompObject.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInstance> DestinationPointDecalMaterial(TEXT("MaterialInstance'/Game/Materials/DestinationPoint/MI_DestinationPointDecal.MI_DestinationPointDecal'"));
	m_movement_destination_point_decal_material = DestinationPointDecalMaterial.Object;

	m_decal = CreateDefaultSubobject<UDecalComponent>("ActiveUnitDecal");
	m_decal->SetupAttachment(RootComponent);
}

void AVEN_PlayerUnit::BeginPlay()
{
	Super::BeginPlay();

	m_decal->SetVisibility(false);
	m_show_advanced_cursor = false;
}

void AVEN_PlayerUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (m_is_currently_moving)
		ContinueMovement();

	if (m_hp_recovery)
		RecoverHPs();
}

void AVEN_PlayerUnit::OnMouseAction(FHitResult hit_result, input_bindings::MOUSE_ACTION mouse_action, float axis)
{
	if (!GetActive())
		return;

	HandleMouseAction(hit_result, mouse_action);
}

void AVEN_PlayerUnit::OnKeyboardAction(input_bindings::KEYBOARD_ACTION keyboard_action, float axis)
{
	if (keyboard_action == input_bindings::KEYBOARD_ACTION::KEYBOARD_SPACE)
	{
		if (!m_anim_instance->IsFree())
			return;

		const auto& battle_system = GetGameMode()->GetBattleSystem();
		if (m_in_battle && !m_is_currently_moving && battle_system && battle_system->IsCurrentTurnOwner(this))
			battle_system->FinishCurrentTurn(this);
	}
}

void AVEN_PlayerUnit::Initialize()
{
	RegisterTacticalViewInstance();
	InitBattleRelatedProperties();
	NotifyBlueprint();
}

void AVEN_PlayerUnit::Uninitialize()
{
	DestroyDestinationPointActor();
	DisableTacticalView();
}

bool AVEN_PlayerUnit::GetActive() const
{
	return m_is_active;
}

void AVEN_PlayerUnit::SetActive(bool is_active)
{
	m_is_active = is_active;
	OnMapIconUpdate(is_active);
	NotifyBlueprint();

	if (!m_is_active)
		DestroyDestinationPointActor();
}

void AVEN_PlayerUnit::FollowMyLead(TArray<FVector> points_to_move, float time_to_reach_point)
{
	if (!m_anim_instance->IsFree())
		return;

	FinishMovement();

	TArray<FVector> sorted_points = points_to_move;
	SortClosestPoints(sorted_points);

	for (int32_t i = 0; i < sorted_points.Num(); ++i)
	{
		FVector closest_point = sorted_points[i];
		BuildTemporaryMovementSpline(closest_point);

		if (m_temporary_movement_spline_is_built)
			break;
	}

	if (m_temporary_movement_spline_is_built)
	{
		float time_to_complete_follow = CalculateTimeToCompleteMovementSpline(m_movement_spline_component_temporary);
		if (time_to_complete_follow > 0.f && time_to_complete_follow < time_to_reach_point)
		{
			//start movement with delay to complete movement of both player units at the same time (lag 0.5 sec)
			GetWorldTimerManager().ClearTimer(m_tmr_before_move);
			GetWorldTimerManager().SetTimer(m_tmr_before_move, this, &AVEN_PlayerUnit::StartMovement, (time_to_reach_point - time_to_complete_follow + 0.5f));
		}
		else
			StartMovement();
	}
}

void AVEN_PlayerUnit::RegisterAnimInstance(UVEN_AnimInstance* instance)
{
	//if (!instance)
		//debug_log("AVEN_PlayerUnit::RegisterAnimInstance. Instance is broken", FColor::Red);

	m_anim_instance = instance;
}

void AVEN_PlayerUnit::OnAnimationUpdate(ANIMATION_NOTIFICATION notification)
{
	if (notification == ANIMATION_NOTIFICATION::ATTACK_ANIMATION_UPDATED)
	{
		const auto& battle_system = GetBattleSystem();
		if (battle_system && battle_system->IsBattleInProgress() && m_focused_target && m_is_active)
		{
			GetBattleSystem()->Attack(this, m_focused_target);
			if (m_in_battle)
				UpdateTurnPoints(-m_attack_price);
		}
	}
	else if (notification == ANIMATION_NOTIFICATION::ITEM_PICK_ANIMATION_UPDATED)
	{
		const auto& casted_interactable_item = Cast<AVEN_InteractableItem>(m_focused_target);
		if (casted_interactable_item && casted_interactable_item->IsCollectible())
		{
			GetGameMode()->AddItemToInventory(casted_interactable_item->GetUniqueActorId());
			casted_interactable_item->OnUpdate(ACTOR_UPDATE::REMOVE);
		}
	}
}

int AVEN_PlayerUnit::GetXP() const
{
	return m_xp_current;
}

void AVEN_PlayerUnit::IncrementXP(int increment_value)
{
	m_xp_current += increment_value;

	if (m_xp_current > m_xp_total)
		m_xp_current = m_xp_total;

	NotifyBlueprint();
}

int AVEN_PlayerUnit::GetMoney() const
{
	return m_money;
}

void AVEN_PlayerUnit::IncrementMoney(int increment_value)
{
	m_money += increment_value;
	NotifyBlueprint();
}

FString AVEN_PlayerUnit::GetUnitName() const
{
	return m_unit_name;
}

bool AVEN_PlayerUnit::IsInBattle() const
{
	return m_in_battle;
}

void AVEN_PlayerUnit::OnStartBattle()
{
	GetWorldTimerManager().ClearTimer(m_tmr_before_move);

	//stop current movement
	if (m_is_currently_moving)
	{
		FinishMovement();
		DestroyMovementSpline(m_movement_spline_actor_fixed);
		DestroyDestinationPointActor();

		if (!GetActive())
			DestroyMovementSpline(m_movement_spline_actor_temporary);
	}

	ResetBattleRelatedProperties();
	AnimInstanceUpdate(ANIMATION_UPDATE::START_BATTLE);
	UpdateCursor(nullptr);
	SetActive(false);

	m_in_battle = true;
	m_hp_recovery = false;
}

void AVEN_PlayerUnit::OnFinishBattle()
{
	m_in_battle = false;
	DisableTacticalView();
	UpdateBattleTurnInfo(false);

	const auto& batte_system = GetBattleSystem();
	if (batte_system && !batte_system->IsGameOver())
	{
		//finish stun if was stunned in fight
		if (m_is_dead)
			AnimInstanceUpdate(ANIMATION_UPDATE::FINISH_STUN);

		m_is_dead = false;
		GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	}

	AnimInstanceUpdate(ANIMATION_UPDATE::FINISH_BATTLE);
	NotifyBlueprint();

	// recover hp after battle
	if (m_hp_current < m_hp_total * HP_RECOVERY_PERCENT)
		m_hp_recovery = true;
}

void AVEN_PlayerUnit::OnStartBattleTurn()
{
	m_decal->SetVisibility(true);
	SetActive(true);
	UpdateBattleTurnInfo();
	GetGameMode()->GetMainCamera()->SetCameraFollowMode(true, this);

	m_show_advanced_cursor = true;
}

void AVEN_PlayerUnit::OnFinishBattleTurn()
{
	DisableTacticalView();
	m_decal->SetVisibility(false);
	UpdateCursor(nullptr);
	SetActive(false);
	DestroyMovementSpline(m_movement_spline_actor_temporary);

	m_show_advanced_cursor = false;
	UpdateAdvancedCursor(nullptr, false);
	UpdateBattleTurnInfo(false);
	GetGameMode()->GetMainCamera()->SetCameraFollowMode(false);
}

void AVEN_PlayerUnit::OnReceiveDamage(float damage_received, AActor* damage_dealer)
{
	m_hp_current -= damage_received;

	if (m_hp_current > m_hp_total)
	{
		m_hp_current = m_hp_total;
	}
	else if (m_hp_current <= 1)
	{
		m_hp_current = 1;
		Die();
	}

	if (!m_is_dead)
	{
		DIRECTION attack_direction = GetAttackDirection(damage_dealer->GetActorRotation());
		ANIMATION_ACTION receive_damage_anim_action = ANIMATION_ACTION::IDLE;

		switch (attack_direction)
		{
			case DIRECTION::BACK: receive_damage_anim_action = ANIMATION_ACTION::RECEIVE_DAMAGE_BACK; break;
			case DIRECTION::FRONT: receive_damage_anim_action = ANIMATION_ACTION::RECEIVE_DAMAGE_FRONT; break;
			case DIRECTION::LEFT: receive_damage_anim_action = ANIMATION_ACTION::RECEIVE_DAMAGE_LEFT; break;
			case DIRECTION::RIGHT: receive_damage_anim_action = ANIMATION_ACTION::RECEIVE_DAMAGE_RIGHT; break;
			default: break;
		}

		AnimInstanceAction(receive_damage_anim_action);
	}

	NotifyBlueprint();
	GetBattleSystem()->UpdateBlueprintBattleQueue();
}

vendetta::ATTACK_TYPE AVEN_PlayerUnit::GetAttackType() const
{
	return (vendetta::ATTACK_TYPE)m_attack_type;
}

int AVEN_PlayerUnit::GetHP() const
{
	return m_hp_current;
}

int AVEN_PlayerUnit::GetTotalHP() const
{
	return m_hp_total;
}

int AVEN_PlayerUnit::GetRandomAttackPower() const
{
	return FMath::RandRange(m_attack_power_min, m_attack_power_max);
}

int AVEN_PlayerUnit::GetAttackRange() const
{
	return m_attack_range;
}

float AVEN_PlayerUnit::GetAttackPrice() const
{
	return m_attack_price;
}

float AVEN_PlayerUnit::GetTurnPointsTotal() const
{
	return m_turn_points_total;
}

float AVEN_PlayerUnit::GetTurnPointsLeft() const
{
	return m_turn_points_left;
}

void AVEN_PlayerUnit::UpdateTurnPoints(float update_on_value)
{
	m_turn_points_left += update_on_value;

	if (m_turn_points_left > m_turn_points_total)
	{
		m_turn_points_left = m_turn_points_total;
	}
	else if (m_turn_points_left <= 0.f)
	{
		m_turn_points_left = 0.f;
	}

	NotifyBlueprint();
	GetBattleSystem()->UpdateBlueprintBattleQueue();

	if (m_in_battle)
		UpdateBattleTurnInfo();
}

void AVEN_PlayerUnit::ResetTurnPoints()
{
	m_turn_points_left = m_turn_points_total;
}

bool AVEN_PlayerUnit::IsDead() const
{
	return m_is_dead;
}

void AVEN_PlayerUnit::ToggleTacticalView()
{
	if (m_tactical_view_is_allowed)
		DisableTacticalView();
	else
	{
		if (m_in_battle && m_anim_instance->IsFree() && !m_is_currently_moving)
			m_tactical_view_is_allowed = true;
	}
}

void AVEN_PlayerUnit::OnUpdateFromTacticalView(FVector main_area_center_point, FVector minor_area_center_point, const TArray<FEnemyUnitInfo>& enemy_units_info)
{
	OnTacticalViewUpdate(true, enemy_units_info);
}

void AVEN_PlayerUnit::OnRequestUnitInfo()
{
	NotifyBlueprint();
}

AVEN_GameMode* AVEN_PlayerUnit::GetGameMode() const
{
	const auto& game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	if (!game_mode)
	{
		//debug_log("AVEN_PlayerUnit::GetGameMode. Game Mode not found!", FColor::Red);
		return nullptr;
	}

	return game_mode;
}

AVEN_MainController* AVEN_PlayerUnit::GetMainController() const
{
	const auto& main_controller = Cast<AVEN_MainController>(GetGameMode()->GetMainController());
	if (!main_controller)
	{
		//debug_log("AVEN_PlayerUnit::GetMainController. Main Controller not found!", FColor::Red);
	}

	return main_controller;
}

UVEN_BattleSystem* AVEN_PlayerUnit::GetBattleSystem() const
{
	const auto& battle_system = GetGameMode()->GetBattleSystem();
	if (!battle_system)
	{
		//debug_log("AVEN_PlayerUnit::GetBattleSystem. Battle System not found!", FColor::Red);
	}

	return battle_system;
}

void AVEN_PlayerUnit::HandleMouseAction(FHitResult hit_result, input_bindings::MOUSE_ACTION mouse_action)
{
	if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MOVE_X || mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MOVE_Y)
	{
		ProcessInteractableActorOnHover(hit_result.GetActor());

		if (!m_move_and_interact_spline_calculated)
			BuildTemporaryMovementSpline(hit_result.Location);

		UpdateCursor(hit_result.GetActor());

		if (m_show_advanced_cursor)
			UpdateAdvancedCursor(hit_result.GetActor());

		if (m_tactical_view_is_allowed)
			m_tactical_view_instance->OnRequestData(true, hit_result.GetActor(), hit_result.Location);
	}
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_RIGHT)
	{
		const auto& hit_actor = hit_result.GetActor();
		const auto& hit_enemy_unit = Cast<AVEN_EnemyUnit>(hit_actor);
		const auto& hit_interactable_npc = Cast<AVEN_InteractableNPC>(hit_actor);
		const auto& hit_interactable_item = Cast<AVEN_InteractableItem>(hit_actor);

		if (Cast<AVEN_PlayerUnit>(hit_actor))
		{
			// there is a custom right click on second player unit handler in Main Controller
			// no need for any actions here
			return;
		}
		else if (hit_enemy_unit)
		{
			if (CanAttackInPlace(hit_enemy_unit))
			{
				ChangeFocusedTarget(hit_actor);
				PrepareAttackAnimation();
			}
			else if (CanAttackAfterMove(hit_enemy_unit))
			{
				ChangeFocusedTarget(hit_actor);
				m_move_and_interact_mode = true;
				PrepareMovement(hit_result);
			}
			else if (CanInitiateBattleInPlace(hit_enemy_unit))
			{
				ChangeFocusedTarget(hit_actor);
				GetBattleSystem()->StartBattle(this, hit_enemy_unit);
			}
			else if (CanInitiateBattleAfterMove(hit_enemy_unit))
			{
				ChangeFocusedTarget(hit_actor);
				m_move_and_interact_mode = true;
				PrepareMovement(hit_result);
			}
			return;
		}
		else if (hit_interactable_npc)
		{
			if (CanInteractWithNpcInPlace(hit_interactable_npc))
			{
				ChangeFocusedTarget(hit_actor);
				RotateToFocusedTarget();
				hit_interactable_npc->OnUpdate(ACTOR_UPDATE::INTERACT, this);
			}
			else if (CanInteractWithNpcAfterMove(hit_interactable_npc))
			{
				ChangeFocusedTarget(hit_actor);
				m_move_and_interact_mode = true;
				PrepareMovement(hit_result);
			}
			return;
		}
		else if (hit_interactable_item)
		{
			if (CanInteractWithItemInPlace(hit_interactable_item))
			{
				ChangeFocusedTarget(hit_actor);
				RotateToFocusedTarget();
				hit_interactable_item->OnUpdate(ACTOR_UPDATE::INTERACT, this);
				AnimInstanceAction(ANIMATION_ACTION::PICK_ITEM);
			}
			else if (CanInteractWithItemAfterMove(hit_interactable_item))
			{
				ChangeFocusedTarget(hit_actor);
				m_move_and_interact_mode = true;
				PrepareMovement(hit_result);
			}
			return;
		}

		if (m_anim_instance->IsFree())
			ChangeFocusedTarget(nullptr);

		if (CanMoveToPreCalculatedPoint())
			PrepareMovement(hit_result);
	}
}

void AVEN_PlayerUnit::BuildTemporaryMovementSpline(const FVector& destination_point)
{
	//destroy previous temporary spline if any
	DestroyMovementSpline(m_movement_spline_actor_temporary);

	if (m_in_battle && m_is_currently_moving)
		return;

	//calculate navigation path
	float half_capsule_height = 0.f;
	float capsule_radius = 0.f;
	GetCapsuleComponent()->GetScaledCapsuleSize(capsule_radius, half_capsule_height);
	FVector start_movement_point = GetCapsuleComponent()->GetComponentLocation();
	start_movement_point.Z -= half_capsule_height;

	UNavigationPath* movement_path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), start_movement_point, destination_point, this);
	m_temporary_movement_spline_is_built = (movement_path->PathPoints.Num() > 0);

	if (!m_temporary_movement_spline_is_built)
		return;

	//create temporary spline based on path
	FActorSpawnParameters spawn_params;
	m_movement_spline_actor_temporary = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), start_movement_point, FRotator::ZeroRotator, spawn_params);
	m_movement_spline_component_temporary = NewObject<USplineComponent>(m_movement_spline_actor_temporary, "Movement Spline");
	m_movement_spline_component_temporary->RegisterComponent();
	m_movement_spline_component_temporary->RemoveSplinePoint(1);

	if (!m_movement_spline_component_temporary)
		return;

	m_movement_spline_actor_temporary->SetRootComponent(m_movement_spline_component_temporary);
	m_movement_spline_actor_temporary->SetActorLocation(start_movement_point);

	//setup created spline with points
	for (int i = 1; i < movement_path->PathPoints.Num(); ++i)
	{
		m_movement_spline_component_temporary->AddSplinePoint(movement_path->PathPoints[i], ESplineCoordinateSpace::World);
		m_movement_spline_component_temporary->SetSplinePointType(i, ESplinePointType::CurveClamped);
	}

	//visualize spline with meshes
	if (m_movement_spline_is_visualized)
	{
		for (int i = 0; i < m_movement_spline_component_temporary->GetNumberOfSplinePoints() - 1; ++i)
		{
			const FString COMPONENT_NAME = "Movement Spline Mesh" + FString::FromInt(i);

			USplineMeshComponent* spline_mesh_comp = NewObject<USplineMeshComponent>(m_movement_spline_actor_temporary, *COMPONENT_NAME);
			spline_mesh_comp->RegisterComponent();

			const auto& movement_spline_mesh = IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::MOVE) ? m_movement_spline_mesh_enabled : m_movement_spline_mesh_disabled;
			if (spline_mesh_comp && movement_spline_mesh)
			{
				spline_mesh_comp->SetMobility(EComponentMobility::Movable);
				spline_mesh_comp->SetStaticMesh(movement_spline_mesh);
				spline_mesh_comp->SetForwardAxis(ESplineMeshAxis::Z);

				spline_mesh_comp->SetStartAndEnd
				(
					m_movement_spline_component_temporary->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local),
					m_movement_spline_component_temporary->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local),
					m_movement_spline_component_temporary->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local),
					m_movement_spline_component_temporary->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local)
				);

				FAttachmentTransformRules attachment_rules(EAttachmentRule::KeepRelative, true);
				spline_mesh_comp->AttachToComponent(m_movement_spline_component_temporary, attachment_rules);
			}
		}
	}
}

void AVEN_PlayerUnit::BuildFixedMovementSpline()
{
	if (!m_movement_spline_actor_temporary || !m_movement_spline_component_temporary)
	{
		//debug_log("AVEN_PlayerUnit::BuildFixedMovementSpline. Movement spline is corrupted", FColor::Red);
		return;
	}

	FActorSpawnParameters spawn_params;
	const auto& spawn_location = m_movement_spline_component_temporary->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	m_movement_spline_actor_fixed = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), spawn_location, FRotator::ZeroRotator, spawn_params);
	m_movement_spline_component_fixed = NewObject<USplineComponent>(m_movement_spline_actor_fixed, "Movement Spline");
	m_movement_spline_component_fixed->RegisterComponent();
	m_movement_spline_component_fixed->RemoveSplinePoint(1);

	if (!m_movement_spline_actor_fixed)
		return;

	m_movement_spline_actor_fixed->SetRootComponent(m_movement_spline_component_fixed);
	m_movement_spline_actor_fixed->SetActorLocation(spawn_location);

	//setup created spline with points
	for (int i = 0; i < m_movement_spline_component_temporary->GetNumberOfSplinePoints(); ++i)
	{
		m_movement_spline_component_fixed->AddSplinePoint(m_movement_spline_component_temporary->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World);
		m_movement_spline_component_fixed->SetSplinePointType(i, ESplinePointType::CurveClamped);
	}
}

void AVEN_PlayerUnit::DestroyMovementSpline(AActor* movement_spline)
{
	if (movement_spline)
	{
		movement_spline->Destroy();
		movement_spline = nullptr;
	}
}

void AVEN_PlayerUnit::BuildDestinationPointActor(FVector destination_point)
{
	DestroyDestinationPointActor();

	if (!m_movement_destination_point_mesh || !m_movement_destination_point_decal_material)
		return;

	FActorSpawnParameters spawn_params;
	m_movement_destination_point_actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), destination_point, FRotator::ZeroRotator, spawn_params);

	if (!m_movement_destination_point_actor)
		return;

	USceneComponent* root = NewObject<UStaticMeshComponent>(m_movement_destination_point_actor, "Empty Root");
	if (root)
	{
		root->RegisterComponent();
		m_movement_destination_point_actor->SetRootComponent(root);
	}

	UStaticMeshComponent* mesh = NewObject<UStaticMeshComponent>(m_movement_destination_point_actor, "Destination Point Mesh");
	if (mesh)
	{
		mesh->RegisterComponent();
		mesh->SetStaticMesh(m_movement_destination_point_mesh);
		mesh->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
		mesh->AttachToComponent(m_movement_destination_point_actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	UDecalComponent* decal = NewObject<UDecalComponent>(m_movement_destination_point_actor, "Destination Point Decal");
	if (decal)
	{
		decal->RegisterComponent();
		decal->SetMaterial(0, m_movement_destination_point_decal_material);
		decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		decal->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
		decal->DecalSize = FVector(50.f, 100.f, 100.f);
		decal->AttachToComponent(m_movement_destination_point_actor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}

	m_movement_destination_point_actor->SetActorEnableCollision(false);
	m_movement_destination_point_actor->SetActorLocation(destination_point);
}

void AVEN_PlayerUnit::DestroyDestinationPointActor()
{
	if (m_movement_destination_point_actor)
	{
		m_movement_destination_point_actor->Destroy();
		m_movement_destination_point_actor = nullptr;
	}
}

void AVEN_PlayerUnit::SetupDestinationPointActor(FVector destination_point)
{
	if (m_move_and_interact_mode && m_movement_spline_component_temporary)
		destination_point = m_movement_spline_component_temporary->GetLocationAtSplinePoint(m_movement_spline_component_temporary->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);

	BuildDestinationPointActor(destination_point);
}

float AVEN_PlayerUnit::CalculateTimeToCompleteMovementSpline(USplineComponent* spline_component)
{
	if (!spline_component)
	{
		//debug_log("AVEN_PlayerUnit::CalculateTimeToCompleteMovementSpline. Spline Component is corrupted", FColor::Red);
		return 0.f;
	}

	return (spline_component->GetSplineLength() / GetMovementComponent()->GetMaxSpeed());
}

void AVEN_PlayerUnit::PrepareMovement(FHitResult hit_result)
{
	if (m_temporary_movement_spline_is_built)
	{
		if ((RootComponent->GetComponentLocation() - hit_result.ImpactPoint).Size() > MOVEMENT_MIN_DISTANCE)
		{
			SetupDestinationPointActor(hit_result.ImpactPoint);
			StartMovement();
		}
	}
}

void AVEN_PlayerUnit::StartMovement()
{
	if (m_is_currently_moving)
	{
		FinishMovement();
		DestroyMovementSpline(m_movement_spline_actor_fixed);
	}

	//re-setup fixed movement spline
	BuildFixedMovementSpline();

	if (!m_movement_spline_component_fixed)
	{
		//debug_log("AVEN_PlayerUnit::StartMovement. m_movement_spline_component_fixed is nullptr", FColor::Red);
		return;
	}

	m_anim_instance->OnUpdateFromOwner(ANIMATION_UPDATE::START_MOVEMENT);
	m_is_currently_moving = true;

	//notify main controller
	const auto& destination_location = m_movement_spline_component_fixed->GetLocationAtSplinePoint(m_movement_spline_component_fixed->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	const auto& destination_rotation = m_movement_spline_component_fixed->GetRotationAtDistanceAlongSpline(m_movement_spline_component_fixed->GetSplineLength() * 0.99, ESplineCoordinateSpace::World);
	GetMainController()->UnitStartedMovement(this, destination_location, destination_rotation, CalculateTimeToCompleteMovementSpline(m_movement_spline_component_fixed));
}

void AVEN_PlayerUnit::ContinueMovement()
{
	if (!m_movement_spline_component_fixed)
	{
		//debug_log("AVEN_PlayerUnit::ContinueMovement. Spline component is missing", FColor::Red);
		return;
	}

	if (m_spline_comleted_movement_distance < m_movement_spline_component_fixed->GetSplineLength())
	{
		m_spline_comleted_movement_distance += GetVelocity().Size() * GetWorld()->GetDeltaSeconds();

		AddMovementInput(GetActorForwardVector(), 1.0f, true);

		FVector spline_location = m_movement_spline_component_fixed->GetLocationAtDistanceAlongSpline(m_spline_comleted_movement_distance, ESplineCoordinateSpace::World);
		FRotator spline_rotation = m_movement_spline_component_fixed->GetRotationAtDistanceAlongSpline(m_spline_comleted_movement_distance, ESplineCoordinateSpace::World);

		spline_location.Z = GetCapsuleComponent()->GetComponentLocation().Z;
		spline_rotation.SetComponentForAxis(EAxis::X, 0.f);
		spline_rotation.SetComponentForAxis(EAxis::Y, 0.f);

		SetActorLocation(spline_location);

		//do not set unit rotation at last spline point - it will always be equal 0.0f
		if (m_movement_spline_component_fixed && m_spline_comleted_movement_distance < (m_movement_spline_component_fixed->GetSplineLength() - 10.f))
			SetActorRotation(spline_rotation);
	}
	else
	{
		FinishMovement();
		DestroyMovementSpline(m_movement_spline_actor_fixed);

		if (m_move_and_interact_mode)
			FinishMoveAndInteract();
		DestroyDestinationPointActor();

		if (!GetActive())
			DestroyMovementSpline(m_movement_spline_actor_temporary);
	}
}

void AVEN_PlayerUnit::FinishMovement()
{
	if (m_in_battle)
		UpdateTurnPoints(-m_spline_comleted_movement_distance);

	m_spline_comleted_movement_distance = 0.f;
	m_is_currently_moving = false;
	m_movement_spline_component_fixed = nullptr;

	GetWorldTimerManager().ClearTimer(m_tmr_before_move);
	GetMainController()->UnitFinishedMovement(this);
}

void AVEN_PlayerUnit::SortClosestPoints(TArray<FVector>& points)
{
	if (!points.Num())
	{
		//debug_log("AVEN_PlayerUnit::SortClosestPoints. No points", FColor::Red);
		return;
	}

	for (int32_t i = 0; i < points.Num() - 1; i++)
	{
		for (int32_t j = 0; j < points.Num() - i - 1; j++)
		{
			if ((GetActorLocation() - points[j]).Size() > (GetActorLocation() - points[j + 1]).Size())
			{
				const auto& tmp = points[j];
				points[j] = points[j + 1];
				points[j + 1] = tmp;
			}
		}
	}
}

bool AVEN_PlayerUnit::IsEnemyUnit(AActor* actor)
{
	return actor && Cast<AVEN_EnemyUnit>(actor);
}

bool AVEN_PlayerUnit::IsNPCUnit(AActor* actor)
{
	return actor && Cast<AVEN_InteractableNPC>(actor);
}

void AVEN_PlayerUnit::AnimInstanceAction(ANIMATION_ACTION anim_action)
{
	if (m_anim_instance)
		m_anim_instance->OnActionFromOwner(anim_action);
}

void AVEN_PlayerUnit::AnimInstanceUpdate(ANIMATION_UPDATE anim_update)
{
	if (m_anim_instance)
		m_anim_instance->OnUpdateFromOwner(anim_update);
}

void AVEN_PlayerUnit::UpdateCursor(AActor* hovered_actor)
{
	//check whether current unit is active and can affect cursor
	if (!GetActive())
		return;

	//get cursor manager
	const auto& cursor_manager = GetGameMode()->GetCursorManager();
	if (!cursor_manager)
	{
		//debug_log("AVEN_PlayerUnit::GetCursorManager. Cursor Manager not found!", FColor::Red);
		return;
	}

	//get camera to check if it is not rotating right now
	const auto& camera = GetGameMode()->GetMainCamera();
	if (!camera)
	{
		//debug_log("AVEN_PlayerUnit::GetCursorManager. Camera not found!", FColor::Red);
		return;
	}

	//default cursor
	CURSOR_TYPE cursor = CURSOR_TYPE::COMMON;

	//just reset cursor if nullptr
	if (!hovered_actor)
	{
		cursor_manager->RequestCursorChange(cursor);
		return;
	}

	//check hovered actors and apply proper customized cursor
	const auto& hit_enemy_unit = Cast<AVEN_EnemyUnit>(hovered_actor);
	const auto& hit_interactable_npc = Cast<AVEN_InteractableNPC>(hovered_actor);
	const auto& hit_interactable_item = Cast<AVEN_InteractableItem>(hovered_actor);

	if (camera->IsCameraInRotationMode())
	{
		cursor = CURSOR_TYPE::CAMERA;
	}
	else if (hit_enemy_unit)
	{
		if (CanAttackInPlace(hit_enemy_unit) || CanAttackAfterMove(hit_enemy_unit) || IsAttackingHoveredActor(hit_enemy_unit))
		{
			if (m_attack_type == (int)ATTACK_TYPE::MELEE)
				cursor = CURSOR_TYPE::ATTACK_MELEE;
			else if (m_attack_type == (int)ATTACK_TYPE::RANGE)
				cursor = CURSOR_TYPE::ATTACK_RANGE;
		}
		else
		{
			if (m_in_battle)
			{
				cursor = CURSOR_TYPE::ERROR;
			}
		}
	}
	else if (hit_interactable_npc)
	{
		if (CanInteractWithNpcInPlace(hit_interactable_npc) || CanInteractWithNpcAfterMove(hit_interactable_npc))
		{
			cursor = CURSOR_TYPE::TALK;
		}
	}
	else if (hit_interactable_item)
	{
		if (CanInteractWithItemInPlace(hit_interactable_item) || CanInteractWithItemAfterMove(hit_interactable_item))
		{
			cursor = CURSOR_TYPE::COLLECT;
		}
	}
	else if (CanMoveToPreCalculatedPoint())
	{
		if (m_in_battle)
		{
			cursor = CURSOR_TYPE::MOVE;
		}
	}
	else
	{
		if (m_in_battle)
		{
			cursor = CURSOR_TYPE::ERROR;
		}
	}

	cursor_manager->RequestCursorChange(cursor);
}

float AVEN_PlayerUnit::GetMovementPrice() const
{
	float price = 0.f;

	if (m_movement_spline_component_temporary)
		price = m_movement_spline_component_temporary->GetSplineLength();

	return price;
}

bool AVEN_PlayerUnit::CanInteractWithNpcInPlace(AVEN_InteractableNPC* npc_unit)
{
	if (!m_anim_instance->IsFree() || m_in_battle)
		return false;

	bool in_interactable_area = npc_unit->GetInteractableRadius() >= calculate_distance(GetActorLocation(), npc_unit->GetActorLocation());
	bool can_be_interacted_right_now = npc_unit->CanBeInteractedRightNow();
	return in_interactable_area && can_be_interacted_right_now;
}

bool AVEN_PlayerUnit::CanInteractWithNpcAfterMove(AVEN_InteractableNPC* npc_unit)
{
	if (!m_anim_instance->IsFree() || m_in_battle)
		return false;

	bool movement_path_is_calculated = GetMovementPrice() > 0.f;
	bool can_be_interacted_right_now = npc_unit->CanBeInteractedRightNow();
	return movement_path_is_calculated && can_be_interacted_right_now;
}

bool AVEN_PlayerUnit::CanInteractWithItemInPlace(AVEN_InteractableItem* item)
{
	if (!m_anim_instance->IsFree() || m_in_battle)
		return false;

	bool in_interactable_area = item->GetInteractableRadius() >= calculate_distance(GetActorLocation(), item->GetActorLocation(), true);
	bool can_be_interacted_right_now = item->CanBeInteractedRightNow();
	return in_interactable_area && can_be_interacted_right_now;
}

bool AVEN_PlayerUnit::CanInteractWithItemAfterMove(AVEN_InteractableItem* item)
{
	if (!m_anim_instance->IsFree() || m_in_battle)
		return false;

	bool movement_path_is_calculated = GetMovementPrice() > 0.f;
	bool can_be_interacted_right_now = item->CanBeInteractedRightNow();
	return movement_path_is_calculated && can_be_interacted_right_now;
}

bool AVEN_PlayerUnit::CanMoveToPreCalculatedPoint()
{
	bool in_idle = m_anim_instance->IsFree();
	bool can_move_in_battle = m_in_battle && !m_is_currently_moving && IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::MOVE);
	return in_idle && m_temporary_movement_spline_is_built && (!m_in_battle || can_move_in_battle);
}

void AVEN_PlayerUnit::ProcessInteractableActorOnHover(AActor* actor)
{
	//reset
	m_move_and_interact_spline_calculated = false;

	if (!m_anim_instance->IsFree())
		return;

	const auto& hit_enemy_unit = Cast<AVEN_EnemyUnit>(actor);
	const auto& hit_interactable_npc = Cast<AVEN_InteractableNPC>(actor);
	const auto& hit_interactable_item = Cast<AVEN_InteractableItem>(actor);

	const bool enemy_valid = hit_enemy_unit != nullptr;
	const bool npc_valid = hit_interactable_npc && hit_interactable_npc->CanBeInteractedRightNow() && !m_in_battle;
	const bool item_valid = hit_interactable_item && hit_interactable_item->CanBeInteractedRightNow() && !m_in_battle;

	if (enemy_valid || npc_valid || item_valid)
	{
		const FVector target_location = actor->GetActorLocation();

		float interaction_range = 0.f;
		if (enemy_valid)
			interaction_range = m_in_battle ? m_attack_range : BATTLE_INITIATE_DISTANCE;
		else if (npc_valid)
			interaction_range = hit_interactable_npc->GetInteractableRadius();
		else if (item_valid)
			interaction_range = hit_interactable_item->GetInteractableRadius();

		//do nothing if in range
		if (calculate_distance(GetActorLocation(), target_location, true) <= interaction_range)
		{
			DestroyMovementSpline(m_movement_spline_actor_temporary);
			return;
		}

		//process move and interact if not in interaction range
		BuildTemporaryMovementSpline(target_location);
		FVector interaction_point_location = GetInteractPointLocation(actor, interaction_range);
		m_move_and_interact_spline_calculated = interaction_point_location != FVector::ZeroVector;
		m_move_and_interact_spline_calculated ? BuildTemporaryMovementSpline(interaction_point_location) : DestroyMovementSpline(m_movement_spline_actor_temporary);
	}
}

void AVEN_PlayerUnit::FinishMoveAndInteract()
{
	m_move_and_interact_mode = false;

	const auto& casted_enemy_unit = Cast<AVEN_EnemyUnit>(m_focused_target);
	if (casted_enemy_unit)
	{
		m_in_battle ? PrepareAttackAnimation() : GetBattleSystem()->StartBattle(this, casted_enemy_unit);
		return;
	}

	const auto& casted_interactable_item = Cast<AVEN_InteractableItem>(m_focused_target);
	if (casted_interactable_item)
	{
		casted_interactable_item->OnUpdate(ACTOR_UPDATE::INTERACT, this);
		RotateToFocusedTarget();
		AnimInstanceAction(ANIMATION_ACTION::PICK_ITEM);
		return;
	}

	const auto& casted_interactable_npc = Cast<AVEN_InteractableNPC>(m_focused_target);
	if (casted_interactable_npc)
	{
		casted_interactable_npc->OnUpdate(ACTOR_UPDATE::INTERACT, this);
		RotateToFocusedTarget();
		return;
	}

	//debug_log("AVEN_PlayerUnit::FinishMoveAndInteract. Invalid focused target", FColor::Red);
}

FVector AVEN_PlayerUnit::GetInteractPointLocation(AActor* interactable_actor, float interaction_radius) const
{
	if (!m_movement_spline_component_temporary)
		return FVector::ZeroVector;

	const FVector target_location = interactable_actor->GetActorLocation();
	const int CHECKPOINTS_FREQUENCY_FACTOR = 50;
	const int CHECKPOINTS_COUNT = (int)(m_movement_spline_component_temporary->GetSplineLength() / CHECKPOINTS_FREQUENCY_FACTOR);

	for (int i = 0; i < CHECKPOINTS_COUNT; ++i)
	{
		const float time_at_current_checkpoint = (m_movement_spline_component_temporary->Duration / CHECKPOINTS_COUNT) * i;
		FVector checkpoint_location = m_movement_spline_component_temporary->GetLocationAtTime(time_at_current_checkpoint, ESplineCoordinateSpace::World);
		checkpoint_location.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		//target is too far from current checkpoint
		if (calculate_distance(checkpoint_location, target_location, true) > interaction_radius)
			continue;

		//trace if is interation range
		FHitResult trace_hit_result;
		FCollisionQueryParams trace_params;
		trace_params.AddIgnoredActor(this);

		if (GetWorld()->LineTraceSingleByChannel(trace_hit_result, checkpoint_location, target_location, ECC_Visibility, trace_params))
		{
			if (trace_hit_result.GetActor() == interactable_actor)
				return checkpoint_location;
		}
	}

	return FVector::ZeroVector;
}

void AVEN_PlayerUnit::ChangeFocusedTarget(AActor* new_target)
{
	m_focused_target = new_target;
	m_move_and_interact_mode = false;
}

void AVEN_PlayerUnit::NotifyBlueprint()
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	if (game_mode->GetCurrentLevel() != ECurrentLevel::GAMEPLAY)
		return;

	FPlayerUnitInfo info;
	info.is_active = m_is_active;
	info.hp_current = m_hp_current;
	info.hp_total = m_hp_total;
	info.tps_current = m_turn_points_left;
	info.tps_total = m_turn_points_total;
	info.xp_current = m_xp_current;
	info.xp_total = m_xp_total;

	OnUnitInfoUpdate(m_unit_type, info);
}

void AVEN_PlayerUnit::InitBattleRelatedProperties()
{
	m_hp_current = m_hp_total;
	ResetBattleRelatedProperties();
}

void AVEN_PlayerUnit::ResetBattleRelatedProperties()
{
	m_is_dead = false;
	m_in_battle = false;
	m_move_and_interact_mode = false;
	m_move_and_interact_spline_calculated = false;
	ResetTurnPoints();
}

void AVEN_PlayerUnit::RegisterTacticalViewInstance()
{
	m_tactical_view_instance = NewObject<UVEN_TactialView>(this, UVEN_TactialView::StaticClass(), FName("tactical_view_instance"));
	if (m_tactical_view_instance)
		m_tactical_view_instance->Initialize(this, GetBattleSystem());
	//else
		//debug_log("AVEN_PlayerUnit::RegisterTacticalViewInstance. Tactical view instance cannot be created", FColor::Red);
}

void AVEN_PlayerUnit::RotateToFocusedTarget()
{
	if (!m_focused_target)
	{
		//debug_log("AVEN_PlayerUnit::RotateToFocusedTarget. m_focused_target is nullptr");
		return;
	}

	FVector visibility_vector = m_focused_target->GetActorLocation() - GetActorLocation();
	FRotator new_ai_rotation = FRotator(GetActorRotation().Pitch, visibility_vector.Rotation().Yaw, GetActorRotation().Roll);
	SetActorRotation(new_ai_rotation);
}

void AVEN_PlayerUnit::PrepareAttackAnimation()
{
	if (!m_focused_target)
	{
		//debug_log("AVEN_PlayerUnit::PrepareAttackAnimation. m_focused_target is nullptr");
		return;
	}

	RotateToFocusedTarget();
	ANIMATION_ACTION action;

	int random_attack_animation = FMath::RandRange(1, 2);
	if (random_attack_animation == 1)
		action = ANIMATION_ACTION::ATTACK_1;
	else
		action = ANIMATION_ACTION::ATTACK_2;

	AnimInstanceAction(action);
}

void AVEN_PlayerUnit::Die()
{
	m_is_dead = true;
	AnimInstanceAction(ANIMATION_ACTION::STUN);
}

bool AVEN_PlayerUnit::IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION action)
{
	if (!m_temporary_movement_spline_is_built)
		return false;

	switch (action)
	{
		case IN_BATTLE_UNIT_ACTION::MOVE:
			return GetMovementPrice() <= GetTurnPointsLeft(); break;
		case IN_BATTLE_UNIT_ACTION::ATTACK:
			return m_attack_price <= GetTurnPointsLeft(); break;
		case IN_BATTLE_UNIT_ACTION::MOVE_AND_ATTACK:
			return (GetMovementPrice() + GetAttackPrice()) <= GetTurnPointsLeft(); break;
		default:
			/*debug_log("AVEN_PlayerUnit::IsEnoughPointsForAction. Invalid action", FColor::Red);*/ break;
	}

	return false;
}

DIRECTION AVEN_PlayerUnit::GetAttackDirection(FRotator attacker_rotation) const
{
	const float self_yaw = GetActorRotation().Yaw;
	const float attacker_yaw = attacker_rotation.Yaw;

	//calculate angle between self and attacker
	float angle = self_yaw - attacker_yaw;
	if (angle < -180)
		angle += 360;
	else if (angle > 180)
		angle -= 360;

	//calculate direction
	DIRECTION direction;
	if (angle >= 135 || angle < -135)
		direction = DIRECTION::BACK;
	else if (angle >= -45 && angle < 45)
		direction = DIRECTION::FRONT;
	else if (angle >= -135 && angle < -45)
		direction = DIRECTION::LEFT;
	else
		direction = DIRECTION::RIGHT;

	return direction;
}

void AVEN_PlayerUnit::RecoverHPs()
{
	int hp_after_recover = m_hp_total * HP_RECOVERY_PERCENT;
	if (m_hp_current >= hp_after_recover)
	{
		m_hp_recovery = false;
		return;
	}

	m_hp_current = FMath::FInterpTo((float)m_hp_current, (float)hp_after_recover, GetWorld()->GetDeltaSeconds(), HP_RECOVERY_SPEED);
	if (m_hp_current > hp_after_recover)
		m_hp_current = hp_after_recover;

	NotifyBlueprint();
}

bool AVEN_PlayerUnit::CanInitiateBattleInPlace(AVEN_EnemyUnit* enemy_unit)
{
	if (!m_anim_instance->IsFree() || m_in_battle || !GetBattleSystem()->GetBattleAllowed() || enemy_unit->IsDead())
		return false;

	return calculate_distance(GetActorLocation(), enemy_unit->GetActorLocation()) < BATTLE_INITIATE_DISTANCE;
}

bool AVEN_PlayerUnit::CanInitiateBattleAfterMove(AVEN_EnemyUnit* enemy_unit)
{
	if (!m_anim_instance->IsFree() || m_in_battle || !GetBattleSystem()->GetBattleAllowed() || enemy_unit->IsDead())
		return false;

	return m_move_and_interact_spline_calculated;
}

bool AVEN_PlayerUnit::CanAttackInPlace(AVEN_EnemyUnit* enemy_unit, bool consider_turn_points)
{
	if (!m_anim_instance->IsFree() || m_is_currently_moving || !m_in_battle || enemy_unit->IsDead() || (consider_turn_points && !IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::ATTACK)))
		return false;

	FHitResult trace_hit_result;
	FVector trace_start_point = GetActorLocation();
	FVector trace_end_point = enemy_unit->GetActorLocation();
	FCollisionQueryParams trace_params;
	trace_params.AddIgnoredActor(this);

	bool no_obstacles_on_attack_trace = true;
	bool valid_attack_range = true;

	if (GetAttackRange() < calculate_distance(trace_start_point, trace_end_point, true))
		valid_attack_range = false;

	if (GetWorld()->LineTraceSingleByChannel(trace_hit_result, trace_start_point, trace_end_point, ECC_Visibility, trace_params))
	{
		if (trace_hit_result.GetActor() != enemy_unit)
			no_obstacles_on_attack_trace = false;
	}

	if (valid_attack_range && no_obstacles_on_attack_trace)
		return true;

	return false;
}

bool AVEN_PlayerUnit::CanAttackAfterMove(AVEN_EnemyUnit* enemy_unit, bool consider_turn_points)
{
	if (!m_anim_instance->IsFree() || !m_in_battle || m_is_currently_moving || enemy_unit->IsDead() || (consider_turn_points && !IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::MOVE_AND_ATTACK)))
		return false;

	return m_move_and_interact_spline_calculated;
}

void AVEN_PlayerUnit::DisableTacticalView()
{
	m_tactical_view_is_allowed = false;
	m_tactical_view_instance->OnRequestData(false);
	OnTacticalViewUpdate(false);
}

void AVEN_PlayerUnit::UpdateAdvancedCursor(AActor* hovered_actor, bool show)
{
	FAdvancedCursorInfo info;
	info.show = show;

	if (!show || IsAttackingHoveredActor(hovered_actor))
	{
		info.show = false;
		OnAdvancedCursorUpdate(info);
		return;
	}

	//calc coords data
	const auto& controller = GetMainController();
	if (!controller)
		return;

	float x = 0.f;
	float y = 0.f;
	controller->GetMousePosition(x, y);

	int32 viewport_width = 0;
	int32 viewport_height = 0;
	controller->GetViewportSize(viewport_width, viewport_height);

	const int32 screen_width = 1920;
	const int32 screen_height = 1080;
	const float screen_to_viewport_size_multiplier = (float)screen_height / (float)viewport_height;

	int32 converted_coord_x = (int32)(screen_to_viewport_size_multiplier * x);
	int32 converted_coord_y = (int32)(screen_to_viewport_size_multiplier * y);
	FVector2D coords_data = FVector2D(converted_coord_x, converted_coord_y);

	//calc action cost
	const auto& enemy_unit_on_hover = Cast<AVEN_EnemyUnit>(hovered_actor);
	float action_cost_raw = 0.f;

	if (enemy_unit_on_hover)
	{
		action_cost_raw += GetAttackPrice();
		if (!CanAttackInPlace(enemy_unit_on_hover, false))
			action_cost_raw += GetMovementPrice();
	}
	else if (m_temporary_movement_spline_is_built)
	{
		action_cost_raw += GetMovementPrice();
	}

	if (action_cost_raw == 0.f)
	{
		info.show = false;
		OnAdvancedCursorUpdate(info);
		return;
	}

	FString action_cost = normalize_string_from_float(action_cost_raw);

	//calc attack possibility
	bool action_is_possible = (enemy_unit_on_hover && CanAttackInPlace(enemy_unit_on_hover)) || (enemy_unit_on_hover && CanAttackAfterMove(enemy_unit_on_hover)) || (!enemy_unit_on_hover && CanMoveToPreCalculatedPoint());

	//gather all data into info
	info.coords = coords_data;
	info.action_cost_str = action_cost;
	info.action_is_allowed = action_is_possible;

	OnAdvancedCursorUpdate(info);
}

void AVEN_PlayerUnit::UpdateBattleTurnInfo(bool show)
{
	FBattleTurnPlayerUnitInfo info;
	info.show = show;

	if (!show)
	{
		OnBattleTurnInfoUpdate(info);
		return;
	}

	info.damage_str = FString::FromInt(m_attack_power_min) + "~" + FString::FromInt(m_attack_power_max);
	info.range_str = FString::FromInt(m_attack_range);
	info.cost_str = normalize_string_from_float(m_attack_price) + " tps";
	info.pts_str = normalize_string_from_float(m_turn_points_left) + " tps";
	info.pts_percent = m_turn_points_left / m_turn_points_total;

	OnBattleTurnInfoUpdate(info);
}

bool AVEN_PlayerUnit::IsAttackingHoveredActor(AActor* hovered_actor) const
{
	bool attack_animation_is_playing = m_anim_instance->GetActiveAnimationAction() == ANIMATION_ACTION::ATTACK_1 || m_anim_instance->GetActiveAnimationAction() == ANIMATION_ACTION::ATTACK_2;
	return (m_move_and_interact_mode || attack_animation_is_playing) && hovered_actor == m_focused_target;
}