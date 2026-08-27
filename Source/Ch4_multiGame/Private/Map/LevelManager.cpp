#include "Public/Map/LevelManager.h"
#include "Public/Map/RoadBase.h"
#include "Public/Map/LevelFloorBase.h"

ALevelManager::ALevelManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALevelManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (bUseAutoArrange)
		{
			ArrangePlacedZones();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("자동 배치 OFF - 에디터 레벨 내 배치를 사용합니다"));
		}
	}
}

void ALevelManager::ArrangePlacedZones()
{
	// 시작 위치
	FTransform NextAttachTransform = GetActorTransform();

	// 시작 Zone
	if (StartRoadActor && StartEnvironmentActor)
	{
		FTransform RoadStartRelative = StartRoadActor->GetStartPointTransform().GetRelativeTransform(StartRoadActor->GetActorTransform());
		FTransform FinalRoadTransform = RoadStartRelative.Inverse() * NextAttachTransform;

		StartRoadActor->SetActorTransform(FinalRoadTransform);
		StartEnvironmentActor->SetActorTransform(FinalRoadTransform);

		NextAttachTransform = StartRoadActor->GetEndPointTransform();
	}

	// 중간 Zone 복사
	TArray<ARoadBase*> MiddleRoads = MiddleRoadActors;
	TArray<ALevelFloorBase*> MiddleEnvironments = MiddleEnvironmentActors;

	// Road와 Environment 개수 확인
	if (MiddleRoads.Num() != MiddleEnvironments.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("중간 Road와 Environment의 개수가 다릅니다. Road: %d / Environment: %d"), MiddleRoads.Num(), MiddleEnvironments.Num());
		return;
	}

	// Road와 Environment를 같은 순서로 셔플
	if (bShuffleMiddleZones)
	{
		for (int32 Index = 0; Index < MiddleRoads.Num(); ++Index)
		{
			const int32 RandomIndex = FMath::RandRange(Index, MiddleRoads.Num() - 1);

			if (Index != RandomIndex)
			{
				MiddleRoads.Swap(Index, RandomIndex);
				MiddleEnvironments.Swap(Index, RandomIndex);
			}
		}
	}

	// 중간 Zone 배치
	for (int32 Index = 0; Index < MiddleRoads.Num(); ++Index)
	{
		ARoadBase* Road = MiddleRoads[Index];
		ALevelFloorBase* Environment = MiddleEnvironments[Index];

		if (!Road || !Environment)
		{
			continue;
		}

		FTransform RoadStartRelative = Road->GetStartPointTransform().GetRelativeTransform(Road->GetActorTransform());
		FTransform FinalRoadTransform = RoadStartRelative.Inverse() * NextAttachTransform;

		Road->SetActorTransform(FinalRoadTransform);
		Environment->SetActorTransform(FinalRoadTransform);

		NextAttachTransform = Road->GetEndPointTransform();
	}

	// 끝 Zone
	if (EndRoadActor && EndEnvironmentActor)
	{
		FTransform RoadStartRelative = EndRoadActor->GetStartPointTransform().GetRelativeTransform(EndRoadActor->GetActorTransform());
		FTransform FinalRoadTransform = RoadStartRelative.Inverse() * NextAttachTransform;

		EndRoadActor->SetActorTransform(FinalRoadTransform);
		EndEnvironmentActor->SetActorTransform(FinalRoadTransform);
	}

	UE_LOG(LogTemp, Warning, TEXT("Road + Environment 자동 배치 완료"));
}