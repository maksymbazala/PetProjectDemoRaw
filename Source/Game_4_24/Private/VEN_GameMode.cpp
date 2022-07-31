// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_GameMode.h"
#include "VEN_EnemyUnit.h"
#include "VEN_InteractableNPC.h"
#include "VEN_FreeFunctions.h"
#include "VEN_Types.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

namespace
{
	const int PLAYER_UNITS_TOTAL = 2;
	const int SERIALIZER_INTERACTABLE_ITEM_GROUP = 10000;
}

using namespace vendetta;

AVEN_GameMode::AVEN_GameMode()
	: m_main_controller(nullptr)
	, m_main_camera(nullptr)
{

}

void AVEN_GameMode::BeginPlay()
{
	Super::BeginPlay();

	m_inventory.Empty();

	m_unique_ids.Empty();
	m_unique_ids.Add(SERIALIZED_OBJECT::INTERACTABLE_ITEM);
	m_unique_ids.Add(SERIALIZED_OBJECT::INTERACTABLE_NPC);
}

TArray<AVEN_PlayerUnit*> AVEN_GameMode::GetPlayerUnits() const
{
	return m_player_units;
}

AVEN_MainController* AVEN_GameMode::GetMainController() const
{
	return m_main_controller;
}

AVEN_Camera* AVEN_GameMode::GetMainCamera() const
{
	return m_main_camera;
}

UVEN_QuestsManager* AVEN_GameMode::GetQuestsManager() const
{
	return m_quests_manager;
}

UVEN_BattleSystem* AVEN_GameMode::GetBattleSystem() const
{
	return m_battle_system;
}

UVEN_CursorManager* AVEN_GameMode::GetCursorManager() const
{
	return m_cursor_manager;
}

TArray<int> AVEN_GameMode::GetInventory() const
{
	return m_inventory;
}

void AVEN_GameMode::RequestUniqueId(AActor* actor)
{
	int unique_id;
	const auto& casted_interactable_item = Cast<AVEN_InteractableItem>(actor);

	if (casted_interactable_item)
	{
		if (m_unique_ids[SERIALIZED_OBJECT::INTERACTABLE_ITEM].Num())
			unique_id = m_unique_ids[SERIALIZED_OBJECT::INTERACTABLE_ITEM][m_unique_ids[SERIALIZED_OBJECT::INTERACTABLE_ITEM].Num() - 1] + 1;
		else
			unique_id = SERIALIZER_INTERACTABLE_ITEM_GROUP;

		m_unique_ids[SERIALIZED_OBJECT::INTERACTABLE_ITEM].Add(unique_id);
		casted_interactable_item->SetUniqueActorId(unique_id);
	}
}

void AVEN_GameMode::AddItemToInventory(int item_id)
{
	if (m_inventory.Find(item_id) != INDEX_NONE)
	{
		//debug_log("AVEN_GameMode::AddItemToInventory. Attempt to add existing item to inventory", FColor::Red);
		return;
	}

	m_inventory.Add(item_id);
}

void AVEN_GameMode::RemoveItemFromInventory(int item_id)
{
	if (m_inventory.Find(item_id) == INDEX_NONE)
	{
		//debug_log("AVEN_GameMode::RemoveItemFromInventory. Attempt to remove unexisting item from inventory", FColor::Red);
		return;
	}

	m_inventory.Remove(item_id);
}

void AVEN_GameMode::OnCursorChange(CURSOR_TYPE new_cursor)
{
	GameModeOnCursorChanged((int)new_cursor);
}

void AVEN_GameMode::OnActorReady(AActor* actor)
{
	GameModeOnActorReady(actor);
}

void AVEN_GameMode::OnLevelLoaded(FName level_name)
{
	if (level_name == "Menu")
		m_current_level = ECurrentLevel::MENU;
	else if (level_name == "Level")
		m_current_level = ECurrentLevel::GAMEPLAY;

	switch (m_current_level)
	{
		case ECurrentLevel::MENU:
		{
			OnAmbientMusicUpdate(EAmbientMusicType::MENU_AMBIENT);
		} break;
		case ECurrentLevel::GAMEPLAY:
		{
			InitializeAll();
			OnAmbientMusicUpdate(EAmbientMusicType::COMMON_AMBIENT);
		} break;
		default: break;
	}
}

void AVEN_GameMode::OnLevelPreUnload()
{
	UninitializePlayerUnits();

	if (m_battle_system)
		m_battle_system->Uninitialize();
}

ECurrentLevel AVEN_GameMode::GetCurrentLevel() const
{
	return m_current_level;
}

