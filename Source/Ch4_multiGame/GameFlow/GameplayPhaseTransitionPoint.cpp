// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/GameplayPhaseTransitionPoint.h"

#include "Cargo/CargoActor.h"
#include "Cart/CartCargoTrackerComponent.h"
#include "Ch4_multiGame.h"
#include "Ch4_multiGameGameMode.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AGameplayPhaseTransitionPoint::AGameplayPhaseTransitionPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void AGameplayPhaseTransitionPoint::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bStartPreparationOnBeginPlay)
	{
		StartPreparationTimer();
	}
}

ACh4_multiGameGameMode* AGameplayPhaseTransitionPoint::GetGameRule() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GameModeForTesting.IsValid())
	{
		return GameModeForTesting.Get();
	}
#endif
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACh4_multiGameGameMode>() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void AGameplayPhaseTransitionPoint::SetGameModeForTesting(ACh4_multiGameGameMode* GameMode)
{
	GameModeForTesting = GameMode;
}
#endif

bool AGameplayPhaseTransitionPoint::StartPreparationTimer()
{
	if (!HasAuthority() || GetNetMode() == NM_Client || bPreparationTimerStarted || bTransitionStarted)
	{
		return false;
	}
	const ACh4_multiGameGameState* State = GetWorld()->GetGameState<ACh4_multiGameGameState>();
	if (!GetGameRule() || !State || State->GetCurrentGamePhase() != ECh4GamePhase::Waiting
		|| State->GetInitialCargoCount() != 0 || !FMath::IsFinite(PreparationDurationSeconds)
		|| PreparationDurationSeconds < 0.0f || (GetNetMode() != NM_Standalone && !GetIsReplicated()))
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Timer rejected: requires uninitialized Waiting, a finite non-negative duration and a replicated transition actor in multiplayer"));
		return false;
	}
	bPreparationTimerStarted = true;
	if (PreparationDurationSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PreparationTimer, this,
			&AGameplayPhaseTransitionPoint::OnPreparationExpired, PreparationDurationSeconds, false);
	}
	else
	{
		PreparationTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGameplayPhaseTransitionPoint::OnPreparationExpired);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Waiting for %.2f seconds"), PreparationDurationSeconds);
	return true;
}

void AGameplayPhaseTransitionPoint::OnPreparationExpired()
{
	bPreparationElapsed = true;
	TryStartMainGameplay();
}

bool AGameplayPhaseTransitionPoint::BuildPlayerMoves()
{
	PlayerMoves.Reset();
	TArray<ATargetPoint*> Destinations;
	for (ATargetPoint* Point : PlayerDestinationPoints)
	{
		if (!IsValid(Point) || Point->GetWorld() != GetWorld())
		{
			continue;
		}
		const bool bDuplicate = Destinations.ContainsByPredicate([Point](const ATargetPoint* Other)
		{
			return Point == Other || Point->GetActorLocation().Equals(Other->GetActorLocation(), 1.0f);
		});
		if (!bDuplicate)
		{
			Destinations.Add(Point);
		}
	}
	TSet<APawn*> AssignedPawns;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
		if (!IsValid(PC) || AssignedPawns.Contains(Pawn))
		{
			continue;
		}
		if (!IsValid(Pawn))
		{
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] A connected Player has no Pawn yet; keep Waiting"));
			PlayerMoves.Reset();
			return false;
		}
		if (!Destinations.IsValidIndex(PlayerMoves.Num()))
		{
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Not enough distinct Player destinations; keep the entire group in Waiting"));
			PlayerMoves.Reset();
			return false;
		}
		FPlayerMove Move;
		Move.Controller = PC;
		Move.Pawn = Pawn;
		Move.Destination = Destinations[PlayerMoves.Num()]->GetActorTransform();
		PlayerMoves.Add(Move);
		AssignedPawns.Add(Pawn);
	}
	return true;
}

