#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelFloorBase.generated.h"

class UPostProcessComponent;
class UBoxComponent;

UCLASS()
class CH4_MULTIGAME_API ALevelFloorBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ALevelFloorBase();

protected:
	virtual void BeginPlay() override;

public:	
	// 시작점 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Anchors")
	TObjectPtr<USceneComponent> StartPoint;

	// 끝점 컴포넌트 (다음 구역이 붙을 위치)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Anchors")
	TObjectPtr<USceneComponent> EndPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
	TObjectPtr<UBoxComponent> CollisionBox;
	
	
	UFUNCTION()
	void OnCollisionBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	
	// 시작점의 월드 Transform(위치/회전)을 반환하는 함수
	FTransform GetStartPointTransform() const { return StartPoint->GetComponentTransform(); }

	// 끝점의 월드 Transform(위치/회전)을 반환하는 함수
	FTransform GetEndPointTransform() const { return EndPoint->GetComponentTransform(); }
};
