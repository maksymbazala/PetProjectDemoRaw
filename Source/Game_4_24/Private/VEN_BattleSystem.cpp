// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_BattleSystem.h"
#include "VEN_GameMode.h"
#include "VEN_PlayerUnit.h"
#include "VEN_EnemyUnit.h"
#include "VEN_MainController.h"
#include "VEN_FreeFunctions.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine.h"

using namespace vendetta;

namespace
{
	constexpr const float NERBY_UNITS_SPHERE_RADIUS = 3000.f;
	constexpr const float NERBY_UNITS_HEIGHT_DELTA = 100.f;
	constexpr const float DELAY_BETWEEN_TURNS = 1.f;
}

UVEN_BattleSystem::UVEN_BattleSystem()
	: m_battle_is_in_progress(false)
	, m_battle_is_allowed(true)
	, m_game_over(false)
	, m_battle_round_starter(nullptr)
{

}

void UVEN_BattleSystem::Initialize()
{
	m_battle_units_queue.Empty();
	m_battle_died_units.Empty();
	m_enemy_unit_reserved_attack_points.Empty();
	SetBattleAllowed(true);
}

void UVEN_BattleSystem::Uninitialize()
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	if (m_battle_is_in_progress)
	{
		game_mode->GetWorld()->GetTimerManager().ClearTimer(m_tmr_before_next_turn);
		m_battle_is_in_progress = false;
	}
}

void UVEN_BattleSystem::InitializeBattleTeleportPoints(FTransform point1, FTransform point2)
{
	m_teleport_point_1 = point1;
	m_teleport_point_2 = point2;
}

bool UVEN_BattleSystem::GetBattleAllowed() const
{
	return m_battle_is_allowed;
}

void UVEN_BattleSystem::SetBattleAllowed(bool is_allowed)
{
	m_battle_is_allowed = is_allowed;
}

bool UVEN_BattleSystem::IsBattleInProgress() const
{
	return m_battle_is_in_progress;
}

bool UVEN_BattleSystem::IsGameOver() const
{
	return m_game_over;
}

void UVEN_BattleSystem::Notify(BATTLE_NOTIFY_TYPE notification)
{
	if (notification == BATTLE_NOTIFY_TYPE::START_BATTLE || notification == BATTLE_NOTIFY_TYPE::FINISH_BATTLE)
	{
		//notify main controller
		const auto& main_controller = GetMainController();
		if (main_controller)
		{
			if (notification == BATTLE_NOTIFY_TYPE::START_BATTLE)
				main_controller->OnStartBattle();
			else if (notification == BATTLE_NOTIFY_TYPE::FINISH_BATTLE)
				main_controller->OnFinishBattle();
		}
		else
		{
			//debug_log("UVEN_BattleSystem::Notify. Main Controller is nullptr", FColor::Red);
			return;
		}

		//notify alive units
		for (const auto& unit : m_battle_units_queue)
		{
			const auto& player_unit_c = Cast<AVEN_PlayerUnit>(unit);
			const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(unit);

			if (player_unit_c)
			{
				if (notification == BATTLE_NOTIFY_TYPE::START_BATTLE)
					player_unit_c->OnStartBattle();
				else if (notification == BATTLE_NOTIFY_TYPE::FINISH_BATTLE)
					player_unit_c->OnFinishBattle();
			}	
			else if (enemy_unit_c)
			{
				if (notification == BATTLE_NOTIFY_TYPE::START_BATTLE)
					enemy_unit_c->OnStartBattle();
				else if (notification == BATTLE_NOTIFY_TYPE::FINISH_BATTLE)
					enemy_unit_c->OnFinishBattle();
			}
			//else
				//debug_log("UVEN_BattleSystem::Notify. Invalid battle receiver", FColor::Red);
		}

		//notify died units about finish battle separately
		if (notification == BATTLE_NOTIFY_TYPE::FINISH_BATTLE)
		{
			for (const auto& unit : m_battle_died_units)
			{
				const auto& player_unit_c = Cast<AVEN_PlayerUnit>(unit);
				const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(unit);

				if (player_unit_c)
					player_unit_c->OnFinishBattle();
				else if (enemy_unit_c)
					enemy_unit_c->OnFinishBattle();
			}
		}
	}
	else if (notification == BATTLE_NOTIFY_TYPE::START_NEW_TURN || notification == BATTLE_NOTIFY_TYPE::FINISH_CURRENT_TURN)
	{
		const auto& player_unit_c = Cast<AVEN_PlayerUnit>(GetCurrentTurnOwner());
		const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(GetCurrentTurnOwner());

		if (player_unit_c)
		{
			if (notification == BATTLE_NOTIFY_TYPE::START_NEW_TURN)
				player_unit_c->OnStartBattleTurn();
			else if (notification == BATTLE_NOTIFY_TYPE::FINISH_CURRENT_TURN)
				player_unit_c->OnFinishBattleTurn();
		}
		else if (enemy_unit_c)
		{
			if (notification == BATTLE_NOTIFY_TYPE::START_NEW_TURN)
				enemy_unit_c->OnStartBattleTurn();
			else if (notification == BATTLE_NOTIFY_TYPE::FINISH_CURRENT_TURN)
				enemy_unit_c->OnFinishBattleTurn();
		}
		//else
			//debug_log("UVEN_BattleSystem::Notify. Invalid turn receiver", FColor::Red);
	}
}

