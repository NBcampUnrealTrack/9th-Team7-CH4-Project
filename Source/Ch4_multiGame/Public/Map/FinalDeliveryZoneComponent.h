#pragma once
#include "CoreMinimal.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Components/BoxComponent.h"
#include "FinalDeliveryZoneComponent.generated.h"

UCLASS(ClassGroup=(GameFlow), meta=(BlueprintSpawnableComponent))
class CH4_MULTIGAME_API UFinalDeliveryZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UFinalDeliveryZoneComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnGamePhaseChanged(ECh4GamePhase NewGamePhase);

	void EvaluateDeliveryTarget(AActor* OtherActor);
	void EvaluateOverlappingTargets();
};