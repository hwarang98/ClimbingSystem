// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/CustomMovementComponent.h"

#include "ClimbingSystemCharacter.h"
#include "DebugHelper.h"
#include "MotionWarpingComponent.h"
#include "AI/NavigationSystemBase.h"
#include "Chaos/Utilities.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void UCustomMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningPlayerAnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();

	if (OwningPlayerAnimInstance)
	{
		OwningPlayerAnimInstance->OnMontageEnded.AddDynamic(this, &ThisClass::OnClimbMontageEnded);
		OwningPlayerAnimInstance->OnMontageBlendingOut.AddDynamic(this, &ThisClass::OnClimbMontageEnded);
	}
	
	OwningPlayerCharacter = Cast<AClimbingSystemCharacter>(CharacterOwner);
}

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);

		OnEnterClimbStateDelegate.ExecuteIfBound();
	}

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
		const FRotator CleanStandRotation = FRotator(0.f, DirtyRotation.Yaw, 0.f);

		UpdatedComponent->SetRelativeRotation(CleanStandRotation);
		
		StopMovementImmediately();
		OnExitClimbStateDelegate.ExecuteIfBound();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UCustomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimb(deltaTime, Iterations);
	}
	Super::PhysCustom(deltaTime, Iterations);
}

float UCustomMovementComponent::GetMaxSpeed() const
{
	if (IsClimbing())
	{
		return MaxClimbSpeed;
	}
	
	return Super::GetMaxSpeed();
}

float UCustomMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return MaxClimbAcceleration;
	}
	
	return Super::GetMaxAcceleration();
}

FVector UCustomMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	if (IsFalling() && OwningPlayerAnimInstance && OwningPlayerAnimInstance->IsAnyMontagePlaying())
	{
		return RootMotionVelocity;
	}
	
	return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
}

void UCustomMovementComponent::ToggleClimbing(bool bEnableClimb)
{
	if(bEnableClimb)
	{
		if(CanStartClimbing())
		{
			//Enter the climb state
			PlayClimbMontage(IdleToClimbMontage);
		}
		else if (CanClimbDownLedge())
		{
			PlayClimbMontage(ClimbDownLedgeMontage);
		}
		else
		{
			TryStartVaulting();
		}
	}
	if (!bEnableClimb)
	{
		//Stop climbing
		StopClimbing();
	}
}

void UCustomMovementComponent::HandleHopUp()
{
	FVector HopUpTargetPoint;
	if (CheckCanHopUp(HopUpTargetPoint))
	{
		SetMotionWarpTarget(HopUpTargetPointName, HopUpTargetPoint);
		PlayClimbMontage(HopUpMontage);
	}
}

bool UCustomMovementComponent::CheckCanHopUp(FVector& OutHopUpTargetPosition)
{
	const FHitResult HopUpHit = TraceFromEyeHeight(100.f, -20.f);
	const FHitResult SafetyLedgeHit = TraceFromEyeHeight(100.f, 150.f);

	if (HopUpHit.bBlockingHit && SafetyLedgeHit.bBlockingHit)
	{
		OutHopUpTargetPosition = HopUpHit.ImpactPoint;
		return true;
	}
	return false;
}

void UCustomMovementComponent::HandleHopDown()
{
	FVector HopDownTargetPoint;
	if (CheckCanHopDown(HopDownTargetPoint))
	{
		SetMotionWarpTarget(HopDownTargetPointName, HopDownTargetPoint);
		PlayClimbMontage(HopDownMontage);
	}
}

bool UCustomMovementComponent::CheckCanHopDown(FVector& OutHopDownTargetPosition)
{
	const FHitResult HopDownHit = TraceFromEyeHeight(100.f, -300.f);

	if (HopDownHit.bBlockingHit)
	{
		OutHopDownTargetPosition = HopDownHit.ImpactPoint;
		return true;
	}
	
	return false;
}

void UCustomMovementComponent::RequestHopping()
{
	const FVector UnrotatedLastInputVector = UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), GetLastInputVector());

	const float DotResult = FVector::DotProduct(UnrotatedLastInputVector.GetSafeNormal(), FVector::UpVector);
	

	if (DotResult >= 0.9f)
	{
		HandleHopUp();
	}
	else if (DotResult <= -0.9f)
	{
		HandleHopDown();
	}
}

bool UCustomMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::MOVE_Climb;
}

FVector UCustomMovementComponent::GetUnrotatedClimbVelocity() const
{
	return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), Velocity);
}

