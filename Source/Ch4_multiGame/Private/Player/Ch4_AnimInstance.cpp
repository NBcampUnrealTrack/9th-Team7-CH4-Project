#include "Player/Ch4_AnimInstance.h"

#include "Player/Ch4_PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UCh4_AnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwnerCharacter = Cast<ACh4_PlayerCharacter>(GetOwningActor());
	if (IsValid(OwnerCharacter) == true)
	{
		OwnerCharacterMovementComponent = OwnerCharacter->GetCharacterMovement();
	}
}

void UCh4_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (IsValid(OwnerCharacter) == false || IsValid(OwnerCharacterMovementComponent) == false)
	{
		return;
	}

	Velocity = OwnerCharacterMovementComponent->Velocity;
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Length();
	bShouldMove = ((OwnerCharacterMovementComponent->GetCurrentAcceleration().IsNearlyZero()) == false)
	&& (3.f < GroundSpeed);
	bIsFalling = OwnerCharacterMovementComponent->IsFalling();
	bIsGrabbing = OwnerCharacter->GrabbedComponent != nullptr;
}

