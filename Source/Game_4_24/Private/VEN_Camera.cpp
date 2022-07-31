// Fill out your copyright notice in the Description page of Project Settings.


#include "VEN_Camera.h"
#include "VEN_GameMode.h"
#include "VEN_FreeFunctions.h"

#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Volume.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Math/UnrealMathUtility.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

namespace
{
	constexpr const int32 SCREEN_BORDER_ACTIVE_PADDING = 20;

	constexpr const float CAMERA_SPEED_DEFAULT = 1500.f;
	constexpr const float CAMERA_LAG_SPEED = 7.f;
	constexpr const float CAMERA_ROTATION_ANGLE_MAX = 80.f;
	constexpr const float CAMERA_ROTATION_ANGLE_MIN = 30.f;
	constexpr const float CAMERA_SCROLLING_LAG_TIME = 2.f;
	constexpr const float CAMERA_OVERLAP_SPHERE_RADIUS = 500.f;
	constexpr const float SPRINGARM_LENGTH_DEFAULT = 1300.f;
	constexpr const float SPRINGARM_LENGTH_MAX = 1800.f;
	constexpr const float SPRINGARM_LENGTH_MIN = 400.f;
	constexpr const float SPRINGARM_LENGTH_NOT_SET = -1.f;
	constexpr const float SPRINGARM_CHANGE_STEP = 100.f;
	constexpr const float SPRINGARM_ROTATION_SPEED = 2.3f;
	constexpr const float SPRINGARM_ZOOMING_SPEED = 5.f;
	constexpr const float SPRINGARM_ZOOMING_ERROR = .01f;
	constexpr const float TRACE_COMMON_HEIGHT_VALUE = 300.f;

	constexpr const float TRACE_JUMPOVER_HEIGHT_VALUE = 5000.f;

	const FName CAMERA_TRACE_TRIGGER_TAG = "camera_trace";
	const FName CAMERA_JUMPOVER_TRIGGER_TAG = "camera_jump_over";
	const FName CAMERA_OVERLAP_TREE_TAG = "actor_tree";
	const FName CAMERA_OVERLAP_CUSTOM_COLLISION_TEST_TAG = "camera_custom_collision_test";

	const FString CAMERA_JUMPOVER_TRIGGER_NAME = "CameraJumpOverVolume";
}

using namespace vendetta;

AVEN_Camera::AVEN_Camera()
	: m_camera(nullptr)
	, m_spring_arm(nullptr)
	, m_collision_sphere(nullptr)
	, m_opacity_sphere(nullptr)
	, m_zooming_is_enabled(true)
	, m_camera_follow_mode(false)
	, m_camera_custom_collision_test(false)
{
	PrimaryActorTick.bCanEverTick = true;

	m_collision_sphere = CreateDefaultSubobject<USphereComponent>(TEXT("collision_sphere"));
	m_collision_sphere->OnComponentBeginOverlap.AddDynamic(this, &AVEN_Camera::OnCameraPointBeginOverlapCollision);
	m_collision_sphere->OnComponentEndOverlap.AddDynamic(this, &AVEN_Camera::OnCameraPointEndOverlapCollision);

	RootComponent = m_collision_sphere;

	m_spring_arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("springArm"));
	m_spring_arm->SetupAttachment(m_collision_sphere);
	m_spring_arm->TargetArmLength = SPRINGARM_LENGTH_DEFAULT;
	m_spring_arm->bDoCollisionTest = false;
	m_spring_arm->bEnableCameraLag = true;
	m_spring_arm->CameraLagSpeed = CAMERA_LAG_SPEED;

	m_camera = CreateDefaultSubobject<UCameraComponent>(TEXT("camera"));
	m_camera->SetupAttachment(m_spring_arm);

	m_opacity_sphere = CreateDefaultSubobject<USphereComponent>(TEXT("opacity_sphere"));
	m_opacity_sphere->SetRelativeLocation(FVector(200.f, 0.f, 0.f));
	m_opacity_sphere->SetSphereRadius(CAMERA_OVERLAP_SPHERE_RADIUS);
	m_opacity_sphere->OnComponentBeginOverlap.AddDynamic(this, &AVEN_Camera::OnCameraOpacityBeginOverlapCollision);
	m_opacity_sphere->OnComponentEndOverlap.AddDynamic(this, &AVEN_Camera::OnCameraOpacityEndOverlapCollision);
	m_opacity_sphere->SetupAttachment(m_camera);

	static ConstructorHelpers::FObjectFinder<UMaterialInstance> TransparentMaterialAsset(TEXT("MaterialInstance'/Game/Materials/TransparentTrees/MI_TransparentTree.MI_TransparentTree'"));
	if (TransparentMaterialAsset.Succeeded())
	{
		m_transparent_material = TransparentMaterialAsset.Object;
	}
}

