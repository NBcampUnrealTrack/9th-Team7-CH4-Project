#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "BounceComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CH4_MULTIGAME_API UBounceComponent : public UArrowComponent
{
	GENERATED_BODY()

public:
	UBounceComponent();

	UFUNCTION()
	void OnParentBeginOverlap(
	   UPrimitiveComponent* OverlappedComponent,
	   AActor* OtherActor,
	   UPrimitiveComponent* OtherComp,
	   int32 OtherBodyIndex,
	   bool bFromSweep,
	   const FHitResult& SweepResult);

protected:
	virtual void BeginPlay() override;

	// 튕겨내는 세기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
	float BounceForce = 2000.0f;

	// 바운스 방향을 직접 지정할 때 사용하는 로컬 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
	FVector CustomBounceDirection = FVector(1.0f, 0.0f, 0.0f);

	// true: BounceComponent의 로컬 X축(Forward) 방향 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
	bool bUseComponentForwardVector = true;

public:
	// 바운스 실행 함수 (서버 권한이 있을 때만 동작)
	UFUNCTION(BlueprintCallable, Category = "Bounce")
	void BounceActor(AActor* TargetActor, UPrimitiveComponent* TargetComp = nullptr);

private:
	// 실제 바운스 로직 수행
	void ExecuteBounce(AActor* TargetActor, UPrimitiveComponent* TargetComp);
};