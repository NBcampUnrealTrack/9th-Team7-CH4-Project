#pragma once

#include "CoreMinimal.h"
#include "Map/RoadBase.h"
#include "WobbleRoad.generated.h"

UCLASS()
class CH4_MULTIGAME_API AWobbleRoad : public ARoadBase
{
	GENERATED_BODY()

public:
	AWobbleRoad();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

protected:
	void UpdateWobble();

	void ResetSplineToOriginal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// 출렁임 활성화 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_EnableWobble, Category = "Wobble")
	bool bEnableWobble = true;

	// 위아래로 움직이는 최대 높이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (EditCondition = "bEnableWobble", ClampMin = "0.0"))
	float WobbleAmplitude = 100.0f;

	// 물결이 움직이는 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (EditCondition = "bEnableWobble", ClampMin = "0.0"))
	float WobbleSpeed = 2.0f;

	// 물결의 파장
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wobble", meta = (EditCondition = "bEnableWobble", ClampMin = "0.001"))
	float WobbleFrequency = 0.01f;

	// 서버 기준 물결 시작 시간
	UPROPERTY(Replicated)
	float WobbleStartServerTime = 0.0f;

	UFUNCTION()
	void OnRep_EnableWobble();

private:
	// 출렁임이 시작되기 전 원래 Spline 포인트 위치
	UPROPERTY()
	TArray<FVector> OriginalSplinePointLocations;
};