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

	// 서버에서 셔플된 중간 구역의 인덱스 순서 (클라이언트에 동기화)
	UPROPERTY(ReplicatedUsing = OnRep_MiddleZoneOrder)
	TArray<int32> MiddleZoneOrder;

	// 시작 Zone
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
	ARoadBase* StartRoadActor;

	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
	ALevelFloorBase* StartEnvironmentActor;
	
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
	AZonePostProcessVolume* StartPostProcessVolume;
	
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Start")
	ALandscape* StartLandscapeActor;

	// 중간 Zone
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
	TArray<ARoadBase*> MiddleRoadActors;

	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
	TArray<ALevelFloorBase*> MiddleEnvironmentActors;
	
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
	TArray<AZonePostProcessVolume*> MiddlePostProcessVolumes;
	
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
	TArray<ALandscape*> MiddleLandscapeActors;

	// 끝 Zone
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

protected:
	virtual void BeginPlay() override;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 셔플 인덱스가 서버에서 클라이언트로 넘어왔을 때 실행
	UFUNCTION()
	void OnRep_MiddleZoneOrder();
};