bool UCustomMovementComponent::CanStartClimbing()
{
	if (IsFalling() || !TraceClimbableSurfaces() || !TraceFromEyeHeight(100.f).bBlockingHit)
	{
		return false;
	}

	return true;
}

bool UCustomMovementComponent::CheckShouldStopClimbing()
{
	if (ClimbableSurfacesTracedResults.IsEmpty())
	{
		return true;
	}

	const float DotResult = FVector::DotProduct(CurrentClimbableSurfaceNormal, FVector::UpVector);
	const float DegreeDifferent = FMath::RadiansToDegrees(FMath::Acos(DotResult));

	if (DegreeDifferent <= MaxClimbableSurfaceAngle)
	{
		return true;
	}

	return false;
}

bool UCustomMovementComponent::CheckHasReachedFloor()
{
	// 캐릭터의 '아래 방향' 벡터를 계산 (UpVector의 반대)
	// 캐릭터가 회전해 있을 수 있으므로, 단순히 FVector::DownVector를 쓰는 대신 컴포넌트의 실제 Up 방향을 기준으로 계산함.
	const FVector DownVector = -UpdatedComponent->GetUpVector();

	// 트레이스 시작 위치를 약간 아래로 오프셋 (50cm 정도)
	// 캐릭터가 벽을 타고 내려올 때 바닥 근처를 탐색하기 위함.
	const FVector StartOffset = DownVector * 50.f;

	// 캡슐 트레이스 시작/끝 위치 계산
	// Start: 현재 위치에서 약간 아래로 이동한 지점
	// End: Start보다 조금 더 아래 (DownVector 방향으로)
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + DownVector;

	// 캡슐 모양으로 아래 방향에 충돌체를 쏴서 바닥을 탐지
	// 여러 개의 충돌 결과를 받을 수 있음.
	TArray<FHitResult> PossibleFloorHits = DoCapsuleTraceMultiByObject(Start, End, false, false);

	// 아무 것도 감지되지 않았다면 바닥에 닿지 않은 상태이므로 false 반환
	if (PossibleFloorHits.IsEmpty())
	{
		return false;
	}

	// 감지된 충돌 결과들(PossibleFloorHits)을 하나씩 검사
	for (const FHitResult& PossibleResult : PossibleFloorHits)
	{
		// 바닥 조건 판정:
		// 1. 충돌한 면의 노멀(ImpactNormal)이 위쪽 방향(FVector::UpVector)과 거의 평행해야 함
		//    - 즉, 수평면(바닥)에 가까운 면이어야 함.
		// 2. 현재 속도의 Z값이 -10.f보다 작아야 함
		//    - 캐릭터가 실제로 아래쪽으로 이동 중일 때만 바닥 도착으로 간주.
		const bool bFloorReached = FVector::Parallel(-PossibleResult.ImpactNormal, FVector::UpVector) && GetUnrotatedClimbVelocity().Z < -10.f;
			// GetUnrotatedClimbVelocity().Z < -10.f;

		// 위 두 조건을 만족하면 바닥에 도달했다고 판단하고 true 반환
		if (bFloorReached)
		{
			return true;
		}
	}

	// 모든 충돌 결과를 확인했지만 바닥 조건에 부합하는 면이 없다면 false 반환
	return false;
}

bool UCustomMovementComponent::CheckHasReachedLedge()
{
	// FHitResult LedgeHitResult = TraceFromEyeHeight(100.f, 50.f);
	FHitResult LedgeHitResult = TraceFromEyeHeight(50.f, 0.f);

	if (!LedgeHitResult.bBlockingHit)
	{
		const FVector WalkableSurfaceTraceStart = LedgeHitResult.TraceEnd;
		const FVector DownVector = -UpdatedComponent->GetUpVector();
		const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;

		FHitResult WalkableSurfaceHitResult = DoLineTraceSingleByObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd, false, false);

		if (WalkableSurfaceHitResult.bBlockingHit && GetUnrotatedClimbVelocity().Z > 10.f)
		{
			return true;
		}
	}

	return false;
}

