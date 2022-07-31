// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"

#include "VEN_EnemyUnit.generated.h"

class AVEN_GameMode;
class UVEN_BattleSystem;
class UVEN_QuestsManager;
class UVEN_AnimInstance;
class USplineComponent;
class UPawnSensingComponent;
class UDecalComponent;

UCLASS()
class GAME_4_24_API AVEN_EnemyUnit : public ACharacter
{
	GENERATED_BODY()

public:

	AVEN_EnemyUnit();

protected:

	virtual void BeginPlay() override;
	void Tick(float DeltaTime) override;

public:

	void Initialize();
	USkeletalMeshComponent* GetEnemyUnitMesh() const;
	void RegisterAnimInstance(UVEN_AnimInstance* instance);
	void OnAnimationUpdate(vendetta::ANIMATION_NOTIFICATION notification);
	int GetAnimationIndex() const;

	/* BATTLE */

	void OnStartBattle();
	void OnFinishBattle();
	void OnStartBattleTurn();
	void OnFinishBattleTurn();
	void OnTargetDied();
	void OnReceiveDamage(float damage_received, AActor* damage_dealer);
	EEnemyUnitType GetUnitType() const;
	int GetHP() const;
	int GetTotalHP() const;
	int GetAttackPowerMin() const;
	int GetAttackPowerMax() const;
	int GetRandomAttackPower() const;
	int GetAttackRange() const;
	float GetAttackPrice() const;
	float GetTurnPointsTotal() const;
	float GetTurnPointsLeft() const;
	void UpdateTurnPoints(float update_on_value);
	void ResetTurnPoints();
	bool IsDead() const;
	TArray<vendetta::QUEST_ID> GetRelatedQuestIds() const;
	void EnableSensing(bool enable = true);

protected:

	UFUNCTION()
	void OnSeePawn(APawn* other_pawn);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Map Icon Update"))
	void OnMapIconUpdate(bool is_alive);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Rank Widget Update"))
	void OnRankWidgetUpdate(bool show, float percent = 0.f);

private:

	AVEN_GameMode* GetGameMode() const;
	UVEN_BattleSystem* GetBattleSystem() const;
	UVEN_QuestsManager* GetQuestsManager() const;
	void BuildMovementSpline(const FVector& destination_point, float limited_length = 0.f);
	void DestroyMovementSpline();
	void StartMovement();
	void ContinueMovement();
	void FinishMovement();
	void SetupRelatedQuests();
	void AnimInstanceAction(vendetta::ANIMATION_ACTION anim_action);
	void AnimInstanceUpdate(vendetta::ANIMATION_UPDATE anim_update);

	/* BATTLE */

	void InitBattleRelatedProperties();
	void AttackAnimationStart();
	void Die();
	bool IsEnoughPointsForAction(vendetta::IN_BATTLE_UNIT_ACTION action);
	void CalculateAttackPoints(TMap<AActor*, TArray<FVector>>& attack_points_by_player_unit);
	void GatherAllCalculatedPoints(TMap<AActor*, TArray<FVector>>& attack_points_by_player_unit, TArray<FVector>& all_points);
	void ValidateAttackPoints(TArray<FVector>& points);
	void SortAttackPoints(TArray<FVector>& points);
	FVector GetClosestAttackPoint(TArray<FVector>& points) const;
	AActor* GetAttackPointTarget(FVector point, TMap<AActor*, TArray<FVector>>& attack_points_by_player_unit) const;
	void MakeDecision();
	void ReservePoint(FVector point);
	void SensingPlayerUnit(float DeltaTime);
	void RotateToPlayerUnit();
	void FindTarget();
	vendetta::DIRECTION GetAttackDirection(FRotator attacker_rotation) const;

public:

	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	EBattleQueueUnitIconType m_unit_icon_type;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	EEnemyUnitType m_unit_type;

protected:

	UPROPERTY(EditAnywhere, Category = "Custom Animation")
	int m_animation_index;
	UPROPERTY(EditAnywhere, Category = "Decal")
	UDecalComponent* m_decal;

	/* BATTLE */

	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	int m_hp_total;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	float m_turn_points_total;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	int m_attack_power_min;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	int m_attack_power_max;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	int m_attack_range;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	float m_attack_price;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	UPawnSensingComponent* m_sensing_component;

private:

	UPROPERTY()
	UVEN_AnimInstance* m_anim_instance;
	UPROPERTY()
	AActor* m_movement_spline_actor;
	UPROPERTY()
	USplineComponent* m_movement_spline_component;

	bool m_movement_spline_is_built;
	bool m_is_currently_moving;
	float m_spline_comleted_movement_distance;

	TArray<vendetta::QUEST_ID> m_related_quests_ids;

	/* BATTLE */

	bool m_in_battle;
	bool m_is_dead;

	int m_hp_current;
	float m_turn_points_left;

	FVector m_reserved_point;
	UPROPERTY()
	AActor* m_current_target;

	bool m_sensing_is_enabled;
	bool m_sensing_is_active;
	float m_sensing_current_percent;
	FRotator m_rotation_before_sensing;

};
