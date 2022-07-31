// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "VEN_QuestsManager.generated.h"

class UVEN_Quest;
class AVEN_GameMode;
class AVEN_InteractableNPC;

UCLASS()
class GAME_4_24_API UVEN_QuestsManager : public UObject
{
	GENERATED_BODY()

public:

	UVEN_QuestsManager();
	void Initialize();
	void OnUpdate(vendetta::QUESTS_MANAGER_UPDATE qm_update, TArray<vendetta::QUEST_ID> quests_ids, AActor* interaction_owner);
	void RegisterNPC(AVEN_InteractableNPC* npc);
	UVEN_Quest* GetQuestById(vendetta::QUEST_ID quest_id) const;
	TArray<AVEN_InteractableNPC*> GetInteractableNPCsByQuestId(vendetta::QUEST_ID quest_id) const;

private:

	void UpdateQuestProgress(vendetta::QUESTS_MANAGER_UPDATE qm_update, vendetta::QUEST_ID quest_id = {}, AActor* update_requestor = nullptr);
	AVEN_GameMode* GetGameMode() const;
	void AllowInitialQuests(AVEN_InteractableNPC* npc);
	void NotifyNPC(UVEN_Quest* changed_quest);
	void UpdateQuestsList(bool show, UVEN_Quest* quest = nullptr);

private:

	UPROPERTY()
	TArray<UVEN_Quest*> m_quests;
	UPROPERTY()
	TArray<AVEN_InteractableNPC*> m_registered_npcs;
};
