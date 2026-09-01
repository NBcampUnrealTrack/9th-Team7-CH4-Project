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
    else
    {
        // 클라이언트: MiddleZoneOrder가 비어있으면 기본 순서로 초기화하고 배치 실행
        if (MiddleZoneOrder.Num() == 0)
        {
            MiddleZoneOrder.Empty();
            for (int32 i = 0; i < MiddleRoadActors.Num(); ++i)
            {
                MiddleZoneOrder.Add(i);
            }
            ArrangePlacedZones();
        }
    }
}

void ALevelManager::OnRep_MiddleZoneOrder()
{
    // 클라이언트: 서버로부터 셔플 순서를 전달받으면 배치 실행
    ArrangePlacedZones();
}

void ALevelManager::ArrangePlacedZones()
{
    float EnvironmentBaseZ = 0.0f;

    if (StartEnvironmentActor)
    {
        EnvironmentBaseZ = StartEnvironmentActor->GetActorLocation().Z;
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

    // LevelManager 위치를 최초 기준점으로 사용
    FTransform NextAttachTransform = GetActorTransform();

    // 1. End Zone
    // End Road의 EndPoint를 LevelManager 위치에 맞춤
    if (EndRoadActor && EndEnvironmentActor)
    {
        FTransform RoadEndRelative =
            EndRoadActor->GetEndPointTransform().GetRelativeTransform(
                EndRoadActor->GetActorTransform());

        FTransform FinalRoadTransform =
            RoadEndRelative.Inverse() * NextAttachTransform;

        EndRoadActor->SetActorTransform(FinalRoadTransform);

        // Environment는 Road의 X/Y만 따라가고 Z는 고정
        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform =
            EndEnvironmentActor->GetActorTransform();

        EnvironmentTransform.SetLocation(EnvironmentLocation);

        EndEnvironmentActor->SetActorTransform(EnvironmentTransform);

        // End Road의 StartPoint를 다음 연결 기준으로 사용
        NextAttachTransform = EndRoadActor->GetStartPointTransform();
    }

    // 2. Middle Zone
    // 기존 Zone 순서를 유지하기 위해 뒤에서부터 배치
    for (int32 Index = MiddleZoneOrder.Num() - 1; Index >= 0; --Index)
    {
        const int32 TargetIndex = MiddleZoneOrder[Index];

        if (!MiddleRoadActors.IsValidIndex(TargetIndex) ||
            !MiddleEnvironmentActors.IsValidIndex(TargetIndex))
        {
            continue;
        }

        ARoadBase* Road = MiddleRoadActors[TargetIndex];
        ALevelFloorBase* Environment = MiddleEnvironmentActors[TargetIndex];

        if (!Road || !Environment)
        {
            continue;
        }

        // 현재 Road의 EndPoint를 이전 Road의 StartPoint에 맞춤
        FTransform RoadEndRelative =
            Road->GetEndPointTransform().GetRelativeTransform(
                Road->GetActorTransform());

        FTransform FinalRoadTransform =
            RoadEndRelative.Inverse() * NextAttachTransform;

        Road->SetActorTransform(FinalRoadTransform);

        // Environment는 Road의 X/Y만 따라가고 Z는 고정
        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform =
            Environment->GetActorTransform();

        EnvironmentTransform.SetLocation(EnvironmentLocation);

        Environment->SetActorTransform(EnvironmentTransform);

        // 현재 Road의 StartPoint를 다음 연결 기준으로 사용
        NextAttachTransform = Road->GetStartPointTransform();
    }

    // 3. Start Zone
    if (StartRoadActor && StartEnvironmentActor)
    {
        // Start Road의 EndPoint를 마지막 Middle의 StartPoint에 맞춤
        FTransform RoadEndRelative =
            StartRoadActor->GetEndPointTransform().GetRelativeTransform(
                StartRoadActor->GetActorTransform());

        FTransform FinalRoadTransform =
            RoadEndRelative.Inverse() * NextAttachTransform;

        StartRoadActor->SetActorTransform(FinalRoadTransform);

        // Environment는 Road의 X/Y만 따라가고 Z는 고정
        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform =
            StartEnvironmentActor->GetActorTransform();

        EnvironmentTransform.SetLocation(EnvironmentLocation);

        StartEnvironmentActor->SetActorTransform(EnvironmentTransform);
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[%s] End Zone 기준 자동 배치 완료"),
        HasAuthority() ? TEXT("Server") : TEXT("Client"));
}

void ALevelManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ALevelManager, bUseAutoArrange);
    DOREPLIFETIME(ALevelManager, bShuffleMiddleZones);
    DOREPLIFETIME(ALevelManager, MiddleZoneOrder); // 인덱스 배열 동기화
}
