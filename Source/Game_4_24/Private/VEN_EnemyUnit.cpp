// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_EnemyUnit.h"
#include "VEN_GameMode.h"
#include "VEN_BattleSystem.h"
#include "VEN_QuestsManager.h"
#include "VEN_PlayerUnit.h"
#include "VEN_AnimInstance.h"
#include "VEN_FreeFunctions.h"

#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

namespace
{
	constexpr const float DISTANCE_TO_CLOSEST_POINT_ERROR = 30.f;
	constexpr const float MIN_DISTANCE_BETWEEN_RESERVED_POINTS = 100.f;
	constexpr const float TURN_POINTS_LEFT_ERROR = 10.f;
	constexpr const float SENSING_ANGLE = 60.f;
	constexpr const float SENSING_RADIUS = 3000.f;
	constexpr const float SENSING_INTERVAL = .25f;
	constexpr const float SENSING_MIN_DISTANCE_TIME = 1.f;
	constexpr const float SENSING_MAX_DISTANCE_TIME = 3.f;
	constexpr const float SENSING_COOLDOWN_TIME = 5.f;
	constexpr const int REWARD_XP_FOR_DEATH = 120.f;

	const FString QUEST_ID_TAG_PREFIX = "quest_id_";
}

using namespace vendetta;

AVEN_EnemyUnit::AVEN_EnemyUnit()
	: m_animation_index(-1)
	, m_decal(nullptr)
	, m_movement_spline_is_built(false)
	, m_is_currently_moving(false)
	, m_spline_comleted_movement_distance(0.f)
{
	PrimaryActorTick.bCanEverTick = true;

	m_sensing_component = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("Enemy Unit sensing component"));
	m_sensing_component->SetPeripheralVisionAngle(SENSING_ANGLE);
	m_sensing_component->SightRadius = SENSING_RADIUS;
	m_sensing_component->SensingInterval = SENSING_INTERVAL;
	m_sensing_component->bOnlySensePlayers = false;
	m_sensing_component->OnSeePawn.AddDynamic(this, &AVEN_EnemyUnit::OnSeePawn);

	m_decal = CreateDefaultSubobject<UDecalComponent>("ActiveUnitDecal");
	m_decal->SetupAttachment(RootComponent);
}

void AVEN_EnemyUnit::BeginPlay()
{
	Super::BeginPlay();


}

void AVEN_EnemyUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (m_sensing_is_active)
		SensingPlayerUnit(DeltaTime);
	if (m_is_currently_moving)
		ContinueMovement();
}

void AVEN_EnemyUnit::Initialize()
{
	InitBattleRelatedProperties();
	SetupRelatedQuests();
	m_decal->SetVisibility(false);
	OnRankWidgetUpdate(true);
	EnableSensing();
}

USkeletalMeshComponent* AVEN_EnemyUnit::GetEnemyUnitMesh() const
{
	return GetMesh();
}

void AVEN_EnemyUnit::RegisterAnimInstance(UVEN_AnimInstance* instance)
{
	//if (!instance)
		//debug_log("AVEN_EnemyUnit::RegisterAnimInstance. Instance is broken", FColor::Red);

	m_anim_instance = instance;
}

void AVEN_EnemyUnit::OnAnimationUpdate(ANIMATION_NOTIFICATION notification)
{
	const auto& battle_system = GetBattleSystem();
	if (!battle_system || !battle_system->IsBattleInProgress() || !battle_system->IsCurrentTurnOwner(this))
		return;

	if (notification == ANIMATION_NOTIFICATION::ATTACK_ANIMATION_UPDATED)
	{
		if(m_current_target)
		{
			GetBattleSystem()->Attack(this, m_current_target);
			UpdateTurnPoints(-m_attack_price);
		}
	}
	else if (notification == ANIMATION_NOTIFICATION::ATTACK_ANIMATION_FINISHED)
	{
		MakeDecision();
	}
}

