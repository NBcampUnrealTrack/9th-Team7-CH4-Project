#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CartBase.generated.h"

class ACh4_PlayerCharacter;

UCLASS()
class CH4_MULTIGAME_API ACartBase : public AActor
{
    GENERATED_BODY()

public:
    ACartBase();
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 잡기 요청. 서버가 가장 가까운 빈 앵커를 찾아 배정한다. */
    UFUNCTION(Server, Reliable)
    void ServerRequestGrab(ACh4_PlayerCharacter* Player);

    /** 놓기 요청. */
    UFUNCTION(Server, Reliable)
    void ServerRequestRelease(ACh4_PlayerCharacter* Player);

    /** 매 프레임 입력 전달. 유실돼도 다음 프레임에 갱신되므로 Unreliable. */
    UFUNCTION(Server, Unreliable)
    void ServerSetMoveInput(ACh4_PlayerCharacter* Player, FVector2D Input);

    /** 이 플레이어가 배정받은 앵커. 없으면 nullptr. */
    USceneComponent* GetAnchorFor(const ACh4_PlayerCharacter* Player) const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Cart")
    TObjectPtr<UStaticMeshComponent> CartMesh;

    UPROPERTY(VisibleAnywhere, Category="Cart|Wheels")
    TArray<TObjectPtr<USceneComponent>> Wheels;

    UPROPERTY(VisibleAnywhere, Category="Cart|Grab")
    TArray<TObjectPtr<USceneComponent>> Anchors;

    // ── 서스펜션 ──
    UPROPERTY(EditAnywhere, Category="Cart|Suspension")
    float SuspRestLength = 19.0f;

    UPROPERTY(EditAnywhere, Category="Cart|Suspension")
    float SpringStrength = 80000.0f;

    UPROPERTY(EditAnywhere, Category="Cart|Suspension")
    float SpringDamper = 3500.0f;

    // ── 그립 ──
    UPROPERTY(EditAnywhere, Category="Cart|Grip")
    float GripFront = -500.0f;

    UPROPERTY(EditAnywhere, Category="Cart|Grip")
    float GripRear = -1500.0f;

    // ── 조작 ──
    UPROPERTY(EditAnywhere, Category="Cart|Push")
    float PushForce = 30000.0f;

    UPROPERTY(EditAnywhere, Category="Cart|Push")
    float BrakeForce = 3900.0f;

    UPROPERTY(EditAnywhere, Category="Cart|Grab")
    float GrabDistance = 150.0f;
    
    // ── 자세 복원 ──
    UPROPERTY(EditAnywhere, Category="Cart|Upright")
    float UprightTorque = 3900.0f;

private:
    /** 앵커 인덱스별 점유자. 클라이언트도 손 위치 맞추려면 알아야 하므로 복제. */
    UPROPERTY(Replicated)
    TArray<TObjectPtr<ACh4_PlayerCharacter>> AnchorOccupants;

    /** 서버 전용. 플레이어별 최신 입력값. */
    TMap<TObjectPtr<ACh4_PlayerCharacter>, FVector2D> PlayerInputs;

    void ApplySuspension(float DeltaTime);
    void ApplyGrip(float DeltaTime);
    void ApplyPlayerForces(float DeltaTime);

    int32 FindClosestFreeAnchor(const ACh4_PlayerCharacter* Player) const;
    void ReleaseAnchorFor(const ACh4_PlayerCharacter* Player);
    
    void ApplyUprightTorque(float DeltaTime);
};