void UVEN_BattleSystem::StartBattle(AActor* attacker, AActor* defender)
{
	if (!GetBattleAllowed())
	{
		//debug_log("UVEN_BattleSystem::StartBattle. Attempt to start battle when its forbidden", FColor::Red);
		return;
	}

	if (!attacker || !defender)
	{
		//debug_log("UVEN_BattleSystem::StartBattle. Attacker or Defender is nullptr", FColor::Red);
		return;
	}

	m_battle_is_in_progress = true;
	SetBattleAllowed(false);
	SetupBattleUnitsQueue(attacker, defender);
	m_battle_round_starter = GetCurrentTurnOwner();
	Notify(BATTLE_NOTIFY_TYPE::START_BATTLE);
	UpdateBlueprintBattleQueue();
	GetGameMode()->OnBattlePopupShow(EWidgetBattleRequestType::START_BATTLE);
	GetGameMode()->OnAmbientMusicUpdate(EAmbientMusicType::INBATTLE_AMBIENT);
}

void UVEN_BattleSystem::FinishBattle()
{
	m_battle_is_in_progress = false;

	GetGameMode()->GetWorld()->GetTimerManager().ClearTimer(m_tmr_before_next_turn);

	ResetAllTurnPoints();
	AdjustUnitsCollisions();
	Notify(BATTLE_NOTIFY_TYPE::FINISH_BATTLE);
	GetGameMode()->GameModeOnBattleQueueChanged({});
	m_battle_units_queue.Empty();
	m_battle_died_units.Empty();

	GetGameMode()->OnBattlePopupShow(m_game_over ? EWidgetBattleRequestType::DEFEAT : EWidgetBattleRequestType::VICTORY);
	GetGameMode()->OnAmbientMusicUpdate(EAmbientMusicType::COMMON_AMBIENT);
}

