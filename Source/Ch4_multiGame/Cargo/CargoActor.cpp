// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/CargoActor.h"

#include "Cargo/CargoDataAsset.h"
#include "Ch4_multiGame.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFramework/GameModeBase.h"
#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
	constexpr float FallbackCargoMassKg = 1.0f;
}

ACargoActor::ACargoActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetReplicates(true);
	SetReplicateMovement(true);

	CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMesh);

	CargoMesh->SetMobility(EComponentMobility::Movable);
	CargoMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	CargoMesh->SetSimulatePhysics(true);
	CargoMesh->SetEnableGravity(true);
	CargoMesh->SetNotifyRigidBodyCollision(true);
	CargoMesh->OnComponentHit.AddDynamic(this, &ACargoActor::OnCargoMeshHit);
}

void ACargoActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCargoData();
}

void ACargoActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyCargoData();
}

void ACargoActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACargoActor, CargoState);
}

int32 ACargoActor::GetDeliveryScore() const
{
	if (IsLost() || !IsValid(CargoData))
	{
		return 0;
	}

	return FMath::Max(CargoData->DeliveryScore, 0);
}

bool ACargoActor::ApplyCargoData()
{
	// Construction runs repeatedly in the Editor; defer validation logs until BeginPlay.
	const bool bShouldLogWarnings = HasActorBegunPlay();
	if (!IsValid(CargoData))
	{
		if (bShouldLogWarnings)
		{
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Cargo] %s has no CargoData"), *GetNameSafe(this));
		}
		return false;
	}

	const bool bHasValidMesh = IsValid(CargoData->StaticMesh);
	CargoMesh->SetStaticMesh(bHasValidMesh ? CargoData->StaticMesh.Get() : nullptr);
	if (!bHasValidMesh && bShouldLogWarnings)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Cargo] %s CargoData '%s' has no StaticMesh"),
			*GetNameSafe(this), *GetNameSafe(CargoData));
	}

	const float ValidatedMassKg = CargoData->MassKg > 0.0f ? CargoData->MassKg : FallbackCargoMassKg;
	if (CargoData->MassKg <= 0.0f && bShouldLogWarnings)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Cargo] %s CargoData '%s' has invalid MassKg %.3f; using %.3f kg"),
			*GetNameSafe(this), *GetNameSafe(CargoData), CargoData->MassKg, ValidatedMassKg);
	}

	const float ValidatedLinearDamping = FMath::Max(CargoData->LinearDamping, 0.0f);
	const float ValidatedAngularDamping = FMath::Max(CargoData->AngularDamping, 0.0f);
	if (CargoData->bBreakableFromGroundImpact && bShouldLogWarnings)
	{
		if (CargoData->GroundImpactsToBreak <= 0)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Cargo] %s CargoData '%s' has invalid GroundImpactsToBreak %d; using 1"),
				*GetNameSafe(this), *GetNameSafe(CargoData), CargoData->GroundImpactsToBreak);
		}
		if (CargoData->MinimumGroundImpactImpulse < 0.0f || CargoData->GroundImpactCooldownSeconds < 0.0f)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Cargo] %s CargoData '%s' has negative ground-impact values; using zero minimums"),
				*GetNameSafe(this), *GetNameSafe(CargoData));
		}
		if (CargoData->MinimumGroundNormalZ < 0.0f || CargoData->MinimumGroundNormalZ > 1.0f)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Cargo] %s CargoData '%s' has invalid MinimumGroundNormalZ %.3f; clamping to [0, 1]"),
				*GetNameSafe(this), *GetNameSafe(CargoData), CargoData->MinimumGroundNormalZ);
		}
	}

	CargoMesh->SetMassOverrideInKg(NAME_None, ValidatedMassKg, true);
	CargoMesh->SetLinearDamping(ValidatedLinearDamping);
	CargoMesh->SetAngularDamping(ValidatedAngularDamping);
	CargoMesh->SetPhysMaterialOverride(IsValid(CargoData->PhysicalMaterial)
		? CargoData->PhysicalMaterial.Get()
		: nullptr);
	CargoMesh->SetEnableGravity(CargoData->bEnableGravity);

	return bHasValidMesh;
}

void ACargoActor::OnCargoMeshHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || !IsValid(OtherComp))
	{
		return;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	ProcessGroundImpact(
		OtherComp->GetCollisionObjectType(),
		OtherComp->GetMobility() == EComponentMobility::Static,
		Hit.ImpactNormal.Z,
		NormalImpulse.Size(),
		World->GetTimeSeconds(),
		Hit.ImpactPoint,
		Hit.ImpactNormal);
}

