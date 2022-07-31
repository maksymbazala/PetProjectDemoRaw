// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_InteractableNPC.h"
#include "VEN_GameMode.h"
#include "VEN_PlayerUnit.h"
#include "VEN_Quest.h"
#include "VEN_AnimInstance.h"
#include "VEN_FreeFunctions.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"

namespace
{
	constexpr const float CAPSULE_RADIUS = 50.f;
	constexpr const float CAPSULE_HALF_HEIGHT = 100.f;
	constexpr const float INTERACTABLE_RADIUS = 300.f;

	const FString QUEST_ID_TAG_PREFIX = "quest_id_";
}

using namespace vendetta;

AVEN_InteractableNPC::AVEN_InteractableNPC()
	: m_anim_instance(nullptr)
	, m_interaction_owner(nullptr)
	, m_unique_id(-1)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AVEN_InteractableNPC::BeginPlay()
{
	Super::BeginPlay();


}

void AVEN_InteractableNPC::Initialize()
{
	if (!GetGameMode())
		return;

	SetupUniqueId();
	SetupRelatedQuests();
	OnQuestMarkChanged(false, false);

	const auto& quest_manager = GetGameMode()->GetQuestsManager();
	if (quest_manager)
		GetGameMode()->GetQuestsManager()->RegisterNPC(this);

	const auto& main_camera = GetGameMode()->GetMainCamera();
	if (main_camera)
		OnRegisterMainCamera(main_camera->GetMainCameraComponent());
}

USkeletalMeshComponent* AVEN_InteractableNPC::GetIteractableNPCMesh() const
{
	return GetMesh();
}

void AVEN_InteractableNPC::SetUniqueActorId(int id)
{
	if (m_unique_id != -1)
	{
		//debug_log("AVEN_InteractableNPC::SetUniqueActorId. Attempt to re-set unique id");
		return;
	}

	m_unique_id = id;
}

int AVEN_InteractableNPC::GetUniqueActorId() const
{
	return m_unique_id;
}

TArray<QUEST_ID> AVEN_InteractableNPC::GetRelatedQuestIds() const
{
	return m_related_quests_ids;
}

void AVEN_InteractableNPC::OnUpdate(ACTOR_UPDATE update, AActor* update_requestor)
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	const auto& quest_manager = game_mode->GetQuestsManager();
	if (!quest_manager)
		return;

	if (update == ACTOR_UPDATE::INTERACT)
	{
		m_interaction_owner = Cast<AVEN_PlayerUnit>(update_requestor);
		const auto& active_quest = GetActiveQuest();
		if (m_interaction_owner && active_quest)
			PrepareQuestWindow(active_quest);
	}
}

void AVEN_InteractableNPC::OnRelatedQuestChanged(UVEN_Quest* changed_quest)
{
	const QUEST_ID id = changed_quest->GetId();
	const QUEST_STATUS status = changed_quest->GetStatus();
	bool is_allowed = status == QUEST_STATUS::ALLOWED;
	bool can_be_completed = status == QUEST_STATUS::CAN_BE_COMPLETED;
	OnQuestMarkChanged(is_allowed, can_be_completed);

	if (id == QUEST_ID::SAVE_SISTER_PART_1 && status == QUEST_STATUS::COMPLETED ||
		id == QUEST_ID::SAVE_SISTER_PART_2 && (status == QUEST_STATUS::UNALLOWED || status == QUEST_STATUS::COMPLETED))
	{
		if (m_anim_instance)
			m_anim_instance->OnUpdateFromOwner(ANIMATION_UPDATE::START_NEXT_ACTION);
	}

	if (status == QUEST_STATUS::COMPLETED)
		PayPlayerUnitForQuest(changed_quest->GetRewardXP(), changed_quest->GetRewardMoney());
}

float AVEN_InteractableNPC::GetInteractableRadius() const
{
	return INTERACTABLE_RADIUS;
}

bool AVEN_InteractableNPC::CanBeInteractedRightNow()
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return false;

	const auto& battle_system = game_mode->GetBattleSystem();
	if (!battle_system || battle_system->IsBattleInProgress())
		return false;

	const auto& quest_manager = game_mode->GetQuestsManager();
	if (!quest_manager)
		return false;

	for (const auto& quest_id : m_related_quests_ids)
	{
		const auto& quest = quest_manager->GetQuestById(quest_id);
		if (quest && (quest->GetStatus() == QUEST_STATUS::ALLOWED || quest->GetStatus() == QUEST_STATUS::CAN_BE_COMPLETED))
			return true;
	}

	return false;
}

