// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_AnimInstance.h"
#include "VEN_PlayerUnit.h"
#include "VEN_EnemyUnit.h"
#include "VEN_InteractableNPC.h"
#include "VEN_FreeFunctions.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine.h"

using namespace vendetta;

UVEN_AnimInstance::UVEN_AnimInstance()
	: AnimationIndex(-1)
	, ActionIndex(0)
	, Speed(0.f)
	, InBattle(false)
	, PickItem(false)
	, Attack_1(false)
	, Attack_2(false)
	, StunStart(false)
	, StunFinish(false)
	, ReceiveDamageFront(false)
	, ReceiveDamageBack(false)
	, ReceiveDamageLeft(false)
	, ReceiveDamageRight(false)
	, Die(false)
	, m_owner(nullptr)
	, m_active_action(ANIMATION_ACTION::IDLE)
	, m_active_action_can_be_force_stopped(false)
{

}

void UVEN_AnimInstance::NativeBeginPlay()
{
	UAnimInstance::NativeBeginPlay();

	//register owner
	m_owner = GetOwningActor();
	if (!m_owner)
	{
		//debug_log("UVEN_AnimInstance::NativeBeginPlay. Owner is broken", FColor::Red);
		return;
	}
		
	OwningUnitRegistration();
}

void UVEN_AnimInstance::OnUpdateFromBlueprint(int command_id)
{
	ANIMATION_NOTIFICATION command_id_c = (ANIMATION_NOTIFICATION)command_id;

	//reset active action
	bool reset_active_action = false;
	switch (command_id_c)
	{
		case ANIMATION_NOTIFICATION::ITEM_PICK_ANIMATION_FINISHED: reset_active_action =
			m_active_action == ANIMATION_ACTION::PICK_ITEM; break;
		case ANIMATION_NOTIFICATION::ATTACK_ANIMATION_FINISHED: reset_active_action = (
			m_active_action == ANIMATION_ACTION::ATTACK_1 ||
			m_active_action == ANIMATION_ACTION::ATTACK_2); break;
		case ANIMATION_NOTIFICATION::RECEIVE_DAMAGE_ANIMATION_FINISHED: reset_active_action = (
			m_active_action == ANIMATION_ACTION::RECEIVE_DAMAGE_BACK ||
			m_active_action == ANIMATION_ACTION::RECEIVE_DAMAGE_FRONT ||
			m_active_action == ANIMATION_ACTION::RECEIVE_DAMAGE_LEFT ||
			m_active_action == ANIMATION_ACTION::RECEIVE_DAMAGE_RIGHT); break;
		case ANIMATION_NOTIFICATION::DEATH_ANIMATION_FINISHED: reset_active_action =
			m_active_action == ANIMATION_ACTION::DIE; break;
		default: break;
	}

	UpdateForceStopStatus(command_id_c);
	if (reset_active_action)
		ResetActiveAction();

	const auto& owner_player_unit = Cast<AVEN_PlayerUnit>(m_owner);
	if (owner_player_unit)
		owner_player_unit->OnAnimationUpdate(command_id_c);

	const auto& owner_enemy_unit = Cast<AVEN_EnemyUnit>(m_owner);
	if (owner_enemy_unit)
		owner_enemy_unit->OnAnimationUpdate(command_id_c);
}