void UVEN_BattleSystem::Attack(AActor* attacker, AActor* defender)
{
	const auto& player_unit_attacker_c = Cast<AVEN_PlayerUnit>(attacker);
	const auto& player_unit_defender_c = Cast<AVEN_PlayerUnit>(defender);
	const auto& enemy_unit_attacker_c = Cast<AVEN_EnemyUnit>(attacker);
	const auto& enemy_unit_defender_c = Cast<AVEN_EnemyUnit>(defender);

	if (player_unit_attacker_c && enemy_unit_defender_c)
	{
		const int attack_power = player_unit_attacker_c->GetRandomAttackPower();
		enemy_unit_defender_c->OnReceiveDamage(attack_power, player_unit_attacker_c);
		GetGameMode()->OnWidgetFlyingDataUpdate(EWidgetFlyingDataType::NEGATIVE_DAMAGE, attack_power, enemy_unit_defender_c->GetMesh()->GetComponentLocation());

		if (enemy_unit_defender_c->IsDead())
		{
			if (CanBattleBeFinished())
			{
				FinishCurrentTurn(player_unit_attacker_c);
				return;
			}

			if (defender == m_battle_round_starter)
				ShiftRoundStarter();
		}
	}
	else if (enemy_unit_attacker_c && player_unit_defender_c)
	{
		const int attack_power = enemy_unit_attacker_c->GetRandomAttackPower();
		player_unit_defender_c->OnReceiveDamage(attack_power, enemy_unit_attacker_c);
		GetGameMode()->OnWidgetFlyingDataUpdate(EWidgetFlyingDataType::NEGATIVE_DAMAGE, attack_power, player_unit_defender_c->GetMesh()->GetComponentLocation());

		if (player_unit_defender_c->IsDead())
		{
			enemy_unit_attacker_c->OnTargetDied();

			if (defender == m_battle_round_starter)
				ShiftRoundStarter();
		}
	}
	else
	{
		//debug_log("UVEN_BattleSystem::Attack. Invalid attacker/defender!", FColor::Red);
	}
}

void UVEN_BattleSystem::PrepareNextTurn()
{
	if (!m_battle_is_in_progress)
		return;

	if (!GetCurrentTurnOwner())
	{
		//debug_log("UVEN_BattleSystem::PrepareNextTurn. Cannot retrieve current turn owner", FColor::Red);
		return;
	}
	if (!GetCurrentTurnOwner()->GetWorld())
	{
		//debug_log("UVEN_BattleSystem::PrepareNextTurn. Cannot retrieve world of current turn owner", FColor::Red);
		return;
	}

	AdjustUnitsCollisions();

	//clear reserved points at the end of round
	if (GetCurrentTurnOwner() == m_battle_round_starter)
		m_enemy_unit_reserved_attack_points.Empty();

	GetGameMode()->GetWorld()->GetTimerManager().ClearTimer(m_tmr_before_next_turn);
	GetGameMode()->GetWorld()->GetTimerManager().SetTimer(m_tmr_before_next_turn, this, &UVEN_BattleSystem::StartNextTurn, DELAY_BETWEEN_TURNS);
}

void UVEN_BattleSystem::StartNextTurn()
{
	if (!IsBattleInProgress() || !GetCurrentTurnOwner())
		return;

	ResetAllTurnPoints();

	UpdateBlueprintBattleQueue();
	Notify(BATTLE_NOTIFY_TYPE::START_NEW_TURN);
	GetGameMode()->OnBattleTurnOwnerUpdate(true, Cast<AVEN_PlayerUnit>(GetCurrentTurnOwner()) != nullptr);
}

void UVEN_BattleSystem::FinishCurrentTurn(AActor* requestor)
{
	if (requestor != GetCurrentTurnOwner())
		return;

	Notify(BATTLE_NOTIFY_TYPE::FINISH_CURRENT_TURN);
	GetGameMode()->OnBattleTurnOwnerUpdate(false);
	UpdateBattleUnitsQueue();
	UpdateBlueprintBattleQueue();

	if (CanBattleBeFinished())
	{
		FinishBattle();
		if (IsGameOver())
			Notify(BATTLE_NOTIFY_TYPE::GAME_OVER);

		return;
	}

	ShiftBattleUnitsQueue();
	PrepareNextTurn();
}

TArray<AActor*> UVEN_BattleSystem::GetAllUnitsInBattle() const
{
	return m_battle_units_queue;
}

