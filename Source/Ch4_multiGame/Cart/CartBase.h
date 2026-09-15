#pragma once

#include "CoreMinimal.h"
#include "Cart/CartStabilizationMath.h"
#include "GameFramework/Actor.h"
#include "CartBase.generated.h"

class ACh4_PlayerCharacter;
class UPhysicsConstraintComponent;

UCLASS()
class CH4_MULTIGAME_API ACartBase : public AActor
{
    GENERATED_BODY()

public:
    ACartBase();
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Server-only Cart operations. Remote clients reach these through their owned Character RPC. */
    bool TryGrabPlayer(ACh4_PlayerCharacter* Player);
    bool ReleasePlayer(ACh4_PlayerCharacter* Player);
    bool SetPlayerMoveInput(ACh4_PlayerCharacter* Player, const FVector2D& Input);

    /** Explicit preparation ownership; Waiting alone never locks the Cart. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cart|Preparation")
    bool SetPreparationLocked(bool bLocked);

    UFUNCTION(BlueprintPure, Category="Cart|Preparation")
    bool IsPreparationLocked() const { return bPreparationLocked; }

    /** 이 플레이어가 배정받은 앵커. 없으면 nullptr. */
    USceneComponent* GetAnchorFor(const ACh4_PlayerCharacter* Player) const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category="Cart")
    TObjectPtr<USceneComponent> CartRoot;
    
    UPROPERTY(VisibleAnywhere, Category="Cart")
    TObjectPtr<UStaticMeshComponent> CartMesh;

    /** World-up angular safety net. Translation and yaw remain unrestricted. */
    UPROPERTY(VisibleAnywhere, Category="Cart|Stabilization")
    TObjectPtr<UPhysicsConstraintComponent> UprightSafetyConstraint;

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

    // ── 앵커별 힘 배율 ──
    /** 손잡이(앵커 0, 1). 주 추진 담당이라 앞뒤가 세다. */
    UPROPERTY(EditAnywhere, Category="Cart|Push|Anchor Scale")
    float HandleForwardScale = 1.5f;

    UPROPERTY(EditAnywhere, Category="Cart|Push|Anchor Scale")
    float HandleSideScale = 0.3f;

    /** 중간 사이드(앵커 3, 5). 뒷바퀴 근처라 밀면 뒤가 밀린다. */
    UPROPERTY(EditAnywhere, Category="Cart|Push|Anchor Scale")
    float MidForwardScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="Cart|Push|Anchor Scale")
    float MidSideScale = 0.3f;

    /** 앞 사이드(앵커 4, 6). 지렛대가 길어 방향 전환이 잘 된다. */
    UPROPERTY(EditAnywhere, Category="Cart|Push|Anchor Scale")
    float FrontForwardScale = 1.5f;

    UPROPERTY(EditAnywhere, Category="Cart|Push|Anchor Scale")
    float FrontSideScale = 0.3f;

    UPROPERTY(EditAnywhere, Category="Cart|Grab")
    float GrabDistance = 150.0f;
    
    // ── 자세 안정화 ──
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization")
    FVector CartCenterOfMassOffset = FVector(-20.0f, 0.0f, -18.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.01"))
    FVector CartInertiaTensorScale = FVector(2.0f, 2.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0"))
    float CartAngularDamping = 3.0f;

    /** Degrees per second. UPrimitiveComponent converts this for Chaos. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0", Units="DegreesPerSecond"))
    float MaximumAngularVelocityDegrees = 240.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0", ClampMax="89.0", Units="Degrees"))
    float StabilizationDeadZoneDegrees = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0", ClampMax="89.0", Units="Degrees"))
    float StabilizationFullAssistDegrees = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0"))
    float StabilizationProportionalGain = 6.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0"))
    float StabilizationDerivativeGain = 3.0f;

    /** Maximum correction passed to AddTorqueInRadians with acceleration change enabled. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization", meta=(ClampMin="0.0"))
    float MaximumCorrectionAngularAcceleration = 8.0f;

    /** World-up correction only starts at this angle while fewer than two wheels touch ground. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Ground Reference", meta=(ClampMin="0.0", ClampMax="89.0", Units="Degrees"))
    float EmergencyWorldUpTiltDegrees = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Ground Reference", meta=(ClampMin="1", ClampMax="4"))
    int32 MinimumGroundContactCount = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Ground Reference", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MinimumGroundNormalWorldUpDot = 0.2f;

    /** Extra continuous damping near the safety limit lowers the impact speed before the hard wall. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint", meta=(ClampMin="1.0"))
    float LimitApproachDampingMultiplier = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint")
    bool bEnableUprightSafetyConstraint = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint", meta=(ClampMin="1.0", ClampMax="89.0", Units="Degrees"))
    float MaximumTiltAngleDegrees = 55.0f;

    /** Off preserves the non-compliant 55-degree final safety wall. Enable for PIE comparison only. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint")
    bool bUseSoftAngularLimit = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint",
        meta=(ClampMin="0.0", EditCondition="bUseSoftAngularLimit"))
    float SoftAngularLimitStiffness = 50.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint",
        meta=(ClampMin="0.0", EditCondition="bUseSoftAngularLimit"))
    float SoftAngularLimitDamping = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Stabilization|Constraint",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float ConstraintRestitution = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cart|Debug")
    bool bDrawCartPhysicsDebug = false;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCh4CartStabilizationConfigurationTest;
#endif

    /** 앵커 인덱스별 점유자. 클라이언트도 손 위치 맞추려면 알아야 하므로 복제. */
    UPROPERTY(Replicated)
    TArray<TObjectPtr<ACh4_PlayerCharacter>> AnchorOccupants;

    /** 클라이언트가 복제된 위치를 따라가는 속도. 클수록 빠르게 붙는다. */
    UPROPERTY(EditAnywhere, Category="Cart|Network", meta=(ClampMin="1.0"))
    float ClientInterpSpeed = 12.0f;
    
    UPROPERTY(ReplicatedUsing = OnRep_PreparationLocked)
    bool bPreparationLocked = false;

    UFUNCTION()
    void OnRep_PreparationLocked();

    bool bWasSimulatingBeforePreparationLock = true;

    /** 서버 전용. 플레이어별 최신 입력값. */
    TMap<TObjectPtr<ACh4_PlayerCharacter>, FVector2D> PlayerInputs;

    /** Current server-frame suspension contacts, reused by stabilization. */
    TArray<FVector> SuspensionContactNormals;

    FVector LastStabilizationTargetUp = FVector::ZeroVector;
    float LastStabilizationTiltDegrees = 0.0f;
    float EffectiveStabilizationDeadZoneDegrees = 8.0f;
    float EffectiveStabilizationFullAssistDegrees = 30.0f;
    float EffectiveMaximumTiltAngleDegrees = 55.0f;

    void ApplySuspension(float DeltaTime);
    void ApplyGrip(float DeltaTime);
    void ApplyPlayerForces(float DeltaTime);

    int32 FindClosestFreeAnchor(const ACh4_PlayerCharacter* Player) const;
    void ReleaseAnchorFor(const ACh4_PlayerCharacter* Player);
    void CleanupInvalidPlayers();

    void ApplyUprightStabilization(float DeltaTime);
    void InitializeStabilizationSettings();
    void ConfigureUprightSafetyConstraint();
    void DrawCartPhysicsDebug() const;
    
    /** 서버: 물리 결과를 액터 루트에 반영해 복제되게 한다. */
    void SyncRootToPhysics();

    /** 클라이언트: 복제된 루트 위치로 메시를 부드럽게 따라가게 한다. */
    void InterpolateClientTransform(float DeltaTime);
};