int AVEN_EnemyUnit::GetAnimationIndex() const
{
	return m_animation_index;
}

void AVEN_EnemyUnit::OnStartBattle()
{
	m_in_battle = true;
	m_sensing_is_active = false;
	m_sensing_current_percent = 0.f;
	EnableSensing(false);
	AnimInstanceUpdate(ANIMATION_UPDATE::START_BATTLE);
	OnRankWidgetUpdate(true, 1.f);
}

void AVEN_EnemyUnit::OnFinishBattle()
{
	if (m_is_dead)
		return;

	m_in_battle = false;
	EnableSensing(true);
	AnimInstanceUpdate(ANIMATION_UPDATE::FINISH_BATTLE);
}

void AVEN_EnemyUnit::OnStartBattleTurn()
{
	FindTarget();
	MakeDecision();
	m_decal->SetVisibility(true);
	GetGameMode()->GetMainCamera()->SetCameraFollowMode(true, this);
}

void AVEN_EnemyUnit::OnFinishBattleTurn()
{
	m_current_target = nullptr;
	m_decal->SetVisibility(false);
	GetGameMode()->GetMainCamera()->SetCameraFollowMode(false);
}

void AVEN_EnemyUnit::OnTargetDied()
{
	FindTarget();
}

void AVEN_EnemyUnit::OnReceiveDamage(float damage_received, AActor* damage_dealer)
{
	m_hp_current -= damage_received;

	if (m_hp_current > m_hp_total)
	{
		m_hp_current = m_hp_total;
	}
	else if (m_hp_current <= 0)
	{
		m_hp_current = 0;
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

		if (receive_damage_anim_action != ANIMATION_ACTION::IDLE)
			AnimInstanceAction(receive_damage_anim_action);
	}
	else
	{
		const auto& player_unit_attacker = Cast<AVEN_PlayerUnit>(damage_dealer);
		if (player_unit_attacker)
			player_unit_attacker->IncrementXP(REWARD_XP_FOR_DEATH);
	}

	GetBattleSystem()->UpdateBlueprintBattleQueue();
}

EEnemyUnitType AVEN_EnemyUnit::GetUnitType() const
{
	return m_unit_type;
}

int AVEN_EnemyUnit::GetHP() const
{
	return m_hp_current;
}

int AVEN_EnemyUnit::GetTotalHP() const
{
	return m_hp_total;
}

int AVEN_EnemyUnit::GetAttackPowerMin() const
{
	return m_attack_power_min;
}

int AVEN_EnemyUnit::GetAttackPowerMax() const
{
	return m_attack_power_max;
}

int AVEN_EnemyUnit::GetRandomAttackPower() const
{
	return FMath::RandRange(m_attack_power_min, m_attack_power_max);
}

int AVEN_EnemyUnit::GetAttackRange() const
{
	return m_attack_range;
}

float AVEN_EnemyUnit::GetAttackPrice() const
{
	return m_attack_price;
}

float AVEN_EnemyUnit::GetTurnPointsTotal() const
{
	return m_turn_points_total;
}

float AVEN_EnemyUnit::GetTurnPointsLeft() const
{
	return m_turn_points_left;
}

void AVEN_EnemyUnit::UpdateTurnPoints(float update_on_value)
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

	GetBattleSystem()->UpdateBlueprintBattleQueue();
}

void AVEN_EnemyUnit::ResetTurnPoints()
{
	m_turn_points_left = m_turn_points_total;
}

bool AVEN_EnemyUnit::IsDead() const
{
	return m_is_dead;
}

TArray<vendetta::QUEST_ID> AVEN_EnemyUnit::GetRelatedQuestIds() const
{
	return m_related_quests_ids;
}

void AVEN_EnemyUnit::EnableSensing(bool enable)
{
	m_sensing_is_enabled = enable;
}