void AVEN_Camera::BeginPlay()
{
	Super::BeginPlay();

	m_controller = Cast<APlayerController>(GetController());
	m_controller->GetViewportSize(m_screen_width, m_screen_height);
	m_faded_actors_materials.Empty();

	m_current_spring_arm_length = m_spring_arm->TargetArmLength;
	EnableCustomCollisionTest(true);

	auto game_mode = GetGameMode();
	if (game_mode)
		game_mode->OnActorReady(this);
}

void AVEN_Camera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (m_camera_rotation_mode)
		RotateCamera();
	if (m_camera_follow_mode)
		UpdateCameraFollowPosition();
	if (!m_camera_rotation_mode && !m_camera_follow_mode)
	{
		//CheckMouseOnScreenBorders();
		CheckCameraLineTrace();
	}
}

UCameraComponent* AVEN_Camera::GetMainCameraComponent() const
{
	return m_camera;
}

USceneComponent* AVEN_Camera::GetMainCameraRoot() const
{
	return RootComponent;
}

void AVEN_Camera::OnMouseAction(input_bindings::MOUSE_ACTION mouse_action, float axis)
{
	if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MOVE_X)
		MoveYaw(axis);
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MOVE_Y)
		MovePitch(axis);
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MIDDLE_PRESSED)
		RotationPrepare();
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_MIDDLE_RELEASED)
		RotationRelease();
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_SCROLL_UP)
		ScrollUp();
	else if (mouse_action == input_bindings::MOUSE_ACTION::MOUSE_SCROLL_DOWN)
		ScrollDown();
}

void AVEN_Camera::OnKeyboardAction(input_bindings::KEYBOARD_ACTION keyboard_action, float axis)
{
	if (keyboard_action == input_bindings::KEYBOARD_ACTION::KEYBOARD_FORWARD)
		MoveForward(axis);
	else if (keyboard_action == input_bindings::KEYBOARD_ACTION::KEYBOARD_RIGHT)
		MoveRight(axis);
	else if (keyboard_action == input_bindings::KEYBOARD_ACTION::KEYBOARD_C)
		SetCameraFollowMode(!m_camera_follow_mode);
}

bool AVEN_Camera::IsCameraInRotationMode() const
{
	return m_camera_rotation_mode;
}

bool AVEN_Camera::IsCameraInFollowMode() const
{
	return m_camera_follow_mode;
}

void AVEN_Camera::SetCameraFollowMode(bool enable, AActor* target_actor)
{
	m_camera_follow_mode = enable;
	m_camera_follow_target_actor = target_actor;
}

void AVEN_Camera::RotationPrepare()
{
	m_camera_rotation_mode = true;
}

void AVEN_Camera::RotationRelease()
{
	m_camera_rotation_mode = false;
}

void AVEN_Camera::OnCameraPointBeginOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && (OtherActor->Tags.Find(CAMERA_JUMPOVER_TRIGGER_TAG) != INDEX_NONE || OtherActor->GetName().Contains(CAMERA_JUMPOVER_TRIGGER_NAME)))
	{
		m_camera_jump_over_mode = true;
		m_camera_jump_over_finished = false;
		m_camera_jump_over_impact_point = SweepResult.ImpactPoint;	
	}
}

void AVEN_Camera::OnCameraPointEndOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && (OtherActor->Tags.Find(CAMERA_JUMPOVER_TRIGGER_TAG) != INDEX_NONE || OtherActor->GetName().Contains(CAMERA_JUMPOVER_TRIGGER_NAME)))
	{
		m_camera_jump_over_mode = false;
		m_camera_jump_over_impact_point = FVector::ZeroVector;
	}
}

