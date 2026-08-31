#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelManager.generated.h"

class ARoadBase;
class ALevelFloorBase;

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

	// 중간 Zone
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
	TArray<ARoadBase*> MiddleRoadActors;

	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Middle")
	TArray<ALevelFloorBase*> MiddleEnvironmentActors;

	// 끝 Zone
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
	ARoadBase* EndRoadActor;

	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|End")
	ALevelFloorBase* EndEnvironmentActor;

protected:
	virtual void BeginPlay() override;

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 셔플 인덱스가 서버에서 클라이언트로 넘어왔을 때 실행
	UFUNCTION()
	void OnRep_MiddleZoneOrder();
};