TArray<AActor*> UVEN_BattleSystem::GetPlayerUnitsInBattle() const
{
	TArray<AActor*> player_units;

	for (const auto& unit : m_battle_units_queue)
	{
		if (Cast<AVEN_PlayerUnit>(unit))
			player_units.Add(unit);
	}

	return player_units;
}

TArray<AActor*> UVEN_BattleSystem::GetEnemyUnitsInBattle() const
{
	TArray<AActor*> enemy_units;

	for (const auto& unit : m_battle_units_queue)
	{
		if (Cast<AVEN_EnemyUnit>(unit))
			enemy_units.Add(unit);
	}

	return enemy_units;
}

TArray<FVector> UVEN_BattleSystem::GetEnemyUnitReservedAttackPoints() const
{
	return m_enemy_unit_reserved_attack_points;
}

void UVEN_BattleSystem::AddEnemyUnitReservedAttackPoint(FVector point)
{
	if (!m_enemy_unit_reserved_attack_points.Contains(point))
		m_enemy_unit_reserved_attack_points.Add(point);
	//else
		//debug_log("UVEN_BattleSystem::ChangeEnemyUnitReservedAttackPoint. Attempt to duplicate existing point.", FColor::Red);
}

bool UVEN_BattleSystem::IsCurrentTurnOwner(AActor* actor) const
{
	return GetCurrentTurnOwner() == actor;
}

void UVEN_BattleSystem::UpdateBlueprintBattleQueue()
{
	if (!m_battle_units_queue.Num())
		return;

	const int FIXED_DRAWN_QUEUE_SIZE = 5;
	TArray<FBattleQueueUnitInfo> normalized_queue;

	int battle_queue_index = 0;
	for (int i = 0; i < FIXED_DRAWN_QUEUE_SIZE; ++i)
	{
		normalized_queue.Add({});
		auto& info = normalized_queue[normalized_queue.Num() - 1];

		if (battle_queue_index == m_battle_units_queue.Num())
			battle_queue_index = 0;

		const auto& player_unit_c = Cast<AVEN_PlayerUnit>(m_battle_units_queue[battle_queue_index]);
		if (player_unit_c)
		{
			info.icon_type = player_unit_c->m_unit_icon_type;
			info.hp_percent = (float)player_unit_c->GetHP() / (float)player_unit_c->GetTotalHP();
			info.tps_percent = player_unit_c->GetTurnPointsLeft() / player_unit_c->GetTurnPointsTotal();

			battle_queue_index++;
			continue;
		}

		const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(m_battle_units_queue[battle_queue_index]);
		if (enemy_unit_c)
		{
			info.icon_type = enemy_unit_c->m_unit_icon_type;
			info.hp_percent = (float)enemy_unit_c->GetHP() / (float)enemy_unit_c->GetTotalHP();
			info.tps_percent = enemy_unit_c->GetTurnPointsLeft() / enemy_unit_c->GetTurnPointsTotal();

			battle_queue_index++;
			continue;
		}
	}

	GetGameMode()->GameModeOnBattleQueueChanged(normalized_queue);
}

void UVEN_BattleSystem::OnStartPopupShown()
{
	PrepareNextTurn();
}

void UVEN_BattleSystem::OnFinalPopupShown()
{
	if (!IsGameOver())
		SetBattleAllowed(true);
}

