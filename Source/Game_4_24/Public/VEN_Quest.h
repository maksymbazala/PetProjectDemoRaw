// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "VEN_Quest.generated.h"

class UVEN_QuestsManager;

UCLASS()
class GAME_4_24_API UVEN_Quest : public UObject
{
	GENERATED_BODY()
	
public:

	UVEN_Quest();

	void Init(vendetta::QUEST_ID id, bool is_allowed, bool is_auto_completable, bool is_completable_on_init, FString title_text, TArray<FString> start_text, TArray<FString> completion_text, FString aim_text, int reward_money, int reward_exp, int32_t custom_counter_max = -1);
	void OnUpdate(vendetta::QUEST_UPDATE update);

	vendetta::QUEST_ID GetId() const;
	vendetta::QUEST_STATUS GetStatus() const;
	bool IsProcessed() const;
	int32_t GetCustomCounterValue() const;
	int32_t GetCustomCounterMaxValue() const;
	bool IsAutoCompletable() const;
	int GetRewardXP() const;
	int GetRewardMoney() const;
	FString GetTitleText() const;
	TArray<FString> GetStartText() const;
	TArray<FString> GetCompletionText() const;
	FString GetAimText() const;

private:

	vendetta::QUEST_ID m_id;
	bool m_is_allowed;
	bool m_is_auto_completable;
	bool m_is_completable_on_init;
	bool m_is_processed;
	FString m_title_text;
	TArray<FString> m_start_text;
	TArray<FString> m_completion_text;
	FString m_aim_text;
	int m_reward_money;
	int m_reward_exp;
	int32_t m_custom_counter_max;

	int32_t m_custom_counter;
	vendetta::QUEST_STATUS m_status;

};