bool ACargoActor::ProcessGroundImpact(
	const ECollisionChannel OtherObjectType,
	const bool bOtherComponentIsStatic,
	const float ImpactNormalZ,
	const float NormalImpulseMagnitude,
	const double CurrentTimeSeconds,
	const FVector& ImpactPoint,
	const FVector& ImpactNormal)
{
	if (!HasAuthority() || IsLost() || bBreakInProgress || !IsValid(CargoData)
		|| !CargoData->bBreakableFromGroundImpact)
	{
		return false;
	}

	if (OtherObjectType != ECC_WorldStatic || !bOtherComponentIsStatic)
	{
		UE_LOG(LogCh4_multiGame, VeryVerbose,
			TEXT("[Cargo] Ground impact ignored for %s: collision target is not static world ground"),
			*GetNameSafe(this));
		return false;
	}

	const float RequiredNormalZ = FMath::Clamp(CargoData->MinimumGroundNormalZ, 0.0f, 1.0f);
	if (!FMath::IsFinite(ImpactNormalZ) || ImpactNormalZ < RequiredNormalZ)
	{
		UE_LOG(LogCh4_multiGame, VeryVerbose,
			TEXT("[Cargo] Ground impact ignored for %s: normal Z %.3f is below %.3f"),
			*GetNameSafe(this), ImpactNormalZ, RequiredNormalZ);
		return false;
	}

	const float RequiredImpulse = FMath::Max(CargoData->MinimumGroundImpactImpulse, 0.0f);
	if (!FMath::IsFinite(NormalImpulseMagnitude)
		|| NormalImpulseMagnitude <= UE_SMALL_NUMBER
		|| NormalImpulseMagnitude < RequiredImpulse)
	{
		UE_LOG(LogCh4_multiGame, VeryVerbose,
			TEXT("[Cargo] Ground impact ignored for %s: impulse %.3f is below %.3f"),
			*GetNameSafe(this), NormalImpulseMagnitude, RequiredImpulse);
		return false;
	}

	if (!FMath::IsFinite(CurrentTimeSeconds) || CurrentTimeSeconds < NextAllowedGroundImpactTimeSeconds)
	{
		UE_LOG(LogCh4_multiGame, VeryVerbose,
			TEXT("[Cargo] Ground impact ignored for %s: cooldown until %.3f"),
			*GetNameSafe(this), NextAllowedGroundImpactTimeSeconds);
		return false;
	}

	const float CooldownSeconds = FMath::Max(CargoData->GroundImpactCooldownSeconds, 0.0f);
	NextAllowedGroundImpactTimeSeconds = CurrentTimeSeconds + CooldownSeconds;
	++GroundImpactCount;

	const int32 RequiredImpactCount = FMath::Max(CargoData->GroundImpactsToBreak, 1);
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Cargo] Ground impact accepted for %s: %d / %d (impulse %.3f)"),
		*GetNameSafe(this), GroundImpactCount, RequiredImpactCount, NormalImpulseMagnitude);

	if (GroundImpactCount < RequiredImpactCount)
	{
		return true;
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Cargo] Break threshold reached for %s"), *GetNameSafe(this));

	if (!TryBreakCargo(ImpactPoint, ImpactNormal))
	{
		// The rejected hit is rolled back. Existing accepted wear remains, and a new
		// strong impact is required after GameFlow enters a phase that accepts loss.
		GroundImpactCount = FMath::Max(RequiredImpactCount - 1, 0);
		UE_LOG(LogCh4_multiGame, Verbose,
			TEXT("[Cargo] Break rejected for %s; impact count restored to %d / %d"),
			*GetNameSafe(this), GroundImpactCount, RequiredImpactCount);
	}

	return true;
}

bool ACargoActor::TryBreakCargo(const FVector& ImpactPoint, const FVector& ImpactNormal)
{
	if (!HasAuthority() || bBreakInProgress || IsLost())
	{
		return false;
	}

	bBreakInProgress = true;
	if (!MarkAsLost())
	{
		bBreakInProgress = false;
		return false;
	}

	if (IsValid(CargoData) && IsValid(CargoData->BreakEffect))
	{
		MulticastPlayBreakEffect(ImpactPoint, ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
	}

	CargoMesh->SetNotifyRigidBodyCollision(false);
	CargoMesh->SetSimulatePhysics(false);
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorEnableCollision(false);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Cargo] Cargo destroyed after ground impacts: %s"), *GetNameSafe(this));

	if (!Destroy())
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Cargo] Failed to destroy Lost Cargo actor: %s"), *GetNameSafe(this));
	}

	return true;
}

void ACargoActor::MulticastPlayBreakEffect_Implementation(
	const FVector_NetQuantize ImpactPoint,
	const FVector_NetQuantizeNormal ImpactNormal)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(CargoData) || !IsValid(CargoData->BreakEffect))
	{
		return;
	}

	const FVector SurfaceNormal = FVector(ImpactNormal).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector EffectScale(
		FMath::Max(CargoData->BreakEffectScale.X, 0.0f),
		FMath::Max(CargoData->BreakEffectScale.Y, 0.0f),
		FMath::Max(CargoData->BreakEffectScale.Z, 0.0f));

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		CargoData->BreakEffect,
		ImpactPoint,
		FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator(),
		EffectScale,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}

bool ACargoActor::MarkAsLost()
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Cargo] Rejected client-side Lost request for %s"), *GetNameSafe(this));
		return false;
	}

	if (CargoState == ECargoState::Lost)
	{
		UE_LOG(LogCh4_multiGame, Verbose,
			TEXT("[Cargo] Ignored duplicate Lost request for %s"), *GetNameSafe(this));
		return false;
	}

	IGameFlowRuleInterface* GameFlowRule = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	GameFlowRule = GameFlowRuleOverrideForTesting;
#endif
	if (!GameFlowRule)
	{
		UWorld* const World = GetWorld();
		GameFlowRule = World
			? Cast<IGameFlowRuleInterface>(World->GetAuthGameMode())
			: nullptr;
	}
	if (!GameFlowRule)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Cargo] %s requires an authoritative IGameFlowRuleInterface to become Lost"),
			*GetNameSafe(this));
		return false;
	}

	if (!GameFlowRule->NotifyCargoLost(1))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Cargo] GameFlow rejected the Lost notification for %s"), *GetNameSafe(this));
		return false;
	}

	CargoState = ECargoState::Lost;
	ForceNetUpdate();

	UE_LOG(LogCh4_multiGame, Log, TEXT("[Cargo] %s changed to Lost"), *GetNameSafe(this));
	return true;
}

void ACargoActor::OnRep_CargoState()
{
	UE_LOG(LogCh4_multiGame, Verbose,
		TEXT("[Cargo] %s replicated state changed to %s"),
		*GetNameSafe(this),
		CargoState == ECargoState::Lost ? TEXT("Lost") : TEXT("Active"));
}
