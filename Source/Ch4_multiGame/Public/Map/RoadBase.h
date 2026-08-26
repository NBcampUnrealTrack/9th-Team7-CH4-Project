#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "RoadBase.generated.h"


class USplineComponent;

UCLASS()
class CH4_MULTIGAME_API ARoadBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ARoadBase();
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
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
};