bool UCustomMovementComponent::CanClimbDownLedge()
{
	if (IsFalling())
	{
		return false;
	}

	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector DownVector = -UpdatedComponent->GetUpVector();

	const FVector WalkableSurfaceTraceStart = ComponentLocation + ComponentForward * ClimbDownWalkableSurfaceTraceOffset;
	const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;

	const FHitResult WalkableSurfaceHit = DoLineTraceSingleByObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd, false, false);

	const FVector LedgeTraceStart = WalkableSurfaceHit.TraceStart + ComponentForward * ClimbDownLedgeTraceOffset;
	const FVector LedgeTraceEnd = LedgeTraceStart + DownVector * 200.f;
	// const FVector LedgeTraceEnd = LedgeTraceStart + DownVector * 300.f;

	const FHitResult LedgeTraceHit = DoLineTraceSingleByObject(LedgeTraceStart, LedgeTraceEnd, false, false);

	if (WalkableSurfaceHit.bBlockingHit && !LedgeTraceHit.bBlockingHit)
	{
		return true;
	}

	return false;
}

/**
 * @brief 캐릭터가 볼팅을 시작할 수 있는지 확인하고 시작 및 착지 위치를 계산
 *
 * 이 함수는 캐릭터의 현재 상태와 주변 환경을 기반으로 볼팅 가능 여부를 판단하며,
 * 볼팅 시작 위치 및 끝 위치를 계산합니다.
 * 만약 캐릭터가 공중에 있으면 볼팅은 불가능하며, 이 경우 `false`를 반환합니다.
 *
 * 계산 과정은 다음과 같습니다:
 * - 캐릭터의 현재 위치와 방향을 기준으로 상단으로 올린 시작 위치 및 하단으로 연장된 끝 위치를 트레이스
 * - 각 스텝의 트레이스를 통해 시작 위치(`OutVaultStartPosition`)와 착지 위치(`OutVaultLandPosition`)를 감지
 *
 * 만약 시작 및 착지 위치 모두 유효하면 볼팅 가능 여부를 `true`로 반환합니다.
 *
 * @param OutVaultStartPosition 계산된 볼팅 시작 위치를 저장하는 출력 인자
 * @param OutVaultLandPosition 계산된 볼팅 착지 위치를 저장하는 출력 인자
 * @return 볼팅을 시작할 수 있으면 `true`, 그렇지 않으면 `false`
 */
bool UCustomMovementComponent::CanStartVaulting(FVector& OutVaultStartPosition, FVector& OutVaultLandPosition)
{
	if (IsFalling())
	{
		return false;
	}

	OutVaultStartPosition = FVector::ZeroVector;
	OutVaultLandPosition = FVector::ZeroVector;
	
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector UpVector = UpdatedComponent->GetUpVector();
	const FVector DownVector = -UpdatedComponent->GetUpVector();

	for (int32 i = 0; i < VaultTraceSteps; ++i)
	{
		const FVector StartTrace = ComponentLocation + UpVector * 100.f + ComponentForward * 80.f * (i + 1);
		const FVector EndTrace = StartTrace + DownVector * 100.f * (i + 1);

		const FHitResult VaultTraceHit = DoLineTraceSingleByObject(StartTrace, EndTrace, false, false);

		const FVector VaultingTraceHitImpactPoint = VaultTraceHit.ImpactPoint;

		// 첫번째 볼팅 위치 선정
		if (i == 0 && VaultTraceHit.bBlockingHit)
		{
			OutVaultStartPosition = VaultingTraceHitImpactPoint;
		}

		if (i == VaultLandTraceIndex && VaultTraceHit.bBlockingHit)
		{
			OutVaultLandPosition = VaultingTraceHitImpactPoint;
		}
	}

	if (OutVaultStartPosition != FVector::ZeroVector && OutVaultLandPosition != FVector::ZeroVector)
	{
		return true;
	}
	else
	{
		return false;
	}
	
}

void UCustomMovementComponent::TryStartVaulting()
{
	FVector VaultStartPosition;
	FVector VaultLandPosition;
	
	if (CanStartVaulting(VaultStartPosition, VaultLandPosition))
	{
		SetMotionWarpTarget(VaultStartPointName, VaultStartPosition);
		SetMotionWarpTarget(VaultLandPointName, VaultLandPosition);
		
		StartClimbing();
		PlayClimbMontage(VaultMontage);
	}
}

void UCustomMovementComponent::StartClimbing()
{
	SetMovementMode(MOVE_Custom, ECustomMovementMode::MOVE_Climb);
}

void UCustomMovementComponent::StopClimbing()
{
	SetMovementMode(MOVE_Falling);
}

