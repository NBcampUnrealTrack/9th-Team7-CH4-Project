#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindObstacle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class CH4_MULTIGAME_API AWindObstacle : public AActor
{
	GENERATED_BODY()

public:
	AWindObstacle();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnWindAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnWindAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ApplyWindToActor(AActor* TargetActor);
	void Tick(float DeltaTime);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wind")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wind")
	TObjectPtr<UBoxComponent> WindArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wind")
	TObjectPtr<UStaticMeshComponent> WindMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind")
	float WindStrength = 500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.01"))
	float WindTickInterval = 0.05f;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> AffectedActors;

	float WindTimer = 0.0f;
};