// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_Quest.h"
#include "VEN_GameMode.h"
#include "VEN_FreeFunctions.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine.h"

using namespace vendetta;

UVEN_Quest::UVEN_Quest()
	: m_id(QUEST_ID::NONE)
	, m_is_allowed(false)
	, m_is_auto_completable(false)
	, m_is_completable_on_init(false)
	, m_is_processed(false)
	, m_title_text("")
	, m_start_text({})
	, m_completion_text({})
	, m_aim_text("")
	, m_reward_money(-1)
	, m_reward_exp(-1)
	, m_custom_counter_max(0)
	, m_custom_counter(0)
	, m_status(QUEST_STATUS::UNALLOWED)
{

}

void UVEN_Quest::Init(QUEST_ID id, bool is_allowed, bool is_auto_completable, bool is_completable_on_init, FString title_text, TArray<FString> start_text, TArray<FString> completion_text, FString aim_text, int reward_money, int reward_exp, int32_t custom_counter_max)
{
	m_id = id;
	m_is_allowed = is_allowed;
	m_is_auto_completable = is_auto_completable;
	m_is_completable_on_init = is_completable_on_init;
	m_title_text = title_text;
	m_start_text = start_text;
	m_completion_text = completion_text;
	m_aim_text = aim_text;
	m_reward_money = reward_money;
	m_reward_exp = reward_exp;
	m_custom_counter_max = custom_counter_max;
}

void UVEN_Quest::OnUpdate(QUEST_UPDATE update)
{
	switch (update)
	{
		case UNALLOW:
		{
			m_status = QUEST_STATUS::UNALLOWED;
		}break;
		case ALLOW: 
		{
			m_status = m_is_completable_on_init ? QUEST_STATUS::CAN_BE_COMPLETED : QUEST_STATUS::ALLOWED;
		}break;
		case START_PROGRESS:
		{
			m_status = QUEST_STATUS::IN_PROGRESS;
			m_is_processed = true;
		}break;
		case INCREMENT_COUNTER:
		{
			m_custom_counter++;
		}break;
		case FINISH_PROGRESS:
		{
			m_status = m_is_auto_completable ? QUEST_STATUS::COMPLETED : QUEST_STATUS::CAN_BE_COMPLETED;
		}break;
		case COMPLETE:
		{
			m_status = QUEST_STATUS::COMPLETED;
		}break;
		default:
		{
			//debug_log("UVEN_Quest::OnUpdate. Invalid update type: " + FString::FromInt((int)update), FColor::Red);
			return;
		}
	}
}

QUEST_ID UVEN_Quest::GetId() const
{
	return m_id;
}

QUEST_STATUS UVEN_Quest::GetStatus() const
{
	return m_status;
}

bool UVEN_Quest::IsProcessed() const
{
	return m_is_processed;
}

int32_t UVEN_Quest::GetCustomCounterValue() const
{
	return m_custom_counter;
}

int32_t UVEN_Quest::GetCustomCounterMaxValue() const
{
	return m_custom_counter_max;
}

bool UVEN_Quest::IsAutoCompletable() const
{
	return m_is_auto_completable;
}

int UVEN_Quest::GetRewardXP() const
{
	return m_reward_exp;
}

int UVEN_Quest::GetRewardMoney() const
{
	return m_reward_money;
}

FString UVEN_Quest::GetTitleText() const
{
	return m_title_text;
}

TArray<FString> UVEN_Quest::GetStartText() const
{
	return m_start_text;
}

TArray<FString> UVEN_Quest::GetCompletionText() const
{
	return m_completion_text;
}

FString UVEN_Quest::GetAimText() const
{
	return m_aim_text;
}