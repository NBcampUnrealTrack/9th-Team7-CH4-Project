#include "Player/Ch4_PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

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
}

void ACh4_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACh4_PlayerCharacter::InputActionMove);
		if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACh4_PlayerCharacter::InputActionLook);
		if (JumpAction) EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &ACh4_PlayerCharacter::InputActionJump);
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
    	
	Jump();
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

	// 다시 걷기 가능
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACh4_PlayerCharacter::OnRep_IsStunned()
{
	// 서버에서 복제된 값이 클라이언트에 도착하는 순간 특별한 작업을 해야 할 때 사용
	// 이펙트, 사운드, 애니메이션 등등
	
	if (bIsStunned)
	{
		if (StunMontage)
		{
			PlayAnimMontage(StunMontage);
		}

		if (IsLocallyControlled())
		{
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
		if (StunMontage)
		{
			StopAnimMontage(StunMontage);
		}
	}
}
