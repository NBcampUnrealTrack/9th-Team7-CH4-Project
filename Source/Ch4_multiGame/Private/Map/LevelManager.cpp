#include "Public/Map/LevelManager.h"
#include "Public/Map/RoadBase.h"
#include "Public/Map/LevelFloorBase.h"
#include "Net/UnrealNetwork.h"

ALevelManager::ALevelManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
}

void ALevelManager::BeginPlay()
{
    Super::BeginPlay();

    if (!bUseAutoArrange)
    {
        return;
    }

    if (HasAuthority())
    {
        // 1. 서버: 기본 인덱스 배열 생성 (0, 1, 2...)
        MiddleZoneOrder.Empty();
        for (int32 i = 0; i < MiddleRoadActors.Num(); ++i)
        {
            MiddleZoneOrder.Add(i);
        }

        // 2. 서버: 셔플 옵션이 켜져 있다면 인덱스만 셔플
        if (bShuffleMiddleZones)
        {
            for (int32 Index = 0; Index < MiddleZoneOrder.Num(); ++Index)
            {
                const int32 RandomIndex = FMath::RandRange(Index, MiddleZoneOrder.Num() - 1);
                if (Index != RandomIndex)
                {
                    MiddleZoneOrder.Swap(Index, RandomIndex);
                }
            }
        }
        // 3. 서버 본인 레벨 배치 실행
        ArrangePlacedZones();
    }
}

void ALevelManager::OnRep_MiddleZoneOrder()
{
    // 클라이언트: 서버로부터 셔플 순서를 전달받으면 배치 실행
    ArrangePlacedZones();
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
        FTransform RoadStartRelative = StartRoadActor->GetStartPointTransform().GetRelativeTransform(StartRoadActor->GetActorTransform());

        FTransform FinalRoadTransform = RoadStartRelative.Inverse() * NextAttachTransform;

        StartRoadActor->SetActorTransform(FinalRoadTransform);
        StartEnvironmentActor->SetActorTransform(FinalRoadTransform);

        NextAttachTransform = StartRoadActor->GetEndPointTransform();
    }

    if (MiddleRoadActors.Num() != MiddleEnvironmentActors.Num())
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("중간 Road와 Environment의 개수가 다릅니다. Road: %d / Environment: %d"),
            MiddleRoadActors.Num(),
            MiddleEnvironmentActors.Num());

        return;
    }

    // 중간 Zone 배치 (서버에서 전송받은 MiddleZoneOrder 순서대로 배치)
    for (int32 Index = 0; Index < MiddleZoneOrder.Num(); ++Index)
    {
        int32 TargetIndex = MiddleZoneOrder[Index];

        if (!MiddleRoadActors.IsValidIndex(TargetIndex) || !MiddleEnvironmentActors.IsValidIndex(TargetIndex))
        {
            continue;
        }

        ARoadBase* Road = MiddleRoadActors[TargetIndex];
        ALevelFloorBase* Environment = MiddleEnvironmentActors[TargetIndex];

        if (!Road || !Environment)
        {
            continue;
        }

        // Road의 StartPoint를 이전 Road의 EndPoint에 연결
        FTransform RoadStartRelative = Road->GetStartPointTransform().GetRelativeTransform(Road->GetActorTransform());

        FTransform FinalRoadTransform = RoadStartRelative.Inverse() * NextAttachTransform;

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
        FTransform RoadStartRelative = EndRoadActor->GetStartPointTransform().GetRelativeTransform(EndRoadActor->GetActorTransform());

        FTransform FinalRoadTransform = RoadStartRelative.Inverse() * NextAttachTransform;

        EndRoadActor->SetActorTransform(FinalRoadTransform);

        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform = EndEnvironmentActor->GetActorTransform();
        EnvironmentTransform.SetLocation(EnvironmentLocation);

        EndEnvironmentActor->SetActorTransform(EnvironmentTransform);
    }

    UE_LOG(LogTemp, Warning, TEXT("[%s] Road + Environment 자동 배치 완료"), HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

void ALevelManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ALevelManager, bUseAutoArrange);
    DOREPLIFETIME(ALevelManager, bShuffleMiddleZones);
    DOREPLIFETIME(ALevelManager, MiddleZoneOrder); // 인덱스 배열 동기화
}