void AVEN_EnemyUnit::OnSeePawn(APawn* other_pawn)
{
	if (!m_sensing_is_enabled || m_sensing_is_active || !GetBattleSystem() || !GetBattleSystem()->GetBattleAllowed())
		return;

	const auto& sensed_player_unit = Cast<AVEN_PlayerUnit>(other_pawn);
	if (!sensed_player_unit || sensed_player_unit->IsDead())
		return;

	if (m_sensing_component->CouldSeePawn(other_pawn) && m_sensing_component->HasLineOfSightTo(other_pawn))
	{
		m_sensing_is_active = true;
		m_current_target = sensed_player_unit;
		m_rotation_before_sensing = GetActorRotation();
	}
}

AVEN_GameMode* AVEN_EnemyUnit::GetGameMode() const
{
	const auto& game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("AVEN_EnemyUnit::GetGameMode. Game Mode not found!", FColor::Red);

	return game_mode;
}

UVEN_BattleSystem* AVEN_EnemyUnit::GetBattleSystem() const
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
	{
		//debug_log("AVEN_EnemyUnit::GetBattleSystem. Game Mode not found!", FColor::Red);
		return nullptr;
	}

	const auto& battle_system = Cast<UVEN_BattleSystem>(game_mode->GetBattleSystem());
	if (!battle_system)
	{
		//debug_log("AVEN_EnemyUnit::GetBattleSystem. Battle System not found!", FColor::Red);
	}

	return battle_system;
}

UVEN_QuestsManager* AVEN_EnemyUnit::GetQuestsManager() const
{
	auto game_mode = GetGameMode();
	if (!game_mode)
	{
		//debug_log("AVEN_EnemyUnit::GetQuestsManager. Game Mode not found!", FColor::Red);
		return nullptr;
	}

	return game_mode->GetQuestsManager();
}

void AVEN_EnemyUnit::BuildMovementSpline(const FVector& destination_point, float limited_length)
{
	DestroyMovementSpline();

	//calculate navigation path
	float half_capsule_height = 0.f;
	float capsule_radius = 0.f;
	GetCapsuleComponent()->GetScaledCapsuleSize(capsule_radius, half_capsule_height);
	FVector start_movement_point = GetCapsuleComponent()->GetComponentLocation();
	start_movement_point.Z -= half_capsule_height;

	UNavigationPath* movement_path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), start_movement_point, destination_point, this);
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

	//cut spline if limited length is defined
	if (limited_length)
	{
		FVector new_last_spline_point_location = m_movement_spline_component->GetLocationAtDistanceAlongSpline(limited_length, ESplineCoordinateSpace::World);
		int total_spline_points_before_rebuild = m_movement_spline_component->GetNumberOfSplinePoints();
		int index_of_first_excess_spline_point = 0;

		for (int i = 1; i < total_spline_points_before_rebuild; ++i)
		{
			if (m_movement_spline_component->GetDistanceAlongSplineAtSplinePoint(i) >= limited_length)
			{
				index_of_first_excess_spline_point = i;
				break;
			}
		}

		if (index_of_first_excess_spline_point < total_spline_points_before_rebuild)
		{
			for (int i = total_spline_points_before_rebuild - 1; i >= index_of_first_excess_spline_point; --i)
				m_movement_spline_component->RemoveSplinePoint(i);

			m_movement_spline_component->AddSplinePoint(new_last_spline_point_location, ESplineCoordinateSpace::World);
			m_movement_spline_component->SetSplinePointType(index_of_first_excess_spline_point, ESplinePointType::CurveClamped);
		}

		m_movement_spline_is_built = m_movement_spline_component->GetNumberOfSplinePoints() > 0;
	}
}

void AVEN_EnemyUnit::DestroyMovementSpline()
{
	if (m_movement_spline_actor)
	{
		m_movement_spline_actor->Destroy();
		m_movement_spline_actor = nullptr;
	}
}

