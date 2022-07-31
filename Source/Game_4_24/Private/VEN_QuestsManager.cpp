// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_QuestsManager.h"
#include "VEN_Quest.h"
#include "VEN_InteractableItem.h"
#include "VEN_InteractableNPC.h"
#include "VEN_GameMode.h"
#include "VEN_FreeFunctions.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine.h"

using namespace vendetta;

UVEN_QuestsManager::UVEN_QuestsManager()
{

}

void UVEN_QuestsManager::Initialize()
{
	m_quests.Empty();
	m_registered_npcs.Empty();

	//create quests
	m_quests.Add(NewObject<UVEN_Quest>(this, UVEN_Quest::StaticClass(), FName("quest_0")));
	m_quests.Add(NewObject<UVEN_Quest>(this, UVEN_Quest::StaticClass(), FName("quest_1")));
	m_quests.Add(NewObject<UVEN_Quest>(this, UVEN_Quest::StaticClass(), FName("quest_2")));
	m_quests.Add(NewObject<UVEN_Quest>(this, UVEN_Quest::StaticClass(), FName("quest_3")));
	m_quests.Add(NewObject<UVEN_Quest>(this, UVEN_Quest::StaticClass(), FName("quest_4")));

	//initialize quests
	m_quests[QUEST_ID::WELCOME]->Init(QUEST_ID::WELCOME, false, false, true, "Visit", {},
		{
			"[Fisherman]: Greetings, strangers. What brought you to our land?",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": We are looking for the village headman, we have business with him. Can you tell us where we can find him?",
			"[Fisherman]: I haven't seen him for a long time. They say that he went to the capital, but he may have already returned. Ask around in the village.",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Thank you."
		}, "", 0, 0);
	m_quests[QUEST_ID::HEALING_HERBS]->Init(QUEST_ID::HEALING_HERBS, false, false, false, "Vicious circle",
		{
			"[Old Man]: Sorry to bother you, could you help the desperate old man?",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Actually, we have things to do.",
			"[Old man]: Help me, and I can make it worth your while. You are my only hope!",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": I'm listening.",
			"[Old man]: My old lady is sick and needs a herbal plants from the forest. I would have collected them myself, but there are wolves.. Alas, my last try to collect these herbs was unsuccessful, I could hardly escape from these animals. This year they started hunting too close to the village. And you... You have a weapon. I will be very grateful if you can help me!",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Okay, what do these herbs look like and where can we find them?",
			"[Old man]: There are fields in front of me, there is a bridge behind them, and then you see the forest. You will find herbs there. The herbs are yellow, you will not confuse them with anything."
		},
		{
			"[Old Man]: Thank you! Here, take this, this is all I have. God bless you!",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Where can we find the village headman?",
			"[Old Man]: Sorry, I don't know that. But I think you can ask his daughter. She lives next door."
		}, "Gather herbs", 45, 100, 5);
	m_quests[QUEST_ID::SAVE_SISTER_PART_1]->Init(QUEST_ID::SAVE_SISTER_PART_1, false, true, false, "Pride and Justice",
		{
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Hello. We're looking for.. What happened, why are you crying?",
			"[Headman's Daughter]: My sister.. She turned them down. And these soldiers... They got angry, they took my sister away. They will kill her! Please save her!",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Calm down, who took her and where?",
			"[Headman's Daughter]: They are guards, although they are even worse than robbers. She must have been taken to an old fortress nearby.",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": Okay, we'll try to help."
		}, {}, "Fight the enemy and release sister", 0, 0, 3);
	m_quests[QUEST_ID::SAVE_SISTER_PART_2]->Init(QUEST_ID::SAVE_SISTER_PART_2, false, false, true, "Pride and Justice", {},
		{
			"[Rescued girl]: Thank you so much for saving me. These soldiers have been terrorizing our village for months. Finally you put an end to it. How can I even repay you?",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": We have business to discuss with your father. Just tell us where he is.",
			"[Rescued Girl]: He was just about to return from the capital. I'll show you where to find him. And.. I hope he doesn't find out about this incident..."
		}, "", 0, 300);
	m_quests[QUEST_ID::GOOD_BYE]->Init(QUEST_ID::GOOD_BYE, false, true, false, "Things are coming to a head",
		{
			"[Headman]: Greetings. I heard that you helped our people. Im indebted to you! How can I help?",
			QUEST_PLAYER_UNIT_MENTION_REPLACER + ": An important business brought us to you. Can we discuss it face to face?",
			"[Headman]: Of course, but I'm afraid this is the last quest written by developers. So we can get down to business right now, or you can continue your journey through our region. You can come back at your convenience. I will wait for you here."
		}, {}, "", 10000, 800);
}

void UVEN_QuestsManager::OnUpdate(QUESTS_MANAGER_UPDATE qm_update, TArray<QUEST_ID> quests_ids, AActor* interaction_owner)
{
	for (auto quest_id : quests_ids)
		UpdateQuestProgress(qm_update, quest_id, interaction_owner);
}