void AVEN_GameMode::OnUpdateFromBattlePopupWidget(EWidgetBattleResponseType response)
{
	if (m_battle_system)
	{
		switch (response)
		{
			case EWidgetBattleResponseType::START_BATTLE_SHOWN: m_battle_system->OnStartPopupShown(); break;
			case EWidgetBattleResponseType::VICTORY_SHOWN: m_battle_system->OnFinalPopupShown();  break;
			case EWidgetBattleResponseType::DEFEAT_SHOWN: m_battle_system->OnFinalPopupShown(); break;
			default: break;
		}
	}
}

UCameraComponent* AVEN_GameMode::GetMainCameraComponent() const
{
	if (m_main_camera)
		return m_main_camera->GetMainCameraComponent();

	return nullptr;
}

USceneComponent* AVEN_GameMode::GetMainCameraRoot() const
{
	if (m_main_camera)
		return m_main_camera->GetMainCameraRoot();

	return nullptr;
}

void AVEN_GameMode::InitializeAll()
{
	GatherPlayerUnits();
	InitializeController();
	InitializeCamera();
	InitializeQuestsManager();
	InitializeBattleSystem();
	InitializeCursorManager();
	InitializePlayerUnits();
	InitializeEnemyUnits();
	InitializeNPCUnits();
}

void AVEN_GameMode::InitializePlayerUnits()
{
	if (!m_player_units.Num())
		return;

	for (const auto& player_unit : m_player_units)
		player_unit->Initialize();
}

void AVEN_GameMode::InitializeEnemyUnits()
{
	TArray<AActor*> found_actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVEN_EnemyUnit::StaticClass(), found_actors);

	for (auto actor : found_actors)
	{
		const auto& enemy_unit = Cast<AVEN_EnemyUnit>(actor);
		if (enemy_unit)
			enemy_unit->Initialize();
	}
}

void AVEN_GameMode::InitializeNPCUnits()
{
	TArray<AActor*> found_actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVEN_InteractableNPC::StaticClass(), found_actors);

	for (auto actor : found_actors)
	{
		const auto& npc_unit = Cast<AVEN_InteractableNPC>(actor);
		if (npc_unit)
			npc_unit->Initialize();
	}
}

void AVEN_GameMode::InitializeController()
{
	m_main_controller = Cast<AVEN_MainController>(GetWorld()->GetFirstPlayerController());

	if (!m_main_controller)
	{
		//debug_log("AVEN_GameMode::InitializeController. Controller not found", FColor::Red);
		return;
	}

	m_main_controller->Initialize();
}

void AVEN_GameMode::InitializeCamera()
{
	if (!m_main_controller)
	{
		//debug_log("AVEN_GameMode::InitializeCamera. Cannot initialize camera due to uninitialized controller", FColor::Red);
		return;
	}

	m_main_camera = Cast<AVEN_Camera>(m_main_controller->GetPawn());
	//if (!m_main_camera)
		//debug_log("AVEN_GameMode::InitializeCamera. Camera not found", FColor::Red);
}

void AVEN_GameMode::InitializeQuestsManager()
{
	m_quests_manager = NewObject<UVEN_QuestsManager>(this, UVEN_QuestsManager::StaticClass(), FName("quests_manager"));

	if (m_quests_manager)
		m_quests_manager->Initialize();
	//else
		//debug_log("AVEN_GameMode::InitializeQuestsManager. Quests Manager is nullptr", FColor::Red);
}

void AVEN_GameMode::InitializeBattleSystem()
{
	m_battle_system = NewObject<UVEN_BattleSystem>(this, UVEN_BattleSystem::StaticClass(), FName("battle_system"));

	if (m_battle_system)
	{
		m_battle_system->Initialize();
		m_battle_system->InitializeBattleTeleportPoints(m_battle_teleport_point_1, m_battle_teleport_point_2);
	}
	//else
		//debug_log("AVEN_GameMode::InitializeBattleSystem. Battle System is nullptr", FColor::Red);
}

void AVEN_GameMode::InitializeCursorManager()
{
	m_cursor_manager = NewObject<UVEN_CursorManager>(this, UVEN_CursorManager::StaticClass(), FName("cursor_manager"));

	if (m_cursor_manager)
		m_cursor_manager->Initialize();
	//else
		//debug_log("AVEN_GameMode::InitializeCursorManager. Cursor Manager is nullptr", FColor::Red);
}

void AVEN_GameMode::GatherPlayerUnits()
{
	TArray<AActor*> found_actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVEN_PlayerUnit::StaticClass(), found_actors);

	m_player_units.Empty();
	for (auto actor : found_actors)
		m_player_units.Add(Cast<AVEN_PlayerUnit>(actor));

	//if (m_player_units.Num() != PLAYER_UNITS_TOTAL)
		//debug_log("AVEN_GameMode::InitializePlayerUnits. Number of player units != PLAYER_UNITS_TOTAL", FColor::Red);
}

void AVEN_GameMode::UninitializePlayerUnits()
{
	for (const auto& player_unit : m_player_units)
	{
		if (player_unit)
			player_unit->Uninitialize();
	}
}