// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_MainController.h"
#include "VEN_GameMode.h"
#include "VEN_Camera.h"
#include "VEN_PlayerUnit.h"
#include "VEN_EnemyUnit.h"
#include "VEN_InteractableItem.h"
#include "VEN_InteractableNPC.h"
#include "VEN_BattleSystem.h"
#include "VEN_FreeFunctions.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

using namespace vendetta;

AVEN_MainController::AVEN_MainController()
	: m_follow_my_lead_mode(true)
	, m_hovered_actor(nullptr)
	, m_cached_battle_is_allowed(true)
	, m_cached_follow_my_lead_mode(m_follow_my_lead_mode)
	, m_cached_active_player(nullptr)
	, m_unit_selection_is_allowed(true)
	, m_is_locked(true)
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AVEN_MainController::BeginPlay()
{
	Super::BeginPlay();


}

void AVEN_MainController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAxis("KeyboardMoveForward", this, &AVEN_MainController::KeyboardForward);
		InputComponent->BindAxis("KeyboardMoveRight", this, &AVEN_MainController::KeyboardRight);

		InputComponent->BindAxis("MouseMoveYaw", this, &AVEN_MainController::MouseYaw);
		InputComponent->BindAxis("MouseMovePitch", this, &AVEN_MainController::MousePitch);

		InputComponent->BindAction("MouseScrollUp", IE_Released, this, &AVEN_MainController::MouseScrollUp);
		InputComponent->BindAction("MouseScrollDown", IE_Released, this, &AVEN_MainController::MouseScrollDown);

		InputComponent->BindAction("MouseClickMiddle", IE_Pressed, this, &AVEN_MainController::MouseMiddlePressed);
		InputComponent->BindAction("MouseClickMiddle", IE_Released, this, &AVEN_MainController::MouseMiddleReleased);

		InputComponent->BindAction("MouseClickLeft", IE_Released, this, &AVEN_MainController::MouseLeftClicked);
		InputComponent->BindAction("MouseClickRight", IE_Released, this, &AVEN_MainController::MouseRightClicked);

		InputComponent->BindAction("KeyboardSpace", IE_Released, this, &AVEN_MainController::KeyboardSpace);
		InputComponent->BindAction("KeyboardF", IE_Released, this, &AVEN_MainController::KeyboardF);
		InputComponent->BindAction("KeyboardC", IE_Released, this, &AVEN_MainController::KeyboardC);
		InputComponent->BindAction("KeyboardT", IE_Released, this, &AVEN_MainController::KeyboardT);
	}
}

void AVEN_MainController::Tick(float DeltaTime)
{
	APlayerController::Tick(DeltaTime);

}

void AVEN_MainController::Initialize()
{
	if (!GetGameMode()->GetPlayerUnits().Num())
	{
		//debug_log("AVEN_MainController::Initialize. Player units cannot be retrieved", FColor::Red);
		return;
	}

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->SetCameraFollowMode(true, GetActivePlayerUnit());

	m_cached_battle_is_allowed = true;
	m_cached_follow_my_lead_mode = true;
	m_cached_active_player = nullptr;
	m_unit_selection_is_allowed = true;
	m_follow_my_lead_mode = true;

	//Everything else done within level blueprint
}

void AVEN_MainController::UnitStartedMovement(AVEN_PlayerUnit* unit, FVector destination_location, FRotator destination_rotation, float time_to_complete_movement)
{
	if (GetActivePlayerUnit() && GetActivePlayerUnit() == unit)
	{
		//follow active player unit by inactive ones
		if (m_follow_my_lead_mode)
		{
			TArray<FVector> fml_points;
			CalculateFollowMyLeadDestinationPoints(fml_points, destination_location, destination_rotation);

			for (const auto& player_unit : GetGameMode()->GetPlayerUnits())
			{
				if (!player_unit->GetActive())
					player_unit->FollowMyLead(fml_points, time_to_complete_movement);
			}
		}
	}
}

void AVEN_MainController::UnitFinishedMovement(AVEN_PlayerUnit* unit)
{

}

