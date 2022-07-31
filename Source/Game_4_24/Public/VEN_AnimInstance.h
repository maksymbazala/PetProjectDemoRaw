// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VEN_Types.h"

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "VEN_AnimInstance.generated.h"

class AActor;

UCLASS()
class GAME_4_24_API UVEN_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:

	UVEN_AnimInstance();

public:

	virtual void NativeBeginPlay() override;

	//methods
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "NotifyFromBlueprint"))
	void OnUpdateFromBlueprint(int command_id);
	void OnUpdateFromOwner(vendetta::ANIMATION_UPDATE update);
	void OnActionFromOwner(vendetta::ANIMATION_ACTION action);
	vendetta::ANIMATION_ACTION GetActiveAnimationAction() const;
	bool IsFree();

public:

	//varibles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance General")
	int AnimationIndex;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance General")
	int ActionIndex;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Unit Related")
	float Speed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Unit Related")
	bool InBattle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool PickItem;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool Attack_1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool Attack_2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool StunStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool StunFinish;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool ReceiveDamageFront;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool ReceiveDamageBack;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool ReceiveDamageLeft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool ReceiveDamageRight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VEN_AnimInstance Battle Related")
	bool Die;

private:

	void OwningUnitRegistration();
	void UpdateForceStopStatus(vendetta::ANIMATION_NOTIFICATION notification);
	void ResetActiveAction();
	bool TryToStopActiveAction();

private:

	UPROPERTY()
	AActor* m_owner;

	vendetta::ANIMATION_ACTION m_active_action;
	bool m_active_action_can_be_force_stopped;

};
