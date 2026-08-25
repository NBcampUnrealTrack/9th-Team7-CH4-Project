#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Ch4_AnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class CH4_MULTIGAME_API UCh4_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
protected:
	UPROPERTY()
	TObjectPtr<class ACh4_PlayerCharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<class UCharacterMovementComponent> OwnerCharacterMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Velocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float GroundSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bShouldMove;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsFalling;
};