void UCustomMovementComponent::PhysClimb(float deltaTime, int32 Iterations)
{
	// 너무 짧은 델타 타임(즉, 거의 시간이 흐르지 않은 프레임)에서는 물리 계산을 생략
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	TraceClimbableSurfaces();
	ProcessClimbableSurfaceInfo();

	if (CheckShouldStopClimbing() || CheckHasReachedFloor())
	{
		StopClimbing();
	}

	// 루트 모션(root motion) 적용 전의 속도를 복원
	// (애니메이션 루트 모션으로 인해 덮어쓰기된 속도를 복구)
	RestorePreAdditiveRootMotionVelocity();

	// 현재 루트 모션이 없고, 루트 모션이 속도를 덮어쓰지도 않는다면
	// 일반적인 물리 기반 속도 계산 수행
	if(!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		// 등반 시 감속 값을 적용하여 속도 계산 (마찰력, 중력 없음)
		CalcVelocity(deltaTime, 0.f, true, MaxBreakClimbDeceleration);
	}

	// 루트 모션이 존재한다면, 그에 따라 속도 값을 업데이트
	ApplyRootMotionToVelocity(deltaTime);

	// 이동 전 위치 저장 (이동 후 속도 계산에 사용)
	FVector OldLocation = UpdatedComponent->GetComponentLocation();

	// 이동할 거리 = 속도 × 시간
	const FVector Adjusted = Velocity * deltaTime;

	// 충돌 결과 정보를 저장할 Hit 구조체
	FHitResult Hit(1.f);

	// 실제 이동 시도. 충돌이 있으면 Hit에 정보가 담김.
	SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);

	// 이동 도중 충돌이 발생한 경우 (Hit.Time < 1.f)
	if (Hit.Time < 1.f)
	{
		// 충돌에 대한 반응 처리 (속도 조정, 이벤트 발생 등)
		HandleImpact(Hit, deltaTime, Adjusted);

		// 충돌 표면을 따라 미끄러지듯 이동 (경사면이나 벽 등)
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	// 루트 모션이 적용되지 않은 경우, 실제 이동한 거리로부터 새로운 속도를 재계산
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	SnapMovementToClimbableSurfaces(deltaTime);

	if (CheckHasReachedLedge())
	{
		StopClimbing();
		PlayClimbMontage(ClimbToTopMontage);
	}
}

/**
 * @brief 등반 가능한 표면 정보를 처리하여 현재 등반 표면의 평균 위치와 법선을 계산
 *
 * 이 함수는 이전 프레임에서 수행된 표면 트레이스 결과(`ClimbableSurfacesTracedResults`)를 기반으로,
 * 캐릭터가 현재 등반 중인 표면의 중심 위치(`CurrentClimbableSurfaceLocation`)와 방향(`CurrentClimbableSurfaceNormal`)을 계산합니다.
 * 
 * 계산 방식은 다음과 같습니다:
 * - 각 트레이스의 충돌 지점(`ImpactPoint`)을 모두 더해 평균을 내어 표면의 중심점을 구함
 * - 각 트레이스의 법선 벡터(`Normal`)를 모두 더해 평균 방향을 구한 뒤, 정규화하여 단위 벡터로 변환
 * 
 * 만약 등반 가능한 표면이 감지되지 않았다면(`ClimbableSurfacesTracedResults.IsEmpty()`), 아무 작업도 수행하지 않고 반환합니다.
 *
 * @note 이 함수는 등반 중인 표면의 중심과 방향을 지속적으로 업데이트하기 위해 매 프레임 호출될 수 있습니다.
 * @see ClimbableSurfacesTracedResults
 * @see FVector
 * @see FHitResult
 */
void UCustomMovementComponent::ProcessClimbableSurfaceInfo()
{
	// 현재 등반 가능한 표면의 위치와 노멀(법선 벡터)을 초기화
	CurrentClimbableSurfaceLocation = FVector::ZeroVector;
	CurrentClimbableSurfaceNormal = FVector::ZeroVector;
	
	// 등반 가능한 표면을 감지한 결과(Trace 결과)가 없으면 바로 반환
	if (ClimbableSurfacesTracedResults.IsEmpty())
	{
		return;
	}

	// 감지된 모든 표면에 대해 ImpactPoint(충돌 지점)과 Normal(법선 벡터)을 누적합
	for (const FHitResult& TracedHitResult : ClimbableSurfacesTracedResults)
	{
		CurrentClimbableSurfaceLocation += TracedHitResult.ImpactPoint;
		CurrentClimbableSurfaceNormal += TracedHitResult.Normal;
	}

	// 평균 위치 계산 (감지된 모든 충돌 지점의 평균)
	CurrentClimbableSurfaceLocation /= ClimbableSurfacesTracedResults.Num();

	// 평균 노멀 계산 후 단위 벡터로 정규화
	CurrentClimbableSurfaceNormal = CurrentClimbableSurfaceNormal.GetSafeNormal();
}