bool AGameplayPhaseTransitionPoint::TryStartMainGameplay()
{
	if (!HasAuthority() || GetNetMode() == NM_Client || !bPreparationElapsed || bTransitionStarted)
	{
		return false;
	}
	ACh4_multiGameGameMode* Rule = GetGameRule();
	const ACh4_multiGameGameState* State = GetWorld()->GetGameState<ACh4_multiGameGameState>();
	if (!Rule || !State || State->GetCurrentGamePhase() != ECh4GamePhase::Waiting || State->GetInitialCargoCount() != 0)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Transition rejected: another path already initialized or started GameFlow"));
		return false;
	}
	if (!IsValid(CartActor) || !IsValid(CartDestination) || CartActor->GetWorld() != GetWorld()
		|| CartDestination->GetWorld() != GetWorld() || !CartActor->GetRootComponent()
		|| CartActor->GetRootComponent()->Mobility != EComponentMobility::Movable
		|| !CartActor->Implements<UGameFlowTargetInterface>())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Configure a movable Cart target and a same-world Cart destination"));
		return false;
	}
	TArray<UCartCargoTrackerComponent*> Trackers;
	CartActor->GetComponents(Trackers);
	if (Trackers.Num() != 1 || !BuildPlayerMoves())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Requires exactly one Cart tracker and valid Player destinations"));
		return false;
	}
	UCartCargoTrackerComponent* Tracker = Trackers[0];
	Tracker->UpdateOverlaps();
	TArray<ACargoActor*> CargoSnapshot = Tracker->GetTrackedCargoSnapshot();
	// Grab disables collision and attaches the Cargo root to the player's hand. Never steal that load.
	CargoSnapshot.RemoveAll([](const ACargoActor* Cargo)
	{
		return Cargo->GetAttachParentActor() != nullptr;
	});
	if (CargoSnapshot.IsEmpty())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Empty Cart: no initialization or teleport; remain Waiting"));
		return false;
	}
	const FTransform OldCart = CartActor->GetActorTransform();
	FTransform NewCart = CartDestination->GetActorTransform();
	NewCart.SetScale3D(OldCart.GetScale3D());
	TArray<FCh4GameplayActorMove> Moves;
	FCh4GameplayActorMove& CartMove = Moves.AddDefaulted_GetRef();
	CartMove.Actor = CartActor;
	CartMove.Destination = NewCart;
	for (ACargoActor* Cargo : CargoSnapshot)
	{
		FCh4GameplayActorMove& Move = Moves.AddDefaulted_GetRef();
		Move.Actor = Cargo;
		Move.Destination = Cargo->GetActorTransform().GetRelativeTransform(OldCart) * NewCart;
	}
	for (FCh4GameplayActorMove& Move : Moves)
	{
		if (!IsValid(Move.Actor) || !Move.Actor->GetRootComponent()
			|| Move.Actor->GetRootComponent()->Mobility != EComponentMobility::Movable
			|| Move.Destination.ContainsNaN()
			|| (GetNetMode() != NM_Standalone && (!Move.Actor->GetIsReplicated() || !Move.Actor->IsReplicatingMovement())))
		{
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Load must be movable and, in multiplayer, replicate Actor and Movement: %s"), *GetNameSafe(Move.Actor));
			return false;
		}
		TArray<UPrimitiveComponent*> Components;
		Move.Actor->GetComponents(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (Component->IsSimulatingPhysics()) Move.SimulatingComponents.Add(Component);
		}
	}
	// Guard before the GameState broadcast prevents synchronous re-entry as well as duplicate timers.
	bTransitionStarted = true;
	if (!Rule->InitializePreparationCargo(CargoSnapshot))
	{
		bTransitionStarted = false;
		return false;
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Snapshot fixed: %d Cargo, %d Players"), CargoSnapshot.Num(), PlayerMoves.Num());
	MulticastMoveLoad(Moves);
	if (bTeleportSucceeded)
	{
		MovePlayers();
	}
	return bTeleportSucceeded;
}

