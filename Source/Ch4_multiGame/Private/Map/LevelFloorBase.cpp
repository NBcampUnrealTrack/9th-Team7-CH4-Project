#include "Public/Map/LevelFloorBase.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/Character.h"

ALevelFloorBase::ALevelFloorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates=true;
	SetReplicateMovement(true);

	// 루트 컴포넌트 기본 설정
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComp);

	// 시작점, 끝점 씬 컴포넌트 생성
	StartPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StartPoint"));
	StartPoint->SetupAttachment(RootComponent);

	EndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
	EndPoint->SetupAttachment(RootComponent);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	
	// 컴포넌트 생성 및 루트 부착
	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SplineComponent->SetupAttachment(RootComponent);

	SplineMeshComponent = CreateDefaultSubobject<USplineMeshComponent>(TEXT("SplineMeshComponent"));
	SplineMeshComponent->SetupAttachment(RootComponent);
	
}


void ALevelFloorBase::BeginPlay()
{
	Super::BeginPlay();
	
	CollisionBox->OnComponentBeginOverlap.AddDynamic(
		this,
		&ALevelFloorBase::OnCollisionBoxBeginOverlap
	);
	
}

void ALevelFloorBase::OnCollisionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);

	if (!PlayerCharacter)
	{
		return;
	}
	
}

