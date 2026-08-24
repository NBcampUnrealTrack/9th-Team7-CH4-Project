#include "Map/FinalDeliveryZoneComponent.h"
#include "Ch4_multiGame.h"
#include "Ch4_multiGameGameMode.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

UFinalDeliveryZoneComponent::UFinalDeliveryZoneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFinalDeliveryZoneComponent::BeginPlay()
{
	Super::BeginPlay();

	//SetBoxExtent(FVector(300.0f, 300.0f, 150.0f));
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SetGenerateOverlapEvents(true);

	OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&UFinalDeliveryZoneComponent::OnTriggerBeginOverlap
	);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

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
	if (GetOwner() && GetOwner()->HasAuthority())
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
	if (!GetOwner() ||
		!GetOwner()->HasAuthority() ||
		NewGamePhase != ECh4GamePhase::Playing)
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&UFinalDeliveryZoneComponent::EvaluateOverlappingTargets
	);
}

void UFinalDeliveryZoneComponent::EvaluateDeliveryTarget(AActor* OtherActor)
{
	if (!GetOwner() ||
		!GetOwner()->HasAuthority() ||
		!IsValid(OtherActor) ||
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

	if (ACh4_multiGameGameMode* GameMode =
		GetWorld()->GetAuthGameMode<ACh4_multiGameGameMode>())
	{
		UE_LOG(
			LogCh4_multiGame,
			Log,
			TEXT("[GameFlow] Final Delivery target entered the zone: %s"),
			*GetNameSafe(OtherActor)
		);

		GameMode->TryCompleteGame();
	}
	else
	{
		UE_LOG(
			LogCh4_multiGame,
			Error,
			TEXT("[GameFlow] FinalDeliveryZoneComponent requires ACh4_multiGameGameMode")
		);
	}
}

void UFinalDeliveryZoneComponent::EvaluateOverlappingTargets()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
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
			break;
		}
	}
}