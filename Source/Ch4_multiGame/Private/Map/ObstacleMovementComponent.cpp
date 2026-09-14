#include "Map/ObstacleMovementComponent.h"
#include "GameFramework/Actor.h"

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

    InitialRelativeLocation = GetRelativeLocation();

    if (bUseRotatedMovementDirection)
    {
        NormalizedMovementDirection = GetComponentTransform().TransformVectorNoScale(MovementDirection).GetSafeNormal();
    }
    else
    {
        NormalizedMovementDirection = MovementDirection.GetSafeNormal();
    }

    NormalizedRotationDirection = RotationDirection.GetSafeNormal();

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
    if (!OwnerActor || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (bUseMovement && MovementDistance > 0.0f && MovementSpeed > 0.0f && !NormalizedMovementDirection.IsNearlyZero())
    {
        MovementElapsedTime += DeltaTime;

        const float CycleDistance = MovementDistance * 2.0f;
        const float DistanceAlongPath = FMath::Fmod(MovementElapsedTime * MovementSpeed, CycleDistance);
        const float CurrentDistance = DistanceAlongPath <= MovementDistance ? DistanceAlongPath : CycleDistance - DistanceAlongPath;

        const FVector NewRelativeLocation = InitialRelativeLocation + NormalizedMovementDirection * CurrentDistance;
        SetRelativeLocation(NewRelativeLocation);
    }

    if (bUseRotation && RotationSpeed > 0.0f && !NormalizedRotationDirection.IsNearlyZero())
    {
        const float RotationAngleRadians = FMath::DegreesToRadians(RotationSpeed * DeltaTime);
        const FQuat RotationQuat(NormalizedRotationDirection, RotationAngleRadians);

        AddLocalRotation(RotationQuat);
    }
}