void AVEN_MainController::OnStartBattle()
{
	m_unit_selection_is_allowed = false;

	//cache pre-battle states
	m_cached_follow_my_lead_mode = m_follow_my_lead_mode;
	m_cached_active_player = GetActivePlayerUnit();

	//setup pre-battle properties
	EnableFollowMyLeadMode(false);
	GetGameMode()->OnWidgetButtonStateChange(EWidgetButtonType::FOLLOW_MY_LEAD, false);
	GetActivePlayerUnit()->SetActive(false);
}

void AVEN_MainController::OnFinishBattle()
{
	m_unit_selection_is_allowed = true;

	//retrieve and apply post-battle properties from cached data
	EnableFollowMyLeadMode(m_cached_follow_my_lead_mode);
	GetGameMode()->OnWidgetButtonStateChange(EWidgetButtonType::FOLLOW_MY_LEAD, m_unit_selection_is_allowed, m_follow_my_lead_mode ? EWidgetButtonState::ACTIVE : EWidgetButtonState::INACTIVE);
	SetActivePlayerUnitRef(m_cached_active_player);
}

bool AVEN_MainController::IsLocked() const
{
	return m_is_locked;
}

void AVEN_MainController::OnButtonClickFromWidget(EWidgetButtonType button_type)
{
	if (m_is_locked)
		return;

	switch (button_type)
	{
		case EWidgetButtonType::FINISH_TURN:
		{
			//simply map finish turn to Space button action
			KeyboardSpace();
		} break;
		case EWidgetButtonType::FOLLOW_MY_LEAD:
		{
			//simply map toggle follow my lead mode to F button action
			KeyboardF();
			GetGameMode()->OnWidgetButtonStateChange(EWidgetButtonType::FOLLOW_MY_LEAD, m_unit_selection_is_allowed, m_follow_my_lead_mode ? EWidgetButtonState::ACTIVE : EWidgetButtonState::INACTIVE);
		} break;
		default: break;
	}
}

void AVEN_MainController::OnPlayerUnitPanelClickFromWidget(EPlayerUnitType unit_type)
{
	if (!m_unit_selection_is_allowed || m_is_locked)
		return;

	const auto& active_player_unit = GetActivePlayerUnit();
	if (!active_player_unit)
		return;

	auto active_player_unit_type = active_player_unit->m_unit_type; 
	if (active_player_unit_type == unit_type)
		KeyboardC(); //follow camera if clicked on active player unit panel
	else
	{
		SetActivePlayerUnitEnum(unit_type);

		const auto& main_camera = GetGameMode()->GetMainCamera();
		if (main_camera)
			main_camera->SetCameraFollowMode(main_camera->IsCameraInFollowMode(), GetActivePlayerUnit());
	}
}

void AVEN_MainController::LockMainController(bool lock)
{
	m_is_locked = lock;

	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	const auto& main_camera = game_mode->GetMainCamera();
	if (!main_camera)
		return;

	const auto& battle_system = game_mode->GetBattleSystem();
	if (!battle_system)
		return;

	if (lock)
	{
		main_camera->RotationRelease();

		m_cached_battle_is_allowed = battle_system->GetBattleAllowed();
		if (!battle_system->IsBattleInProgress())
			battle_system->SetBattleAllowed(false);
	}
	else
	{
		if (!battle_system->IsBattleInProgress())
			battle_system->SetBattleAllowed(m_cached_battle_is_allowed);
	}
}

void AVEN_MainController::SetActivePlayerUnitRef(AVEN_PlayerUnit* player_unit)
{
	if (m_is_locked)
		return;

	for (const auto& player_unit_it : GetGameMode()->GetPlayerUnits())
		player_unit_it->SetActive(player_unit_it == player_unit);
}

void AVEN_MainController::SetActivePlayerUnitEnum(EPlayerUnitType player_unit_type)
{
	if (m_is_locked)
		return;

	for (const auto& player_unit_it : GetGameMode()->GetPlayerUnits())
		player_unit_it->SetActive(player_unit_it->m_unit_type == player_unit_type);
}

void AVEN_MainController::KeyboardForward(float axis)
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnKeyboardAction(input_bindings::KEYBOARD_ACTION::KEYBOARD_FORWARD, axis);
}

