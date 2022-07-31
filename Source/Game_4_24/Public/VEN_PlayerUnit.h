// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_InputBindings.h"
#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "VEN_PlayerUnit.generated.h"

class USplineComponent;
class UStaticMesh;
class UDecalComponent;
class UMaterialInstance;
class AVEN_GameMode;
class AVEN_MainController;
class AVEN_InteractableItem;
class AVEN_InteractableNPC;
class AVEN_EnemyUnit;
class UVEN_BattleSystem;
class UVEN_AnimInstance;
class UVEN_TactialView;


UCLASS()
class GAME_4_24_API AVEN_PlayerUnit : public ACharacter
{
	GENERATED_BODY()

public:

	AVEN_PlayerUnit();

protected:

	virtual void BeginPlay() override;
	void Tick(float DeltaTime) override;

public:

	void OnMouseAction(FHitResult hit_result, vendetta::input_bindings::MOUSE_ACTION mouse_action, float axis = 0.f);
	void OnKeyboardAction(vendetta::input_bindings::KEYBOARD_ACTION keyboard_action, float axis = 0.f);

	void Initialize();
	void Uninitialize();
	bool GetActive() const;
	void SetActive(bool is_active);
	void FollowMyLead(TArray<FVector> points_to_move, float time_to_reach_point);
	void RegisterAnimInstance(UVEN_AnimInstance* instance);
	void OnAnimationUpdate(vendetta::ANIMATION_NOTIFICATION notification);
	int GetXP() const;
	void IncrementXP(int increment_value);
	int GetMoney() const;
	void IncrementMoney(int increment_value);
	FString GetUnitName() const;

	/* BATTLE */

	bool IsInBattle() const;
	void OnStartBattle();
	void OnFinishBattle();
	void OnStartBattleTurn();
	void OnFinishBattleTurn();
	void OnReceiveDamage(float damage_received, AActor* damage_dealer);
	vendetta::ATTACK_TYPE GetAttackType() const;
	int GetHP() const;
	int GetTotalHP() const;
	int GetRandomAttackPower() const;
	int GetAttackRange() const;
	float GetAttackPrice() const;
	float GetTurnPointsTotal() const;
	float GetTurnPointsLeft() const;
	void UpdateTurnPoints(float update_on_value);
	void ResetTurnPoints();
	bool IsDead() const;
	void ToggleTacticalView();
	void OnUpdateFromTacticalView(FVector main_area_center_point, FVector minor_area_center_point, const TArray<FEnemyUnitInfo>& enemy_units_info);

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Player Unit")
	bool m_is_active;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Player Unit")
	FString m_unit_name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Player Unit")
	EPlayerUnitType m_unit_type;
	UPROPERTY(EditAnywhere, Category = "In Battle Properties")
	EBattleQueueUnitIconType m_unit_icon_type;

protected:

	/* ### Blueprint Implementable ### */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Map Icon Update"))
	void OnMapIconUpdate(bool is_active);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Tactical View Update"))
	void OnTacticalViewUpdate(bool show, const TArray<FEnemyUnitInfo>& enemy_units_info = {});
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Update XP"))
	void OnUpdateXP(int new_xp_vale);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Unit Info Update"))
	void OnUnitInfoUpdate(EPlayerUnitType unit_type, FPlayerUnitInfo unit_info);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Advanced Cursor Update"))
	void OnAdvancedCursorUpdate(FAdvancedCursorInfo advanced_cursor_info);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Battle Turn Info Update"))
	void OnBattleTurnInfoUpdate(FBattleTurnPlayerUnitInfo battle_turn_info);

	/* ### Blueprint Callable ### */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Request Unit Info Update"))
	void OnRequestUnitInfo();

protected:

	UPROPERTY(EditAnywhere, Category = "Decal")
	UDecalComponent* m_decal;
	UPROPERTY(EditAnywhere, Category = "General")
	int m_xp_total;;
	UPROPERTY(EditAnywhere, Category = "General")
	int m_money;

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
	int m_attack_type;

private:

	//main getters
	AVEN_GameMode* GetGameMode() const;
	AVEN_MainController* GetMainController() const;
	UVEN_BattleSystem* GetBattleSystem() const;
	void HandleMouseAction(FHitResult hit_result, vendetta::input_bindings::MOUSE_ACTION mouse_action);

