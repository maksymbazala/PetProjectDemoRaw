// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VEN_InputBindings.h"

#include "VEN_Camera.generated.h"

class AVEN_GameMode;
class APlayerController;
class UCameraComponent;
class USpringArmComponent;
class USphereComponent;
class USceneComponent;


UCLASS()
class GAME_4_24_API AVEN_Camera : public APawn
{
	GENERATED_BODY()

public:

	AVEN_Camera();

protected:

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

public:

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Change Camera PostProcess Material"))
	void OnChangeCameraPostProcessMaterial(int material_index);

	void OnMouseAction(vendetta::input_bindings::MOUSE_ACTION mouse_action, float axis = 0.f);
	void OnKeyboardAction(vendetta::input_bindings::KEYBOARD_ACTION keyboard_action, float axis = 0.f);
	bool IsCameraInRotationMode() const;
	bool IsCameraInFollowMode() const;
	void SetCameraFollowMode(bool enable = true, AActor* target_actor = nullptr);
	void RotationPrepare();
	void RotationRelease();

	UCameraComponent* GetMainCameraComponent() const;
	USceneComponent* GetMainCameraRoot() const;

protected:

	UFUNCTION()
	void OnCameraPointBeginOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnCameraPointEndOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	UFUNCTION()
	void OnCameraOpacityBeginOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnCameraOpacityEndOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:

	void MoveForward(float axis);
	void MoveRight(float axis);
	void MoveYaw(float axis);
	void MovePitch(float axis);
	void ScrollUp();
	void ScrollDown();
	void ProcessScrolling(bool zoom_in);
	void ScrollingTimerStep();

	void RotateCamera();
	void CheckMouseOnScreenBorders();
	void CheckCameraLineTrace();
	void UpdateCameraFollowPosition();
	void EnableCustomCollisionTest(bool enable);
	void DoCustomCollisionTest();

	AVEN_GameMode* GetGameMode() const;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Setup")
	UCameraComponent* m_camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Setup")
	USpringArmComponent* m_spring_arm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Setup")
	USphereComponent* m_collision_sphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Setup")
	USphereComponent* m_opacity_sphere;
	UPROPERTY()
	APlayerController* m_controller;

private:

	FVector2D m_mouse_position;

	int32 m_screen_width;
	int32 m_screen_height;
	bool m_camera_rotation_mode;

	float m_spring_arm_length_after_zoom;

	UPROPERTY()
	FTimerHandle m_camera_zooming_timer;

	bool m_camera_jump_over_mode;
	bool m_camera_jump_over_finished;
	FVector m_camera_jump_over_impact_point;

	UPROPERTY()
	UMaterialInterface* m_transparent_material;
	TMap<AActor*, TArray<UMaterialInterface*>> m_faded_actors_materials;

	bool m_zooming_is_enabled;
	bool m_camera_follow_mode;
	bool m_camera_custom_collision_test;
	float m_current_spring_arm_length;

	UPROPERTY()
	AActor* m_camera_follow_target_actor;
};
