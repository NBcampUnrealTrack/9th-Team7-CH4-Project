// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cargo/CargoActor.h"

#include "Cargo/CargoDataAsset.h"
#include "Ch4_multiGame.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
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

	CargoMesh->SetMassOverrideInKg(NAME_None, ValidatedMassKg, true);
	CargoMesh->SetLinearDamping(ValidatedLinearDamping);
	CargoMesh->SetAngularDamping(ValidatedAngularDamping);
	CargoMesh->SetPhysMaterialOverride(IsValid(CargoData->PhysicalMaterial)
		? CargoData->PhysicalMaterial.Get()
		: nullptr);
	CargoMesh->SetEnableGravity(CargoData->bEnableGravity);

	return bHasValidMesh;
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