void AVEN_MainController::KeyboardRight(float axis)
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnKeyboardAction(input_bindings::KEYBOARD_ACTION::KEYBOARD_RIGHT, axis);
}

void AVEN_MainController::MouseYaw(float axis)
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnMouseAction(input_bindings::MOUSE_ACTION::MOUSE_MOVE_X, axis);
	HandlePlayerUnitMouseAction(input_bindings::MOUSE_ACTION::MOUSE_MOVE_X, axis);
	HandleActorOnHover();
}

void AVEN_MainController::MousePitch(float axis)
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnMouseAction(input_bindings::MOUSE_ACTION::MOUSE_MOVE_Y, axis);
	HandlePlayerUnitMouseAction(input_bindings::MOUSE_ACTION::MOUSE_MOVE_Y, axis);
	HandleActorOnHover();
}

void AVEN_MainController::MouseScrollUp()
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnMouseAction(input_bindings::MOUSE_ACTION::MOUSE_SCROLL_UP);
}

void AVEN_MainController::MouseScrollDown()
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnMouseAction(input_bindings::MOUSE_ACTION::MOUSE_SCROLL_DOWN);
}

void AVEN_MainController::MouseMiddlePressed()
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnMouseAction(input_bindings::MOUSE_ACTION::MOUSE_MIDDLE_PRESSED);
}

void AVEN_MainController::MouseMiddleReleased()
{
	if (m_is_locked)
		return;

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnMouseAction(input_bindings::MOUSE_ACTION::MOUSE_MIDDLE_RELEASED);
}

void AVEN_MainController::MouseLeftClicked()
{
	if (m_is_locked)
		return;

	HandlePlayerUnitMouseAction(input_bindings::MOUSE_ACTION::MOUSE_LEFT);
}

void AVEN_MainController::MouseRightClicked()
{
	if (m_is_locked)
		return;

	HandlePlayerUnitMouseAction(input_bindings::MOUSE_ACTION::MOUSE_RIGHT);
}

void AVEN_MainController::KeyboardSpace()
{
	if (m_is_locked)
		return;

	if (GetActivePlayerUnit())
		GetActivePlayerUnit()->OnKeyboardAction(input_bindings::KEYBOARD_ACTION::KEYBOARD_SPACE);
}

void AVEN_MainController::KeyboardF()
{
	if (m_is_locked)
		return;

	//toggle follow my lead mode
	EnableFollowMyLeadMode(!m_follow_my_lead_mode);
	GetGameMode()->OnWidgetButtonStateChange(EWidgetButtonType::FOLLOW_MY_LEAD, m_unit_selection_is_allowed, m_follow_my_lead_mode ? EWidgetButtonState::ACTIVE : EWidgetButtonState::INACTIVE);
}

void AVEN_MainController::KeyboardC()
{
	if (m_is_locked)
		return;

	//follow camera
	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		main_camera->OnKeyboardAction(input_bindings::KEYBOARD_ACTION::KEYBOARD_C);
}

void AVEN_MainController::KeyboardT()
{
	if (m_is_locked)
		return;

	const auto& active_player_unit = GetActivePlayerUnit();
	if (active_player_unit && active_player_unit->IsInBattle())
		active_player_unit->ToggleTacticalView();
}

void AVEN_MainController::HandlePlayerUnitMouseAction(input_bindings::MOUSE_ACTION mouse_action, float axis)
{
	if (m_is_locked)
		return;

	if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_LEFT)
	{
		FHitResult HitResultVisibility;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResultVisibility);
		HandlePlayerUnitSelection(HitResultVisibility);
	}
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MOVE_X ||
		mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MOVE_Y ||
		mouse_action == input_bindings::MOUSE_ACTION::MOUSE_RIGHT)
	{
		FHitResult HitResultVisibility;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResultVisibility);

		if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_RIGHT)
			HandlePlayerUnitSelection(HitResultVisibility, true);

		if (GetActivePlayerUnit())
			GetActivePlayerUnit()->OnMouseAction(HitResultVisibility, mouse_action, axis);
	}
}