void AVEN_EnemyUnit::StartMovement()
{
	if (!m_movement_spline_component || !m_movement_spline_is_built)
	{
		//debug_log("AVEN_EnemyUnit::StartMovement. Movement spline is not built", FColor::Red);
		GetBattleSystem()->FinishCurrentTurn(this);
		return;
	}

	m_is_currently_moving = true;
}

void AVEN_EnemyUnit::ContinueMovement()
{
	if (!m_movement_spline_component)
	{
		m_is_currently_moving = false;
		//debug_log("AVEN_EnemyUnit::ContinueMovement. Spline component is missing", FColor::Red);
		GetBattleSystem()->FinishCurrentTurn(this);
		return;
	}

	if (m_spline_comleted_movement_distance < m_movement_spline_component->GetSplineLength())
	{
		m_spline_comleted_movement_distance += GetVelocity().Size() * GetWorld()->GetDeltaSeconds();

		AddMovementInput(GetActorForwardVector(), 1.0f, true);

		FVector spline_location = m_movement_spline_component->GetLocationAtDistanceAlongSpline(m_spline_comleted_movement_distance, ESplineCoordinateSpace::World);
		FRotator spline_rotation = m_movement_spline_component->GetRotationAtDistanceAlongSpline(m_spline_comleted_movement_distance, ESplineCoordinateSpace::World);

		spline_location.Z = GetCapsuleComponent()->GetComponentLocation().Z;
		spline_rotation.SetComponentForAxis(EAxis::X, 0.f);
		spline_rotation.SetComponentForAxis(EAxis::Y, 0.f);

		SetActorLocation(spline_location);

		//do not set unit rotation at last spline point - it will always be equal 0.0f
		if (m_spline_comleted_movement_distance < (m_movement_spline_component->GetSplineLength() - 10.f))
			SetActorRotation(spline_rotation);
	}
	else
	{
		FinishMovement();
		DestroyMovementSpline();
	}
}

void AVEN_EnemyUnit::FinishMovement()
{
	float spent_points_on_movement = -m_spline_comleted_movement_distance;
	m_spline_comleted_movement_distance = 0.f;
	m_is_currently_moving = false;
	m_movement_spline_component = nullptr;

	if (m_in_battle)
	{
		UpdateTurnPoints(spent_points_on_movement);

		if (m_current_target)
			RotateToPlayerUnit();

		MakeDecision();
	}
}

void AVEN_EnemyUnit::SetupRelatedQuests()
{
	m_related_quests_ids.Empty();

	for (const auto& tag : Tags)
	{
		FString tag_str = tag.ToString();
		if (tag_str.Contains(QUEST_ID_TAG_PREFIX))
		{
			int32_t quest_id = FCString::Atoi(*(tag_str.Mid(QUEST_ID_TAG_PREFIX.Len())));
			m_related_quests_ids.Add((QUEST_ID)quest_id);
		}
	}
}

void AVEN_EnemyUnit::AnimInstanceAction(ANIMATION_ACTION anim_action)
{
	if (m_anim_instance)
		m_anim_instance->OnActionFromOwner(anim_action);
}

void AVEN_EnemyUnit::AnimInstanceUpdate(ANIMATION_UPDATE anim_update)
{
	if (m_anim_instance)
		m_anim_instance->OnUpdateFromOwner(anim_update);
}

void AVEN_EnemyUnit::InitBattleRelatedProperties()
{
	m_is_dead = false;
	m_in_battle = false;
	m_sensing_current_percent = 0.f;
	m_hp_current = m_hp_total;
	m_turn_points_left = m_turn_points_total;
	m_reserved_point = FVector::ZeroVector;
	m_rotation_before_sensing = FRotator::ZeroRotator;
}

void AVEN_EnemyUnit::AttackAnimationStart()
{
	RotateToPlayerUnit();

	ANIMATION_ACTION action;

	int random_attack_animation = FMath::RandRange(1, 2);
	if (random_attack_animation == 1)
		action = ANIMATION_ACTION::ATTACK_1;
	else
		action = ANIMATION_ACTION::ATTACK_2;

	AnimInstanceAction(action);
}

