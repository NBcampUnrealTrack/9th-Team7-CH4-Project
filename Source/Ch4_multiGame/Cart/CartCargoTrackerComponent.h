// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "CartCargoTrackerComponent.generated.h"

class ACargoActor;

/**
 * Detection-only box for the Cart's load space. Attach to its moving chassis and size in the Editor.
 * The owning Cart implements GameFlowTargetInterface and forwards GetDeliveryScoreSummary here.
 * Cargo membership is server-only; clients read the final score from the existing GameState.
 */
UCLASS(ClassGroup=(Cart), meta=(BlueprintSpawnableComponent))
class CH4_MULTIGAME_API UCartCargoTrackerComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UCartCargoTrackerComponent(const FObjectInitializer& ObjectInitializer);

	/** Server-only snapshot. Returns zero on clients; does not replicate live UI data. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Cart|Cargo")
	int32 GetTrackedCargoCount() const;

	/** Uses ACargoActor::GetDeliveryScore() for each valid, active tracked Cargo. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Cart|Cargo")
	int32 GetTrackedCargoScore() const;

	/** Empty server carts have score data (0 / 0). Clients return bHasScoreData=false. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Cart|Cargo")
	FCh4DeliveryScoreSummary BuildDeliveryScoreSummary() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool HasServerAuthority() const;
	void RegisterCargo(ACargoActor* Cargo);
	void UnregisterCargo(ACargoActor* Cargo);

	UFUNCTION()
	void OnCargoBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCargoEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	/** Weak, unique, unreplicated membership. Const snapshots prune invalid/Lost entries on demand. */
	mutable TSet<TWeakObjectPtr<ACargoActor>> TrackedCargo;
};