void AVEN_Camera::OnCameraOpacityBeginOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->Tags.Find(CAMERA_OVERLAP_TREE_TAG) != INDEX_NONE)
	{
		TArray<UMaterialInterface*> actor_component_materials;
		OtherComp->GetUsedMaterials(actor_component_materials);

		for (int32 i = 0; i < OtherComp->GetNumMaterials(); ++i)
		{
			m_faded_actors_materials.Add(OtherActor, actor_component_materials);
			OtherComp->SetMaterial(i, m_transparent_material);
		}
	}
}

void AVEN_Camera::OnCameraOpacityEndOverlapCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor->Tags.Find(CAMERA_OVERLAP_TREE_TAG) != INDEX_NONE)
	{
		const auto& actor_component_materials = (*m_faded_actors_materials.Find(OtherActor));
		if (actor_component_materials.Num() == OtherComp->GetNumMaterials())
		{
			for (int32 i = 0; i < OtherComp->GetNumMaterials(); ++i)
				OtherComp->SetMaterial(i, actor_component_materials[i]);

			m_faded_actors_materials.Remove(OtherActor);
		}
	}
}

void AVEN_Camera::MoveForward(float axis)
{
	float x_to_y_ratio = m_spring_arm->GetForwardVector().X / (FMath::Abs(m_spring_arm->GetForwardVector().X) + FMath::Abs(m_spring_arm->GetForwardVector().Y));
	float y_to_x_ratio = m_spring_arm->GetForwardVector().Y / (FMath::Abs(m_spring_arm->GetForwardVector().X) + FMath::Abs(m_spring_arm->GetForwardVector().Y));

	FVector camera_offset = FVector::ZeroVector;
	camera_offset.X = axis * x_to_y_ratio * CAMERA_SPEED_DEFAULT * GetWorld()->GetDeltaSeconds();
	camera_offset.Y = axis * y_to_x_ratio * CAMERA_SPEED_DEFAULT * GetWorld()->GetDeltaSeconds();

	RootComponent->MoveComponent(camera_offset, FRotator::ZeroRotator, true);

	if (axis != 0.f)
	{
		SetCameraFollowMode(false);
		EnableCustomCollisionTest(false);
	}
}

void AVEN_Camera::MoveRight(float axis)
{
	float x_to_y_ratio = m_spring_arm->GetRightVector().X / (FMath::Abs(m_spring_arm->GetRightVector().X) + FMath::Abs(m_spring_arm->GetRightVector().Y));
	float y_to_x_ratio = m_spring_arm->GetRightVector().Y / (FMath::Abs(m_spring_arm->GetRightVector().X) + FMath::Abs(m_spring_arm->GetRightVector().Y));

	FVector camera_offset = FVector::ZeroVector;
	camera_offset.X = axis * x_to_y_ratio * CAMERA_SPEED_DEFAULT * GetWorld()->GetDeltaSeconds();
	camera_offset.Y = axis * y_to_x_ratio * CAMERA_SPEED_DEFAULT * GetWorld()->GetDeltaSeconds();

	RootComponent->MoveComponent(camera_offset, FRotator::ZeroRotator, true);

	if (axis != 0.f)
	{
		SetCameraFollowMode(false);
		EnableCustomCollisionTest(false);
	}
}

void AVEN_Camera::MoveYaw(float axis)
{
	m_mouse_position.X = axis;
}

void AVEN_Camera::MovePitch(float axis)
{
	m_mouse_position.Y = axis;
}

void AVEN_Camera::ScrollUp()
{
	ProcessScrolling(true);
}

void AVEN_Camera::ScrollDown()
{
	ProcessScrolling(false);
}

void AVEN_Camera::ProcessScrolling(bool zoom_in)
{
	if (!m_zooming_is_enabled)
		return;

	float zooming_value = SPRINGARM_CHANGE_STEP;
	if (zoom_in)
		zooming_value *= -1;

	if (GetWorldTimerManager().IsTimerActive(m_camera_zooming_timer))
		zooming_value += m_spring_arm_length_after_zoom - m_spring_arm->TargetArmLength;

	if ((m_spring_arm->TargetArmLength + zooming_value) <= SPRINGARM_LENGTH_MAX && (m_spring_arm->TargetArmLength + zooming_value) >= SPRINGARM_LENGTH_MIN)
	{
		m_spring_arm_length_after_zoom = m_spring_arm->TargetArmLength + zooming_value;
		GetWorldTimerManager().ClearTimer(m_camera_zooming_timer);
		GetWorldTimerManager().SetTimer(m_camera_zooming_timer, this, &AVEN_Camera::ScrollingTimerStep, GetWorld()->GetDeltaSeconds(), true);
	}

	m_current_spring_arm_length = m_spring_arm->TargetArmLength;
}

