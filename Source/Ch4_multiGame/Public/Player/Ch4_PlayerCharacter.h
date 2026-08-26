#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Ch4_PlayerCharacter.generated.h"

UCLASS()
class CH4_MULTIGAME_API ACh4_PlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACh4_PlayerCharacter();
	
protected:
	virtual void BeginPlay() override;
	
public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category="Camera")
	class USpringArmComponent* SpringArmComponent;
	
	UPROPERTY(VisibleAnywhere, Category="Camera")
	class UCameraComponent* CameraComponent;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputMappingContext* InputMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* JumpAction;
	
protected:
	void InputActionMove(const struct FInputActionValue& Value);
	void InputActionLook(const struct FInputActionValue& Value);
	void InputActionJump(const struct FInputActionValue& Value);
};
