// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemCharacter.h"

#include "ClimbingSystemPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/CustomMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "Engine/LocalPlayer.h"

#include "DebugHelper.h"

AClimbingSystemCharacter::AClimbingSystemCharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer.SetDefaultSubobjectClass<UCustomMovementComponent>(CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	CustomMovementComponent = Cast<UCustomMovementComponent>(GetCharacterMovement());
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("Motion Warping Component"));
}

void AClimbingSystemCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::HandleGroundMovementInput);
		EnhancedInputComponent->BindAction(ClimbMoveAction, ETriggerEvent::Triggered, this, &ThisClass::HandleClimbMovementInput);
		

		// Looking
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);

		// Climbing
		EnhancedInputComponent->BindAction(ClimbAction, ETriggerEvent::Started, this, &ThisClass::OnClimbActionStarted);
		EnhancedInputComponent->BindAction(ClimbHopAction, ETriggerEvent::Started, this, &ThisClass::OnClimbHopActionStarted);
	}
}

void AClimbingSystemCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (CustomMovementComponent)
	{
		CustomMovementComponent->OnEnterClimbStateDelegate.BindUObject(this, &ThisClass::OnPlayerEnterClimbState);
		CustomMovementComponent->OnExitClimbStateDelegate.BindUObject(this, &ThisClass::OnPlayerExitClimbState);
	}
}

void AClimbingSystemCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AClimbingSystemCharacter::OnClimbActionStarted(const FInputActionValue& Value)
{
	if (!CustomMovementComponent)
	{
		return;
	}

	if(!CustomMovementComponent->IsClimbing())
	{
		CustomMovementComponent->ToggleClimbing(true);
	}
	else
	{
		CustomMovementComponent->ToggleClimbing(false);
	}
}

void AClimbingSystemCharacter::OnClimbHopActionStarted(const FInputActionValue& Value)
{
	if (CustomMovementComponent)
	{
		CustomMovementComponent->RequestHopping();
	}
}

void AClimbingSystemCharacter::OnPlayerEnterClimbState()
{
	if (AClimbingSystemPlayerController* MyPlayerController = Cast<AClimbingSystemPlayerController>(GetController()))
	{
		// 2. 컨트롤러에게 "등반 입력 상태로 전환해"라고 알림
		MyPlayerController->EnableClimbingInputState();
	}
}

void AClimbingSystemCharacter::OnPlayerExitClimbState()
{
	// 1. 현재 캐릭터를 조종하는 플레이어 컨트롤러를 가져옴
	if (AClimbingSystemPlayerController* MyPlayerController = Cast<AClimbingSystemPlayerController>(GetController()))
	{
		// 2. 컨트롤러에게 "기본 입력 상태로 복원해"라고 알림
		MyPlayerController->EnableDefaultInputState();
	}
}

void AClimbingSystemCharacter::HandleGroundMovementInput(const FInputActionValue & Value)
{
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AClimbingSystemCharacter::HandleClimbMovementInput(const FInputActionValue & Value) 
{
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	const FVector ForwardDirection = FVector::CrossProduct(
		-CustomMovementComponent->GetClimbableSurfaceNormal(),
		GetActorRightVector()
	);

	const FVector RightDirection = FVector::CrossProduct(
		-CustomMovementComponent->GetClimbableSurfaceNormal(),
		-GetActorUpVector()
	);

	// add movement 
	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void AClimbingSystemCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AClimbingSystemCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AClimbingSystemCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
