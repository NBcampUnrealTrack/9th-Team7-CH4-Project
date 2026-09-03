#include "Player/Ch4_PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/EmotionDataAsset.h"
#include "Player/GrabbableInterface.h"
#include "Engine/OverlapResult.h"

ACh4_PlayerCharacter::ACh4_PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->TargetArmLength = 300.0f;
	SpringArmComponent->bUsePawnControlRotation = true;
	
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);
	CameraComponent->bUsePawnControlRotation = false;
	
	bUseControllerRotationYaw = false; // 카메라 이동에 따라 캐릭터 몸이 같이 움직이지 않음
	
	HatMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMeshComponent"));
	HatMeshComponent->SetupAttachment(GetMesh(), HatSocketName);
	HatMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACh4_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (InputMappingContext)
			{
				Subsystem->AddMappingContext(InputMappingContext, 0);
			}
		}
	}
}

void ACh4_PlayerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, bIsStunned);
	DOREPLIFETIME(ThisClass, GrabbedComponent);
}

void ACh4_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACh4_PlayerCharacter::InputActionMove);
		if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACh4_PlayerCharacter::InputActionLook);
		if (JumpAction) EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionJump);
		if (Emote1Action) EIC->BindAction(Emote1Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote1);
		if (Emote2Action) EIC->BindAction(Emote2Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote2);
		if (Emote3Action) EIC->BindAction(Emote3Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote3);
		if (Emote4Action) EIC->BindAction(Emote4Action, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionEmote4);
		if (GrabAction) EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &ACh4_PlayerCharacter::InputActionGrab);
		if (SkinChangeAction) EIC->BindAction(SkinChangeAction, ETriggerEvent::Started, this, &ACh4_PlayerCharacter::InputActionSkinChange);
	}
}

void ACh4_PlayerCharacter::SetHatMesh(class UStaticMesh* NewHat)
{
	if (HatMeshComponent == nullptr)
	{
		return;
	}

	if (NewHat)
	{
		HatMeshComponent->SetStaticMesh(NewHat);
		HatMeshComponent->SetVisibility(true);
	}
	else
	{
		HatMeshComponent->SetStaticMesh(nullptr);
		HatMeshComponent->SetVisibility(false);
	}
}

void ACh4_PlayerCharacter::InputActionMove(const struct FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	const FVector2D MoveVec = Value.Get<FVector2D>();
	
	if (Controller == nullptr)
	{
		return;
	}

	if (MoveVec.IsNearlyZero() == false)
	{
		InterruptEmotionMontage();
	}

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MoveVec.Y);
	AddMovementInput(RightDirection, MoveVec.X);
}

void ACh4_PlayerCharacter::InputActionLook(const struct FInputActionValue& Value)
{
	const FVector2D LookVec = Value.Get<FVector2D>();
	
	AddControllerYawInput(LookVec.X);
	AddControllerPitchInput(LookVec.Y);
}

void ACh4_PlayerCharacter::InputActionJump(const FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}
	
	if (GetCharacterMovement()->IsFalling())
	{
		return;
    }

	InterruptEmotionMontage();
    	
	Jump();
}

void ACh4_PlayerCharacter::InputActionEmote1(const struct FInputActionValue& Value)
{
	PlayEmotion(EEmotionType::Emote1);
}

void ACh4_PlayerCharacter::InputActionEmote2(const struct FInputActionValue& Value)
{
	PlayEmotion(EEmotionType::Emote2);
}

void ACh4_PlayerCharacter::InputActionEmote3(const struct FInputActionValue& Value)
{
	PlayEmotion(EEmotionType::Emote3);
}

void ACh4_PlayerCharacter::InputActionEmote4(const struct FInputActionValue& Value)
{
	PlayEmotion(EEmotionType::Emote4);
}

void ACh4_PlayerCharacter::InputActionGrab(const FInputActionValue& Value)
{
	if (bIsStunned)
	{
		return;
	}

	if (bIsGrabActionInProgress)
	{
		return; // 몽타주 재생 중엔 입력 무시
	}
	
	bIsGrabActionInProgress = true;
	
	if (GrabbedComponent != nullptr)
	{
		if (HasAuthority())
		{
			MulticastRPC_PlayGrabReleaseMontage();
		}
		else
		{
			ServerRPC_PlayGrabReleaseMontage();
		}
		return;
	}

	if (HasAuthority())
	{
		MulticastRPC_PlayGrabMontage();
	}
	else
	{
		ServerRPC_PlayGrabMontage();
	}
}

void ACh4_PlayerCharacter::InputActionSkinChange(const struct FInputActionValue& Value)
{
	
}

