#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelManager.generated.h"


class ALevelFloorBase;

UCLASS()
class CH4_MULTIGAME_API ALevelManager : public AActor
{
	GENERATED_BODY()
	
public:	
	ALevelManager();
	
	void ArrangePlacedZones();

	// 에디터 디테일 패널에서 켜고 끌 수 있는 스위치
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Settings")
	bool bUseAutoArrange = true;

	// 중간 맵 무작위 셔플 여부 (OFF 시 MiddleZoneActors에 넣은 순서대로 연결)
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup|Settings", meta = (EditCondition = "bUseAutoArrange"))
	bool bShuffleMiddleZones = true;

	// 에디터 레벨에 배치해 둔 시작 테마 액터
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup")
	ALevelFloorBase* StartZoneActor;

	// 에디터 레벨에 배치해 둔 중간 테마 액터들
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup")
	TArray<ALevelFloorBase*> MiddleZoneActors;

	// 에디터 레벨에 배치해 둔 끝 테마 액터
	UPROPERTY(EditInstanceOnly, Category = "Zone Setup")
	ALevelFloorBase* EndZoneActor;

protected:
	virtual void BeginPlay() override;
	

};