void AVEN_MainController::HandlePlayerUnitKeyboardeAction(input_bindings::KEYBOARD_ACTION keyboard_action, float axis)
{
	if (m_is_locked)
		return;

	if (GetActivePlayerUnit())
		GetActivePlayerUnit()->OnKeyboardAction(keyboard_action, axis);
}

void AVEN_MainController::HandlePlayerUnitSelection(FHitResult hit_result, bool enable_follow)
{
	if (!m_unit_selection_is_allowed || m_is_locked)
		return;

	const auto& clicked_player_unit = Cast<AVEN_PlayerUnit>(hit_result.GetActor());
	if (clicked_player_unit && clicked_player_unit != GetActivePlayerUnit())
	{
		SetActivePlayerUnitRef(clicked_player_unit);

		if (enable_follow)
			EnableFollowMyLeadMode(true);

		GetGameMode()->OnWidgetButtonStateChange(EWidgetButtonType::FOLLOW_MY_LEAD, m_unit_selection_is_allowed, m_follow_my_lead_mode ? EWidgetButtonState::ACTIVE : EWidgetButtonState::INACTIVE);

		const auto& main_camera = GetGameMode()->GetMainCamera();
		if (main_camera)
			main_camera->SetCameraFollowMode(main_camera->IsCameraInFollowMode(), clicked_player_unit);
	}
}

void AVEN_MainController::HandleActorOnHover()
{
	if (m_is_locked)
		return;

	FHitResult HitResultVisibility;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResultVisibility);
	const auto& hovered_actor = HitResultVisibility.GetActor();
	const auto& camera = GetGameMode()->GetMainCamera();
	if (!camera)
		return;

	if (hovered_actor)
	{
		if (m_hovered_actor)
		{
			//stop highlighting previous mesh
			const auto& player_unit_on_hover = Cast<AVEN_PlayerUnit>(m_hovered_actor);
			const auto& enemy_unit_on_hover = Cast<AVEN_EnemyUnit>(m_hovered_actor);
			const auto& inter_item_on_hover = Cast<AVEN_InteractableItem>(m_hovered_actor);
			const auto& inter_npc_on_hover = Cast<AVEN_InteractableNPC>(m_hovered_actor);

			TArray<UActorComponent*> static_mesh_components;
			TArray<UActorComponent*> skeletal_mesh_components;

			if (player_unit_on_hover)
			{
				player_unit_on_hover->GetMesh()->SetRenderCustomDepth(false);
			}
			else if (enemy_unit_on_hover)
			{
				enemy_unit_on_hover->GetComponents(UStaticMeshComponent::StaticClass(), static_mesh_components);
				enemy_unit_on_hover->GetComponents(USkeletalMeshComponent::StaticClass(), skeletal_mesh_components);
			}
			else if (inter_item_on_hover)
			{
				inter_item_on_hover->GetIteractableItemMesh()->SetRenderCustomDepth(false);
			}
			else if (inter_npc_on_hover)
			{
				inter_npc_on_hover->GetComponents(UStaticMeshComponent::StaticClass(), static_mesh_components);
				inter_npc_on_hover->GetComponents(USkeletalMeshComponent::StaticClass(), skeletal_mesh_components);
			}

			for (auto mesh_comp : static_mesh_components)
			{
				const auto& mesh_comp_c = Cast<UPrimitiveComponent>(mesh_comp);
				if (mesh_comp_c)
					mesh_comp_c->SetRenderCustomDepth(false);
			}
			for (auto mesh_comp : skeletal_mesh_components)
			{
				const auto& mesh_comp_c = Cast<UPrimitiveComponent>(mesh_comp);
				if (mesh_comp_c)
					mesh_comp_c->SetRenderCustomDepth(false);
			}

			m_hovered_actor = nullptr;
		}

		//reset camera post process material
		CAMERA_PP_MATERIAL_TYPE material_type = CAMERA_PP_MATERIAL_TYPE::DEFAULT;

		if (hovered_actor == m_hovered_actor)
			return;

		//start highlighting new mesh
		const auto& player_unit_on_hover = Cast<AVEN_PlayerUnit>(hovered_actor);
		const auto& enemy_unit_on_hover = Cast<AVEN_EnemyUnit>(hovered_actor);
		const auto& inter_item_on_hover = Cast<AVEN_InteractableItem>(hovered_actor);
		const auto& inter_npc_on_hover = Cast<AVEN_InteractableNPC>(hovered_actor);

		TArray<UActorComponent*> static_mesh_components;
		TArray<UActorComponent*> skeletal_mesh_components;

		if (player_unit_on_hover)
		{
			player_unit_on_hover->GetMesh()->SetRenderCustomDepth(true);
			m_hovered_actor = hovered_actor;
		}
		else if (enemy_unit_on_hover && !enemy_unit_on_hover->IsDead())
		{
			enemy_unit_on_hover->GetComponents(UStaticMeshComponent::StaticClass(), static_mesh_components);
			enemy_unit_on_hover->GetComponents(USkeletalMeshComponent::StaticClass(), skeletal_mesh_components);
			material_type = CAMERA_PP_MATERIAL_TYPE::ENEMY_UNIT;
			m_hovered_actor = hovered_actor;
		}
		else if (inter_item_on_hover)
		{
			if (!inter_item_on_hover->CanBeInteractedRightNow())
				return;

			inter_item_on_hover->GetIteractableItemMesh()->SetRenderCustomDepth(true);
			m_hovered_actor = hovered_actor;
		}
		else if (inter_npc_on_hover)
		{
			if (!inter_npc_on_hover->CanBeInteractedRightNow())
				return;

			inter_npc_on_hover->GetComponents(UStaticMeshComponent::StaticClass(), static_mesh_components);
			inter_npc_on_hover->GetComponents(USkeletalMeshComponent::StaticClass(), skeletal_mesh_components);
			m_hovered_actor = hovered_actor;
		}

		for (auto mesh_comp : static_mesh_components)
		{
			const auto& mesh_comp_c = Cast<UPrimitiveComponent>(mesh_comp);
			if (mesh_comp_c)
				mesh_comp_c->SetRenderCustomDepth(true);
		}
		for (auto mesh_comp : skeletal_mesh_components)
		{
			const auto& mesh_comp_c = Cast<UPrimitiveComponent>(mesh_comp);
			if (mesh_comp_c)
				mesh_comp_c->SetRenderCustomDepth(true);
		}

		camera->OnChangeCameraPostProcessMaterial((int)material_type);
	}
}