void AVEN_EnemyUnit::Die()
{
	m_is_dead = true;
	GetQuestsManager()->OnUpdate(QUESTS_MANAGER_UPDATE::ENEMY_UNIT_KILLED, m_related_quests_ids, this);
	AnimInstanceAction(ANIMATION_ACTION::DIE);
	SetActorEnableCollision(false);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	OnMapIconUpdate(false);
	OnRankWidgetUpdate(false);
}

bool AVEN_EnemyUnit::IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION action)
{
	switch (action)
	{
		case IN_BATTLE_UNIT_ACTION::MOVE:
			return m_movement_spline_component->GetSplineLength() <= m_turn_points_left; break;
		case IN_BATTLE_UNIT_ACTION::ATTACK:
			return m_attack_price <= m_turn_points_left; break;
		case IN_BATTLE_UNIT_ACTION::MOVE_AND_ATTACK:
			return (m_movement_spline_component->GetSplineLength() + m_attack_price) <= m_turn_points_left; break;
		default:
			/*debug_log("AVEN_EnemyUnit::IsEnoughPointsForAction. Invalid action", FColor::Red);*/ break;
	}

	return false;
}

void AVEN_EnemyUnit::CalculateAttackPoints(TMap<AActor*, TArray<FVector>>& attack_points_by_player_unit)
{
	const auto& player_units = GetBattleSystem()->GetPlayerUnitsInBattle();
	TMap<AActor*, FTransform> player_units_transforms;

	for (const auto& player_unit : player_units)
	{
		const auto& player_unit_c = Cast<AVEN_PlayerUnit>(player_unit);
		if (player_unit_c->IsDead())
			continue;

		attack_points_by_player_unit.Add(player_unit);
		player_units_transforms.Add(player_unit);
		player_units_transforms[player_unit] = player_unit->GetTransform();
	}

	if (!player_units_transforms.Num())
		return;

	for (const auto& player_unit_transform : player_units_transforms)
	{
		const float DISTANCE_TO_POINTS = GetAttackRange();
		const TArray<float> ANGLE_TO_POINTS = { 0.f, 90.f, 180.f, 270.f };

		for (int32_t i = 0; i < ANGLE_TO_POINTS.Num(); ++i)
		{
			FVector distance = FVector(DISTANCE_TO_POINTS, 0, 0);
			FVector point_offset = distance.RotateAngleAxis(ANGLE_TO_POINTS[i] + FRotator(player_unit_transform.Value.GetRotation()).Yaw, FVector(0, 0, 1));
			FVector new_point_location = player_unit_transform.Value.GetLocation() + point_offset;
			attack_points_by_player_unit[player_unit_transform.Key].Add(new_point_location);
		}
	}
}

void AVEN_EnemyUnit::GatherAllCalculatedPoints(TMap<AActor*, TArray<FVector>>& attack_points_by_player_unit, TArray<FVector>& all_points)
{
	if (!attack_points_by_player_unit.Num())
		return;

	for (const auto& attack_point_by_player : attack_points_by_player_unit)
	{
		all_points.Append(attack_point_by_player.Value);
	}
}

