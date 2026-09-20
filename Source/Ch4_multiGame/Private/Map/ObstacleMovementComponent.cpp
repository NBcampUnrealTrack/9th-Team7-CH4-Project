#include "Map/ObstacleMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

UObstacleMovementComponent::UObstacleMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UObstacleMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    // 이동 방향 및 회전 축 정규화
    if (bUseRotatedMovementDirection)
    {
        NormalizedMovementDirection = OwnerActor->GetActorTransform().TransformVectorNoScale(MovementDirection).GetSafeNormal();
    }
    else
    {
        NormalizedMovementDirection = MovementDirection.GetSafeNormal();
    }

    NormalizedRotationDirection = RotationDirection.GetSafeNormal();

    // 멀티플레이어 환경: 서버에서만 이동 동기화 설정
    if (OwnerActor->HasAuthority())
    {
        OwnerActor->SetReplicates(true);
        OwnerActor->SetReplicateMovement(true);
    }
}

void UObstacleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* OwnerActor = GetOwner();

    // 멀티플레이어 권한 체크: 서버에서만 이동 연산 실행
    if (!OwnerActor || !OwnerActor->HasAuthority())
    {
        return;
    }

    // [핵심] LevelManager의 BeginPlay 도로 정렬이 완벽히 끝난 첫 번째 Tick 시점에 최신 상대 위치를 기준점으로 확정
    if (!bInitializedLocation)
    {
        if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
        {
            InitialRelativeLocation = RootComp->GetRelativeLocation();
            bInitializedLocation = true;
        }
    }

    // 1. 왕복 이동 처리
    if (bUseMovement && MovementDistance > 0.0f && MovementSpeed > 0.0f && !NormalizedMovementDirection.IsNearlyZero())
    {
        MovementElapsedTime += DeltaTime;

        const float CycleDistance = MovementDistance * 2.0f;
        const float DistanceAlongPath = FMath::Fmod(MovementElapsedTime * MovementSpeed, CycleDistance);
        const float CurrentDistance = DistanceAlongPath <= MovementDistance ? DistanceAlongPath : CycleDistance - DistanceAlongPath;

        const FVector NewRelativeLocation = InitialRelativeLocation + NormalizedMovementDirection * CurrentDistance;
        
        // 소유 액터 전체의 상대 위치 업데이트
        OwnerActor->SetActorRelativeLocation(NewRelativeLocation);
    }

    // 2. 회전 처리
    if (bUseRotation && RotationSpeed > 0.0f && !NormalizedRotationDirection.IsNearlyZero())
    {
        const float RotationAngleRadians = FMath::DegreesToRadians(RotationSpeed * DeltaTime);
        const FQuat RotationQuat(NormalizedRotationDirection, RotationAngleRadians);

        // 소유 액터 전체 회전
        OwnerActor->AddActorLocalRotation(RotationQuat);
    }
}