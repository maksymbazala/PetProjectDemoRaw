// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Particles/ParticleSystemComponent.h"

#include "VEN_InteractableItem.generated.h"

class AVEN_GameMode;

UCLASS()
class GAME_4_24_API AVEN_InteractableItem : public AActor
{
	GENERATED_BODY()
	
public:	

	AVEN_InteractableItem();

protected:

	virtual void BeginPlay() override;

public:

	UStaticMeshComponent* GetIteractableItemMesh() const;
	void SetUniqueActorId(int id);
	int GetUniqueActorId() const;
	void OnUpdate(vendetta::ACTOR_UPDATE update, AActor* update_requestor = nullptr);
	bool IsCollectible() const;
	float GetInteractableRadius() const;
	bool CanBeInteractedRightNow();

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UCapsuleComponent* m_capsule;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* m_mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UParticleSystemComponent* m_particle_system;
	UPROPERTY(EditAnywhere)
	bool m_is_collectible;

private:

	void SetupUniqueId();
	void InitializeRelatedQuestsIds();
	AVEN_GameMode* GetGameMode() const;
	void ActivateParticleSystem();

private:

	TArray<vendetta::QUEST_ID> m_related_quests_ids;
	int m_unique_id;

};