AVEN_GameMode* AVEN_MainController::GetGameMode()
{
	auto game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("AVEN_MainController::GetGameMode. Game Mode not found!", FColor::Red);

	return game_mode;
}

AVEN_PlayerUnit* AVEN_MainController::GetActivePlayerUnit()
{
	const auto game_mode = GetGameMode();
	if (!game_mode)
		return nullptr;

	for (const auto& player_unit : game_mode->GetPlayerUnits())
	{
		if (player_unit && player_unit->GetActive())
			return player_unit;
	}

	return nullptr;
}

void AVEN_MainController::CalculateFollowMyLeadDestinationPoints(TArray<FVector>& points, FVector destination_location, FRotator destination_rotation)
{
	points.Empty();

	const int32_t NUM_OF_POINTS = 3;
	const TArray<float> DISTANCE_TO_POINTS = { 300.f, 300.f, 400.f };
	const TArray<float> ANGLE_TO_POINTS = { 130.f, 230.f, 180.f };

	for (int32_t i = 0; i < NUM_OF_POINTS; ++i)
	{
		FVector distance = FVector(DISTANCE_TO_POINTS[i], 0, 0);
		FVector point_offset = distance.RotateAngleAxis(ANGLE_TO_POINTS[i] + destination_rotation.Yaw, FVector(0, 0, 1));
		FVector new_point_location = destination_location + point_offset;
		points.Add(new_point_location);
	}
}

void AVEN_MainController::EnableFollowMyLeadMode(bool enable)
{
	if (m_is_locked)
		return;

	if (!enable)
	{
		m_follow_my_lead_mode = false;
		return;
	}

	//a lil bit more difficult check if we want to set mode ON

	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	const auto& battle_system = game_mode->GetBattleSystem();
	if (!battle_system)
		return;

	if (battle_system->IsBattleInProgress())
		return;

	m_follow_my_lead_mode = true;
}