// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/FinalDeliveryZone.h"

#include "Ch4_multiGame.h"
#include "Ch4_multiGameGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

AFinalDeliveryZone::AFinalDeliveryZone()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	TriggerCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerCollision"));
	TriggerCollision->SetupAttachment(SceneRoot);
	TriggerCollision->SetBoxExtent(FVector(300.0f, 300.0f, 150.0f));
	TriggerCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerCollision->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	TriggerCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	TriggerCollision->SetGenerateOverlapEvents(true);
	TriggerCollision->OnComponentBeginOverlap.AddDynamic(this, &AFinalDeliveryZone::OnTriggerBeginOverlap);
}

void AFinalDeliveryZone::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (ACh4_multiGameGameState* GameFlowState = GetWorld()->GetGameState<ACh4_multiGameGameState>())
	{
		GameFlowState->OnGamePhaseChanged.AddUniqueDynamic(this, &AFinalDeliveryZone::OnGamePhaseChanged);

		if (GameFlowState->GetCurrentGamePhase() == ECh4GamePhase::Playing)
		{
			GetWorldTimerManager().SetTimerForNextTick(this, &AFinalDeliveryZone::EvaluateOverlappingTargets);
		}
	}
}

void AFinalDeliveryZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (ACh4_multiGameGameState* GameFlowState = GetWorld()->GetGameState<ACh4_multiGameGameState>())
		{
			GameFlowState->OnGamePhaseChanged.RemoveDynamic(this, &AFinalDeliveryZone::OnGamePhaseChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AFinalDeliveryZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	EvaluateDeliveryTarget(OtherActor);
}

void AFinalDeliveryZone::OnGamePhaseChanged(const ECh4GamePhase NewGamePhase)
{
	if (!HasAuthority() || NewGamePhase != ECh4GamePhase::Playing)
	{
		return;
	}

	// Overlap state can finish registering later in the same frame as BeginPlay.
	GetWorldTimerManager().SetTimerForNextTick(this, &AFinalDeliveryZone::EvaluateOverlappingTargets);
}

void AFinalDeliveryZone::EvaluateDeliveryTarget(AActor* OtherActor)
{
	if (!HasAuthority() || !IsValid(OtherActor) || OtherActor == this
		|| !OtherActor->GetClass()->ImplementsInterface(UGameFlowTargetInterface::StaticClass()))
	{
		return;
	}

	ACh4_multiGameGameState* GameFlowState = GetWorld()->GetGameState<ACh4_multiGameGameState>();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	if (ACh4_multiGameGameMode* GameMode = GetWorld()->GetAuthGameMode<ACh4_multiGameGameMode>())
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Final Delivery target entered the zone: %s"), *GetNameSafe(OtherActor));
		GameMode->TryCompleteGame();
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Error, TEXT("[GameFlow] FinalDeliveryZone requires ACh4_multiGameGameMode"));
	}
}

void AFinalDeliveryZone::EvaluateOverlappingTargets()
{
	if (!HasAuthority())
	{
		return;
	}

	ACh4_multiGameGameState* GameFlowState = GetWorld()->GetGameState<ACh4_multiGameGameState>();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	TriggerCollision->GetOverlappingActors(OverlappingActors);

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (IsValid(OverlappingActor)
			&& OverlappingActor->GetClass()->ImplementsInterface(UGameFlowTargetInterface::StaticClass()))
		{
			EvaluateDeliveryTarget(OverlappingActor);
			break;
		}
	}
}
