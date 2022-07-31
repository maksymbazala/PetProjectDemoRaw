// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TimerManager.h"

#include "VEN_BattleSystem.generated.h"

class AActor;
class AVEN_GameMode;
class AVEN_MainController;

UCLASS()
class GAME_4_24_API UVEN_BattleSystem : public UObject
{
	GENERATED_BODY()
		
public:

	enum BATTLE_NOTIFY_TYPE
	{
		START_BATTLE,
		FINISH_BATTLE,
		START_NEW_TURN,
		FINISH_CURRENT_TURN,
		GAME_OVER
	};

	UVEN_BattleSystem();
	
public:

	void Initialize();
	void Uninitialize();
	void InitializeBattleTeleportPoints(FTransform point1, FTransform point2);
	bool GetBattleAllowed() const;
	void SetBattleAllowed(bool is_allowed);
	bool IsBattleInProgress() const;
	bool IsGameOver() const;
	void StartBattle(AActor* attacker, AActor* defender);
	void FinishBattle();
	void Attack(AActor* attacker, AActor* defender);
	void PrepareNextTurn();
	void StartNextTurn();
	void FinishCurrentTurn(AActor* requestor);
	TArray<AActor*> GetAllUnitsInBattle() const;
	TArray<AActor*> GetPlayerUnitsInBattle() const;
	TArray<AActor*> GetEnemyUnitsInBattle() const;
	TArray<FVector> GetEnemyUnitReservedAttackPoints() const;
	void AddEnemyUnitReservedAttackPoint(FVector point);
	bool IsCurrentTurnOwner(AActor* actor) const;
	void UpdateBlueprintBattleQueue();
	void OnStartPopupShown();
	void OnFinalPopupShown();

private:

	void Notify(BATTLE_NOTIFY_TYPE notification);
	void SetupBattleUnitsQueue(AActor* attacker, AActor* defender);
	void UpdateBattleUnitsQueue();
	void ShiftBattleUnitsQueue();
	AActor* GetCurrentTurnOwner() const;

	AVEN_GameMode* GetGameMode() const;
	AVEN_MainController* GetMainController() const;
	void AdjustUnitsCollisions();
	bool CanBattleBeFinished();
	void ShiftRoundStarter();
	void ResetAllTurnPoints();
	void TeleportStragglerPlayerUnit();

private:

	bool m_battle_is_in_progress;
	bool m_battle_is_allowed;
	bool m_game_over;

	UPROPERTY()
	AActor* m_battle_round_starter;

	UPROPERTY()
	TArray<AActor*> m_battle_units_queue;
	UPROPERTY()
	TArray<AActor*> m_battle_died_units;
	TArray<FVector> m_enemy_unit_reserved_attack_points;

	UPROPERTY()
	FTimerHandle m_tmr_before_next_turn;

	FTransform m_teleport_point_1;
	FTransform m_teleport_point_2;

};
