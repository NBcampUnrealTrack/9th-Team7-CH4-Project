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

	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Settings")
	bool bUseAutoArrange = true;

	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Settings", meta = (EditCondition = "bUseAutoArrange"))
	bool bShuffleMiddleZones = true;

	UPROPERTY(ReplicatedUsing = OnRep_bArrangeComplete)
	bool bArrangeComplete = false;

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
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ArrangePlacedZones();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ArrangePlacedZones();

	UFUNCTION()
	void OnRep_bArrangeComplete();
};