void UVEN_BattleSystem::SetupBattleUnitsQueue(AActor* attacker, AActor* defender)
{
	TArray<AActor*> ally_units;
	TArray<TEnumAsByte<EObjectTypeQuery>> filter;
	TArray<AActor*> actors_to_ignore;

	actors_to_ignore.Add(attacker);

	if (Cast<AVEN_PlayerUnit>(attacker))
		UKismetSystemLibrary::SphereOverlapActors(GetWorld(), attacker->GetActorLocation(), NERBY_UNITS_SPHERE_RADIUS, filter, AVEN_PlayerUnit::StaticClass(), actors_to_ignore, ally_units);
	else if (Cast<AVEN_EnemyUnit>(attacker))
		UKismetSystemLibrary::SphereOverlapActors(GetWorld(), attacker->GetActorLocation(), NERBY_UNITS_SPHERE_RADIUS, filter, AVEN_EnemyUnit::StaticClass(), actors_to_ignore, ally_units);

	m_battle_units_queue.Add(attacker);
	for (auto ally_unit : ally_units)
	{
		if (FMath::Abs(attacker->GetActorLocation().Z - ally_unit->GetActorLocation().Z) < NERBY_UNITS_HEIGHT_DELTA)
			m_battle_units_queue.Add(ally_unit);
	}

	ally_units.Empty();
	filter.Empty();
	actors_to_ignore.Empty();
	actors_to_ignore.Add(defender);

	if (Cast<AVEN_PlayerUnit>(defender))
		UKismetSystemLibrary::SphereOverlapActors(GetWorld(), defender->GetActorLocation(), NERBY_UNITS_SPHERE_RADIUS, filter, AVEN_PlayerUnit::StaticClass(), actors_to_ignore, ally_units);
	else if (Cast<AVEN_EnemyUnit>(defender))
		UKismetSystemLibrary::SphereOverlapActors(GetWorld(), defender->GetActorLocation(), NERBY_UNITS_SPHERE_RADIUS, filter, AVEN_EnemyUnit::StaticClass(), actors_to_ignore, ally_units);

	m_battle_units_queue.Add(defender);
	for (auto ally_unit : ally_units)
	{
		if (FMath::Abs(defender->GetActorLocation().Z - ally_unit->GetActorLocation().Z) < NERBY_UNITS_HEIGHT_DELTA)
			m_battle_units_queue.Add(ally_unit);
	}

	int player_units_count = 0;
	for (auto unit : m_battle_units_queue)
	{
		if (Cast<AVEN_PlayerUnit>(unit))
			player_units_count++;
	}

	if (player_units_count < 2)
		TeleportStragglerPlayerUnit();
}

void UVEN_BattleSystem::UpdateBattleUnitsQueue()
{
	if (!m_battle_units_queue.Num())
		return;

	for (const auto& unit : m_battle_units_queue)
	{
		const auto& player_unit_c = Cast<AVEN_PlayerUnit>(unit);
		const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(unit);

		if ((player_unit_c && player_unit_c->IsDead()) || (enemy_unit_c && enemy_unit_c->IsDead()))
			m_battle_died_units.Add(unit);
	}

	if (m_battle_died_units.Num())
	{
		for (const auto& dead_unit : m_battle_died_units)
			m_battle_units_queue.Remove(dead_unit);
	}
}

void UVEN_BattleSystem::ShiftBattleUnitsQueue()
{
	auto current_owner = m_battle_units_queue[0];
	m_battle_units_queue.Remove(current_owner);
	m_battle_units_queue.Add(current_owner);
}

AActor* UVEN_BattleSystem::GetCurrentTurnOwner() const
{
	if (m_battle_units_queue.Num())
		return m_battle_units_queue[0];

	//debug_log("UVEN_BattleSystem::GetCurrentTurnOwner. Attempt to access empty array", FColor::Red);
	return nullptr;
}

AVEN_GameMode* UVEN_BattleSystem::GetGameMode() const
{
	const auto& game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("UVEN_BattleSystem::GetGameMode. Game Mode not found!", FColor::Red);

	return game_mode;
}

AVEN_MainController* UVEN_BattleSystem::GetMainController() const
{
	const auto& game_mode = GetGameMode();
	if (!GetGameMode())
	{
		//debug_log("UVEN_BattleSystem::GetMainController. Game Mode not found!", FColor::Red);
		return nullptr;
	}

	const auto& main_controller = Cast<AVEN_MainController>(game_mode->GetMainController());
	if (!main_controller)
	{
		//debug_log("UVEN_BattleSystem::GetMainController. Main Controller not found!", FColor::Red);
	}

	return main_controller;
}

