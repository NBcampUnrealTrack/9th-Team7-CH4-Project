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
	FTransform NextAttachTransform = GetActorTransform();

	// 최초 Start Environment의 Z를 전체 Environment의 기준 높이로 저장
	float EnvironmentBaseZ = 0.0f;

	if (StartEnvironmentActor)
	{
		EnvironmentBaseZ = StartEnvironmentActor->GetActorLocation().Z;
	}

	// 시작 Zone
	if (StartRoadActor && StartEnvironmentActor)
	{
		FTransform RoadStartRelative =
			StartRoadActor->GetStartPointTransform()
			.GetRelativeTransform(StartRoadActor->GetActorTransform());

		FTransform FinalRoadTransform =
			RoadStartRelative.Inverse() * NextAttachTransform;

		// Start Road와 Start Environment는 함께 배치
		StartRoadActor->SetActorTransform(FinalRoadTransform);
		StartEnvironmentActor->SetActorTransform(FinalRoadTransform);

		// 다음 Road의 기준은 Start Road의 EndPoint
		NextAttachTransform = StartRoadActor->GetEndPointTransform();
	}

	TArray<ARoadBase*> MiddleRoads = MiddleRoadActors;
	TArray<ALevelFloorBase*> MiddleEnvironments = MiddleEnvironmentActors;

	if (MiddleRoads.Num() != MiddleEnvironments.Num())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("중간 Road와 Environment의 개수가 다릅니다. Road: %d / Environment: %d"),
			MiddleRoads.Num(),
			MiddleEnvironments.Num());

		return;
	}

	// Road와 Environment의 순서를 함께 섞음
	if (bShuffleMiddleZones)
	{
		for (int32 Index = 0; Index < MiddleRoads.Num(); ++Index)
		{
			const int32 RandomIndex =
				FMath::RandRange(Index, MiddleRoads.Num() - 1);

			if (Index != RandomIndex)
			{
				MiddleRoads.Swap(Index, RandomIndex);
				MiddleEnvironments.Swap(Index, RandomIndex);
			}
		}
	}

	// 중간 Zone
	for (int32 Index = 0; Index < MiddleRoads.Num(); ++Index)
	{
		ARoadBase* Road = MiddleRoads[Index];
		ALevelFloorBase* Environment = MiddleEnvironments[Index];

		if (!Road || !Environment)
		{
			continue;
		}

		// Road의 StartPoint를 이전 Road의 EndPoint에 연결
		FTransform RoadStartRelative =
			Road->GetStartPointTransform()
			.GetRelativeTransform(Road->GetActorTransform());

		FTransform FinalRoadTransform =
			RoadStartRelative.Inverse() * NextAttachTransform;

		// Road는 그대로 배치
		Road->SetActorTransform(FinalRoadTransform);

		// Environment는 Road의 X/Y만 사용하고 Z는 최초 Environment 높이 유지
		FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
		EnvironmentLocation.Z = EnvironmentBaseZ;

		FTransform EnvironmentTransform = Environment->GetActorTransform();
		EnvironmentTransform.SetLocation(EnvironmentLocation);

		Environment->SetActorTransform(EnvironmentTransform);

		// 다음 Road는 현재 Road의 EndPoint를 기준으로 연결
		NextAttachTransform = Road->GetEndPointTransform();
	}

	// 끝 Zone
	if (EndRoadActor && EndEnvironmentActor)
	{
		FTransform RoadStartRelative =
			EndRoadActor->GetStartPointTransform()
			.GetRelativeTransform(EndRoadActor->GetActorTransform());

		FTransform FinalRoadTransform =
			RoadStartRelative.Inverse() * NextAttachTransform;

		// End Road 배치
		EndRoadActor->SetActorTransform(FinalRoadTransform);

		// End Environment는 최초 Environment 높이 유지
		FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
		EnvironmentLocation.Z = EnvironmentBaseZ;

		FTransform EnvironmentTransform = EndEnvironmentActor->GetActorTransform();
		EnvironmentTransform.SetLocation(EnvironmentLocation);

		EndEnvironmentActor->SetActorTransform(EnvironmentTransform);
	}

	UE_LOG(LogTemp, Warning, TEXT("Road + Environment 자동 배치 완료"));
}