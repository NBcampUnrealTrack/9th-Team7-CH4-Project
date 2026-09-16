// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/GameplayPhaseTransitionPoint.h"

#include "Cargo/CargoActor.h"
#include "Cart/CartBase.h"
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
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] BeginPlay: Actor=%s Authority=%d AutoStart=%d Duration=%.2f Cart=%s CartDestination=%s PlayerDestinations=%d"),
		*GetName(), HasAuthority(), bStartPreparationOnBeginPlay, PreparationDurationSeconds,
		*GetNameSafe(CartActor), *GetNameSafe(CartDestination), PlayerDestinationPoints.Num());
	if (HasAuthority())
	{
		if (ACh4_multiGameGameMode* Rule = GetGameRule())
		{
			Rule->RegisterGameplayCart(Cast<ACartBase>(CartActor));
		}
		if (bStartPreparationOnBeginPlay)
		{
			StartPreparationTimer();
		}
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
	ACh4_multiGameGameState* State = GetWorld()->GetGameState<ACh4_multiGameGameState>();
	if (!GetGameRule() || !State || State->GetCurrentGamePhase() != ECh4GamePhase::Waiting
		|| State->GetInitialCargoCount() != 0 || !FMath::IsFinite(PreparationDurationSeconds)
		|| PreparationDurationSeconds < 0.0f || (GetNetMode() != NM_Standalone && !GetIsReplicated()))
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Timer rejected: requires uninitialized Waiting, a finite non-negative duration and a replicated transition actor in multiplayer"));
		return false;
	}
	if (ACartBase* PreparationCart = Cast<ACartBase>(CartActor))
	{
		if (PreparationCart->GetWorld() != GetWorld() || !PreparationCart->SetPreparationLocked(true))
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Preparation] Timer rejected: failed to lock configured Cart %s"), *GetNameSafe(CartActor));
			return false;
		}
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Preparation] Cart lock unavailable: assign the ForestLevel ACartBase instance to CartActor on %s"),
			*GetName());
	}
	bPreparationTimerStarted = true;
	State->SetPreparationTimer(PreparationDurationSeconds);
	if (PreparationDurationSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PreparationTimer, this,
			&AGameplayPhaseTransitionPoint::OnPreparationExpired, PreparationDurationSeconds, false);
	}
	else
	{
		PreparationTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGameplayPhaseTransitionPoint::OnPreparationExpired);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Started: %.2fs; server timer active"), PreparationDurationSeconds);
	return true;
}

