#include "Public/Map/LevelManager.h"
#include "Landscape.h"
#include "Public/Map/RoadBase.h"
#include "Public/Map/LevelFloorBase.h"
#include "Net/UnrealNetwork.h"
#include "Public/Map/ZonePostProcessVolume.h"
#include "GameFlow/FinalDeliveryZone.h"
#include "Kismet/GameplayStatics.h"
#include "PCGComponent.h"

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
        //서버에서 매 판 무작위 PCG Seed 생성
        PCGSeed = FMath::RandRange(1, 999999);
        
        MiddleZoneOrder.Empty();

        for (int32 Index = 0; Index < MiddleRoadActors.Num(); ++Index)
        {
            MiddleZoneOrder.Add(Index);
        }

        if (bShuffleMiddleZones)
        {
            for (int32 Index = 0; Index < MiddleZoneOrder.Num() - 1; ++Index)
            {
                const int32 RandomIndex = FMath::RandRange(Index, MiddleZoneOrder.Num() - 1);
                MiddleZoneOrder.Swap(Index, RandomIndex);
            }
        }

        // 도로 및 배경 재배치
        ArrangePlacedZones();
        
        // 이동된 도로 및 스태틱 메쉬의 콜리전/트랜스폼이 월드에 동기화 완료된 후 PCG 생성 (0.05초 지연)
        GetWorldTimerManager().SetTimer(PCGGenerateTimerHandle, this, &ALevelManager::TriggerPCGGeneration, 0.1f, false);
    }
}

void ALevelManager::OnRep_MiddleZoneOrder()
{
    if (!bUseAutoArrange)
    {
        return;
    }

    ArrangePlacedZones();
    
    //만약 PCGSeed가 이미도착해 있는 상태라면 PCG 생성 타이머 가동
    if (PCGSeed != 0 && GetWorld())
    {
        GetWorldTimerManager().SetTimer(PCGGenerateTimerHandle, this, &ALevelManager::TriggerPCGGeneration, 0.05f, false);
    }
}

void ALevelManager::OnRep_PCGSeed()
{
    // MiddleZoneOrder가 아직 도착 안 해서 도로 배치가 안 되었다면 생성 대기
    // 도로 배치가 이미 끝난 상태(MiddleZoneOrder가 채워짐)일 때만 PCG 생성 실행
    if (MiddleZoneOrder.Num() > 0 || !bShuffleMiddleZones)
    {
        if (GetWorld())
        {
            GetWorldTimerManager().SetTimer(PCGGenerateTimerHandle, this, &ALevelManager::TriggerPCGGeneration, 0.05f, false);
        }
    }
}

void ALevelManager::TriggerPCGGeneration()
{
    UE_LOG(LogTemp, Warning, TEXT("========== TriggerPCGGeneration =========="));

    if (PCGSeed == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PCGSeed is 0"));
        return;
    }

    int32 CurrentIndex = 0;

    if (StartRoadActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("Generate StartRoad: %s"), *StartRoadActor->GetName());
        StartRoadActor->GenerateObstacles(PCGSeed + CurrentIndex++);
    }

    for (ARoadBase* Road : MiddleRoadActors)
    {
        if (Road)
        {
            UE_LOG(LogTemp, Warning, TEXT("Generate MiddleRoad: %s"), *Road->GetName());
            Road->GenerateObstacles(PCGSeed + CurrentIndex++);
        }
    }

    if (EndRoadActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("Generate EndRoad: %s"), *EndRoadActor->GetName());
        EndRoadActor->GenerateObstacles(PCGSeed + CurrentIndex++);
    }
}