void ACh4_PlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// Falling 상태로 진입했을 때
	if (GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		FallStartTime = GetWorld()->GetTimeSeconds();
	}
}

void ACh4_PlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (HasAuthority() == false)
	{
		return;
	}
	
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// 낙하한 시간 = 착지 시간 - 낙하 시작 시간
	const float FallDuration = CurrentTime - FallStartTime;

	// 설정한 시간 이상 낙하했다면 경직
	if (FallDuration >= FallStunThreshold)
	{
		OnStun();
	}
}

void ACh4_PlayerCharacter::OnStun()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	if (bIsStunned)
	{
		return;
	}
	
	bIsStunned = true;
	
	// 서버(호스트)는 RepNotify가 자동으로 안 불리므로 직접 호출
    OnRep_IsStunned();
	
	// 현재 이동 중이었다면 즉시 정지
	GetCharacterMovement()->StopMovementImmediately();

	// 이동 비활성화
	GetCharacterMovement()->DisableMovement();

	// 기존 타이머 제거
	GetWorldTimerManager().ClearTimer(StunTimerHandle);

	// StunDuration 후 경직 종료
	GetWorldTimerManager().SetTimer(
		StunTimerHandle,
		this,
		&ACh4_PlayerCharacter::EndStun,
		StunDuration,
		false);
}

void ACh4_PlayerCharacter::EndStun()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	bIsStunned = false;
	
	// 서버(호스트)는 RepNotify가 자동으로 안 불리므로 직접 호출
	OnRep_IsStunned();
	
	// 다시 걷기 가능
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACh4_PlayerCharacter::OnRep_IsStunned()
{
	// 서버에서 복제된 bIsStunned 값이 클라이언트에 도착하는 순간 특별한 작업을 해야 할 때 사용
	// 이펙트, 사운드, 애니메이션 등등
	
	if (bIsStunned)
	{
		// 모든 클라이언트에서 경직 애니메이션 재생
		if (StunMontage)
		{
			PlayAnimMontage(StunMontage);
		}

		// 경직된 캐릭터를 직접 조작하는 클라이언트에서만 HitStop, CameraShake
		if (IsLocallyControlled())
		{
			StartHitStop();
			
			if (APlayerController* PC =	Cast<APlayerController>(GetController()))
			{
				if (StunCameraShake)
				{
					PC->ClientStartCameraShake(StunCameraShake);
				}
			}
		}
	}
	else
	{
		// HitStop 중이었다면 자신의 클라이언트에서만 복구
		if (IsLocallyControlled())
		{
			GetWorldTimerManager().ClearTimer(HitStopTimerHandle);

			CustomTimeDilation = 1.0f;
		}

		if (StunMontage)
		{
			StopAnimMontage(StunMontage);
		}
	}
}

void ACh4_PlayerCharacter::StartHitStop()
{
	// 경직 캐릭터의 시간만 살짝 지연
	CustomTimeDilation = HitStopTimeDilation;
	
	// 기존 HitStop 타이머가 있다면 제거
	GetWorldTimerManager().ClearTimer(HitStopTimerHandle);

	// HitStop 종료 타이머
	GetWorldTimerManager().SetTimer(
		HitStopTimerHandle,
		this,
		&ACh4_PlayerCharacter::EndHitStop,
		HitStopDuration,
		false
	);
}

void ACh4_PlayerCharacter::EndHitStop()
{
	// 시간 속도를 원래대로 복구
	CustomTimeDilation = 1.0f;
}

UAnimMontage* ACh4_PlayerCharacter::FindEmotionMontage(EEmotionType EmotionType) const
{
	if (EmotionDataAsset == nullptr)
	{
		return nullptr;
	}

	for (const FEmotionData& EmotionData : EmotionDataAsset->EmotionDataList)
	{
		if (EmotionData.EmotionType == EmotionType)
		{
			return EmotionData.EmoteMontage;
		}
	}

	return nullptr;
}

void ACh4_PlayerCharacter::PlayEmotion(EEmotionType EmotionType)
{
	if (HasAuthority())
	{
		MulticastRPC_PlayEmotion(EmotionType);
	}
	else
	{
		ServerRPC_PlayEmotion(EmotionType);
	}
}

void ACh4_PlayerCharacter::ServerRPC_PlayEmotion_Implementation(EEmotionType EmotionType)
{
	MulticastRPC_PlayEmotion(EmotionType);
}

void ACh4_PlayerCharacter::MulticastRPC_PlayEmotion_Implementation(EEmotionType EmotionType)
{
	UAnimMontage* EmotionMontage = FindEmotionMontage(EmotionType);

	if (EmotionMontage == nullptr)
	{
		return;
	}

	PlayAnimMontage(EmotionMontage);
}

