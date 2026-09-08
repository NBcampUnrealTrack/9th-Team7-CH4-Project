#include "UI/MainMenu/Ch4_PanoramicTitleCamera.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ACh4_PanoramicTitleCamera::ACh4_PanoramicTitleCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->SetupAttachment(RootComponent);
	SpringArmComponent->TargetArmLength = ArmLength;
	SpringArmComponent->bDoCollisionTest = false;
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritRoll = false;
	SpringArmComponent->bInheritYaw = true;
	SpringArmComponent->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	CameraComponent->FieldOfView = CameraFOV;
}

void ACh4_PanoramicTitleCamera::BeginPlay()
{
	Super::BeginPlay();

	if (SpringArmComponent)
	{
		SpringArmComponent->TargetArmLength = ArmLength;
		SpringArmComponent->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));
	}
	if (CameraComponent)
	{
		CameraComponent->FieldOfView = CameraFOV;
	}

	if (bAutoActivate)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTargetWithBlend(this, 0.0f);
			PC->bShowMouseCursor = true;
			FInputModeUIOnly InputMode;
			PC->SetInputMode(InputMode);
		}
	}
}

void ACh4_PanoramicTitleCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AddActorLocalRotation(FRotator(0.0f, RotationSpeed * DeltaTime, 0.0f));
}