bool ALevelManager::IsLevelManagerManagedActor(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    if (Actor == this)
    {
        return true;
    }

    if (Actor == StartRoadActor || Actor == StartEnvironmentActor || Actor == StartPostProcessVolume || Actor == StartLandscapeActor)
    {
        return true;
    }

    if (Actor == EndRoadActor || Actor == EndEnvironmentActor || Actor == EndLandscapeActor || Actor == FinalDeliveryZoneActor || Actor == EndPostProcessVolume)
    {
        return true;
    }

    for (ARoadBase* Road : MiddleRoadActors)
    {
        if (Actor == Road)
        {
            return true;
        }
    }

    for (ALevelFloorBase* Environment : MiddleEnvironmentActors)
    {
        if (Actor == Environment)
        {
            return true;
        }
    }

    for (AZonePostProcessVolume* PostProcessVolume : MiddlePostProcessVolumes)
    {
        if (Actor == PostProcessVolume)
        {
            return true;
        }
    }

    for (ALandscape* LandscapeActor : MiddleLandscapeActors)
    {
        if (Actor == LandscapeActor)
        {
            return true;
        }
    }

    return false;
}

void ALevelManager::MoveTaggedActorsWithRoad(ARoadBase* Road, const FTransform& OriginalRoadTransform, const FTransform& FinalRoadTransform)
{
    if (!Road)
    {
        return;
    }

    const TArray<FName>& RoadTags = Road->Tags;

    if (RoadTags.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] Road [%s] has no Tags"), HasAuthority() ? TEXT("Server") : TEXT("Client"), *Road->GetName());
        return;
    }

    const FVector RoadLocationDelta = FinalRoadTransform.GetLocation() - OriginalRoadTransform.GetLocation();

    for (const FName& RoadTag : RoadTags)
    {
        if (RoadTag.IsNone())
        {
            continue;
        }

        TArray<AActor*> TaggedActors;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), RoadTag, TaggedActors);

        UE_LOG(LogTemp, Warning, TEXT("[%s] Road [%s] Tag [%s] Actor Count = %d"), HasAuthority() ? TEXT("Server") : TEXT("Client"), *Road->GetName(), *RoadTag.ToString(), TaggedActors.Num());

        for (AActor* Actor : TaggedActors)
        {
            if (!Actor || Actor == Road || IsLevelManagerManagedActor(Actor))
            {
                continue;
            }

            USceneComponent* ActorRootComponent = Actor->GetRootComponent();

            if (!ActorRootComponent)
            {
                continue;
            }

            const FVector OriginalActorLocation = Actor->GetActorLocation();
            const FVector FinalActorLocation = OriginalActorLocation + RoadLocationDelta;

            const EComponentMobility::Type OriginalMobility = ActorRootComponent->GetMobility();

            ActorRootComponent->SetMobility(EComponentMobility::Movable);
            Actor->SetActorLocation(FinalActorLocation);
            ActorRootComponent->SetMobility(OriginalMobility);

            UE_LOG(LogTemp, Verbose, TEXT("[%s] Moved Actor [%s] with Road [%s] Tag [%s]"), HasAuthority() ? TEXT("Server") : TEXT("Client"), *Actor->GetName(), *Road->GetName(), *RoadTag.ToString());
        }
    }
}