void ACh4_PlayerCharacter::InterruptEmotionMontage()
{
	StopEmotionMontages();

	if (HasAuthority())
	{
		MulticastRPC_InterruptEmotionMontage();
	}
	else
	{
		ServerRPC_InterruptEmotionMontage();
	}
}

void ACh4_PlayerCharacter::StopEmotionMontages(float BlendOutTime)
{
	if (EmotionDataAsset == nullptr)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr)
	{
		return;
	}

	for (const FEmotionData& EmotionData : EmotionDataAsset->EmotionDataList)
	{
		if (EmotionData.EmoteMontage)
		{
			AnimInstance->Montage_Stop(BlendOutTime, EmotionData.EmoteMontage);
		}
	}
}

void ACh4_PlayerCharacter::ServerRPC_InterruptEmotionMontage_Implementation()
{
	MulticastRPC_InterruptEmotionMontage();
}

void ACh4_PlayerCharacter::MulticastRPC_InterruptEmotionMontage_Implementation()
{
	StopEmotionMontages();
}

void ACh4_PlayerCharacter::OnRep_GrabbedComponent()
{
	// 클라이언트에서 잡음/놓음 시각 효과 처리용 (필요 시 사용)
}

void ACh4_PlayerCharacter::OnGrabNotify()
{
	if (HasAuthority() == false && IsLocallyControlled() == false)
	{
		return; // 서버 또는 본인 조종 클라이언트만 판정
	}
	
	bIsGrabActionInProgress = false;
	
	if (GrabbedComponent != nullptr)
	{
		return;
	}
	
	FVector SocketLoc = GetMesh()->GetSocketLocation(GrabSocketName);
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(GrabRadius);
	
	bool bHit = GetWorld()->OverlapMultiByChannel(
		Overlaps, SocketLoc, FQuat::Identity, ECC_PhysicsBody, Sphere);
	
	if (bHit == false)
	{
		return;
	}
	
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* HitActor = Result.GetActor();
		if (HitActor && HitActor->Implements<UGrabbableInterface>())
		{
			IGrabbableInterface* GI = Cast<IGrabbableInterface>(HitActor);
			ServerRPC_AttachGrab(GI->GetGrabbableComponent());
			break;
		}
	}
}

void ACh4_PlayerCharacter::ServerRPC_AttachGrab_Implementation(UPrimitiveComponent* TargetComponent)
{
	if (TargetComponent == nullptr || GrabbedComponent != nullptr)
	{
		return;
	}

	GrabbedComponent = TargetComponent;
	MulticastRPC_AttachGrab(TargetComponent);
}

void ACh4_PlayerCharacter::MulticastRPC_AttachGrab_Implementation(UPrimitiveComponent* TargetComponent)
{
	if (TargetComponent == nullptr)
	{
		return;
	}

	TargetComponent->SetSimulatePhysics(false);
	TargetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TargetComponent->AttachToComponent(
	   GetMesh(),
	   FAttachmentTransformRules::SnapToTargetNotIncludingScale,
	   GrabSocketName);
}

void ACh4_PlayerCharacter::ServerRPC_ReleaseGrab_Implementation()
{
	if (GrabbedComponent == nullptr)
	{
		return;
	}
	
	MulticastRPC_ReleaseGrab(GrabbedComponent);
	GrabbedComponent = nullptr;
}

void ACh4_PlayerCharacter::MulticastRPC_ReleaseGrab_Implementation(UPrimitiveComponent* TargetComponent)
{
	TargetComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	TargetComponent->SetSimulatePhysics(true);
	TargetComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void ACh4_PlayerCharacter::ServerRPC_PlayGrabMontage_Implementation()
{
	MulticastRPC_PlayGrabMontage();
}

void ACh4_PlayerCharacter::MulticastRPC_PlayGrabMontage_Implementation()
{
	if (GrabMontage)
	{
		PlayAnimMontage(GrabMontage);
	}
}

void ACh4_PlayerCharacter::OnGrabReleaseNotify()
{
	if (HasAuthority() == false && IsLocallyControlled() == false)
	{
		return;
	}

	bIsGrabActionInProgress = false;
	
	if (GrabbedComponent != nullptr)
	{
		ServerRPC_ReleaseGrab();
	}
}

void ACh4_PlayerCharacter::ServerRPC_PlayGrabReleaseMontage_Implementation()
{
	MulticastRPC_PlayGrabReleaseMontage();
}

void ACh4_PlayerCharacter::MulticastRPC_PlayGrabReleaseMontage_Implementation()
{
	if (GrabReleaseMontage)
	{
		PlayAnimMontage(GrabReleaseMontage);
	}
}