void UVEN_AnimInstance::OnActionFromOwner(ANIMATION_ACTION action)
{
	if (!TryToStopActiveAction())
		return;

	m_active_action = action;
	m_active_action_can_be_force_stopped = false;

	switch (action)
	{
		case ANIMATION_ACTION::PICK_ITEM: PickItem = true; break;
		case ANIMATION_ACTION::ATTACK_1: Attack_1 = true; break;
		case ANIMATION_ACTION::ATTACK_2: Attack_2 = true; break;
		case ANIMATION_ACTION::RECEIVE_DAMAGE_BACK: ReceiveDamageBack = true; break;
		case ANIMATION_ACTION::RECEIVE_DAMAGE_FRONT: ReceiveDamageFront = true; break;
		case ANIMATION_ACTION::RECEIVE_DAMAGE_LEFT: ReceiveDamageLeft = true; break;
		case ANIMATION_ACTION::RECEIVE_DAMAGE_RIGHT: ReceiveDamageRight = true; break;
		case ANIMATION_ACTION::STUN: StunStart = true; break;
		case ANIMATION_ACTION::DIE: Die = true; break;
		default: /*debug_log("UVEN_AnimInstance::OnActionFromOwner. Invalid action", FColor::Red);*/ break;
	}
}

void UVEN_AnimInstance::OnUpdateFromOwner(ANIMATION_UPDATE update)
{
	switch (update)
	{
		case ANIMATION_UPDATE::START_BATTLE:
		{
			InBattle = true;
			TryToStopActiveAction();
		}break;
		case ANIMATION_UPDATE::FINISH_BATTLE:
		{
			InBattle = false;
		}break;
		case ANIMATION_UPDATE::FINISH_STUN:
		{
			StunFinish = true;
			ResetActiveAction();
		}break;
		case ANIMATION_UPDATE::START_NEXT_ACTION:
		{
			ActionIndex += 1;
		}break;
		case ANIMATION_UPDATE::START_MOVEMENT:
		{
			TryToStopActiveAction();
		}break;
		default:
		{
			//debug_log("UVEN_AnimInstance::OnUpdateFromOwner. Invalid update", FColor::Red);
		}break;
	}
}

ANIMATION_ACTION UVEN_AnimInstance::GetActiveAnimationAction() const
{
	return m_active_action;
}

void UVEN_AnimInstance::OwningUnitRegistration()
{
	if (!m_owner)
	{
		//debug_log("UVEN_AnimInstance::OwningUnitRegistration. Owner is broken", FColor::Red);
		return;
	}

	const auto& owner_player_unit = Cast<AVEN_PlayerUnit>(m_owner);
	if (owner_player_unit)
	{
		owner_player_unit->RegisterAnimInstance(this);
		return;
	}

	const auto& owner_enemy_unit = Cast<AVEN_EnemyUnit>(m_owner);
	if (owner_enemy_unit)
	{
		owner_enemy_unit->RegisterAnimInstance(this);
		AnimationIndex = owner_enemy_unit->GetAnimationIndex();
		return;
	}

	const auto& owner_npc_unit = Cast<AVEN_InteractableNPC>(m_owner);
	if (owner_npc_unit)
	{
		owner_npc_unit->RegisterAnimInstance(this);
		return;
	}
}

void UVEN_AnimInstance::UpdateForceStopStatus(ANIMATION_NOTIFICATION notification)
{
	if (notification == ANIMATION_NOTIFICATION::ITEM_PICK_ANIMATION_UPDATED)
		m_active_action_can_be_force_stopped = true;
}

bool UVEN_AnimInstance::IsFree()
{
	return (m_active_action == ANIMATION_ACTION::IDLE ||
		(m_active_action == ANIMATION_ACTION::PICK_ITEM && m_active_action_can_be_force_stopped));
}

void UVEN_AnimInstance::ResetActiveAction()
{
	m_active_action = ANIMATION_ACTION::IDLE;
}

bool UVEN_AnimInstance::TryToStopActiveAction()
{
	if (m_active_action == ANIMATION_ACTION::IDLE)
	{
		return true;
	}
	else if (m_active_action == ANIMATION_ACTION::PICK_ITEM && m_active_action_can_be_force_stopped)
	{
		PickItem = false;
		ResetActiveAction();
		return true;
	}

	//debug_log("UVEN_AnimInstance::TryToStopActiveAction. Action cannot be force stopped. Ower: " + m_owner->GetName() + ", active action: " + FString::FromInt((int)m_active_action), FColor::Red);
	return false;
}