/**
 * @brief 캐릭터를 현재 등반 가능한 표면에 부드럽게 밀착시키는 함수입니다.
 *
 * 캐릭터의 현재 위치와 등반 표면의 위치(`CurrentClimbableSurfaceLocation`)를 비교하여,
 * 캐릭터가 벽면과 일정 거리 이상 떨어져 있을 경우, 표면 방향으로 보정 이동시킵니다.
 * 
 * @param DeltaTime 프레임 간 경과 시간 (프레임 독립적 보간용)
 *
 * 동작 과정:
 * 1. 캐릭터의 전방 벡터(`ForwardVector`)와 위치를 구합니다.
 * 2. 캐릭터에서 벽까지의 벡터를 전방축에 투영하여 벽과의 거리(떨어진 정도)를 계산합니다.  
 * 3. 그 거리만큼 벽의 법선 반대 방향(-Normal)으로 이동 벡터(`SnapVector`)를 구합니다.  
 * 4. `MoveComponent()`를 통해 부드럽게 이동시켜 벽면에 밀착시킵니다.
 *
 * @note 등반 중 캐릭터가 벽에서 미세하게 떨어져 있거나, 
 *       프레임별 위치 보정이 필요한 상황에서 사용됩니다.
 *
 * @see UPrimitiveComponent::MoveComponent
 * @see CurrentClimbableSurfaceLocation
 * @see CurrentClimbableSurfaceNormal
 */
void UCustomMovementComponent::SnapMovementToClimbableSurfaces(float DeltaTime)
{
	// 1. 현재 캐릭터의 전방 방향과 위치를 가져옴
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();

	// 2️. 캐릭터 → 등반 표면까지의 벡터를 계산한 뒤,
	//     그 벡터를 캐릭터 전방 방향에 투영(Project)
	//     → 캐릭터가 벽과 얼마나 떨어져 있는지를 "전방 축 기준"으로 구함
	const FVector ProjectedCharacterToSurface = 
		(CurrentClimbableSurfaceLocation - ComponentLocation).ProjectOnTo(ComponentForward);

	// 3️. 벽면 법선(-Normal) 방향으로 벽 쪽으로 밀착(Snap)
	//     ProjectedCharacterToSurface.Length()은 캐릭터와 벽 사이의 거리
	const FVector SnapVector = -CurrentClimbableSurfaceNormal * ProjectedCharacterToSurface.Length();
	
	// 4. 실제 이동 수행
	//     DeltaTime과 MaxClimbSpeed를 곱해 프레임 독립적이고 자연스러운 이동 속도 보정
	//     MoveComponent는 충돌 처리를 포함한 안전한 이동 함수
	UpdatedComponent->MoveComponent(
		SnapVector * DeltaTime * MaxClimbSpeed, 
		UpdatedComponent->GetComponentQuat(), 
		true
	);
}

void UCustomMovementComponent::PlayClimbMontage(TObjectPtr<UAnimMontage> MontageToPlay)
{
	if (!MontageToPlay || !OwningPlayerAnimInstance || OwningPlayerAnimInstance->IsAnyMontagePlaying())
	{
		return;
	}

	OwningPlayerAnimInstance->Montage_Play(MontageToPlay);
}

void UCustomMovementComponent::SetMotionWarpTarget(const FName& InWarpTargetName, const FVector& InTargetPosition)
{
	if (!OwningPlayerCharacter)
	{
		return;
	}

	OwningPlayerCharacter->GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromLocation(InWarpTargetName, InTargetPosition);
}

