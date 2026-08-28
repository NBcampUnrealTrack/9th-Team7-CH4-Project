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
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
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
	
	UPROPERTY(EditDefaultsOnly, Category = "Stun")
	TObjectPtr<class UAnimMontage> StunMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Stun")
	TSubclassOf<class UCameraShakeBase> StunCameraShake;
	
protected:
	void InputActionMove(const struct FInputActionValue& Value);
	void InputActionLook(const struct FInputActionValue& Value);
	void InputActionJump(const struct FInputActionValue& Value);
	
	// MovementMode가 변경될 때 호출
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	// 착지했을 때 호출
	virtual void Landed(const FHitResult& Hit) override;
	
	void OnStun();

	void EndStun();

	UFUNCTION()
	void OnRep_IsStunned();
	
	// 낙하 시작 시간
	float FallStartTime = 0.0f;

	// 이 시간 이상 낙하하면 경직
	UPROPERTY(EditDefaultsOnly, Category="Stun")
	float FallStunThreshold = 2.0f;

	// 경직 지속 시간
	UPROPERTY(EditDefaultsOnly, Category="Stun")
	float StunDuration = 1.0f;
	
	// 현재 경직 상태
	UPROPERTY(ReplicatedUsing = OnRep_IsStunned)
	bool bIsStunned = false;
	
	// 경직 종료 타이머
	FTimerHandle StunTimerHandle;

	// HitStop 지연 시간
	UPROPERTY(EditDefaultsOnly, Category = "Stun")
	float HitStopTimeDilation = 0.1f;

	// HitStop 지속 시간
	UPROPERTY(EditDefaultsOnly, Category = "Stun")
	float HitStopDuration = 0.03f;

	FTimerHandle HitStopTimerHandle;

	void StartHitStop();
	void EndHitStop();
};
