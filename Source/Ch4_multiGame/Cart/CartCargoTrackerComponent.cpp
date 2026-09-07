// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cart/CartCargoTrackerComponent.h"

#include "Cargo/CargoActor.h"
#include "Ch4_multiGame.h"
#include "GameFramework/Actor.h"

UCartCargoTrackerComponent::UCartCargoTrackerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	SetMobility(EComponentMobility::Movable);
	SetCanEverAffectNavigation(false);

	// Placeholder only: fit the box to the actual Cart's interior in its Blueprint.
	InitBoxExtent(FVector(50.0f));
	BodyInstance.bAutoWeld = false;
	SetSimulatePhysics(false);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SetGenerateOverlapEvents(true);
}

bool UCartCargoTrackerComponent::HasServerAuthority() const
{
	return IsValid(GetOwner()) && GetOwner()->HasAuthority() && GetNetMode() != NM_Client;
}

void UCartCargoTrackerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasServerAuthority())
	{
		// Clients neither need overlap queries nor own a live Cargo list.
		SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	OnComponentBeginOverlap.AddUniqueDynamic(this, &UCartCargoTrackerComponent::OnCargoBeginOverlap);
	OnComponentEndOverlap.AddUniqueDynamic(this, &UCartCargoTrackerComponent::OnCargoEndOverlap);

	// Include preloaded Cargo even if overlap registration preceded this component's BeginPlay.
	UpdateOverlaps();
	TArray<AActor*> OverlappingCargo;
	GetOverlappingActors(OverlappingCargo, ACargoActor::StaticClass());
	for (AActor* Cargo : OverlappingCargo)
	{
		RegisterCargo(Cast<ACargoActor>(Cargo));
	}
}

void UCartCargoTrackerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnComponentBeginOverlap.RemoveDynamic(this, &UCartCargoTrackerComponent::OnCargoBeginOverlap);
	OnComponentEndOverlap.RemoveDynamic(this, &UCartCargoTrackerComponent::OnCargoEndOverlap);
	TrackedCargo.Reset();
	Super::EndPlay(EndPlayReason);
}

void UCartCargoTrackerComponent::OnCargoBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	RegisterCargo(Cast<ACargoActor>(OtherActor));
}

void UCartCargoTrackerComponent::RegisterCargo(ACargoActor* Cargo)
{
	if (!HasServerAuthority() || !IsValid(Cargo) || Cargo == GetOwner()
		|| Cargo->IsActorBeingDestroyed() || Cargo->IsLost() || Cargo->GetWorld() != GetWorld())
	{
		return;
	}

	bool bAlreadyTracked = false;
	TrackedCargo.Add(TWeakObjectPtr<ACargoActor>(Cargo), &bAlreadyTracked);
	if (!bAlreadyTracked)
	{
		UE_LOG(LogCh4_multiGame, Verbose, TEXT("[CartCargo] %s Cargo entered: %s (Count: %d)"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Cargo), GetTrackedCargoCount());
	}
}

void UCartCargoTrackerComponent::OnCargoEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (!HasServerAuthority())
	{
		return;
	}

	ACargoActor* Cargo = Cast<ACargoActor>(OtherActor);
	// A Cargo may have multiple overlapping bodies/components. Keep it until the last one leaves.
	if (Cargo && !IsOverlappingActor(Cargo))
	{
		UnregisterCargo(Cargo);
	}
}

void UCartCargoTrackerComponent::UnregisterCargo(ACargoActor* Cargo)
{
	if (HasServerAuthority() && TrackedCargo.Remove(TWeakObjectPtr<ACargoActor>(Cargo)) > 0)
	{
		// Leaving the Cart never changes Cargo state or notifies GameFlow of a loss.
		UE_LOG(LogCh4_multiGame, Verbose, TEXT("[CartCargo] %s Cargo left: %s (Count: %d)"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Cargo), GetTrackedCargoCount());
	}
}

int32 UCartCargoTrackerComponent::GetTrackedCargoCount() const
{
	return BuildDeliveryScoreSummary().DeliveredCargoCount;
}

int32 UCartCargoTrackerComponent::GetTrackedCargoScore() const
{
	return BuildDeliveryScoreSummary().DeliveredCargoScore;
}

FCh4DeliveryScoreSummary UCartCargoTrackerComponent::BuildDeliveryScoreSummary() const
{
	FCh4DeliveryScoreSummary Summary;
	if (!HasServerAuthority())
	{
		return Summary;
	}

	Summary.bHasScoreData = true;
	int64 TotalScore = 0;
	for (auto It = TrackedCargo.CreateIterator(); It; ++It)
	{
		const ACargoActor* Cargo = It->Get();
		if (!IsValid(Cargo) || Cargo->IsActorBeingDestroyed() || Cargo->IsLost())
		{
			It.RemoveCurrent();
			continue;
		}

		++Summary.DeliveredCargoCount;
		TotalScore += Cargo->GetDeliveryScore();
	}

	// The shared summary uses int32; avoid wrapping a large positive total into a negative score.
	Summary.DeliveredCargoScore = static_cast<int32>(FMath::Min<int64>(TotalScore, MAX_int32));
	UE_LOG(LogCh4_multiGame, VeryVerbose, TEXT("[CartCargo] %s Delivery summary: %d Cargo / %d Score"),
		*GetNameSafe(GetOwner()), Summary.DeliveredCargoCount, Summary.DeliveredCargoScore);
	return Summary;
}
