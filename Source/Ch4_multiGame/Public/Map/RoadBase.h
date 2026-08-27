#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
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
	
	void DrawBackgroundGuides();
	
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
	
	// 배경을 배치할 영역을 보여주는 에디터용 가이드(지울예정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment Guide")
	TObjectPtr<UBoxComponent> BackgroundBounds;

	// 배경 영역을 몇 등분할지 설정(지울예정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment Guide")
	int32 DivisionCount = 7;
	
	// 배경 영역을 등분 표시할지 설정(지울예정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment Guide")
	bool bShowBackgroundGuides = true;
	
	// 배경 영역 표시할지 설정(지울예정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment Guide")
	bool bShowBackgroundBounds = true;
	
};
