// Copyright Epic Games, Inc. All Rights Reserved.


#include "ClimbingSystemPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "ClimbingSystem.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AClimbingSystemPlayerController::EnableDefaultInputState()
{
	// 1. 등반 컨텍스트를 모두 제거합니다.
	for (UInputMappingContext* Context : ClimbMappingContexts)
	{
		RemoveInputMappingContext(Context);
	}

	// 2. 기본 컨텍스트를 모두 추가합니다. (우선순위를 0으로 설정)
	for (UInputMappingContext* Context : DefaultMappingContexts)
	{
		AddInputMappingContext(Context, 0);
	}
}

void AClimbingSystemPlayerController::EnableClimbingInputState()
{
	// 1. 기본 컨텍스트를 모두 제거합니다.
	for (UInputMappingContext* Context : DefaultMappingContexts)
	{
		RemoveInputMappingContext(Context);
	}

	// 2. 등반 컨텍스트를 모두 추가합니다. (우선순위를 1로 설정)
	for (UInputMappingContext* Context : ClimbMappingContexts)
	{
		AddInputMappingContext(Context, 1);
	}
}

void AClimbingSystemPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
	}
}

void AClimbingSystemPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (IsLocalPlayerController())
	{
		// [수정됨] DefaultMappingContexts 배열을 순회하며
		// 헬퍼 함수를 통해 각 컨텍스트를 추가합니다.
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			AddInputMappingContext(CurrentContext, 0);
		}
	}
}

void AClimbingSystemPlayerController::AddInputMappingContext(UInputMappingContext* ContextToAdd, int32 InPriority)
{
	if (!CheckContextToAdd(ContextToAdd))
	{
		return;
	}
	
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(ContextToAdd, InPriority);
		}
	}
}

void AClimbingSystemPlayerController::RemoveInputMappingContext(UInputMappingContext* ContextToAdd)
{
	if (!CheckContextToAdd(ContextToAdd))
	{
		return;
	}
	
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(ContextToAdd);
		}
	}
}

bool AClimbingSystemPlayerController::CheckContextToAdd(UInputMappingContext* ContextToAdd)
{
	if (!ContextToAdd)
	{
		return false;
	}

	return true;
}
