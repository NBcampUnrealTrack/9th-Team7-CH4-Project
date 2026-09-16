#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelManager.generated.h"

class ARoadBase;
class ALevelFloorBase;
class AFinalDeliveryZone;
class AZonePostProcessVolume;
class ALandscape;

UCLASS()
class CH4_MULTIGAME_API ALevelManager : public AActor
{
    GENERATED_BODY()

public:
    ALevelManager();

    void ArrangePlacedZones();

    UPROPERTY(Replicated, EditInstanceOnly, Category = "Zone Setup|Settings")
    bool bUseAutoArrange = true;

    UPROPERTY(Replicated, EditInstanceOnly, Category = "Zone Setup|Settings", meta = (EditCondition = "bUseAutoArrange"))
    bool bShuffleMiddleZones = true;

    UPROPERTY(ReplicatedUsing = OnRep_MiddleZoneOrder)
    TArray<int32> MiddleZoneOrder;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
    ARoadBase* StartRoadActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
    ALevelFloorBase* StartEnvironmentActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
    AZonePostProcessVolume* StartPostProcessVolume;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
    ALandscape* StartLandscapeActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
    TArray<ARoadBase*> MiddleRoadActors;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
    TArray<ALevelFloorBase*> MiddleEnvironmentActors;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
    TArray<AZonePostProcessVolume*> MiddlePostProcessVolumes;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
    TArray<ALandscape*> MiddleLandscapeActors;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
    ARoadBase* EndRoadActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
    ALevelFloorBase* EndEnvironmentActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
    ALandscape* EndLandscapeActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
    AFinalDeliveryZone* FinalDeliveryZoneActor;

    UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
    AZonePostProcessVolume* EndPostProcessVolume;
    
    UPROPERTY(Replicated)
    int32 PCGSeed = 0;

    void TriggerPCGGeneration();

protected:
    virtual void BeginPlay() override;

private:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_MiddleZoneOrder();

    void MoveTaggedActorsWithRoad(ARoadBase* Road, const FTransform& OriginalRoadTransform, const FTransform& FinalRoadTransform);
    bool IsLevelManagerManagedActor(const AActor* Actor) const;
    
    FTimerHandle PCGGenerateTimerHandle;
};