	//movement
	void BuildTemporaryMovementSpline(const FVector& destination_point);
	void BuildFixedMovementSpline();
	void DestroyMovementSpline(AActor* movement_spline);
	void BuildDestinationPointActor(FVector destination_point);
	void DestroyDestinationPointActor();
	void SetupDestinationPointActor(FVector destination_point);
	float CalculateTimeToCompleteMovementSpline(USplineComponent* spline_component);
	void PrepareMovement(FHitResult hit_result);
	void StartMovement();
	void ContinueMovement();
	void FinishMovement();

	void SortClosestPoints(TArray<FVector>& points);
	bool IsEnemyUnit(AActor* actor);
	bool IsNPCUnit(AActor* actor);
	void AnimInstanceAction(vendetta::ANIMATION_ACTION anim_action);
	void AnimInstanceUpdate(vendetta::ANIMATION_UPDATE anim_update);
	void UpdateCursor(AActor* hovered_actor);
	float GetMovementPrice() const;
	bool CanInteractWithNpcInPlace(AVEN_InteractableNPC* npc_unit);
	bool CanInteractWithNpcAfterMove(AVEN_InteractableNPC* npc_unit);
	bool CanInteractWithItemInPlace(AVEN_InteractableItem* item);
	bool CanInteractWithItemAfterMove(AVEN_InteractableItem* item);
	bool CanMoveToPreCalculatedPoint();
	void ProcessInteractableActorOnHover(AActor* actor);
	void FinishMoveAndInteract();
	FVector GetInteractPointLocation(AActor* interactable_actor, float interaction_radius) const;
	void ChangeFocusedTarget(AActor* new_target);
	void NotifyBlueprint();

	/* BATTLE */

	void InitBattleRelatedProperties();
	void ResetBattleRelatedProperties();
	void RegisterTacticalViewInstance();
	void RotateToFocusedTarget();
	void PrepareAttackAnimation();
	void Die();
	bool IsEnoughPointsForAction(vendetta::IN_BATTLE_UNIT_ACTION action);
	vendetta::DIRECTION GetAttackDirection(FRotator attacker_rotation) const;
	void RecoverHPs();
	bool CanInitiateBattleInPlace(AVEN_EnemyUnit* enemy_unit);
	bool CanInitiateBattleAfterMove(AVEN_EnemyUnit* enemy_unit);
	bool CanAttackInPlace(AVEN_EnemyUnit* enemy_unit, bool consider_turn_points = true);
	bool CanAttackAfterMove(AVEN_EnemyUnit* enemy_unit, bool consider_turn_points = true);
	void DisableTacticalView();
	void UpdateAdvancedCursor(AActor* hovered_actor, bool show = true);
	void UpdateBattleTurnInfo(bool show = true);
	bool IsAttackingHoveredActor(AActor* hovered_actor) const;

private:

	UPROPERTY()
	UVEN_AnimInstance* m_anim_instance;
	UPROPERTY()
	AActor* m_movement_spline_actor_temporary;
	UPROPERTY()
	USplineComponent* m_movement_spline_component_temporary;
	UPROPERTY()
	AActor* m_movement_spline_actor_fixed;
	UPROPERTY()
	USplineComponent* m_movement_spline_component_fixed;
	UPROPERTY()
	UStaticMesh* m_movement_spline_mesh_enabled;
	UPROPERTY()
	UStaticMesh* m_movement_spline_mesh_disabled;
	UPROPERTY()
	AActor* m_movement_destination_point_actor;
	UPROPERTY()
	UStaticMesh* m_movement_destination_point_mesh;
	UPROPERTY()
	UMaterialInstance* m_movement_destination_point_decal_material;
	UPROPERTY()
	AActor* m_focused_target;
	UPROPERTY()
	FTimerHandle m_tmr_before_move;

	bool m_is_currently_moving;
	bool m_temporary_movement_spline_is_built;
	bool m_movement_spline_is_visualized;
	float m_spline_comleted_movement_distance;

	bool m_move_and_interact_mode;
	bool m_move_and_interact_spline_calculated;

	/* BATTLE */

	UPROPERTY()
	UVEN_TactialView* m_tactical_view_instance;

	bool m_tactical_view_is_allowed;
	bool m_in_battle;
	bool m_is_dead;
	int m_xp_current;
	int m_hp_current;
	float m_turn_points_left;
	bool m_hp_recovery;
	bool m_show_advanced_cursor;

};