void AGameplayPhaseTransitionPoint::MulticastMoveLoad_Implementation(const TArray<FCh4GameplayActorMove>& Moves)
{
	if (bLoadMoveApplied)
	{
		return;
	}
	bLoadMoveApplied = true;
	bTeleportSucceeded = true;
	PhysicsSuspendedFrame = GFrameCounter;
	// Snapshot every body before moving anything. The actors remain independent; no attach/weld is added.
	for (const FCh4GameplayActorMove& Move : Moves)
	{
		AActor* Actor = Move.Actor;
		if (!IsValid(Actor))
		{
			bTeleportSucceeded = false;
			continue;
		}
		FActorState& SavedActor = SuspendedActors.AddDefaulted_GetRef();
		SavedActor.Actor = Actor;
		SavedActor.Source = Actor->GetActorTransform();
		SavedActor.bTickEnabled = Actor->IsActorTickEnabled();
		Actor->SetActorTickEnabled(false);
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			FPrimitiveState& Saved = SuspendedPrimitives.AddDefaulted_GetRef();
			Saved.Component = Component;
			Saved.Collision = Component->GetCollisionEnabled();
			Saved.bSimulating = Move.SimulatingComponents.Contains(Component);
			Saved.bTickEnabled = Component->IsComponentTickEnabled();
			Saved.Source = Component->GetComponentTransform();
			Saved.Destination = Saved.Source.GetRelativeTransform(SavedActor.Source) * Move.Destination;
		}
	}
	for (const FPrimitiveState& Saved : SuspendedPrimitives)
	{
		if (UPrimitiveComponent* Component = Saved.Component.Get())
		{
			if (Component->IsSimulatingPhysics())
			{
				Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
				Component->SetSimulatePhysics(false);
			}
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetComponentTickEnabled(false);
		}
	}
	// The Cart is first in Moves; all Cargo destinations were derived from the same Cart snapshot.
	for (const FCh4GameplayActorMove& Move : Moves)
	{
		if (IsValid(Move.Actor))
		{
			bTeleportSucceeded &= Move.Actor->SetActorTransform(Move.Destination, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	for (const FPrimitiveState& Saved : SuspendedPrimitives)
	{
		if (Saved.bSimulating && Saved.Component.IsValid())
		{
			// Simulating non-root bodies may already be detached from the actor hierarchy.
			Saved.Component->SetWorldTransform(Saved.Destination, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	if (!bTeleportSucceeded && HasAuthority())
	{
		for (const FActorState& Saved : SuspendedActors)
		{
			if (Saved.Actor.IsValid()) Saved.Actor->SetActorTransform(Saved.Source, false, nullptr, ETeleportType::TeleportPhysics);
		}
		for (const FPrimitiveState& Saved : SuspendedPrimitives)
		{
			if (Saved.bSimulating && Saved.Component.IsValid()) Saved.Component->SetWorldTransform(Saved.Source, false, nullptr, ETeleportType::TeleportPhysics);
		}
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Teleport failed; restored original positions. Restart this match before retrying initialization"));
	}
	RestoreTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGameplayPhaseTransitionPoint::OnLoadRestoreTimer);
}

void AGameplayPhaseTransitionPoint::MovePlayers()
{
	for (const FPlayerMove& Move : PlayerMoves)
	{
		APawn* Pawn = Move.Pawn.Get();
		if (!IsValid(Pawn)) continue;
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
		if (!Pawn->TeleportTo(Move.Destination.GetLocation(), Move.Destination.Rotator(), false, true))
		{
			bTeleportSucceeded = false;
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Player teleport failed: %s; keep Waiting"), *GetNameSafe(Pawn));
		}
		if (APlayerController* PC = Move.Controller.Get())
		{
			PC->SetControlRotation(Move.Destination.Rotator());
			PC->ClientSetRotation(Move.Destination.Rotator(), true);
		}
		Pawn->ForceNetUpdate();
	}
	PlayerMoves.Reset();
}

void AGameplayPhaseTransitionPoint::RestoreLoadPhysics()
{
	// Restore collision for the complete load before any body resumes simulation.
	for (const FPrimitiveState& Saved : SuspendedPrimitives)
	{
		if (Saved.Component.IsValid()) Saved.Component->SetCollisionEnabled(Saved.Collision);
	}
	for (const FPrimitiveState& Saved : SuspendedPrimitives)
	{
		if (UPrimitiveComponent* Component = Saved.Component.Get())
		{
			if (Saved.bSimulating)
			{
				Component->SetSimulatePhysics(true);
				Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			}
			Component->SetComponentTickEnabled(Saved.bTickEnabled);
		}
	}
	for (const FActorState& Saved : SuspendedActors)
	{
		if (AActor* Actor = Saved.Actor.Get())
		{
			Actor->SetActorTickEnabled(Saved.bTickEnabled);
			Actor->UpdateOverlaps();
			if (HasAuthority()) Actor->ForceNetUpdate();
		}
	}
	SuspendedPrimitives.Reset();
	SuspendedActors.Reset();
}

void AGameplayPhaseTransitionPoint::OnLoadRestoreTimer()
{
	// A network RPC can arrive before this frame's TimerManager tick. Enforce an actual frame boundary.
	if (GFrameCounter == PhysicsSuspendedFrame)
	{
		RestoreTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGameplayPhaseTransitionPoint::OnLoadRestoreTimer);
		return;
	}
	if (HasAuthority() && SuspendedActors.ContainsByPredicate([](const FActorState& Saved)
	{
		return !Saved.Actor.IsValid() || Saved.Actor->IsActorBeingDestroyed();
	}))
	{
		bTeleportSucceeded = false;
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] A load actor was destroyed before restoration; keep Waiting"));
	}
	RestoreLoadPhysics();
	if (HasAuthority() && bTeleportSucceeded)
	{
		if (ACh4_multiGameGameMode* Rule = GetGameRule())
		{
			bTransitionComplete = Rule->FinishPreparationTransition();
			UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Physics restored; Game start accepted=%s"), bTransitionComplete ? TEXT("true") : TEXT("false"));
		}
	}
}

void AGameplayPhaseTransitionPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PreparationTimer);
	GetWorldTimerManager().ClearTimer(RestoreTimer);
	RestoreLoadPhysics();
	Super::EndPlay(EndPlayReason);
}
