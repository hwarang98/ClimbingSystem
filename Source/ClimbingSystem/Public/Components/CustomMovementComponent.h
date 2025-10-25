// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomMovementComponent.generated.h"

DECLARE_DELEGATE(FOnEnterClimbState)
DECLARE_DELEGATE(FOnExitClimbState)

class AClimbingSystemCharacter;

UENUM(BlueprintType)
namespace ECustomMovementMode
{
	enum Type
	{
		MOVE_Climb UMETA(DisplayName = "Climb Mode")
	};
}
/**
 * 
 */
UCLASS()
class CLIMBINGSYSTEM_API UCustomMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	void ToggleClimbing(bool bEnableClimb);
	void RequestHopping();
	
	bool IsClimbing() const;

	FVector GetUnrotatedClimbVelocity() const;

	FOnEnterClimbState OnEnterClimbStateDelegate;
	FOnExitClimbState OnExitClimbStateDelegate;
	
	FORCEINLINE FVector GetClimbableSurfaceNormal() const { return CurrentClimbableSurfaceNormal; }

protected:

#pragma region Overriden Functions

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;

#pragma endregion 

private:
#pragma region ClimbTraces

	TArray<FHitResult> DoCapsuleTraceMultiByObject(const FVector& Start, const FVector& End, bool bInShowDebugShape, bool bInDrawPersistantShapes);
	FHitResult DoLineTraceSingleByObject(const FVector& Start, const FVector& End, bool bInShowDebugShape, bool bInDrawPersistantShapes);

#pragma endregion

#pragma region Climb Core

	bool TraceClimbableSurfaces();
	bool CanStartClimbing();
	bool CheckShouldStopClimbing();
	bool CheckHasReachedFloor();
	bool CheckHasReachedLedge();
	bool CanClimbDownLedge();
	bool CanStartVaulting(FVector& OutVaultStartPosition, FVector& OutVaultLandPosition);
	bool CheckCanHopUp(FVector& OutHopUpTargetPosition);
	bool CheckCanHopDown(FVector& OutHopDownTargetPosition);

	void TryStartVaulting();
	void StartClimbing();
	void StopClimbing();
	void PhysClimb(float deltaTime, int32 Iterations);
	void ProcessClimbableSurfaceInfo();
	void SnapMovementToClimbableSurfaces(float DeltaTime);
	void PlayClimbMontage(TObjectPtr<UAnimMontage> MontageToPlay);
	void SetMotionWarpTarget(const FName& InWarpTargetName, const FVector& InTargetPosition);
	void HandleHopUp();
	void HandleHopDown();

	FHitResult TraceFromEyeHeight(float TraceDistance, float TraceStartOffset = 0.f, bool bInShowDebugShape = false, bool bInDrawPersistantShapes = false);
	FQuat GetClimbRotation(float DeltaTime);

	UFUNCTION()
	void OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted);

#pragma endregion

#pragma region Climb Core Variable

	TArray<FHitResult> ClimbableSurfacesTracedResults;
	FVector CurrentClimbableSurfaceLocation;
	FVector CurrentClimbableSurfaceNormal;

	UPROPERTY()
	TObjectPtr<UAnimInstance> OwningPlayerAnimInstance;

	UPROPERTY()
	TObjectPtr<AClimbingSystemCharacter> OwningPlayerCharacter;

#pragma endregion

#pragma region Climb BP Variables

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	TArray<TEnumAsByte<EObjectTypeQuery>> ClimbableSurfaceTraceTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceHalfHeight = 72.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxBreakClimbDeceleration = 400.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxClimbSpeed = 100.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxClimbAcceleration = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbDownWalkableSurfaceTraceOffset = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbDownLedgeTraceOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxClimbableSurfaceAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> IdleToClimbMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ClimbToTopMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Climbing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ClimbDownLedgeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> VaultMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> HopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> HopDownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	int32 VaultTraceSteps = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	int32 VaultLandTraceIndex = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	FName VaultStartPointName = FName("VaultStartPoint");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	FName VaultLandPointName = FName("VaultLandPoint");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	FName HopUpTargetPointName = FName("HopUpTargetPoint");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement | Vaulting", meta = (AllowPrivateAccess = "true"))
	FName HopDownTargetPointName = FName("HopDownTargetPoint");

#pragma endregion
};