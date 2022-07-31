// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_InteractableItem.h"
#include "VEN_GameMode.h"
#include "VEN_PlayerUnit.h"
#include "VEN_Quest.h"
#include "VEN_QuestsManager.h"
#include "VEN_BattleSystem.h"
#include "VEN_FreeFunctions.h"

#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"

namespace
{
	constexpr const float CAPSULE_RADIUS = 30.f;
	constexpr const float INTERACTABLE_RADIUS = 100.f;
	constexpr const int REWARD_XP_FOR_COLLECTING = 15.f;
	const FString QUEST_ID_TAG_PREFIX = "quest_id_";
}
using namespace vendetta;

AVEN_InteractableItem::AVEN_InteractableItem()
	: m_unique_id(-1)
{
	PrimaryActorTick.bCanEverTick = false;

	m_capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractableItemCapsule"));
	m_capsule->SetCapsuleRadius(CAPSULE_RADIUS);
	RootComponent = m_capsule;

	m_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteractableItemMesh"));
	m_mesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TemplateParticleSystem(TEXT("/Game/Particles/PS_Fireflies.PS_Fireflies"));
	m_particle_system = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("InteractableItemParticle"));
	m_particle_system->SetTemplate(TemplateParticleSystem.Object);
	m_particle_system->bAutoActivate = false;
	m_particle_system->SetupAttachment(RootComponent);
	m_particle_system->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	m_is_collectible = false;
}

void AVEN_InteractableItem::BeginPlay()
{
	AActor::BeginPlay();

	SetupUniqueId();
	InitializeRelatedQuestsIds();
	ActivateParticleSystem();
}

UStaticMeshComponent* AVEN_InteractableItem::GetIteractableItemMesh() const
{
	return m_mesh;
}

void AVEN_InteractableItem::SetUniqueActorId(int id)
{
	if (m_unique_id != -1)
	{
		//debug_log("AVEN_InteractableItem::SetUniqueActorId. Attempt to re-set unique id");
		return;
	}

	m_unique_id = id;
}

int AVEN_InteractableItem::GetUniqueActorId() const
{
	return m_unique_id;
}

void AVEN_InteractableItem::OnUpdate(ACTOR_UPDATE update, AActor* update_requestor)
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	const auto& quest_manager = game_mode->GetQuestsManager();
	if (!quest_manager)
		return;

	if (update == ACTOR_UPDATE::INTERACT)
	{
		if (m_related_quests_ids.Num())
		{
			quest_manager->OnUpdate(QUESTS_MANAGER_UPDATE::INTERACT_WITH_ITEM, m_related_quests_ids, this);

			const auto& interaction_owner = Cast<AVEN_PlayerUnit>(update_requestor);
			if (interaction_owner)
				interaction_owner->IncrementXP(REWARD_XP_FOR_COLLECTING);
		}
	}
	if (update == ACTOR_UPDATE::REMOVE)
	{
		GetGameMode()->OnWidgetFlyingDataUpdate(EWidgetFlyingDataType::POSITIVE_HERBS, 1, GetActorLocation());
		Destroy();
	}
}

bool AVEN_InteractableItem::IsCollectible() const
{
	return m_is_collectible;
}

float AVEN_InteractableItem::GetInteractableRadius() const
{
	return INTERACTABLE_RADIUS;
}

bool AVEN_InteractableItem::CanBeInteractedRightNow()
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
		if (quest)
			return true;
	}

	return false;
}

void AVEN_InteractableItem::SetupUniqueId()
{
	const auto& game_mode = GetGameMode();
	if (!game_mode)
		return;

	game_mode->RequestUniqueId(this);
}

void AVEN_InteractableItem::InitializeRelatedQuestsIds()
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

AVEN_GameMode* AVEN_InteractableItem::GetGameMode() const
{
	const auto& game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("AVEN_InteractableItem::GetQuestsManager. Game Mode not found!", FColor::Red);

	return game_mode;
}

void AVEN_InteractableItem::ActivateParticleSystem()
{
	if (m_particle_system)
		m_particle_system->SetActive(true);
	//else
		//debug_log("AVEN_InteractableItem::ActivateParticleSystem. Particles are corrupted", FColor::Red);
}