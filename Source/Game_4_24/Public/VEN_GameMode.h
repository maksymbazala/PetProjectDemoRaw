// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_MainController.h"
#include "VEN_Camera.h"
#include "VEN_PlayerUnit.h"
#include "VEN_InteractableItem.h"
#include "VEN_QuestsManager.h"
#include "VEN_BattleSystem.h"
#include "VEN_CursorManager.h"

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "VEN_GameMode.generated.h"

UCLASS()
class GAME_4_24_API AVEN_GameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:

	AVEN_GameMode();

	void BeginPlay() override;

public:

	enum SERIALIZED_OBJECT
	{
		INTERACTABLE_ITEM,
		INTERACTABLE_NPC
	};

	TArray<AVEN_PlayerUnit*> GetPlayerUnits() const;
	AVEN_Camera* GetMainCamera() const;
	UVEN_QuestsManager* GetQuestsManager() const;
	UVEN_BattleSystem* GetBattleSystem() const;
	UVEN_CursorManager* GetCursorManager() const;
	TArray<int> GetInventory() const;

	void RequestUniqueId(AActor* actor);
	void AddItemToInventory(int item_id);
	void RemoveItemFromInventory(int item_id);
	void OnCursorChange(vendetta::CURSOR_TYPE new_cursor);
	void OnActorReady(AActor* actor);

	/* ### Blueprint Implementable ### */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "CPP Game Mode On Actor Ready"))
	void GameModeOnActorReady(AActor* actor);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "CPP Game Mode On Cusrsor Changed"))
	void GameModeOnCursorChanged(int notification_value);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "CPP Game Mode On Battle Queue Changed"))
	void GameModeOnBattleQueueChanged(const TArray<FBattleQueueUnitInfo>& battle_queue_units_info);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "CPP Game Mode On Button State Change"))
	void OnWidgetButtonStateChange(EWidgetButtonType button_type, bool show, EWidgetButtonState state = EWidgetButtonState::ACTIVE);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quests List Update"))
	void OnQuestsListUpdate(FQuestsListInfo info);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Battle Popup Show"))
	void OnBattlePopupShow(EWidgetBattleRequestType request);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Battle Turn Owner Update"))
	void OnBattleTurnOwnerUpdate(bool show, bool is_player_owner = false);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Widget Flying Data Update"))
	void OnWidgetFlyingDataUpdate(EWidgetFlyingDataType data_type, int data_value, FVector world_position);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Ambient Music Update"))
	void OnAmbientMusicUpdate(EAmbientMusicType music_to_start);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Game Completed"))
	void OnGameCompleted();

	/* ### Blueprint Callable ### */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Level loaded"))
	void OnLevelLoaded(FName level_name);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Level PreUnload"))
	void OnLevelPreUnload();
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Current Level"))
	ECurrentLevel GetCurrentLevel() const;
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Update From Battle Popup Widget"))
	void OnUpdateFromBattlePopupWidget(EWidgetBattleResponseType response);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Main Camera Component"))
	UCameraComponent* GetMainCameraComponent() const;
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Main Camera Root"))
	USceneComponent* GetMainCameraRoot() const;
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Main Controller"))
	AVEN_MainController* GetMainController() const;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Game Mode")
	AVEN_MainController* m_main_controller;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Game Mode")
	AVEN_Camera* m_main_camera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Game Mode")
	TArray<AVEN_PlayerUnit*> m_player_units;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Game Mode")
	TArray<int> m_inventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Game Mode")
	FTransform m_battle_teleport_point_1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPP Game Mode")
	FTransform m_battle_teleport_point_2;

private:

	void InitializeAll();
	void InitializePlayerUnits();
	void InitializeEnemyUnits();
	void InitializeNPCUnits();
	void InitializeController();
	void InitializeCamera();
	void InitializeQuestsManager();
	void InitializeBattleSystem();
	void InitializeCursorManager();
	void GatherPlayerUnits();
	void UninitializePlayerUnits();

private:

	ECurrentLevel m_current_level;

	TMap<SERIALIZED_OBJECT, TArray<int>> m_unique_ids;

	UPROPERTY()
	UVEN_QuestsManager* m_quests_manager;
	UPROPERTY()
	UVEN_BattleSystem* m_battle_system;
	UPROPERTY()
	UVEN_CursorManager* m_cursor_manager;
};
