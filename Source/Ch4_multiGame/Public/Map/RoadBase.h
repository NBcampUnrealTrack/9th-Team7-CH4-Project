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
    
	UFUNCTION(BlueprintCallable, Category = "PCG")
	void GenerateObstacles(int32 InRandomSeed);

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