void AVEN_EnemyUnit::ValidateAttackPoints(TArray<FVector>& points)
{
	if (!points.Num())
		return;

	bool point_is_invalid = false;
	TArray<FVector> invalid_points;
	TArray<FVector> reserved_points = GetBattleSystem()->GetEnemyUnitReservedAttackPoints();
	TArray<AActor*> in_battle_units = GetBattleSystem()->GetAllUnitsInBattle();

	for (const auto& point : points)
	{
		//check if movement spline can be built to point
		BuildMovementSpline(point);
		if (!m_movement_spline_is_built)
		{
			point_is_invalid = true;
		}
		DestroyMovementSpline();

		//check collision with already reserved points
		for (const auto& reserved_point : reserved_points)
		{
			if (calculate_distance(point, reserved_point) <= MIN_DISTANCE_BETWEEN_RESERVED_POINTS)
			{
				point_is_invalid = true;
				break;
			}
		}

		//check collision with other units
		for (const auto& in_battle_unit : in_battle_units)
		{
			if (Cast<AVEN_EnemyUnit>(in_battle_unit) == this)
				continue;

			if (calculate_distance(point, in_battle_unit->GetActorLocation()) <= MIN_DISTANCE_BETWEEN_RESERVED_POINTS)
			{
				point_is_invalid = true;
				break;
			}
		}

		if (point_is_invalid)
			invalid_points.Add(point);

		point_is_invalid = false;
	}

	//remove invalid points from origin array
	for (const auto& invalid_point : invalid_points)
	{
		if (points.Contains(invalid_point))
			points.Remove(invalid_point);
		//else
			//debug_log("AVEN_EnemyUnit::ValidateAttackPoints. Attempt to remove not existing point", FColor::Red);
	}
}

void AVEN_EnemyUnit::SortAttackPoints(TArray<FVector>& points)
{
	if (!points.Num())
		return;

	for (int32_t i = 0; i < points.Num() - 1; i++)
	{
		for (int32_t j = 0; j < points.Num() - i - 1; j++)
		{
			if (calculate_distance(GetActorLocation(), points[j]) > calculate_distance(GetActorLocation(), points[j + 1]))
			{
				const auto& tmp = points[j];
				points[j] = points[j + 1];
				points[j + 1] = tmp;
			}
		}
	}
}

FVector AVEN_EnemyUnit::GetClosestAttackPoint(TArray<FVector>& points) const
{
	return points.Num() ? points[0] : FVector::ZeroVector;
}

AActor* AVEN_EnemyUnit::GetAttackPointTarget(FVector point, TMap<AActor*, TArray<FVector>>& attack_points_by_player_unit) const
{
	for (const auto& attack_point_by_player : attack_points_by_player_unit)
	{
		if (attack_point_by_player.Value.Contains(point))
			return attack_point_by_player.Key;
	}

	//debug_log("AVEN_EnemyUnit::GetAttackPointTarget. Cannot find attack point target", FColor::Red);
	return nullptr;
}

void AVEN_EnemyUnit::MakeDecision()
{
	//player units are unreachable
	if (m_reserved_point == FVector::ZeroVector || !m_current_target)
	{
		GetBattleSystem()->FinishCurrentTurn(this);
		return;
	}

	//enemy unit is on attack point
	if (calculate_distance(m_reserved_point, GetActorLocation(), true) < DISTANCE_TO_CLOSEST_POINT_ERROR)
	{
		if (IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::ATTACK))
		{
			if (!m_current_target)
			{
				//debug_log("AVEN_EnemyUnit::MakeDecision. No current target for attack", FColor::Red);
				GetBattleSystem()->FinishCurrentTurn(this);
				return;
			}
			if (m_in_battle)
			{
				AttackAnimationStart();
				UpdateTurnPoints(-m_attack_price);
				return;
			}
		}
		else
		{
			GetBattleSystem()->FinishCurrentTurn(this);
			return;
		}
	}
	else
	{
		BuildMovementSpline(m_reserved_point);
		if (!m_movement_spline_is_built)
		{
			GetBattleSystem()->FinishCurrentTurn(this);
			return;
		}

		if (IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::MOVE_AND_ATTACK) || IsEnoughPointsForAction(IN_BATTLE_UNIT_ACTION::MOVE))
		{
			//move if enough points and then make a decision once again (in FinishMovement())
			StartMovement();
		}
		else
		{
			//finish if no points or try to come closer to player unit
			if (m_turn_points_left < TURN_POINTS_LEFT_ERROR)
			{
				GetBattleSystem()->FinishCurrentTurn(this);
				return;
			}
			else
			{
				BuildMovementSpline(m_reserved_point, m_turn_points_left);
				if (!m_movement_spline_is_built)
				{
					GetBattleSystem()->FinishCurrentTurn(this);
					return;
				}

				StartMovement();
			}
		}
	}
}