/**
 * @brief 캐릭터가 등반 중인 벽면을 자연스럽게 바라보도록 회전 보정하는 함수
 *
 * 현재 프레임의 델타 타임(DeltaTime)을 기반으로, 캐릭터가 바라보는 방향(전방 벡터)이 등반 표면의 법선 방향과 반대가 되도록
 * 목표 회전을 계산하고, 부드럽게 보간(interpolation)합니다.
 *
 * @param DeltaTime 프레임 간 경과 시간(보간 속도 보정용)
 * @return FQuat 캐릭터가 향해야 할 목표 회전(쿼터니언)
 *
 * 동작 과정:
 * 1. 현재 회전(`CurrentQuat`)을 가져옴  
 * 2. 루트모션 또는 오버라이드 루트모션이 있으면 현재 회전을 그대로 유지  
 * 3. 등반 표면의 법선을 반대로 뒤집은 방향(-Normal)을 X축으로 하는 회전 생성  
 * 4. 현재 회전에서 목표 회전으로 `FMath::QInterpTo()`를 통해 부드럽게 보간  
 *
 * @note 루트모션 애니메이션이 활성화된 경우, 애니메이션에서 회전을 제어하므로 별도 보정하지 않습니다.
 * @see FMath::QInterpTo
 * @see FRotationMatrix::MakeFromX
 * @see UMovementComponent::UpdatedComponent
 */
FQuat UCustomMovementComponent::GetClimbRotation(float DeltaTime)
{
	// 현재 캐릭터(또는 이동 컴포넌트)의 회전값(쿼터니언 형태)을 가져옴
	const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();

	// 루트모션 애니메이션이 활성화되어 있거나, 현재 루트모션이 강제로 속도를 덮어쓰는 중이면
	// 애니메이션 주도 하에 회전해야 하므로 현재 회전을 그대로 반환
	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return CurrentQuat;
	}

	// 등반 표면의 법선(Normal)을 기준으로 "등반 중 바라봐야 할 방향"을 계산
	// MakeFromX()는 주어진 방향 벡터를 'X축이 바라보는 방향'으로 하는 회전 행렬을 생성
	// 즉, 캐릭터의 전방(X축)이 표면의 반대 방향(-Normal)을 향하도록 만듦
	const FQuat TargetQuat = FRotationMatrix::MakeFromX(-CurrentClimbableSurfaceNormal).ToQuat();

	// 현재 회전에서 목표 회전으로 부드럽게 보간 (회전 속도 = 5.f)
	// DeltaTime을 이용해 프레임 독립적인 보간을 수행
	return FMath::QInterpTo(CurrentQuat, TargetQuat, DeltaTime, 5.f);
}

void UCustomMovementComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 클라이밍 상태로 진입
	if (Montage == IdleToClimbMontage || Montage == ClimbDownLedgeMontage)
	{
		StartClimbing();
		StopMovementImmediately();
	}
	
	if (Montage == ClimbToTopMontage || Montage == VaultMontage)
	{
		SetMovementMode(MOVE_Walking);
	}
}

TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiByObject(const FVector & Start, const FVector & End, bool bShowDebugShape, bool bDrawPersistantShapes)
{	
	TArray<FHitResult> OutCapsuleTraceHitResults;

	EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;

	if(bShowDebugShape)
	{
		DebugTraceType = EDrawDebugTrace::ForOneFrame;

		if(bDrawPersistantShapes)
		{
			DebugTraceType = EDrawDebugTrace::Persistent;
		}
	}

	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		Start,
		End,
		ClimbCapsuleTraceRadius,
		ClimbCapsuleTraceHalfHeight,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		DebugTraceType,
		OutCapsuleTraceHitResults,
		false
	);

	return OutCapsuleTraceHitResults;
}

FHitResult UCustomMovementComponent::DoLineTraceSingleByObject(const FVector& Start, const FVector& End, bool bInShowDebugShape, bool bInDrawPersistantShapes)
{
	FHitResult OutResult;
	
	EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;

	if (bInShowDebugShape)
	{
		DebugTraceType = EDrawDebugTrace::ForOneFrame;

		if (bInDrawPersistantShapes)
		{
			DebugTraceType = EDrawDebugTrace::Persistent;

		}
	}
	
	UKismetSystemLibrary::LineTraceSingleForObjects(
	this,
		Start,
		End,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		DebugTraceType,
		OutResult,
		false
	);

	return OutResult;
}

bool UCustomMovementComponent::TraceClimbableSurfaces()
{
	const FVector StartOffset = UpdatedComponent->GetForwardVector() * 30.f;
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector();
	
	ClimbableSurfacesTracedResults = DoCapsuleTraceMultiByObject(Start, End, false, false);

	return !ClimbableSurfacesTracedResults.IsEmpty();
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceStartOffset, bool bInShowDebugShape, bool bInDrawPersistantShapes)
{
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);

	const FVector Start = ComponentLocation + EyeHeightOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;

	return DoLineTraceSingleByObject(Start, End, bInShowDebugShape, bInDrawPersistantShapes);
}
