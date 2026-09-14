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
    
    // 서버에서 생성하여 클라이언트로 복제될 PCG 시드
    UPROPERTY(ReplicatedUsing = OnRep_PCGSeed)
    int32 PCGSeed = 0;

    UFUNCTION()
    void OnRep_PCGSeed();

    // 모든 도로에 PCG 생성 명령을 전달하는 함수
    void TriggerPCGGeneration();

protected:
    virtual void BeginPlay() override;

private:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_MiddleZoneOrder();

    void MoveTaggedActorsWithRoad(ARoadBase* Road, const FTransform& OriginalRoadTransform, const FTransform& FinalRoadTransform);

    bool IsLevelManagerManagedActor(const AActor* Actor) const;
};