#include "Map/ProjectileEnemy.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Map/EnemyProjectile.h"

AProjectileEnemy::AProjectileEnemy()
{
	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
	DetectionSphere->SetupAttachment(RootComponent);
	DetectionSphere->SetSphereRadius(500.0f);
	
	Gun = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gun"));
	Gun->SetupAttachment(GetMesh(), TEXT("GunSocket"));
	
	ProjectileSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(Gun);
}

void AProjectileEnemy::BeginPlay()
{
	Super::BeginPlay();

	DetectionSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&AProjectileEnemy::OnDetectionBeginOverlap
	);
	
	DetectionSphere->OnComponentEndOverlap.AddDynamic(
	this,
	&AProjectileEnemy::OnDetectionEndOverlap
	);
}

void AProjectileEnemy::OnDetectionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);

	if (PlayerCharacter && PlayerCharacter->IsPlayerControlled())
	{
		TargetCharacter = PlayerCharacter;
		bIsAiming = true;

		GetWorld()->GetTimerManager().SetTimer(
		FireTimerHandle,
		this,
		&AProjectileEnemy::StartFiring,
		FirstFireDelay,
		false
		);
		
		UE_LOG(LogTemp, Warning, TEXT("Player entered enemy detection range!"));
	}
}

void AProjectileEnemy::OnDetectionEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor == TargetCharacter)
	{
		TargetCharacter = nullptr;
		bIsAiming = false;

		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
		
		UE_LOG(LogTemp, Warning, TEXT("Player left enemy detection range!"));
	}
}

void AProjectileEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// [반동 감쇄 로직 추가]
	// 매 프레임 RecoilPitch를 0.0f를 향해 부드럽게 보간(감쇄)시킵니다.
	// 15.0f는 복귀 속도입니다. (숫자가 크면 더 빠르게 돌아옵니다)
	RecoilPitch = FMath::FInterpTo(RecoilPitch, 0.0f, DeltaTime, 15.0f);
	
	// 2. 위치 반동 감쇄 (VInterpTo 사용)
	RecoilLocationOffset = FMath::VInterpTo(RecoilLocationOffset, FVector::ZeroVector, DeltaTime, 12.0f);
	
	if (!TargetCharacter)
	{
		return;
	}

	if (bCanOrientToTarget)
	{
		FVector BodyDirection = TargetCharacter->GetActorLocation() - GetActorLocation();
		BodyDirection.Z = 0.0f;

		if (!BodyDirection.IsNearlyZero())
		{
			SetActorRotation(BodyDirection.Rotation());
		}
	}

	FVector ShoulderLocation = GetMesh()->GetBoneLocation(TEXT("Right-shoulder"));
	FVector AimDirection = TargetCharacter->GetActorLocation() - ShoulderLocation;
	FVector LocalAimDirection = GetActorTransform().InverseTransformVectorNoScale(AimDirection);

	float AimPitch = FMath::RadiansToDegrees(
		FMath::Atan2(
			LocalAimDirection.Z,
			FVector2D(LocalAimDirection.X, LocalAimDirection.Y).Size()
		)
	);

	AimRotation = FRotator(AimPitch+RecoilPitch, 0.0f, 0.0f);
}

void AProjectileEnemy::FireProjectile()
{
	if (!ProjectileClass || !ProjectileSpawnPoint)
	{
		return;
	}

	FVector SpawnLocation = ProjectileSpawnPoint->GetComponentLocation();
	FRotator SpawnRotation = ProjectileSpawnPoint->GetComponentRotation();

	GetWorld()->SpawnActor<AEnemyProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation
	);
	
	ApplyRecoil();
}

void AProjectileEnemy::StartFiring()
{
	if (!TargetCharacter)
	{
		return;
	}
	
	FireProjectile();

	GetWorld()->GetTimerManager().SetTimer(
		FireTimerHandle,
		this,
		&AProjectileEnemy::FireProjectile,
		FireInterval,
		true
	);
}

void AProjectileEnemy::ApplyRecoil()
{
	RecoilPitch = RecoilAmount;
	
	// 사격 시 위치 반동 추가 (캐릭터/총 기준 뒤쪽으로 팍 밀림)
	// Local 기준으로 뒤로 밀리도록 X축(또는 구동 방향)에 마이너스 값을 더합니다.
	RecoilLocationOffset = FVector(RecoilBackAmount, 0.0f, 0.0f);
	
	bCanOrientToTarget = false; // 회전 멈춤

	// 기존 타이머가 작동 중이라면 초기화
	GetWorld()->GetTimerManager().ClearTimer(RotationDelayTimerHandle);

	// PostFireRotationDelay 초 후에 ResetRotation 함수 호출
	GetWorld()->GetTimerManager().SetTimer(
		RotationDelayTimerHandle,
		this,
		&AProjectileEnemy::ResetRotation,
		PostFireRotationDelay,
		false
	);

}

void AProjectileEnemy::ResetRotation()
{
	bCanOrientToTarget = true; // 다시 회전 가능하도록 변경
}