void AVEN_Camera::ScrollingTimerStep()
{
	m_spring_arm->TargetArmLength = FMath::FInterpTo(m_spring_arm->TargetArmLength, m_spring_arm_length_after_zoom, GetWorld()->GetDeltaSeconds(), SPRINGARM_ZOOMING_SPEED);
	if (m_spring_arm->TargetArmLength < (m_spring_arm_length_after_zoom * (1 + SPRINGARM_ZOOMING_ERROR)) && 
		m_spring_arm->TargetArmLength > (m_spring_arm_length_after_zoom * (1 - SPRINGARM_ZOOMING_ERROR)))
	{
		GetWorldTimerManager().ClearTimer(m_camera_zooming_timer);
	}
}

void AVEN_Camera::RotateCamera()
{
	float calculated_yaw = m_mouse_position.X * SPRINGARM_ROTATION_SPEED;
	float calculated_pitch = m_mouse_position.Y * SPRINGARM_ROTATION_SPEED;

	FRotator newCameraRotation = m_spring_arm->GetComponentRotation();
	newCameraRotation.Yaw += calculated_yaw;

	if ((newCameraRotation.Pitch + calculated_pitch) > -CAMERA_ROTATION_ANGLE_MAX && (newCameraRotation.Pitch + calculated_pitch) < -CAMERA_ROTATION_ANGLE_MIN)
		newCameraRotation.Pitch += calculated_pitch;

	m_spring_arm->SetWorldRotation(newCameraRotation);
}

void AVEN_Camera::CheckMouseOnScreenBorders()
{
	auto game_mode = GetGameMode();
	if (!game_mode)
		return;

	auto main_controller = game_mode->GetMainController();
	if (!main_controller)
		return;

	if (main_controller->IsLocked())
		return;

	float mouse_x;
	float mouse_y;

	m_controller->GetMousePosition(mouse_x, mouse_y);

	if (mouse_x < static_cast<float>(SCREEN_BORDER_ACTIVE_PADDING))
		MoveRight(-1.0);
	else if (mouse_x > static_cast<float>(m_screen_width - SCREEN_BORDER_ACTIVE_PADDING))
		MoveRight(1.0);
	if (mouse_y < static_cast<float>(SCREEN_BORDER_ACTIVE_PADDING))
		MoveForward(1.0);
	else if (mouse_y > static_cast<float>(m_screen_height - SCREEN_BORDER_ACTIVE_PADDING))
		MoveForward(-1.0);
}

void AVEN_Camera::CheckCameraLineTrace()
{
	FVector camera_offset = FVector::ZeroVector;

	if (m_camera_jump_over_mode)
	{
		if (!m_camera_jump_over_finished)
		{
			FHitResult trace_jumpover_hit_result;
			FVector trace_jumpover_start_point = m_camera_jump_over_impact_point + FVector(0.f, 0.f, TRACE_JUMPOVER_HEIGHT_VALUE);
			FVector trace_jumpover_end_point = FVector(trace_jumpover_start_point.X, trace_jumpover_start_point.Y, trace_jumpover_start_point.Z - TRACE_JUMPOVER_HEIGHT_VALUE);
			FCollisionQueryParams trace_jumpover_collision_params;

			if (GetWorld()->LineTraceSingleByChannel(trace_jumpover_hit_result, trace_jumpover_start_point, trace_jumpover_end_point, ECC_Visibility, trace_jumpover_collision_params))
			{
				if (trace_jumpover_hit_result.bBlockingHit && trace_jumpover_hit_result.GetActor())
				{
					camera_offset.Z = TRACE_JUMPOVER_HEIGHT_VALUE - (trace_jumpover_start_point.Z - trace_jumpover_hit_result.ImpactPoint.Z);
					m_camera_jump_over_finished = true;
				}
			}
		}
	}
	else
	{
		FHitResult trace_common_hit_result;
		FVector trace_common_start_point = RootComponent->GetComponentLocation();
		FVector trace_common_end_point = FVector(trace_common_start_point.X, trace_common_start_point.Y, trace_common_start_point.Z - SPRINGARM_LENGTH_MAX * 2);
		FCollisionQueryParams trace_common_collision_params;

		/* an old line trace from camera comp as trace start point
		float springarm_rotation_angle_rad = FMath::DegreesToRadians(FMath::Abs(m_spring_arm->GetComponentRotation().Pitch));
		float proper_camera_height = FMath::Sin(springarm_rotation_angle_rad) * m_spring_arm->TargetArmLength;
		*/

		if (GetWorld()->LineTraceSingleByChannel(trace_common_hit_result, trace_common_start_point, trace_common_end_point, ECC_Visibility, trace_common_collision_params))
		{
			if (trace_common_hit_result.bBlockingHit && trace_common_hit_result.GetActor() && trace_common_hit_result.GetActor()->Tags.Find(CAMERA_TRACE_TRIGGER_TAG) != INDEX_NONE)
			{
				camera_offset.Z = TRACE_COMMON_HEIGHT_VALUE - (trace_common_start_point.Z - trace_common_hit_result.ImpactPoint.Z);
			}
		}
	}

	RootComponent->MoveComponent(camera_offset, FRotator::ZeroRotator, true);
}

