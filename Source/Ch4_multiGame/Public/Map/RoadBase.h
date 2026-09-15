#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "PCGComponent.h"
#include "RoadBase.generated.h"

class UBoxComponent;
class USplineComponent;

UCLASS()
class CH4_MULTIGAME_API ARoadBase : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	ARoadBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	
	// LevelManager가 호출해 줄 런타임 PCG 장애물 생성 함수
	UFUNCTION(BlueprintCallable, Category = "PCG")
	void GenerateObstacles(int32 InRandomSeed);

	void DrawBackgroundGuides();

	// 모든 Road 컴포넌트를 WorldStatic + Static으로 변경
	void SetRoadComponentsStatic();
	
	// 도로 시작점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road Anchors")
	TObjectPtr<USceneComponent> StartPoint;

	// 도로 끝점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road Anchors")
	TObjectPtr<USceneComponent> EndPoint;

	// 도로 형태를 결정하는 Spline
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline")
	TObjectPtr<USplineComponent> SplineComponent;

	// Spline에 사용할 도로 메시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	TObjectPtr<UStaticMesh> MeshToUse;

	// 메시의 진행 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

	// Spline Mesh 하나의 길이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	float MeshLength = 100.0f;

	// 생성된 Spline Mesh들을 저장
	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;

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
	
	
	// 도로 생성 시 호출할 PCG 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PCG")
	TObjectPtr<UPCGComponent> PCGComponent;
	
	// PCG가 읽어갈 수 있도록 EditAnywhere, BlueprintReadWrite 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	int32 RandomSeed = 0;
};