void AGameplayPhaseTransitionPoint::OnPreparationExpired()
{
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Completed: Actor=%s Authority=%d"), *GetName(), HasAuthority());
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
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Player %s has no Pawn yet; keep Waiting"), *GetNameSafe(PC));
			PlayerMoves.Reset();
			return false;
		}
		if (!Destinations.IsValidIndex(PlayerMoves.Num()))
		{
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Not enough distinct Player destinations: configured=%d valid=%d, no destination for Controller=%s Pawn=%s (player %d); keep the entire group in Waiting"),
				PlayerDestinationPoints.Num(), Destinations.Num(), *GetNameSafe(PC), *GetNameSafe(Pawn), PlayerMoves.Num() + 1);
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
		UE_LOG(LogCh4_multiGame, Verbose, TEXT("[Preparation] Transition guard: Authority=%d Elapsed=%d Started=%d"), HasAuthority(), bPreparationElapsed, bTransitionStarted);
		return false;
	}
	ACh4_multiGameGameMode* Rule = GetGameRule();
	const ACh4_multiGameGameState* State = GetWorld()->GetGameState<ACh4_multiGameGameState>();
	if (!Rule || !State || State->GetCurrentGamePhase() != ECh4GamePhase::Waiting || State->GetInitialCargoCount() != 0)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Transition rejected: GameMode=%s GameState=%s Phase=%d InitialCargo=%d; requires uninitialized Waiting"),
			*GetNameSafe(Rule), *GetNameSafe(State), State ? static_cast<int32>(State->GetCurrentGamePhase()) : -1, State ? State->GetInitialCargoCount() : -1);
		return false;
	}
	if (!IsValid(CartActor))
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Transition blocked: CartActor is not assigned on %s"),
			*GetName());
		return false;
	}
	if (CartActor->GetWorld() != GetWorld())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Invalid Cart target: Cart=%s is not in the transition World"),
			*GetNameSafe(CartActor));
		return false;
	}
	TArray<UCartCargoTrackerComponent*> Trackers;
	CartActor->GetComponents(Trackers);
	if (Trackers.Num() != 1)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Cart %s has %d trackers; requires exactly one CartCargoTrackerComponent"), *GetNameSafe(CartActor), Trackers.Num());
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
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Active cart cargo: %d; Tracker=%s"), CargoSnapshot.Num(), *GetNameSafe(Tracker));
	if (CargoSnapshot.IsEmpty())
	{
		bTransitionStarted = true;
		GetWorldTimerManager().ClearTimer(PreparationTimer);
		GetWorldTimerManager().ClearTimer(RestoreTimer);
		PlayerMoves.Reset();
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] No cargo loaded; returning to lobby"));
		if (!Rule->ReturnToLobbyFromPreparation())
		{
			bTransitionStarted = false;
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Preparation] Lobby return was rejected; preparation remains Waiting and retry is allowed"));
		}
		return false;
	}
	ACartBase* PreparationCart = Cast<ACartBase>(CartActor);
	if (!IsValid(PreparationCart) || !IsValid(CartDestination)
		|| CartDestination->GetWorld() != GetWorld() || !CartActor->GetRootComponent()
		|| CartActor->GetRootComponent()->Mobility != EComponentMobility::Movable
		|| !CartActor->Implements<UGameFlowTargetInterface>())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Invalid Cart target: Cart=%s Destination=%s; requires ACartBase, movable root, GameFlowTargetInterface and same-world references"),
			*GetNameSafe(CartActor), *GetNameSafe(CartDestination));
		return false;
	}
	if (!BuildPlayerMoves()) return false;
	const FTransform OldCart = CartActor->GetActorTransform();
	FTransform NewCart = CartDestination->GetActorTransform();
	NewCart.SetScale3D(OldCart.GetScale3D());
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Cart location: %s -> %s; Distance=%.2f"),
		*OldCart.GetLocation().ToString(), *NewCart.GetLocation().ToString(), FVector::Distance(OldCart.GetLocation(), NewCart.GetLocation()));
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
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Cargo initialization rejected: Snapshot=%d Phase=%d InitialCargo=%d"),
			CargoSnapshot.Num(), static_cast<int32>(State->GetCurrentGamePhase()), State->GetInitialCargoCount());
		bTransitionStarted = false;
		return false;
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Cargo initialized: %d; moving Cart, %d Cargo and %d Players"), CargoSnapshot.Num(), CargoSnapshot.Num(), PlayerMoves.Num());
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
			if (ACartBase* Cart = Cast<ACartBase>(Move.Actor); Cart && HasAuthority())
			{
				// CartRoot is the replicated target while the absolute CartMesh owns server physics.
				// The Cart API moves both even though preparation has already disabled CartMesh simulation.
				bTeleportSucceeded &= Cart->TeleportCartToTransform(Move.Destination);
			}
			else
			{
				bTeleportSucceeded &= Move.Actor->SetActorTransform(
					Move.Destination, false, nullptr, ETeleportType::TeleportPhysics);
			}
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
			if (ACartBase* Cart = Cast<ACartBase>(Saved.Actor.Get()))
			{
				Cart->TeleportCartToTransform(Saved.Source);
			}
			else if (Saved.Actor.IsValid())
			{
				Saved.Actor->SetActorTransform(Saved.Source, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		for (const FPrimitiveState& Saved : SuspendedPrimitives)
		{
			if (Saved.bSimulating && Saved.Component.IsValid()) Saved.Component->SetWorldTransform(Saved.Source, false, nullptr, ETeleportType::TeleportPhysics);
		}
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Teleport failed; restored original positions. Restart this match before retrying initialization"));
	}
	RestoreTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AGameplayPhaseTransitionPoint::OnLoadRestoreTimer);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Load move accepted=%d Authority=%d; physics restore scheduled"), bTeleportSucceeded, HasAuthority());
}

void AGameplayPhaseTransitionPoint::MovePlayers()
{
	int32 MovedPlayers = 0;
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
		else
		{
			++MovedPlayers;
		}
		if (APlayerController* PC = Move.Controller.Get())
		{
			PC->SetControlRotation(Move.Destination.Rotator());
			PC->ClientSetRotation(Move.Destination.Rotator(), true);
		}
		Pawn->ForceNetUpdate();
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Players moved: %d / %d; existing Pawns retained"), MovedPlayers, PlayerMoves.Num());
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
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Physics restored: Authority=%d MoveAccepted=%d"), HasAuthority(), bTeleportSucceeded);
	if (HasAuthority() && bTeleportSucceeded)
	{
		ACartBase* PreparationCart = Cast<ACartBase>(CartActor);
		if (!PreparationCart || !PreparationCart->SetPreparationLocked(false))
		{
			bTeleportSucceeded = false;
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Preparation] Cart unlock failed after load restoration; keep Waiting"));
			return;
		}
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Cart unlocked at Main Area: %s"),
			*GetNameSafe(PreparationCart));
		if (ACh4_multiGameGameMode* Rule = GetGameRule())
		{
			bTransitionComplete = Rule->FinishPreparationTransition();
			UE_LOG(LogCh4_multiGame, Log, TEXT("[Preparation] Physics restored; Game start accepted=%s"), bTransitionComplete ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogCh4_multiGame, Warning, TEXT("[Preparation] Game start rejected: GameMode unavailable after physics restore"));
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