void AVEN_EnemyUnit::ReservePoint(FVector point)
{
	const auto& battle_system = GetBattleSystem();
	if (!battle_system)
	{
		//debug_log("AVEN_EnemyUnit::ReservePoint. Battle System is nullptr", FColor::Red);
		return;
	}

	if (point == FVector::ZeroVector)
		return;

	m_reserved_point = point;
	battle_system->AddEnemyUnitReservedAttackPoint(m_reserved_point);
}

void AVEN_EnemyUnit::SensingPlayerUnit(float DeltaTime)
{
	if (!m_sensing_is_enabled || !m_current_target)
	{
		//debug_log("AVEN_EnemyUnit::SensingPlayerUnit. Sensing is disabled or target is missing", FColor::Red);
		return;
	}

	float update_percent_on_value = 0.f;
	const float distance_to_target = calculate_distance(GetActorLocation(), m_current_target->GetActorLocation());
	const auto& sensed_pawn = Cast<APawn>(m_current_target);

	if (distance_to_target <= SENSING_RADIUS && sensed_pawn && m_sensing_component->CouldSeePawn(sensed_pawn) && m_sensing_component->HasLineOfSightTo(sensed_pawn))
	{
		const float estimated_time_to_attack = SENSING_MIN_DISTANCE_TIME + (SENSING_MAX_DISTANCE_TIME - SENSING_MIN_DISTANCE_TIME) * (distance_to_target / SENSING_RADIUS);
		update_percent_on_value = (100.f / estimated_time_to_attack) * DeltaTime;
	}
	else
	{
		update_percent_on_value = (100.f / SENSING_COOLDOWN_TIME) * DeltaTime * (-1);
	}

	m_sensing_current_percent += update_percent_on_value;
	OnRankWidgetUpdate(true, m_sensing_current_percent / 100);

	if (m_sensing_current_percent >= 100.f)
	{
		m_sensing_current_percent = 100.f;
		m_sensing_is_active = false;

		if (GetBattleSystem()->GetBattleAllowed())
		{
			GetBattleSystem()->StartBattle(this, m_current_target);
			return;
		}
	}
	else if (m_sensing_current_percent <= 0.f)
	{
		m_sensing_current_percent = 0.f;
		m_sensing_is_active = false;
		SetActorRotation(m_rotation_before_sensing);
	}
}

void AVEN_EnemyUnit::RotateToPlayerUnit()
{
	if (!m_current_target)
	{
		//debug_log("AVEN_EnemyUnit::RotateToPlayerUnit. No current target", FColor::Red);
		return;
	}

	FVector visibility_vector = m_current_target->GetActorLocation() - GetActorLocation();
	FRotator new_ai_rotation = FRotator(GetActorRotation().Pitch, visibility_vector.Rotation().Yaw, GetActorRotation().Roll);
	SetActorRotation(new_ai_rotation);
}

void AVEN_EnemyUnit::FindTarget()
{
	//find and gather attack points
	TMap<AActor*, TArray<FVector>> attack_points_by_player_unit;
	TArray<FVector> all_attack_points;
	CalculateAttackPoints(attack_points_by_player_unit);
	GatherAllCalculatedPoints(attack_points_by_player_unit, all_attack_points);
	ValidateAttackPoints(all_attack_points);
	SortAttackPoints(all_attack_points);

	//update reserved attack point
	m_reserved_point = FVector::ZeroVector;
	ReservePoint(GetClosestAttackPoint(all_attack_points));

	//update target
	m_current_target = m_reserved_point == FVector::ZeroVector ? nullptr : GetAttackPointTarget(m_reserved_point, attack_points_by_player_unit);
}

DIRECTION AVEN_EnemyUnit::GetAttackDirection(FRotator attacker_rotation) const
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