void AVEN_Camera::UpdateCameraFollowPosition()
{
	auto game_mode = GetGameMode();
	if (!game_mode)
		return;

	FVector new_socket_location = FVector::ZeroVector;
	if (m_camera_follow_target_actor)
	{
		new_socket_location = FVector(m_camera_follow_target_actor->GetTargetLocation().X, m_camera_follow_target_actor->GetTargetLocation().Y, (m_camera_follow_target_actor->GetTargetLocation().Z + 100.f));
	}
	else
	{
		for (const auto& player_unit : game_mode->GetPlayerUnits())
		{
			if (player_unit && player_unit->GetActive())
			{
				new_socket_location = FVector(player_unit->GetTargetLocation().X, player_unit->GetTargetLocation().Y, (player_unit->GetTargetLocation().Z + 100.f));
				m_camera_follow_target_actor = player_unit;
				break;
			}
		}
	}

	if (m_camera_follow_target_actor)
	{
		RootComponent->SetWorldLocation(new_socket_location);
		EnableCustomCollisionTest(true);
		DoCustomCollisionTest();
	}
}

void AVEN_Camera::EnableCustomCollisionTest(bool enable)
{
	m_camera_custom_collision_test = enable;
	m_spring_arm->bDoCollisionTest = !enable;
}

void AVEN_Camera::DoCustomCollisionTest()
{
	FHitResult trace_custom_collision_test_hit_result;
	FVector trace_custom_collision_test_start_point = RootComponent->GetComponentLocation();
	FVector trace_custom_collision_test_end_point = m_spring_arm->GetComponentLocation() - (m_spring_arm->GetComponentRotation().Vector() * m_current_spring_arm_length);;
	FCollisionQueryParams trace_custom_collision_test_params;

	if (GetWorld()->LineTraceSingleByChannel(trace_custom_collision_test_hit_result, trace_custom_collision_test_start_point, trace_custom_collision_test_end_point, ECC_Visibility, trace_custom_collision_test_params))
	{
		float distance_to_impact = 0.f;

		if (trace_custom_collision_test_hit_result.GetActor() && trace_custom_collision_test_hit_result.GetActor()->Tags.Find(CAMERA_OVERLAP_CUSTOM_COLLISION_TEST_TAG) != INDEX_NONE)
			distance_to_impact = (RootComponent->GetComponentLocation() - trace_custom_collision_test_hit_result.ImpactPoint).Size();

		if (distance_to_impact != 0.f && distance_to_impact <= m_current_spring_arm_length)
		{
			m_spring_arm->TargetArmLength = distance_to_impact;
			m_zooming_is_enabled = false;
		}
		else
		{
			m_zooming_is_enabled = true;
		}
	}
	else
	{
		m_zooming_is_enabled = true;
	}
}

AVEN_GameMode* AVEN_Camera::GetGameMode() const
{
	const auto game_mode = Cast<AVEN_GameMode>(GetWorld()->GetAuthGameMode());
	//if (!game_mode)
		//debug_log("AVEN_Camera::GetGameMode. Game Mode not found!", FColor::Red);

	return game_mode;
}