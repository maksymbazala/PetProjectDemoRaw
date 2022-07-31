// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VEN_InputBindings.h"
#include "VEN_Types.h"

#include "VEN_MainController.generated.h"

class AVEN_GameMode;
class AVEN_PlayerUnit;

UCLASS()
class GAME_4_24_API AVEN_MainController : public APlayerController
{
	GENERATED_BODY()

public:

	AVEN_MainController();

protected:

	void BeginPlay() override;
	void SetupInputComponent() override;
	void Tick(float DeltaTime) override;

public:

	void Initialize();
	void UnitStartedMovement(AVEN_PlayerUnit* unit, FVector destination_location, FRotator destination_rotation, float time_to_complete_movement);
	void UnitFinishedMovement(AVEN_PlayerUnit* unit);
	void OnStartBattle();
	void OnFinishBattle();
	bool IsLocked() const;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "On Button Click From Widget"))
	void OnButtonClickFromWidget(EWidgetButtonType button_type);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "On Player Unit Panel Click From Widget"))
	void OnPlayerUnitPanelClickFromWidget(EPlayerUnitType unit_type);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Lock Main Controller"))
	void LockMainController(bool lock);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Active Player Unit Ref"))
	void SetActivePlayerUnitRef(AVEN_PlayerUnit* player_unit);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Active Player Unit Enum"))
	void SetActivePlayerUnitEnum(EPlayerUnitType player_unit_type);

protected:

	void KeyboardForward(float axis);
	void KeyboardRight(float axis);
	void MouseYaw(float axis);
	void MousePitch(float axis);
	void MouseScrollUp();
	void MouseScrollDown();
	void MouseMiddlePressed();
	void MouseMiddleReleased();
	void MouseLeftClicked();
	void MouseRightClicked();
	void KeyboardSpace();
	void KeyboardF();
	void KeyboardC();
	void KeyboardT();

private:

	void HandlePlayerUnitMouseAction(vendetta::input_bindings::MOUSE_ACTION mouse_action, float axis = 0.f);
	void HandlePlayerUnitKeyboardeAction(vendetta::input_bindings::KEYBOARD_ACTION keyboard_action, float axis = 0.f);
	void HandlePlayerUnitSelection(FHitResult hit_result, bool enable_follow = false);
	void HandleActorOnHover();

	AVEN_GameMode* GetGameMode();
	AVEN_PlayerUnit* GetActivePlayerUnit();
	void CalculateFollowMyLeadDestinationPoints(TArray<FVector>& points, FVector destination_location, FRotator destination_rotation);
	void EnableFollowMyLeadMode(bool enable);

private:

	bool m_follow_my_lead_mode;
	UPROPERTY()
	AActor* m_hovered_actor;

	bool m_cached_battle_is_allowed;
	bool m_cached_follow_my_lead_mode;
	UPROPERTY()
	AVEN_PlayerUnit* m_cached_active_player;

	bool m_unit_selection_is_allowed;
	bool m_is_locked;

};
