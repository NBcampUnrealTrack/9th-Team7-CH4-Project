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

public:
	ARoadBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void OnConstruction(const FTransform& Transform) override;
    
	// [수정] 클라이언트 및 서버 공용 PCG 생성 함수
	UFUNCTION(BlueprintCallable, Category = "PCG")
	void GenerateObstacles(int32 InRandomSeed, bool bIsForce = true);

	void DrawBackgroundGuides();
	void SetRoadComponentsStatic();
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road Anchors")
	TObjectPtr<USceneComponent> StartPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road Anchors")
	TObjectPtr<USceneComponent> EndPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	TObjectPtr<UStaticMesh> MeshToUse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	float MeshLength = 100.0f;

	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;

	FTransform GetStartPointTransform() const { return StartPoint->GetComponentTransform(); }
	FTransform GetEndPointTransform() const { return EndPoint->GetComponentTransform(); }
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PCG")
	TObjectPtr<UPCGComponent> PCGComponent;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	int32 RandomSeed = 0;
};