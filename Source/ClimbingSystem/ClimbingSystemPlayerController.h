// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ClimbingSystemPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AClimbingSystemPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void EnableDefaultInputState();
	void EnableClimbingInputState();
	
protected:
	
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> ClimbMappingContexts;
	
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;
	
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;
	
	TObjectPtr<UUserWidget> MobileControlsWidget;
	
	virtual void BeginPlay() override;
	
	virtual void SetupInputComponent() override;

private:
	void AddInputMappingContext(UInputMappingContext* ContextToAdd, int32 InPriority);
	void RemoveInputMappingContext(UInputMappingContext* ContextToAdd);

	bool CheckContextToAdd(UInputMappingContext* ContextToAdd);
};
