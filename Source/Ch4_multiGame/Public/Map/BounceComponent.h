#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BounceComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CH4_MULTIGAME_API UBounceComponent : public USceneComponent
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
	// 기본값: X축(Forward)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
	FVector CustomBounceDirection = FVector(1.0f, 0.0f, 0.0f);

	// true: BounceComponent의 로컬 X축 방향 사용
	// false: CustomBounceDirection 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
	bool bUseComponentForwardVector = true;

	
public:	
	// 오버랩/충돌 시 외부(트리거 박스 등)에서 호출해 줄 튕기기 함수
	UFUNCTION(BlueprintCallable, Category = "Bounce")
	void BounceActor(AActor* TargetActor, UPrimitiveComponent* TargetComp = nullptr);
};
