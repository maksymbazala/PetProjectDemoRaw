// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "Engine/EngineTypes.h"
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VEN_TactialView.generated.h"

class AVEN_PlayerUnit;
class UVEN_BattleSystem;
class USplineComponent;
class UMaterialInstance;

UCLASS()
class GAME_4_24_API UVEN_TactialView : public UObject
{
	GENERATED_BODY()

public:

	UVEN_TactialView();

public:

	void Initialize(AVEN_PlayerUnit* owner, UVEN_BattleSystem* battle_system);
	void OnRequestData(bool is_allowed, AActor* hovered_actor = nullptr, FVector hovered_location = FVector::ZeroVector);

private:

	void GatherEnemyUnitsRelatedInfo();
	void UpdateDecals();
	void NotifyOwner();
	bool IsHoveredUnit();
	bool CanActorBeAttackedFromPoint(AActor* actor, FVector point) const;
	float GetOwnerMovementPriceToAttackPoint(AActor* actor);

	void BuildMovementSpline(FVector destination_point);
	void DestroyMovementSpline();
	FVector GetAttackPointLocation(AActor* actor) const;

private:

	UPROPERTY()
	AVEN_PlayerUnit* m_owner;
	UPROPERTY()
	AActor* m_hovered_actor;
	UPROPERTY()
	UVEN_BattleSystem* m_battle_system;
	UPROPERTY()
	AActor* m_movement_spline_actor;
	UPROPERTY()
	USplineComponent* m_movement_spline_component;

	UPROPERTY()
	AActor* m_main_area_decal_actor;
	UPROPERTY()
	TArray<AActor*> m_minor_area_decal_actors;
	UPROPERTY()
	UMaterialInstance* m_main_area_decal_material_bow;
	UPROPERTY()
	UMaterialInstance* m_main_area_decal_material_sword;
	UPROPERTY()
	UMaterialInstance* m_main_area_decal_material_minor;

	bool m_is_allowed;
	bool m_movement_spline_is_built;

	FVector m_hovered_location;
	TArray<FEnemyUnitInfo> m_enemy_units_info;
};
