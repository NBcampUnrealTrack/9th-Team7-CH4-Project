#include "Map/WindObstacle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"

AWindObstacle::AWindObstacle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WindArea = CreateDefaultSubobject<UBoxComponent>(TEXT("WindArea"));
	WindArea->SetupAttachment(SceneRoot);
	WindArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WindArea->SetCollisionObjectType(ECC_WorldDynamic);
	WindArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	WindArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	WindArea->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	WindArea->SetGenerateOverlapEvents(true);

	WindMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindMesh"));
	WindMesh->SetupAttachment(SceneRoot);
	WindMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWindObstacle::BeginPlay()
{
	Super::BeginPlay();

	WindArea->OnComponentBeginOverlap.AddDynamic(this, &AWindObstacle::OnWindAreaBeginOverlap);
	WindArea->OnComponentEndOverlap.AddDynamic(this, &AWindObstacle::OnWindAreaEndOverlap);
}

void AWindObstacle::OnWindAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (!AffectedActors.Contains(Character))
		{
			AffectedActors.Add(Character);
		}

		return;
	}

	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		if (!AffectedActors.Contains(OtherActor))
		{
			AffectedActors.Add(OtherActor);
		}
	}
}

void AWindObstacle::OnWindAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(OtherActor))
	{
		return;
	}

	AffectedActors.Remove(OtherActor);
}

void AWindObstacle::ApplyWindToActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector WindForce = GetActorForwardVector() * WindStrength;

	if (ACharacter* Character = Cast<ACharacter>(TargetActor))
	{
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->AddForce(WindForce);
		}

		return;
	}

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent());

	if (PrimitiveComponent && PrimitiveComponent->IsSimulatingPhysics())
	{
		PrimitiveComponent->AddForce(WindForce);
	}
}

void AWindObstacle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	WindTimer += DeltaTime;

	if (WindTimer < WindTickInterval)
	{
		return;
	}

	WindTimer = 0.0f;

	for (int32 Index = AffectedActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* TargetActor = AffectedActors[Index];

		if (!IsValid(TargetActor))
		{
			AffectedActors.RemoveAt(Index);
			continue;
		}

		ApplyWindToActor(TargetActor);
	}
}
