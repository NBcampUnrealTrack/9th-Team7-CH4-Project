#include "Public/Map/LevelManager.h"
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
			UE_LOG(LogTemp,Warning,TEXT("자동 배치 OFF 에디터 레벨내 배치를 사용합니다"))
		}	
	}
}

void ALevelManager::ArrangePlacedZones()
{
	// 순서대로 정렬할 배열 생성
	TArray<ALevelFloorBase*> FinalSequence;
	
	//  시작 구역 추가(고정)
	if (StartZoneActor)
	{
		FinalSequence.Add(StartZoneActor);
	}

	// 2. 중간 맵 처리 (셔플 옵션에 따른 분기)
	TArray<ALevelFloorBase*> MiddleZones = MiddleZoneActors;

	if (bShuffleMiddleZones)
	{
		// [ON] 랜덤으로 순서 섞기
		const int32 NumMiddle = MiddleZones.Num();
		for (int32 i = 0; i < NumMiddle; ++i)
		{
			int32 RandomIndex = FMath::RandRange(i, NumMiddle - 1);
			if (i != RandomIndex)
			{
				MiddleZones.Swap(i, RandomIndex);
			}
		}
	}
	// OFF 일 경우 섞지 않고 MiddleZoneActors 배열에 담긴 순서 그대로 유지

	FinalSequence.Append(MiddleZones);

	//끝 맵 추가 (고정)
	if (EndZoneActor)
	{
		FinalSequence.Add(EndZoneActor);
	}

	// 3. ZoneManager 자신 위치를 시작점으로 설정
	FTransform NextAttachTransform = GetActorTransform();

	// 4. 레벨에 배치된 액터들의 위치(Transform)만 순서대로 변경
	for (ALevelFloorBase* Zone : FinalSequence)
	{
		if (!Zone) continue;

		// StartPoint와 액터 원점 간의 상대적 오프셋 계산
		FTransform ActorToStart = Zone->GetStartPointTransform().GetRelativeTransform(Zone->GetActorTransform());
	
		// TargetTransform 계산 (StartPoint가 NextAttachTransform 위치에 들어맞도록)
		FTransform FinalTargetTransform = ActorToStart.Inverse() * NextAttachTransform;

		//새로 생성하는 게 아니라 이미 존재하는 액터의 위치만 이동
		Zone->SetActorTransform(FinalTargetTransform);

		//이어 붙을 EndPoint 위치 갱신
		NextAttachTransform = Zone->GetEndPointTransform();
	}
}