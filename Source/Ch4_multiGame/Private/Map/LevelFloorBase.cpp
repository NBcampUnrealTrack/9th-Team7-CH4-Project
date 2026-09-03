#include "Public/Map/LevelFloorBase.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "InstancedFoliageActor.h"


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

	// 1. 진입 시 숨길 태그 처리
	if (!BeginHideTargetTag.IsNone())
	{
		TArray<AActor*> HideActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), BeginHideTargetTag, HideActors);
		for (AActor* Actor : HideActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(true);
			}
		}
	}

	// 2. 진입 시 보일 태그 처리
	if (!BeginShowTargetTag.IsNone())
	{
		TArray<AActor*> ShowActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), BeginShowTargetTag, ShowActors);
		for (AActor* Actor : ShowActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}
	
	// 3. 진입 시 인스턴스 폴리지 컴포넌트 숨김 처리
	if (!BeginHideComponentTag.IsNone())
	{
		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;
			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), BeginHideComponentTag);
				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(false, true);
					}
				}
			}
		}
	}

	// 4. 진입 시 인스턴스 폴리지 컴포넌트 표시 처리
	if (!BeginShowComponentTag.IsNone())
	{
		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;
			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), BeginShowComponentTag);
				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(true, true);
					}
				}
			}
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

	// 1. 이탈 시 숨길 태그 처리
	if (!EndHideTargetTag.IsNone())
	{
		TArray<AActor*> HideActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), EndHideTargetTag, HideActors);
		for (AActor* Actor : HideActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(true);
			}
		}
	}

	// 2. 이탈 시 보일 태그 처리
	if (!EndShowTargetTag.IsNone())
	{
		TArray<AActor*> ShowActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), EndShowTargetTag, ShowActors);
		for (AActor* Actor : ShowActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}
	
	// 3. 이탈 시 인스턴스 폴리지 컴포넌트 숨김 처리
	if (!EndHideComponentTag.IsNone())
	{
		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;
			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), EndHideComponentTag);
				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(false, true);
					}
				}
			}
		}
	}

	// 4. 이탈 시 인스턴스 폴리지 컴포넌트 표시 처리
	if (!EndShowComponentTag.IsNone())
	{
		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;
			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps = FoliageActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), EndShowComponentTag);
				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(true, true);
					}
				}
			}
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