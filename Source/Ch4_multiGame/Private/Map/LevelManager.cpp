#include "Public/Map/LevelManager.h"

#include "Landscape.h"
#include "Public/Map/RoadBase.h"
#include "Public/Map/LevelFloorBase.h"
#include "Net/UnrealNetwork.h"
#include "Public/Map/ZonePostProcessVolume.h"
#include "GameFlow/FinalDeliveryZone.h"

ALevelManager::ALevelManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
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
    // ============================================================
    // 배치 전에 이동 대상 Actor들을 Movable로 변경
    // ============================================================

    auto SetRootMobility =
        [](AActor* Actor, EComponentMobility::Type Mobility)
    {
        if (Actor && Actor->GetRootComponent())
        {
            Actor->GetRootComponent()->SetMobility(Mobility);
        }
    };

    // End Zone
    SetRootMobility(EndRoadActor, EComponentMobility::Movable);
    SetRootMobility(EndEnvironmentActor, EComponentMobility::Movable);
    SetRootMobility(EndPostProcessVolume, EComponentMobility::Movable);
    SetRootMobility(FinalDeliveryZoneActor, EComponentMobility::Movable);
    SetRootMobility(EndLandscapeActor, EComponentMobility::Movable);

    // Middle Zone
    for (ARoadBase* Road : MiddleRoadActors)
    {
        SetRootMobility(Road, EComponentMobility::Movable);
    }

    for (ALevelFloorBase* Environment : MiddleEnvironmentActors)
    {
        SetRootMobility(Environment, EComponentMobility::Movable);
    }

    for (AZonePostProcessVolume* PostProcessVolume : MiddlePostProcessVolumes)
    {
        SetRootMobility(PostProcessVolume, EComponentMobility::Movable);
    }

    for (ALandscape* Landscape : MiddleLandscapeActors)
    {
        SetRootMobility(Landscape, EComponentMobility::Movable);
    }

    // Start Zone
    SetRootMobility(StartRoadActor, EComponentMobility::Movable);
    SetRootMobility(StartEnvironmentActor, EComponentMobility::Movable);
    SetRootMobility(StartPostProcessVolume, EComponentMobility::Movable);
    SetRootMobility(StartLandscapeActor, EComponentMobility::Movable);

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
            TEXT("중간 Zone의 개수가 다릅니다. Road: %d / Environment: %d / PostProcess: %d"),
            MiddleRoadActors.Num(),
            MiddleEnvironmentActors.Num(),
            MiddlePostProcessVolumes.Num());

        return;
    }

    // LevelManager 위치를 최초 기준점으로 사용
    FTransform NextAttachTransform = GetActorTransform();

    // ============================================================
    // 1. End Zone
    // ============================================================

    if (EndRoadActor && EndEnvironmentActor)
    {
        const FTransform OriginalEndRoadTransform = EndRoadActor->GetActorTransform();
        const float OriginalRoadZ = EndRoadActor->GetActorLocation().Z;

        FTransform FinalDeliveryRelativeTransform;

        if (FinalDeliveryZoneActor)
        {
            FinalDeliveryRelativeTransform = FinalDeliveryZoneActor->GetActorTransform().GetRelativeTransform(OriginalEndRoadTransform);
        }

        FTransform EndPostProcessRelativeTransform;

        if (EndPostProcessVolume)
        {
            EndPostProcessRelativeTransform = EndPostProcessVolume->GetActorTransform().GetRelativeTransform(OriginalEndRoadTransform);
        }

        FTransform RoadEndRelative = EndRoadActor->GetEndPointTransform().GetRelativeTransform(EndRoadActor->GetActorTransform());

        FTransform FinalRoadTransform = RoadEndRelative.Inverse() * NextAttachTransform;

        // Road가 이동한 Z 거리
        const float RoadZDelta = FinalRoadTransform.GetLocation().Z - OriginalRoadZ;

        EndRoadActor->SetActorTransform(FinalRoadTransform);

        // Environment는 Road의 X/Y만 따라가고 Z는 고정
        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform = EndEnvironmentActor->GetActorTransform();
        EnvironmentTransform.SetLocation(EnvironmentLocation);
        EndEnvironmentActor->SetActorTransform(EnvironmentTransform);

        // Landscape는 Road가 이동한 만큼 Z축으로 이동
        if (EndLandscapeActor)
        {
            FVector LandscapeLocation = EndLandscapeActor->GetActorLocation();
            LandscapeLocation.Z += RoadZDelta;

            FVector Origin;
            FVector Extent;

            EndLandscapeActor->GetActorBounds(false, Origin, Extent);

            LandscapeLocation.X = FinalRoadTransform.GetLocation().X - Extent.X;
            LandscapeLocation.Y = FinalRoadTransform.GetLocation().Y - Extent.Y;

            FTransform LandscapeTransform = EndLandscapeActor->GetActorTransform();
            LandscapeTransform.SetLocation(LandscapeLocation);
            EndLandscapeActor->SetActorTransform(LandscapeTransform);
        }

        // FinalDeliveryZone도 End Road와 동일한 상대 위치를 유지하며 이동
        if (FinalDeliveryZoneActor)
        {
            const FTransform FinalDeliveryTransform = FinalDeliveryRelativeTransform * FinalRoadTransform;
            FinalDeliveryZoneActor->SetActorTransform(FinalDeliveryTransform);
        }

        // End PostProcessVolume도 End Road와 동일한 상대 위치를 유지하며 이동
        if (EndPostProcessVolume)
        {
            const FTransform FinalPostProcessTransform = EndPostProcessRelativeTransform * FinalRoadTransform;
            EndPostProcessVolume->SetActorTransform(FinalPostProcessTransform);
        }

        // End Road의 StartPoint를 다음 연결 기준으로 사용
        NextAttachTransform = EndRoadActor->GetStartPointTransform();
    }

    // ============================================================
    // 2. Middle Zone
    // ============================================================

    for (int32 Index = MiddleZoneOrder.Num() - 1; Index >= 0; --Index)
    {
        const int32 TargetIndex = MiddleZoneOrder[Index];

        if (!MiddleRoadActors.IsValidIndex(TargetIndex) || !MiddleEnvironmentActors.IsValidIndex(TargetIndex))
        {
            continue;
        }

        ARoadBase* Road = MiddleRoadActors[TargetIndex];
        ALevelFloorBase* Environment = MiddleEnvironmentActors[TargetIndex];

        ALandscape* Landscape = nullptr;

        if (MiddleLandscapeActors.IsValidIndex(TargetIndex))
        {
            Landscape = MiddleLandscapeActors[TargetIndex];
        }

        AZonePostProcessVolume* PostProcessVolume = nullptr;

        if (MiddlePostProcessVolumes.IsValidIndex(TargetIndex))
        {
            PostProcessVolume = MiddlePostProcessVolumes[TargetIndex];
        }

        if (!Road || !Environment)
        {
            continue;
        }

        FTransform PostProcessRelativeTransform;

        if (PostProcessVolume)
        {
            PostProcessRelativeTransform = PostProcessVolume->GetActorTransform().GetRelativeTransform(Road->GetActorTransform());
        }

        FTransform RoadEndRelative = Road->GetEndPointTransform().GetRelativeTransform(Road->GetActorTransform());

        FTransform FinalRoadTransform = RoadEndRelative.Inverse() * NextAttachTransform;

        // 배치 전 Road의 Z 위치 저장
        const float OriginalRoadZ = Road->GetActorLocation().Z;

        // Road가 이동한 Z 거리
        const float RoadZDelta = FinalRoadTransform.GetLocation().Z - OriginalRoadZ;

        Road->SetActorTransform(FinalRoadTransform);

        // Environment는 Road의 X/Y만 따라가고 Z는 고정
        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform = Environment->GetActorTransform();
        EnvironmentTransform.SetLocation(EnvironmentLocation);
        Environment->SetActorTransform(EnvironmentTransform);

        // Landscape는 Road가 이동한 만큼 Z축으로 이동
        if (Landscape)
        {
            FVector LandscapeLocation = Landscape->GetActorLocation();
            LandscapeLocation.Z += RoadZDelta;

            FVector Origin;
            FVector Extent;

            Landscape->GetActorBounds(false, Origin, Extent);

            LandscapeLocation.X = FinalRoadTransform.GetLocation().X - Extent.X;
            LandscapeLocation.Y = FinalRoadTransform.GetLocation().Y - Extent.Y;

            FTransform LandscapeTransform = Landscape->GetActorTransform();
            LandscapeTransform.SetLocation(LandscapeLocation);
            Landscape->SetActorTransform(LandscapeTransform);
        }

        if (PostProcessVolume)
        {
            const FTransform FinalPostProcessTransform = PostProcessRelativeTransform * FinalRoadTransform;
            PostProcessVolume->SetActorTransform(FinalPostProcessTransform);
        }

        // 현재 Road의 StartPoint를 다음 연결 기준으로 사용
        NextAttachTransform = Road->GetStartPointTransform();
    }

    // ============================================================
    // 3. Start Zone
    // ============================================================

    if (StartRoadActor && StartEnvironmentActor)
    {
        const FTransform OriginalStartRoadTransform = StartRoadActor->GetActorTransform();
        const float OriginalRoadZ = StartRoadActor->GetActorLocation().Z;

        FTransform StartPostProcessRelativeTransform;

        if (StartPostProcessVolume)
        {
            StartPostProcessRelativeTransform = StartPostProcessVolume->GetActorTransform().GetRelativeTransform(OriginalStartRoadTransform);
        }

        FTransform RoadEndRelative = StartRoadActor->GetEndPointTransform().GetRelativeTransform(StartRoadActor->GetActorTransform());

        FTransform FinalRoadTransform = RoadEndRelative.Inverse() * NextAttachTransform;

        // Road가 이동한 Z 거리
        const float RoadZDelta = FinalRoadTransform.GetLocation().Z - OriginalRoadZ;

        StartRoadActor->SetActorTransform(FinalRoadTransform);

        // Environment는 Road의 X/Y만 따라가고 Z는 고정
        FVector EnvironmentLocation = FinalRoadTransform.GetLocation();
        EnvironmentLocation.Z = EnvironmentBaseZ;

        FTransform EnvironmentTransform = StartEnvironmentActor->GetActorTransform();
        EnvironmentTransform.SetLocation(EnvironmentLocation);
        StartEnvironmentActor->SetActorTransform(EnvironmentTransform);

        // Landscape는 Road가 이동한 만큼 Z축으로 이동
        if (StartLandscapeActor)
        {
            FVector LandscapeLocation = StartLandscapeActor->GetActorLocation();
            LandscapeLocation.Z += RoadZDelta;

            FVector Origin;
            FVector Extent;

            StartLandscapeActor->GetActorBounds(false, Origin, Extent);

            LandscapeLocation.X = FinalRoadTransform.GetLocation().X - Extent.X;
            LandscapeLocation.Y = FinalRoadTransform.GetLocation().Y - Extent.Y;

            FTransform LandscapeTransform = StartLandscapeActor->GetActorTransform();
            LandscapeTransform.SetLocation(LandscapeLocation);
            StartLandscapeActor->SetActorTransform(LandscapeTransform);
        }

        // Start PostProcessVolume을 이동한 Road 기준으로 같이 이동
        if (StartPostProcessVolume)
        {
            const FTransform FinalPostProcessTransform = StartPostProcessRelativeTransform * FinalRoadTransform;
            StartPostProcessVolume->SetActorTransform(FinalPostProcessTransform);
        }
    }

    // ============================================================
    // Road Components를 다시 Static으로 변경
    // ============================================================

    if (EndRoadActor)
    {
        EndRoadActor->SetRoadComponentsStatic();
    }

    for (ARoadBase* Road : MiddleRoadActors)
    {
        if (Road)
        {
            Road->SetRoadComponentsStatic();
        }
    }

    if (StartRoadActor)
    {
        StartRoadActor->SetRoadComponentsStatic();
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
    DOREPLIFETIME(ALevelManager, MiddleZoneOrder);
}