void AVEN_InteractableNPC::RegisterAnimInstance(UVEN_AnimInstance* instance)
{
	//if (!instance)
		//debug_log("AVEN_InteractableNPC::RegisterAnimInstance. Instance is broken", FColor::Red);

	m_anim_instance = instance;
}

void AVEN_InteractableNPC::PrepareQuestWindow(UVEN_Quest* quest)
{
	FWidgetQuestWindowInfo info;

	info.title_text = quest->GetTitleText();
	info.reward_xp = quest->GetRewardXP();
	info.reward_money = quest->GetRewardMoney();

	const auto quest_status = quest->GetStatus();
	if (quest_status == QUEST_STATUS::ALLOWED)
	{
		info.description_text = quest->GetStartText();
		info.buttons_type = EWidgetQuestWindowButtonsType::ACCEPT_DECLINE;
	}
	else if (quest_status == QUEST_STATUS::CAN_BE_COMPLETED)
	{
		info.description_text = quest->GetCompletionText();
		info.buttons_type = EWidgetQuestWindowButtonsType::OK;
	}

	for (auto& text_page : info.description_text)
		text_page = GetQuestTextWithPlayerUnitMentions(text_page);

	OnQuestWindowUpdate(info);
}

void AVEN_InteractableNPC::OnNotifyFromQuestWindow(EWidgetQuestWindowPlayerResponse response)
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	const auto& quest_manager = game_mode->GetQuestsManager();
	if (!quest_manager)
		return;

	if (response == EWidgetQuestWindowPlayerResponse::OK || response == EWidgetQuestWindowPlayerResponse::ACCEPT)
		quest_manager->OnUpdate(QUESTS_MANAGER_UPDATE::INTERACT_WITH_NPC, m_related_quests_ids, this);
}

void AVEN_InteractableNPC::SetupUniqueId()
{
	auto game_mode = GetGameMode();
	if (!game_mode)
	{
		//debug_log("AVEN_InteractableNPC::SetupUniqueId. Game Mode not found!", FColor::Red);
		return;
	}

	game_mode->RequestUniqueId(this);
}

void AVEN_InteractableNPC::SetupRelatedQuests()
{
	m_related_quests_ids.Empty();

	for (const auto& tag : Tags)
	{
		FString tag_str = tag.ToString();
		if (tag_str.Contains(QUEST_ID_TAG_PREFIX))
		{
			int32_t quest_id = FCString::Atoi(*(tag_str.Mid(QUEST_ID_TAG_PREFIX.Len())));
			m_related_quests_ids.Add((QUEST_ID)quest_id);
		}
	}
}

void AVEN_InteractableNPC::PayPlayerUnitForQuest(int xp_value, int money_value)
{
	if (!m_interaction_owner)
	{
		//debug_log("AVEN_InteractableNPC::PayPlayerUnitForQuest. Cannot retrieve interaction owner");
		return;
	}

	m_interaction_owner->IncrementXP(xp_value);
	m_interaction_owner->IncrementMoney(money_value);
}

FString AVEN_InteractableNPC::GetQuestTextWithPlayerUnitMentions(FString initial_text) const
{
	if (!m_interaction_owner)
		return "";

	FString updated_text = "";
	const FString player_unit_name = m_interaction_owner->GetUnitName();
	updated_text = initial_text.Replace(*QUEST_PLAYER_UNIT_MENTION_REPLACER, *FString("[" + (m_interaction_owner->GetUnitName()) + "]"));
	return updated_text;
}

AVEN_GameMode* AVEN_InteractableNPC::GetGameMode() const
{
	auto game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("AVEN_InteractableNPC::GetQuestsManager. Game Mode not found!", FColor::Red);

	return game_mode;
}

UVEN_Quest* AVEN_InteractableNPC::GetActiveQuest() const
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return false;

	const auto& quest_manager = game_mode->GetQuestsManager();
	if (!quest_manager)
		return false;

	for (const auto& quest_id : m_related_quests_ids)
	{
		const auto& quest = quest_manager->GetQuestById(quest_id);
		auto quest_status = quest->GetStatus();
		if (quest_status == QUEST_STATUS::ALLOWED || quest_status == QUEST_STATUS::CAN_BE_COMPLETED)
			return quest;
	}
	return nullptr;

}