void UVEN_QuestsManager::RegisterNPC(AVEN_InteractableNPC* npc)
{
	m_registered_npcs.Add(npc);
	AllowInitialQuests(npc);
}

UVEN_Quest* UVEN_QuestsManager::GetQuestById(QUEST_ID quest_id) const
{
	for (const auto& quest : m_quests)
	{
		if (quest->GetId() == quest_id)
			return quest;
	}

	//debug_log("UVEN_QuestsManager::GetQuestById. Cannot find requested quest : " + FString::FromInt((int)quest_id), FColor::Red);
	return nullptr;
}

TArray<AVEN_InteractableNPC*> UVEN_QuestsManager::GetInteractableNPCsByQuestId(QUEST_ID quest_id) const
{
	TArray<AVEN_InteractableNPC*> result_npcs;

	for (const auto& npc : m_registered_npcs)
	{
		TArray<QUEST_ID> npc_quests = npc->GetRelatedQuestIds();
		if (!npc_quests.Num())
			continue;

		for (auto npc_quest_id : npc_quests)
		{
			if (npc_quest_id == quest_id)
			{
				result_npcs.Add(npc);
				break;
			}
		}
	}

	return result_npcs;
}

void UVEN_QuestsManager::UpdateQuestProgress(QUESTS_MANAGER_UPDATE qm_update, QUEST_ID quest_id, AActor* update_requestor)
{
	if (quest_id >= m_quests.Num())
	{
		//debug_log("UVEN_QuestsManager::UpdateQuestProgress. Quest " + FString::FromInt((int)quest_id) + " has invalid index", FColor::Red);
		return;
	}

	auto quest = m_quests[quest_id];
	auto status = quest->GetStatus();

	if (!quest)
	{
		//debug_log("UVEN_QuestsManager::UpdateQuestProgress. Quest " + FString::FromInt((int)quest_id) + " is corrupted", FColor::Red);
		return;
	}

	switch (quest_id)
	{
		case QUEST_ID::WELCOME:
		{
			if (qm_update == QUESTS_MANAGER_UPDATE::ALLOW_QUEST)
			{
				quest->OnUpdate(QUEST_UPDATE::ALLOW);
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::INTERACT_WITH_NPC)
			{
				if (status == QUEST_STATUS::CAN_BE_COMPLETED)
				{
					quest->OnUpdate(QUEST_UPDATE::COMPLETE);

					//allow second quest
					NotifyNPC(quest);
					UpdateQuestProgress(QUESTS_MANAGER_UPDATE::ALLOW_QUEST, QUEST_ID::HEALING_HERBS);
					return;
				}
			}
		} break;
		case QUEST_ID::HEALING_HERBS:
		{
			if (qm_update == QUESTS_MANAGER_UPDATE::ALLOW_QUEST)
			{
				quest->OnUpdate(QUEST_UPDATE::ALLOW);
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::INTERACT_WITH_NPC)
			{
				if (status == QUEST_STATUS::CAN_BE_COMPLETED)
				{
					quest->OnUpdate(QUEST_UPDATE::COMPLETE);
					UpdateQuestsList(false);

					//remove needed herbs from inventory
					const auto& game_mode = GetGameMode();
					if (game_mode)
					{
						const int herbs_to_collect = quest->GetCustomCounterMaxValue();
						const auto& player_units_inventory = GetGameMode()->GetInventory();
						if (player_units_inventory.Num() >= herbs_to_collect)
						{
							for (int i = 0; i < herbs_to_collect; ++i)
								game_mode->RemoveItemFromInventory(player_units_inventory[i]);
						}
					}

					//allow second quest
					NotifyNPC(quest);
					UpdateQuestProgress(QUESTS_MANAGER_UPDATE::ALLOW_QUEST, QUEST_ID::SAVE_SISTER_PART_1);
					return;
				}
				else if (status == QUEST_STATUS::ALLOWED)
				{
					quest->OnUpdate(QUEST_UPDATE::START_PROGRESS);

					//is aleady finished when taken
					if (quest->GetCustomCounterValue() >= quest->GetCustomCounterMaxValue())
						quest->OnUpdate(QUEST_UPDATE::FINISH_PROGRESS);

					UpdateQuestsList(true, quest);
				}
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::INTERACT_WITH_ITEM)
			{
				if (quest->GetStatus() == QUEST_STATUS::COMPLETED)
					return;

				quest->OnUpdate(QUEST_UPDATE::INCREMENT_COUNTER);

				//is taken and finished
				if (quest->IsProcessed() && quest->GetCustomCounterValue() >= quest->GetCustomCounterMaxValue())
					quest->OnUpdate(QUEST_UPDATE::FINISH_PROGRESS);

				if (quest->IsProcessed())
					UpdateQuestsList(true, quest);
			}
		} break;
		case QUEST_ID::SAVE_SISTER_PART_1:
		{
			if (qm_update == QUESTS_MANAGER_UPDATE::ALLOW_QUEST)
			{
				quest->OnUpdate(QUEST_UPDATE::ALLOW);
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::INTERACT_WITH_NPC)
			{
				if (status == QUEST_STATUS::ALLOWED)
				{
					quest->OnUpdate(QUEST_UPDATE::START_PROGRESS);

					//is aleady finished when taken
					if (quest->GetCustomCounterValue() >= quest->GetCustomCounterMaxValue())
					{
						quest->OnUpdate(QUEST_UPDATE::FINISH_PROGRESS);
						UpdateQuestsList(true, quest);
						NotifyNPC(quest);
						NotifyNPC(m_quests[QUEST_ID::SAVE_SISTER_PART_2]);
						UpdateQuestProgress(QUESTS_MANAGER_UPDATE::ALLOW_QUEST, QUEST_ID::SAVE_SISTER_PART_2);
						return;
					}

					UpdateQuestsList(true, quest);
				}
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::ENEMY_UNIT_KILLED)
			{
				if (quest->GetStatus() == QUEST_STATUS::COMPLETED)
					return;

				quest->OnUpdate(QUEST_UPDATE::INCREMENT_COUNTER);

				//is taken and finished
				if (quest->IsProcessed() && quest->GetCustomCounterValue() >= quest->GetCustomCounterMaxValue())
				{
					quest->OnUpdate(QUEST_UPDATE::FINISH_PROGRESS);
					UpdateQuestsList(true, quest);
					NotifyNPC(quest);
					UpdateQuestProgress(QUESTS_MANAGER_UPDATE::ALLOW_QUEST, QUEST_ID::SAVE_SISTER_PART_2);
					return;
				}

				if (quest->IsProcessed())
					UpdateQuestsList(true, quest);
			}
		} break;
		case QUEST_ID::SAVE_SISTER_PART_2:
		{
			if (qm_update == QUESTS_MANAGER_UPDATE::ALLOW_QUEST)
			{
				quest->OnUpdate(QUEST_UPDATE::ALLOW);
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::INTERACT_WITH_NPC)
			{
				if (status == QUEST_STATUS::CAN_BE_COMPLETED)
				{
					quest->OnUpdate(QUEST_UPDATE::COMPLETE);
					UpdateQuestsList(false);

					//allow second quest
					NotifyNPC(quest);
					UpdateQuestProgress(QUESTS_MANAGER_UPDATE::ALLOW_QUEST, QUEST_ID::GOOD_BYE);
					return;
				}
			}
		} break;
		case QUEST_ID::GOOD_BYE:
		{
			if (qm_update == QUESTS_MANAGER_UPDATE::ALLOW_QUEST)
			{
				quest->OnUpdate(QUEST_UPDATE::ALLOW);
			}
			else if (qm_update == QUESTS_MANAGER_UPDATE::INTERACT_WITH_NPC)
			{
				if (status == QUEST_STATUS::ALLOWED)
				{
					quest->OnUpdate(QUEST_UPDATE::COMPLETE);
					GetGameMode()->OnGameCompleted();
				}
			}
		} break;
		default:
		{
			//debug_log("UVEN_QuestsManager::UpdateQuestProgress. Invalid quest id", FColor::Red);
			return;
		}
	}

	NotifyNPC(quest);
}

