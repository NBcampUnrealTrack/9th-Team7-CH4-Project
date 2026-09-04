#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4_PlayerCharacter.generated.h"

UCLASS()
class CH4_MULTIGAME_API ACh4_PlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACh4_PlayerCharacter();

	/** Applies appearance and animation data without replacing movement, camera, input, or possession. */
	void ApplyCharacterType(ECh4CharacterType CharacterType);
	
protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
public:
	virtual void PawnClientRestart() override;
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
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* Emote1Action;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* Emote2Action;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* Emote3Action;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* Emote4Action;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* GrabAction;
	
	UPROPERTY(EditDefaultsOnly, Category="Input")
    class UInputAction* SkinChangeAction;
	
	UPROPERTY(EditDefaultsOnly, Category="Anim")
	TObjectPtr<class UAnimMontage> StunMontage;

	UPROPERTY(EditDefaultsOnly, Category="Stun")
	TSubclassOf<class UCameraShakeBase> StunCameraShake;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Emotion")
	TObjectPtr<class UEmotionDataAsset> EmotionDataAsset;
	
	UPROPERTY(EditDefaultsOnly, Category="Anim")
	TObjectPtr<class UAnimMontage> GrabMontage;
	
	UPROPERTY(EditDefaultsOnly, Category="Anim")
	TObjectPtr<class UAnimMontage> GrabReleaseMontage;
	
	UPROPERTY(ReplicatedUsing = OnRep_GrabbedComponent)
	TObjectPtr<class UPrimitiveComponent> GrabbedComponent;
	
	void OnGrabNotify();
	void OnGrabReleaseNotify();
	
	UPROPERTY(VisibleAnywhere, Category="Hat")
	TObjectPtr<class UStaticMeshComponent> HatMeshComponent;

	UPROPERTY(EditDefaultsOnly, Category="Hat")
	FName HatSocketName = TEXT("HatSocket");

	UFUNCTION(BlueprintCallable, Category="Hat")
	void SetHatMesh(class UStaticMesh* NewHat);
	
protected:
	void AddPlayerInputMappingContext();
	void ApplyCharacterTypeFromPlayerState();

	void InputActionMove(const struct FInputActionValue& Value);
	void InputActionLook(const struct FInputActionValue& Value);
	void InputActionJump(const struct FInputActionValue& Value);
	void InputActionEmote1(const struct FInputActionValue& Value);
	void InputActionEmote2(const struct FInputActionValue& Value);
	void InputActionEmote3(const struct FInputActionValue& Value);
	void InputActionEmote4(const struct FInputActionValue& Value);
	void InputActionGrab(const struct FInputActionValue& Value);
	void InputActionSkinChange(const struct FInputActionValue& Value);
	
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
	
	UAnimMontage* FindEmotionMontage(EEmotionType EmotionType) const;
	
	void PlayEmotion(EEmotionType EmotionType);

	void InterruptEmotionMontage();

	void StopEmotionMontages(float BlendOutTime = 0.1f);

	UFUNCTION(Server, Reliable)
	void ServerRPC_PlayEmotion(EEmotionType EmotionType);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayEmotion(EEmotionType EmotionType);

	UFUNCTION(Server, Reliable)
	void ServerRPC_InterruptEmotionMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_InterruptEmotionMontage();
	
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	FName GrabSocketName = TEXT("GrabSocket");
	
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	float GrabRadius = 50.0f;
	
	// 그랩/릴리즈 몽타주 재생 중 여부 (입력 잠금용)
	bool bIsGrabActionInProgress = false;
	
	UFUNCTION()
	void OnRep_GrabbedComponent();

	UFUNCTION(Server, Reliable)
	void ServerRPC_AttachGrab(UPrimitiveComponent* TargetComponent);

	UFUNCTION(Server, Reliable)
	void ServerRPC_ReleaseGrab();
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_AttachGrab(UPrimitiveComponent* TargetComponent);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_ReleaseGrab(UPrimitiveComponent* TargetComponent);
	
	UFUNCTION(Server, Reliable)
	void ServerRPC_PlayGrabMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayGrabMontage();
	
	UFUNCTION(Server, Reliable)
	void ServerRPC_PlayGrabReleaseMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPC_PlayGrabReleaseMontage();
};
