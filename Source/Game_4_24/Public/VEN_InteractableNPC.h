// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "VEN_InteractableNPC.generated.h"

class AVEN_GameMode;
class AVEN_Camera;
class AVEN_PlayerUnit;
class UVEN_QuestsManager;
class UVEN_Quest;
class UVEN_AnimInstance;
class USkeletalMeshComponent;
class UCameraComponent;

UCLASS()
class GAME_4_24_API AVEN_InteractableNPC : public ACharacter
{
	GENERATED_BODY()

public:

	AVEN_InteractableNPC();

protected:

	virtual void BeginPlay() override;

public:

	void Initialize();
	USkeletalMeshComponent* GetIteractableNPCMesh() const;
	void SetUniqueActorId(int id);
	int GetUniqueActorId() const;
	TArray<vendetta::QUEST_ID> GetRelatedQuestIds() const;
	void OnUpdate(vendetta::ACTOR_UPDATE update, AActor* update_requestor);
	void OnRelatedQuestChanged(UVEN_Quest* changed_quest);
	float GetInteractableRadius() const;
	bool CanBeInteractedRightNow();
	void RegisterAnimInstance(UVEN_AnimInstance* instance);
	void PrepareQuestWindow(UVEN_Quest* quest);

	/* ### Blueprint Implementable ### */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Register Main Camera"))
	void OnRegisterMainCamera(UCameraComponent* camera);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Mark Changed"))
	void OnQuestMarkChanged(bool is_allowed, bool can_be_completed);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Window Update"))
	void OnQuestWindowUpdate(FWidgetQuestWindowInfo info);

	/* ### Blueprint Callable ### */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "On Notify From Quest Window"))
	void OnNotifyFromQuestWindow(EWidgetQuestWindowPlayerResponse response);

protected:

	UPROPERTY()
	UVEN_AnimInstance* m_anim_instance;
	UPROPERTY()
	AVEN_PlayerUnit* m_interaction_owner;

private:

	void SetupUniqueId();
	void SetupRelatedQuests();
	void PayPlayerUnitForQuest(int xp_value, int money_value);
	FString GetQuestTextWithPlayerUnitMentions(FString initial_text) const;

	AVEN_GameMode* GetGameMode() const;
	UVEN_Quest* GetActiveQuest() const;

private:

	TArray<vendetta::QUEST_ID> m_related_quests_ids;
	int32_t m_unique_id;

};
