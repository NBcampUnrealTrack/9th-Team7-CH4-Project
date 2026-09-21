#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ObstacleMovementComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CH4_MULTIGAME_API UObstacleMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UObstacleMovementComponent();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    // 왕복 이동 사용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Movement")
    bool bUseMovement = true;

    // 이동 방향
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Movement")
    FVector MovementDirection = FVector(1.0f, 0.0f, 0.0f);

    // true: 장애물의 현재 회전 방향을 이동 방향에 적용
    // false: MovementDirection을 액터의 회전과 관계없이 사용
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Movement")
    bool bUseRotatedMovementDirection = false;

    // 이동 속도 (cm/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Movement", meta = (ClampMin = "0.0"))
    float MovementSpeed = 200.0f;

    // 시작 위치에서 이동할 최대 거리 (cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Movement", meta = (ClampMin = "0.0"))
    float MovementDistance = 500.0f;

    // 회전 사용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Rotation")
    bool bUseRotation = false;

    // 회전 축
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Rotation")
    FVector RotationDirection = FVector(0.0f, 0.0f, 1.0f);

    // 회전 속도 (도/초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle Rotation", meta = (ClampMin = "0.0"))
    float RotationSpeed = 90.0f;

private:
    // 게임 시작 시점 소유 액터의 상대 위치
    FVector InitialRelativeLocation;

    // 이동 경과 시간
    float MovementElapsedTime = 0.0f;

    // 실제로 사용할 이동 방향
    FVector NormalizedMovementDirection;

    // 회전 축
    FVector NormalizedRotationDirection;

    // 레벨 매니저에 의한 도로 정렬 완료 후 초기 위치가 확정되었는지 여부
    bool bInitializedLocation = false;
};