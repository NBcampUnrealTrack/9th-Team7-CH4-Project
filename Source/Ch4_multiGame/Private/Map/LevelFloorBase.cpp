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
	bAlwaysRelevant = true;

	// 루트 컴포넌트
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComp);
	
	// 환경 충돌 영역
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
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
	for (const FName& Tag : BeginHideTargetTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		TArray<AActor*> HideActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, HideActors);

		for (AActor* Actor : HideActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(true);
			}
		}
	}

	// 2. 진입 시 보일 태그 처리
	for (const FName& Tag : BeginShowTargetTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		TArray<AActor*> ShowActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, ShowActors);

		for (AActor* Actor : ShowActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}
	
	// 3. 진입 시 인스턴스 폴리지 컴포넌트 숨김 처리
	for (const FName& Tag : BeginHideComponentTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;

			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps =
					FoliageActor->GetComponentsByTag(
						UPrimitiveComponent::StaticClass(),
						Tag);

				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(false, true);
						PrimComp->SetHiddenInGame(true, true);
					}
				}
			}
		}
	}

	// 4. 진입 시 인스턴스 폴리지 컴포넌트 표시 처리
	for (const FName& Tag : BeginShowComponentTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;

			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps =
					FoliageActor->GetComponentsByTag(
						UPrimitiveComponent::StaticClass(),
						Tag);

				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(true, true);
						PrimComp->SetHiddenInGame(false, true);
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
	for (const FName& Tag : EndHideTargetTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		TArray<AActor*> HideActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, HideActors);

		for (AActor* Actor : HideActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(true);
			}
		}
	}

	// 2. 이탈 시 보일 태그 처리
	for (const FName& Tag : EndShowTargetTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		TArray<AActor*> ShowActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, ShowActors);

		for (AActor* Actor : ShowActors)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(false);
			}
		}
	}
	
	// 3. 이탈 시 인스턴스 폴리지 컴포넌트 숨김 처리
	for (const FName& Tag : EndHideComponentTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;

			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps =
					FoliageActor->GetComponentsByTag(
						UPrimitiveComponent::StaticClass(),
						Tag);

				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(false, true);
						PrimComp->SetHiddenInGame(true, true);
					}
				}
			}
		}
	}

	// 4. 이탈 시 인스턴스 폴리지 컴포넌트 표시 처리
	for (const FName& Tag : EndShowComponentTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}

		for (TActorIterator<AInstancedFoliageActor> It(GetWorld()); It; ++It)
		{
			AInstancedFoliageActor* FoliageActor = *It;

			if (FoliageActor)
			{
				TArray<UActorComponent*> Comps =
					FoliageActor->GetComponentsByTag(
						UPrimitiveComponent::StaticClass(),
						Tag);

				for (UActorComponent* Comp : Comps)
				{
					if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
					{
						PrimComp->SetVisibility(true, true);
						PrimComp->SetHiddenInGame(false, true);
					}
				}
			}
		}
	}
}

void ALevelFloorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
}