#include "Public/Map/LevelFloorBase.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ALevelFloorBase::ALevelFloorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// 루트 컴포넌트
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComp);
	
	// 환경 충돌 영역
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);

	// 배경 배치 영역 가이드(지울예정)
	BackgroundBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("BackgroundBounds"));
	BackgroundBounds->SetupAttachment(RootComponent);
	BackgroundBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 모든 환경에서 사용할 기본 배경 영역(지울예정)
	BackgroundBounds->SetBoxExtent(FVector(20000.0f, 20000.0f, 48000.0f));
}

void ALevelFloorBase::BeginPlay()
{
	Super::BeginPlay();

	CollisionBox->OnComponentBeginOverlap.AddDynamic(
		this,
		&ALevelFloorBase::OnCollisionBoxBeginOverlap);
	
	CollisionBox->OnComponentEndOverlap.AddDynamic(
	   this,
	   &ALevelFloorBase::OnCollisionBoxEndOverlap);
	
	//(지울예정)
	DrawBackgroundGuides();
}

void ALevelFloorBase::OnCollisionBoxBeginOverlap(
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
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	// 변수로 지정된 TargetTag를 사용해 액터 검색
	TArray<AActor*> TargetActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TargetTag, TargetActors);

	for (AActor* Actor : TargetActors)
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
		}
	}
}

void ALevelFloorBase::OnCollisionBoxEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
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

	// 이탈할 때도 내 로컬 플레이어인지 확인
	if (!PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	// 변수로 지정된 TargetTag를 사용해 액터 검색
	TArray<AActor*> TargetActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TargetTag, TargetActors);

	for (AActor* Actor : TargetActors)
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
}

void ALevelFloorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	//(지울예정)
	DrawBackgroundGuides();
}

//(지울예정)
void ALevelFloorBase::DrawBackgroundGuides()
{
	if (!BackgroundBounds || DivisionCount <= 1)
	{
		return;
	}

	FlushPersistentDebugLines(GetWorld());

	const FVector BoundsCenter = BackgroundBounds->GetComponentLocation();
	const FVector BoundsExtent = BackgroundBounds->GetScaledBoxExtent();

	const float MinZ = BoundsCenter.Z - BoundsExtent.Z;
	const float MaxZ = BoundsCenter.Z + BoundsExtent.Z;
	const float HalfWidth = BoundsExtent.X;

	// 전체 높이를 DivisionCount만큼 정확하게 나눔
	const float DivisionHeight = (MaxZ - MinZ) / DivisionCount;

	// 양 끝 경계선은 그리지 않고 내부 구분선만 그림
	for (int32 Index = 1; Index < DivisionCount; ++Index)
	{
		const float Z = MinZ + DivisionHeight * Index;

		const FVector LineStart(BoundsCenter.X - HalfWidth, BoundsCenter.Y, Z);
		const FVector LineEnd(BoundsCenter.X + HalfWidth, BoundsCenter.Y, Z);

		DrawDebugLine(GetWorld(), LineStart, LineEnd, FColor::Red, true, -1.0f, 0, 10.0f);
	}
}