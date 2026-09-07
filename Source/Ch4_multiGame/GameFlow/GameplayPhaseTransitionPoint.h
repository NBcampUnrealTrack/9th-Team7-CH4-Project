// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayPhaseTransitionPoint.generated.h"

class ACh4_multiGameGameMode;
class APlayerController;
class ATargetPoint;
class UPrimitiveComponent;

/** A single authoritative load move. Existing actor movement replication covers late joiners. */
USTRUCT()
struct FCh4GameplayActorMove
{
	GENERATED_BODY()
	UPROPERTY()
	TObjectPtr<AActor> Actor;
	UPROPERTY()
	FTransform Destination = FTransform::Identity;
	/** Use server simulation flags even if transient movement replication reaches a peer before the RPC. */
	UPROPERTY()
	TArray<TObjectPtr<UPrimitiveComponent>> SimulatingComponents;
};

/** Per-map preparation settings and a one-shot transition; no Tick or world Cart search. */
UCLASS(Blueprintable)
class CH4_MULTIGAME_API AGameplayPhaseTransitionPoint : public AActor
{
	GENERATED_BODY()

public:
	AGameplayPhaseTransitionPoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Gameplay Transition")
	TObjectPtr<AActor> CartActor;

	/** Position/rotation only. The Cart's original scale is preserved. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Gameplay Transition")
	TObjectPtr<ATargetPoint> CartDestination;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Gameplay Transition")
	TArray<TObjectPtr<ATargetPoint>> PlayerDestinationPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gameplay Transition", meta=(ClampMin="0.0", Units="s"))
	float PreparationDurationSeconds = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gameplay Transition")
	bool bStartPreparationOnBeginPlay = true;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gameplay Transition")
	bool StartPreparationTimer();

	/** Retry after the timer expired with an empty Cart or invalid map settings. Never skips the timer. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gameplay Transition")
	bool TryStartMainGameplay();

	bool IsTransitionComplete() const { return bTransitionComplete; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetGameModeForTesting(ACh4_multiGameGameMode* GameMode);
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FPrimitiveState
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
		bool bSimulating = false;
		bool bTickEnabled = false;
		FTransform Source;
		FTransform Destination;
	};
	struct FActorState
	{
		TWeakObjectPtr<AActor> Actor;
		FTransform Source;
		bool bTickEnabled = false;
	};
	struct FPlayerMove
	{
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<APawn> Pawn;
		FTransform Destination;
	};

	ACh4_multiGameGameMode* GetGameRule() const;
	bool BuildPlayerMoves();
	void OnPreparationExpired();
	void OnLoadRestoreTimer();
	void RestoreLoadPhysics();
	void MovePlayers();

	/** One reliable event, never per-frame: peers also suspend their local solver during the move. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastMoveLoad(const TArray<FCh4GameplayActorMove>& Moves);

	FTimerHandle PreparationTimer;
	FTimerHandle RestoreTimer;
	bool bPreparationTimerStarted = false;
	bool bPreparationElapsed = false;
	bool bTransitionStarted = false;
	bool bTransitionComplete = false;
	bool bLoadMoveApplied = false;
	bool bTeleportSucceeded = false;
	uint64 PhysicsSuspendedFrame = 0;
	TArray<FPrimitiveState> SuspendedPrimitives;
	TArray<FActorState> SuspendedActors;
	TArray<FPlayerMove> PlayerMoves;
#if WITH_DEV_AUTOMATION_TESTS
	TWeakObjectPtr<ACh4_multiGameGameMode> GameModeForTesting;
#endif
};
