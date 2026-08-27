#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelFloorBase.generated.h"

class UBoxComponent;

UCLASS()
class CH4_MULTIGAME_API ALevelFloorBase : public AActor
{
	GENERATED_BODY()

public:
	ALevelFloorBase();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	void DrawBackgroundGuides();

public:
	// 환경의 시작 기준점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment Anchors")
	TObjectPtr<USceneComponent> StartPoint;

	// 환경의 끝 기준점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment Anchors")
	TObjectPtr<USceneComponent> EndPoint;

	// 환경 전체의 충돌 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment")
	TObjectPtr<UBoxComponent> CollisionBox;

	// 배경을 배치할 영역을 보여주는 에디터용 가이드(지울예정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment Guide")
	TObjectPtr<UBoxComponent> BackgroundBounds;

	// 배경 영역을 몇 등분할지 설정(지울예정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment Guide")
	int32 DivisionCount = 7;

	// 시작점의 월드 Transform 반환
	FTransform GetStartPointTransform() const
	{
		return StartPoint->GetComponentTransform();
	}

	// 끝점의 월드 Transform 반환
	FTransform GetEndPointTransform() const
	{
		return EndPoint->GetComponentTransform();
	}

	UFUNCTION()
	void OnCollisionBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};