void UVEN_BattleSystem::AdjustUnitsCollisions()
{
	for (const auto& unit : m_battle_units_queue)
	{
		const auto& unit_character_c = Cast<ACharacter>(unit);
		if (!unit_character_c)
		{
			//debug_log("UVEN_BattleSystem::EnableUnitsCollisions. Unit cannot be casted to ACharacter", FColor::Red);
			continue;
		}

		unit_character_c->GetCapsuleComponent()->SetCanEverAffectNavigation(m_battle_is_in_progress && (unit != GetCurrentTurnOwner()));
	}
}

bool UVEN_BattleSystem::CanBattleBeFinished()
{
	int player_units_left = 0;
	int enemy_units_left = 0;

	for (const auto& unit : m_battle_units_queue)
	{
		const auto& player_unit_c = Cast<AVEN_PlayerUnit>(unit);
		const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(unit);

		if (player_unit_c && !player_unit_c->IsDead())
			player_units_left++;
		else if (enemy_unit_c && !enemy_unit_c->IsDead())
			enemy_units_left++;
	}

	if (!player_units_left)
	{
		m_game_over = true;
		return true;
	}

	return enemy_units_left == 0;
}

void UVEN_BattleSystem::ShiftRoundStarter()
{
	int32_t round_starter_index = -1;
	if (m_battle_units_queue.Find(m_battle_round_starter, round_starter_index))
	{
		if (round_starter_index == (m_battle_units_queue.Num() - 1))
			m_battle_round_starter = m_battle_units_queue[0];
		else
			m_battle_round_starter = m_battle_units_queue[round_starter_index + 1];
	}
	//else
		//debug_log("UVEN_BattleSystem::ShiftRoundStarter. Cannot find round starter", FColor::Red);
}

void UVEN_BattleSystem::ResetAllTurnPoints()
{
	for (const auto& unit : m_battle_units_queue)
	{
		const auto& player_unit_c = Cast<AVEN_PlayerUnit>(unit);
		if (player_unit_c)
		{
			player_unit_c->ResetTurnPoints();
			continue;
		}

		const auto& enemy_unit_c = Cast<AVEN_EnemyUnit>(unit);
		if (enemy_unit_c)
		{
			enemy_unit_c->ResetTurnPoints();
			continue;
		}
	}
}

void UVEN_BattleSystem::TeleportStragglerPlayerUnit()
{
	AVEN_PlayerUnit* player_unit_if_fight = nullptr;
	AVEN_PlayerUnit* player_unit_straggler = nullptr;

	for (auto unit : m_battle_units_queue)
	{
		player_unit_if_fight = Cast<AVEN_PlayerUnit>(unit);
		if (player_unit_if_fight)
			break;
	}

	if (!player_unit_if_fight)
		return;

	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	const auto all_player_units = game_mode->GetPlayerUnits();
	for (const auto& player_unit : all_player_units)
	{
		if (player_unit != player_unit_if_fight)
		{
			player_unit_straggler = player_unit;
			break;
		}
	}

	if (!player_unit_straggler)
		return;

	const float distance_to_1st_point = calculate_distance(player_unit_if_fight->GetActorLocation(), m_teleport_point_1.GetLocation(), true);
	const float distance_to_2nd_point = calculate_distance(player_unit_if_fight->GetActorLocation(), m_teleport_point_2.GetLocation(), true);
	const FTransform teleport_point = distance_to_1st_point > distance_to_2nd_point ? m_teleport_point_1 : m_teleport_point_2;

	player_unit_straggler->SetActorLocation(teleport_point.GetLocation(), false, nullptr, ETeleportType::ResetPhysics);
	player_unit_straggler->SetActorRotation(teleport_point.GetRotation());
	m_battle_units_queue.Add(player_unit_straggler);
}