AVEN_GameMode* UVEN_QuestsManager::GetGameMode() const
{
	auto game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("UVEN_QuestsManager::GetGameMode. Game Mode not found!", FColor::Red);

	return game_mode;
}

void UVEN_QuestsManager::AllowInitialQuests(AVEN_InteractableNPC* npc)
{
	for (auto quest_id : npc->GetRelatedQuestIds())
	{
		if (quest_id == QUEST_ID::WELCOME)
			UpdateQuestProgress(QUESTS_MANAGER_UPDATE::ALLOW_QUEST, QUEST_ID::WELCOME);
	}
}

void UVEN_QuestsManager::NotifyNPC(UVEN_Quest* changed_quest)
{
	auto npcs = GetInteractableNPCsByQuestId(changed_quest->GetId());
	if (npcs.Num())
	{
		for (const auto& npc : npcs)
		{
			npc->OnRelatedQuestChanged(changed_quest);
		}
	}
}

void UVEN_QuestsManager::UpdateQuestsList(bool show, UVEN_Quest* quest)
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	FQuestsListInfo info;
	info.show = show;

	if (!show || !quest)
	{
		game_mode->OnQuestsListUpdate(info);
		return;
	}

	info.can_be_completed = (quest->GetStatus() == QUEST_STATUS::CAN_BE_COMPLETED || quest->GetStatus() == QUEST_STATUS::COMPLETED);
	info.quest_title = quest->GetTitleText();

	if (quest->GetCustomCounterMaxValue() > 0)
		info.quest_aim = FString::FromInt(quest->GetCustomCounterValue()) + "/" + FString::FromInt(quest->GetCustomCounterMaxValue()) + " ";
	info.quest_aim += quest->GetAimText();

	game_mode->OnQuestsListUpdate(info);
}