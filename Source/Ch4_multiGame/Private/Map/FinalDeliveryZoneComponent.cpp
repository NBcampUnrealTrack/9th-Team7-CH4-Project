#include "Map/FinalDeliveryZoneComponent.h"
#include "Ch4_multiGame.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

UFinalDeliveryZoneComponent::UFinalDeliveryZoneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	InitBoxExtent(FVector(300.0f, 300.0f, 150.0f));
}

void UFinalDeliveryZoneComponent::BeginPlay()
{
	Super::BeginPlay();

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SetGenerateOverlapEvents(true);

	if (!HasServerAuthority())
	{
		return;
	}
	OnComponentBeginOverlap.AddUniqueDynamic(this, &UFinalDeliveryZoneComponent::OnTriggerBeginOverlap);

	if (ACh4_multiGameGameState* GameFlowState =
		GetWorld()->GetGameState<ACh4_multiGameGameState>())
	{
		GameFlowState->OnGamePhaseChanged.AddUniqueDynamic(
			this,
			&UFinalDeliveryZoneComponent::OnGamePhaseChanged
		);

		if (GameFlowState->GetCurrentGamePhase() == ECh4GamePhase::Playing)
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(
				this,
				&UFinalDeliveryZoneComponent::EvaluateOverlappingTargets
			);
		}
	}
}

void UFinalDeliveryZoneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasServerAuthority())
	{
		if (ACh4_multiGameGameState* GameFlowState =
			GetWorld()->GetGameState<ACh4_multiGameGameState>())
		{
			GameFlowState->OnGamePhaseChanged.RemoveDynamic(
				this,
				&UFinalDeliveryZoneComponent::OnGamePhaseChanged
			);
		}
	}

	OnComponentBeginOverlap.RemoveDynamic(
		this,
		&UFinalDeliveryZoneComponent::OnTriggerBeginOverlap
	);

	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	NotifiedDeliveryTargets.Reset();
	Super::EndPlay(EndPlayReason);
}

void UFinalDeliveryZoneComponent::OnTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	EvaluateDeliveryTarget(OtherActor);
}

void UFinalDeliveryZoneComponent::OnGamePhaseChanged(
	const ECh4GamePhase NewGamePhase)
{
	if (!HasServerAuthority() || NewGamePhase != ECh4GamePhase::Playing)
	{
		return;
	}

	NotifiedDeliveryTargets.Reset();

	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&UFinalDeliveryZoneComponent::EvaluateOverlappingTargets
	);
}

void UFinalDeliveryZoneComponent::EvaluateDeliveryTarget(AActor* OtherActor)
{
	if (!HasServerAuthority() ||
		!IsValid(OtherActor) ||
		OtherActor->IsActorBeingDestroyed() ||
		OtherActor->GetWorld() != GetWorld() ||
		OtherActor == GetOwner() ||
		!OtherActor->GetClass()->ImplementsInterface(
			UGameFlowTargetInterface::StaticClass()))
	{
		return;
	}

	ACh4_multiGameGameState* GameFlowState =
		GetWorld()->GetGameState<ACh4_multiGameGameState>();

	if (!GameFlowState ||
		GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	const TWeakObjectPtr<AActor> TargetKey(OtherActor);
	if (NotifiedDeliveryTargets.Contains(TargetKey))
	{
		return;
	}

	if (IGameFlowRuleInterface* GameRule =
		Cast<IGameFlowRuleInterface>(GetWorld()->GetAuthGameMode()))
	{
		NotifiedDeliveryTargets.Add(TargetKey);
		if (!GameRule->NotifyGoalReached(OtherActor))
		{
			// Permit another entry after a rejected delivery; keep the reentrancy guard above.
			NotifiedDeliveryTargets.Remove(TargetKey);
		}
	}
	else
	{
		UE_LOG(
			LogCh4_multiGame,
			Error,
			TEXT("[GameFlow] FinalDeliveryZoneComponent requires IGameFlowRuleInterface")
		);
	}
}

void UFinalDeliveryZoneComponent::EvaluateOverlappingTargets()
{
	if (!HasServerAuthority())
	{
		return;
	}

	ACh4_multiGameGameState* GameFlowState =
		GetWorld()->GetGameState<ACh4_multiGameGameState>();

	if (!GameFlowState ||
		GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (IsValid(OverlappingActor) &&
			OverlappingActor->GetClass()->ImplementsInterface(
				UGameFlowTargetInterface::StaticClass()))
		{
			EvaluateDeliveryTarget(OverlappingActor);
		}
	}
}

bool UFinalDeliveryZoneComponent::HasServerAuthority() const
{
	// A nonreplicated, level-placed wrapper can have ROLE_Authority locally on a client.
	return IsValid(GetOwner()) && GetOwner()->HasAuthority() && GetNetMode() != NM_Client;
}
