// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "GameFramework/Actor.h"
#include "FinalDeliveryZone.generated.h"

class UBoxComponent;
class USceneComponent;

/** Server-authoritative overlap zone that requests the final clear evaluation. */
UCLASS()
class AFinalDeliveryZone : public AActor
{
	GENERATED_BODY()

public:
	AFinalDeliveryZone();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> TriggerCollision;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnGamePhaseChanged(ECh4GamePhase NewGamePhase);

	void EvaluateDeliveryTarget(AActor* OtherActor);
	void EvaluateOverlappingTargets();
};
