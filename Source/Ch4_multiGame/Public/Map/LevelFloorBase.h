#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "LevelFloorBase.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UPostProcessComponent;
class UBoxComponent;

UCLASS()
class CH4_MULTIGAME_API ALevelFloorBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ALevelFloorBase();
	
	// 에디터에서 디테일 수치가 변경되거나 위치 이동 시 자동 호출 (Construction Script 역할)
	virtual void OnConstruction(const FTransform& Transform) override;

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
	
	// 1. 스플라인 선 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline")
	TObjectPtr<USplineComponent> SplineComponent;

	// 2. 스플라인 메시 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	TObjectPtr<UStaticMesh> MeshToUse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;
	
	// 메시의 1개당 길이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	float MeshLength = 100.0f; 
	
	// 동적 생성된 스플라인 메시 컴포넌트들을 담아둘 배열
	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;
	
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