void ALevelManager::ArrangePlacedZones()
{
    if (!StartRoadActor || !StartEnvironmentActor || !EndRoadActor || !EndEnvironmentActor)
    {
        UE_LOG(LogTemp, Error, TEXT("LevelManager: Required Start or End actors are missing."));
        return;
    }

    if (MiddleRoadActors.Num() != MiddleEnvironmentActors.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("LevelManager: MiddleRoadActors and MiddleEnvironmentActors counts do not match."));
        return;
    }

    auto UseDeterministicPlacementOnly = [](AActor* Actor)
    {
        if (Actor)
        {
            Actor->SetReplicateMovement(false);
        }
    };

    // MiddleZoneOrder is the replicated source of truth. Disable actor movement
    // replication before applying the arrangement so a later relevancy update
    // cannot overwrite the client's matching local placement.
    UseDeterministicPlacementOnly(StartRoadActor);
    UseDeterministicPlacementOnly(StartEnvironmentActor);
    UseDeterministicPlacementOnly(EndRoadActor);
    UseDeterministicPlacementOnly(EndEnvironmentActor);

    for (ARoadBase* Road : MiddleRoadActors)
    {
        UseDeterministicPlacementOnly(Road);
    }

    for (ALevelFloorBase* Environment : MiddleEnvironmentActors)
    {
        UseDeterministicPlacementOnly(Environment);
    }

    const float EnvironmentBaseZ = StartEnvironmentActor->GetActorLocation().Z;

    auto SetRootMobility = [](AActor* Actor, EComponentMobility::Type Mobility)
    {
        if (!Actor)
        {
            return;
        }

        if (USceneComponent* RootComponent = Actor->GetRootComponent())
        {
            RootComponent->SetMobility(Mobility);
        }
    };

    SetRootMobility(StartRoadActor, EComponentMobility::Movable);
    SetRootMobility(StartEnvironmentActor, EComponentMobility::Movable);
    SetRootMobility(StartPostProcessVolume, EComponentMobility::Movable);
    SetRootMobility(StartLandscapeActor, EComponentMobility::Movable);

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

    for (ALandscape* LandscapeActor : MiddleLandscapeActors)
    {
        SetRootMobility(LandscapeActor, EComponentMobility::Movable);
    }

    SetRootMobility(EndRoadActor, EComponentMobility::Movable);
    SetRootMobility(EndEnvironmentActor, EComponentMobility::Movable);
    SetRootMobility(EndLandscapeActor, EComponentMobility::Movable);
    SetRootMobility(EndPostProcessVolume, EComponentMobility::Movable);
    SetRootMobility(FinalDeliveryZoneActor, EComponentMobility::Movable);

    FTransform NextAttachTransform = GetActorTransform();

    if (EndRoadActor)
    {
        const FTransform OriginalEndRoadTransform = EndRoadActor->GetActorTransform();
        const float OriginalRoadZ = OriginalEndRoadTransform.GetLocation().Z;

        FTransform FinalDeliveryZoneRelativeTransform;
        bool bHasFinalDeliveryZoneRelativeTransform = false;

        if (FinalDeliveryZoneActor)
        {
            FinalDeliveryZoneRelativeTransform = FinalDeliveryZoneActor->GetActorTransform().GetRelativeTransform(OriginalEndRoadTransform);
            bHasFinalDeliveryZoneRelativeTransform = true;
        }

        FTransform EndPostProcessRelativeTransform;
        bool bHasEndPostProcessRelativeTransform = false;

        if (EndPostProcessVolume)
        {
            EndPostProcessRelativeTransform = EndPostProcessVolume->GetActorTransform().GetRelativeTransform(OriginalEndRoadTransform);
            bHasEndPostProcessRelativeTransform = true;
        }

        const FTransform RoadEndRelative = EndRoadActor->GetEndPointTransform().GetRelativeTransform(EndRoadActor->GetActorTransform());
        const FTransform FinalEndRoadTransform = RoadEndRelative.Inverse() * NextAttachTransform;
        const float RoadZDelta = FinalEndRoadTransform.GetLocation().Z - OriginalRoadZ;

        EndRoadActor->SetActorTransform(FinalEndRoadTransform);

        MoveTaggedActorsWithRoad(EndRoadActor, OriginalEndRoadTransform, FinalEndRoadTransform);

        if (EndEnvironmentActor)
        {
            FVector EnvironmentLocation = EndEnvironmentActor->GetActorLocation();
            EnvironmentLocation.X = FinalEndRoadTransform.GetLocation().X;
            EnvironmentLocation.Y = FinalEndRoadTransform.GetLocation().Y;
            EnvironmentLocation.Z = EnvironmentBaseZ;
            EndEnvironmentActor->SetActorLocation(EnvironmentLocation);
        }

        if (EndLandscapeActor)
        {
            FVector LandscapeLocation = EndLandscapeActor->GetActorLocation();
            LandscapeLocation.Z += RoadZDelta;

            FVector Origin;
            FVector Extent;
            EndLandscapeActor->GetActorBounds(false, Origin, Extent);

            LandscapeLocation.X = FinalEndRoadTransform.GetLocation().X - Extent.X;
            LandscapeLocation.Y = FinalEndRoadTransform.GetLocation().Y - Extent.Y;

            EndLandscapeActor->SetActorLocation(LandscapeLocation);
        }

        if (FinalDeliveryZoneActor && bHasFinalDeliveryZoneRelativeTransform)
        {
            FinalDeliveryZoneActor->SetActorTransform(FinalDeliveryZoneRelativeTransform * FinalEndRoadTransform);
        }

        if (EndPostProcessVolume && bHasEndPostProcessRelativeTransform)
        {
            EndPostProcessVolume->SetActorTransform(EndPostProcessRelativeTransform * FinalEndRoadTransform);
        }

        NextAttachTransform = EndRoadActor->GetStartPointTransform();
    }

    for (int32 Index = MiddleZoneOrder.Num() - 1; Index >= 0; --Index)
    {
        const int32 TargetIndex = MiddleZoneOrder[Index];

        if (!MiddleRoadActors.IsValidIndex(TargetIndex) || !MiddleEnvironmentActors.IsValidIndex(TargetIndex))
        {
            UE_LOG(LogTemp, Error, TEXT("LevelManager: Invalid MiddleZoneOrder index [%d]."), TargetIndex);
            continue;
        }

        ARoadBase* Road = MiddleRoadActors[TargetIndex];
        ALevelFloorBase* Environment = MiddleEnvironmentActors[TargetIndex];

        ALandscape* LandscapeActor = MiddleLandscapeActors.IsValidIndex(TargetIndex) ? MiddleLandscapeActors[TargetIndex] : nullptr;
        AZonePostProcessVolume* PostProcessVolume = MiddlePostProcessVolumes.IsValidIndex(TargetIndex) ? MiddlePostProcessVolumes[TargetIndex] : nullptr;

        if (!Road || !Environment)
        {
            continue;
        }

        const FTransform OriginalRoadTransform = Road->GetActorTransform();

        FTransform PostProcessRelativeTransform;
        bool bHasPostProcessRelativeTransform = false;

        if (PostProcessVolume)
        {
            PostProcessRelativeTransform = PostProcessVolume->GetActorTransform().GetRelativeTransform(OriginalRoadTransform);
            bHasPostProcessRelativeTransform = true;
        }

        const float OriginalRoadZ = OriginalRoadTransform.GetLocation().Z;

        const FTransform RoadEndRelative = Road->GetEndPointTransform().GetRelativeTransform(Road->GetActorTransform());
        const FTransform FinalRoadTransform = RoadEndRelative.Inverse() * NextAttachTransform;
        const float RoadZDelta = FinalRoadTransform.GetLocation().Z - OriginalRoadZ;

        Road->SetActorTransform(FinalRoadTransform);

        MoveTaggedActorsWithRoad(Road, OriginalRoadTransform, FinalRoadTransform);

        FVector EnvironmentLocation = Environment->GetActorLocation();
        EnvironmentLocation.X = FinalRoadTransform.GetLocation().X;
        EnvironmentLocation.Y = FinalRoadTransform.GetLocation().Y;
        EnvironmentLocation.Z = EnvironmentBaseZ;
        Environment->SetActorLocation(EnvironmentLocation);

        if (LandscapeActor)
        {
            FVector LandscapeLocation = LandscapeActor->GetActorLocation();
            LandscapeLocation.Z += RoadZDelta;

            FVector Origin;
            FVector Extent;
            LandscapeActor->GetActorBounds(false, Origin, Extent);

            LandscapeLocation.X = FinalRoadTransform.GetLocation().X - Extent.X;
            LandscapeLocation.Y = FinalRoadTransform.GetLocation().Y - Extent.Y;

            LandscapeActor->SetActorLocation(LandscapeLocation);
        }

        if (PostProcessVolume && bHasPostProcessRelativeTransform)
        {
            PostProcessVolume->SetActorTransform(PostProcessRelativeTransform * FinalRoadTransform);
        }

        NextAttachTransform = Road->GetStartPointTransform();
    }

    if (StartRoadActor)
    {
        const FTransform OriginalStartRoadTransform = StartRoadActor->GetActorTransform();
        const float OriginalRoadZ = OriginalStartRoadTransform.GetLocation().Z;

        FTransform StartPostProcessRelativeTransform;
        bool bHasStartPostProcessRelativeTransform = false;

        if (StartPostProcessVolume)
        {
            StartPostProcessRelativeTransform = StartPostProcessVolume->GetActorTransform().GetRelativeTransform(OriginalStartRoadTransform);
            bHasStartPostProcessRelativeTransform = true;
        }

        const FTransform RoadEndRelative = StartRoadActor->GetEndPointTransform().GetRelativeTransform(StartRoadActor->GetActorTransform());
        const FTransform FinalStartRoadTransform = RoadEndRelative.Inverse() * NextAttachTransform;
        const float RoadZDelta = FinalStartRoadTransform.GetLocation().Z - OriginalRoadZ;

        StartRoadActor->SetActorTransform(FinalStartRoadTransform);

        MoveTaggedActorsWithRoad(StartRoadActor, OriginalStartRoadTransform, FinalStartRoadTransform);

        if (StartEnvironmentActor)
        {
            FVector EnvironmentLocation = StartEnvironmentActor->GetActorLocation();
            EnvironmentLocation.X = FinalStartRoadTransform.GetLocation().X;
            EnvironmentLocation.Y = FinalStartRoadTransform.GetLocation().Y;
            EnvironmentLocation.Z = EnvironmentBaseZ;
            StartEnvironmentActor->SetActorLocation(EnvironmentLocation);
        }

        if (StartLandscapeActor)
        {
            FVector LandscapeLocation = StartLandscapeActor->GetActorLocation();
            LandscapeLocation.Z += RoadZDelta;

            FVector Origin;
            FVector Extent;
            StartLandscapeActor->GetActorBounds(false, Origin, Extent);

            LandscapeLocation.X = FinalStartRoadTransform.GetLocation().X - Extent.X;
            LandscapeLocation.Y = FinalStartRoadTransform.GetLocation().Y - Extent.Y;

            StartLandscapeActor->SetActorLocation(LandscapeLocation);
        }

        if (StartPostProcessVolume && bHasStartPostProcessRelativeTransform)
        {
            StartPostProcessVolume->SetActorTransform(StartPostProcessRelativeTransform * FinalStartRoadTransform);
        }
    }

    auto SetRoadComponentsStatic = [](ARoadBase* Road)
    {
        if (!Road)
        {
            return;
        }

        // 도로 액터의 RootComponent는 원래대로 Static 전환
        if (USceneComponent* RootComponent = Road->GetRootComponent())
        {
            RootComponent->SetMobility(EComponentMobility::Static);
        }

        TArray<USceneComponent*> Components;
        Road->GetComponents<USceneComponent>(Components);

        for (USceneComponent* Component : Components)
        {
            if (Component)
            {
                // PCGComponent만 Mobility 변경 대상에서 제외
                if (Component->IsA(UPCGComponent::StaticClass()))
                {
                    continue;
                }

                Component->SetMobility(EComponentMobility::Static);
            }
        }
    };
    
    SetRoadComponentsStatic(StartRoadActor);
    SetRoadComponentsStatic(EndRoadActor);
    
    for (ARoadBase* Road : MiddleRoadActors)
    {
        SetRoadComponentsStatic(Road);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("LevelManager: Auto arrangement complete."));
}

void ALevelManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ALevelManager, bUseAutoArrange);
    DOREPLIFETIME(ALevelManager, bShuffleMiddleZones);
    DOREPLIFETIME(ALevelManager, MiddleZoneOrder);
    DOREPLIFETIME(ALevelManager, PCGSeed);
}
