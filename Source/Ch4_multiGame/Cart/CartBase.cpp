#include "Cart/CartBase.h"

#include "Ch4_multiGame.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Player/Ch4_PlayerCharacter.h"

ACartBase::ACartBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // 멀티: 서버가 물리를 돌리고 클라는 결과를 받는다.
    bReplicates = true;
    SetReplicateMovement(true);

    // 물리 물체라 위치가 자주 바뀐다. 복제 빈도를 올려 보간 품질을 높인다.
    SetNetUpdateFrequency(60.0f);
    SetMinNetUpdateFrequency(30.0f);

    // ── 카트 본체 ──
    CartRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CartRoot"));
    SetRootComponent(CartRoot);

    CartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartMesh"));
    CartMesh->SetupAttachment(CartRoot);

    // 물리로 독립해서 움직이므로 부모 트랜스폼을 따르지 않는다.
    CartMesh->SetUsingAbsoluteLocation(true);
    CartMesh->SetUsingAbsoluteRotation(true);

    UprightSafetyConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("UprightSafetyConstraint"));
    UprightSafetyConstraint->SetupAttachment(CartMesh);
    
    CartMesh->SetLinearDamping(0.5f);
    CartMesh->SetAngularDamping(CartAngularDamping);

    CartMesh->SetCollisionObjectType(ECC_PhysicsBody);
    CartMesh->SetCollisionResponseToAllChannels(ECR_Block);

    CartMesh->SetSimulatePhysics(true);
    
    // ── 이하 바퀴, 앵커는 기존과 동일 ──
    
    // ── 바퀴 (서스펜션 레이 시작점) ──
    const FVector WheelOffsets[] = {
        FVector(-106.0f, -67.0f, -25.0f),   // FL
        FVector(-106.0f,  67.0f, -25.0f),   // FR
        FVector(  63.0f, -67.0f, -25.0f),   // RL
        FVector(  63.0f,  67.0f, -25.0f),   // RR
    };
    const TCHAR* WheelNames[] = { TEXT("Wheel_FL"), TEXT("Wheel_FR"),
                                  TEXT("Wheel_RL"), TEXT("Wheel_RR") };

    for (int32 i = 0; i < 4; ++i)
    {
        USceneComponent* Wheel = CreateDefaultSubobject<USceneComponent>(WheelNames[i]);
        Wheel->SetupAttachment(CartMesh);
        Wheel->SetRelativeLocation(WheelOffsets[i]);
        Wheels.Add(Wheel);
    }

    // ── 앵커 (플레이어가 서는 자리) ──
    const FVector AnchorLocations[] = {
        FVector( 240.0f,  -50.0f, 38.0f),
        FVector( 240.0f,   50.0f, 38.0f),
        FVector(  50.0f, -190.0f, 38.0f),
        FVector( -50.0f, -190.0f, 38.0f),
        FVector(  50.0f,  190.0f, 38.0f),
        FVector( -50.0f,  190.0f, 38.0f),
    };
    const FRotator AnchorRotations[] = {
        FRotator(0.0f, 180.0f, 0.0f),
        FRotator(0.0f, 180.0f, 0.0f),
        FRotator(0.0f,  90.0f, 0.0f),
        FRotator(0.0f,  90.0f, 0.0f),
        FRotator(0.0f, -90.0f, 0.0f),
        FRotator(0.0f, -90.0f, 0.0f),
    };

    for (int32 i = 0; i < 6; ++i)
    {
        const FString Name = FString::Printf(TEXT("Anchor_%d"), i + 1);
        USceneComponent* Anchor = CreateDefaultSubobject<USceneComponent>(*Name);
        Anchor->SetupAttachment(CartMesh);
        Anchor->SetRelativeLocation(AnchorLocations[i]);
        Anchor->SetRelativeRotation(AnchorRotations[i]);
        Anchors.Add(Anchor);
    }

    AnchorOccupants.SetNum(6);
}

void ACartBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACartBase, AnchorOccupants);
    DOREPLIFETIME(ACartBase, bPreparationLocked);
}

void ACartBase::BeginPlay()
{
    Super::BeginPlay();

    if (!CartMesh)
    {
        return;
    }

    // 메시가 절대 좌표를 쓰므로, 시작 시 액터 위치와 일치시킨다.
    if (CartRoot)
    {
        CartMesh->SetWorldLocationAndRotation(
            CartRoot->GetComponentLocation(),
            CartRoot->GetComponentRotation());
    }
    
    InitializeStabilizationSettings();
    if (HasAuthority())
    {
        CartMesh->SetEnableGravity(true);
        CartMesh->SetSimulatePhysics(!bPreparationLocked);
        CartMesh->SetMassOverrideInKg(NAME_None, 220.0f, true);
        CartMesh->SetAngularDamping(FMath::Max(CartAngularDamping, 0.0f));

        FBodyInstance* BodyInstance = CartMesh->GetBodyInstance();
        if (BodyInstance)
        {
            BodyInstance->InertiaTensorScale = FVector(
                FMath::Max(CartInertiaTensorScale.X, UE_KINDA_SMALL_NUMBER),
                FMath::Max(CartInertiaTensorScale.Y, UE_KINDA_SMALL_NUMBER),
                FMath::Max(CartInertiaTensorScale.Z, UE_KINDA_SMALL_NUMBER));
            BodyInstance->UpdateMassProperties();
        }

        CartMesh->SetCenterOfMass(CartCenterOfMassOffset);
        CartMesh->SetPhysicsMaxAngularVelocityInDegrees(
            FMath::Max(MaximumAngularVelocityDegrees, 0.0f), false, NAME_None);
        ConfigureUprightSafetyConstraint();
    }
    else
    {
        DisableClientPhysics();
    }

    UE_LOG(LogCh4_multiGame, Log,
        TEXT("[CartNetwork] Initialized Cart=%s Role=%s PreparationLocked=%d Physics=%d Root=%s Mesh=%s ComponentReplicated=%d"),
        *GetName(), HasAuthority() ? TEXT("Server") : TEXT("RemoteClient"), bPreparationLocked ? 1 : 0,
        CartMesh->IsSimulatingPhysics() ? 1 : 0,
        *CartRoot->GetComponentLocation().ToString(), *CartMesh->GetComponentLocation().ToString(),
        CartMesh->GetIsReplicated() ? 1 : 0);
}

void ACartBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority())
    {
        InterpolateClientTransform(DeltaTime);
        return;
    }

    CleanupInvalidPlayers();

    if (bPreparationLocked || !CartMesh || !CartMesh->IsSimulatingPhysics())
    {
        return;
    }

    ApplySuspension(DeltaTime);
    ApplyGrip(DeltaTime);
    ApplyUprightStabilization(DeltaTime);
    ApplyPlayerForces(DeltaTime);

    SyncRootToPhysics();

    if (bDrawCartPhysicsDebug)
    {
        DrawCartPhysicsDebug();
    }
}

void ACartBase::OnRep_PreparationLocked()
{
    if (!HasAuthority())
    {
        DisableClientPhysics();
        UE_LOG(LogCh4_multiGame, Log,
            TEXT("[CartNetwork] Client preparation state Cart=%s Locked=%d Physics=%d"),
            *GetName(), bPreparationLocked ? 1 : 0,
            CartMesh && CartMesh->IsSimulatingPhysics() ? 1 : 0);
    }
}

void ACartBase::ApplySuspension(float DeltaTime)
{
    SuspensionContactNormals.Reset(Wheels.Num());

    UWorld* World = GetWorld();
    if (!World || !CartMesh)
    {
        return;
    }

    const FVector CartUp = CartMesh->GetUpVector();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    for (USceneComponent* Wheel : Wheels)
    {
        if (!Wheel)
        {
            continue;
        }

        const FVector Start = Wheel->GetComponentLocation();
        const FVector End = Start - CartUp * SuspRestLength;

        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(
            Hit, Start, End, ECC_Visibility, Params);
        if (bDrawCartPhysicsDebug)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("[CartPhysics] Wheel %s | Hit:%d | Dist:%.1f"),
                *Wheel->GetName(), bHit ? 1 : 0, bHit ? Hit.Distance : -1.0f);
            DrawDebugLine(World, Start, End, bHit ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);
        }

        if (!bHit)
        {
            continue;   // 바퀴가 떠 있으면 힘을 주지 않는다.
        }

        const FVector ContactNormal = Hit.ImpactNormal.GetSafeNormal();
        if (!ContactNormal.ContainsNaN()
            && !ContactNormal.IsNearlyZero()
            && FVector::DotProduct(ContactNormal, FVector::UpVector) >= 0.2f)
        {
            SuspensionContactNormals.Add(ContactNormal);
        }

        // 눌린 정도: 지면이 가까울수록 크다.
        const float Compression = SuspRestLength - Hit.Distance;

        // 그 지점의 상하 속도. 스프링이 튕기는 걸 억제한다.
        const FVector PointVelocity = CartMesh->GetPhysicsLinearVelocityAtPoint(Start);
        const float UpSpeed = FVector::DotProduct(PointVelocity, CartUp);

        const float ForceSize = (Compression * SpringStrength) - (UpSpeed * SpringDamper);

        CartMesh->AddForceAtLocation(CartUp * ForceSize, Start);
    }
}

void ACartBase::ApplyGrip(float DeltaTime)
{
    if (!CartMesh)
    {
        return;
    }

    const FVector CartRight = CartMesh->GetRightVector();

    for (int32 i = 0; i < Wheels.Num(); ++i)
    {
        USceneComponent* Wheel = Wheels[i];
        if (!Wheel)
        {
            continue;
        }

        const FVector WheelLocation = Wheel->GetComponentLocation();

        // 이 바퀴가 옆으로 얼마나 미끄러지고 있는가.
        const FVector PointVelocity = CartMesh->GetPhysicsLinearVelocityAtPoint(WheelLocation);
        const float SideSpeed = FVector::DotProduct(PointVelocity, CartRight);

        // 앞바퀴는 캐스터라 약하게, 뒷바퀴는 강하게 잡는다.
        const float Grip = (i < 2) ? GripFront : GripRear;

        CartMesh->AddForceAtLocation(CartRight * SideSpeed * Grip, WheelLocation);
    }
}

void ACartBase::ApplyUprightStabilization(float DeltaTime)
{
    if (!CartMesh)
    {
        return;
    }

    const FVector CartUp = CartMesh->GetUpVector();
    const FVector AngularVelocityRadians = CartMesh->GetPhysicsAngularVelocityInRadians();

    FVector TargetUp = FVector::ZeroVector;
    const bool bHasGroundReference = Ch4CartStabilization::TryCalculateAverageGroundNormal(
        SuspensionContactNormals,
        FMath::Clamp(MinimumGroundContactCount, 1, FMath::Max(Wheels.Num(), 1)),
        FMath::Clamp(MinimumGroundNormalWorldUpDot, 0.0f, 1.0f),
        TargetUp);

    if (!bHasGroundReference)
    {
        const Ch4CartStabilization::FCorrectionResult WorldUpResult =
            Ch4CartStabilization::CalculateCorrection(
                CartUp,
                FVector::UpVector,
                AngularVelocityRadians,
                EffectiveStabilizationDeadZoneDegrees,
                EffectiveStabilizationFullAssistDegrees,
                StabilizationProportionalGain,
                StabilizationDerivativeGain,
                MaximumCorrectionAngularAcceleration);

        if (!WorldUpResult.bIsValid
            || WorldUpResult.TiltAngleDegrees < FMath::Max(EmergencyWorldUpTiltDegrees, 0.0f))
        {
            LastStabilizationTargetUp = FVector::ZeroVector;
            LastStabilizationTiltDegrees = WorldUpResult.TiltAngleDegrees;
            return;
        }

        TargetUp = FVector::UpVector;
    }

    const Ch4CartStabilization::FCorrectionResult BaseCorrection =
        Ch4CartStabilization::CalculateCorrection(
            CartUp,
            TargetUp,
            AngularVelocityRadians,
            EffectiveStabilizationDeadZoneDegrees,
            EffectiveStabilizationFullAssistDegrees,
            StabilizationProportionalGain,
            StabilizationDerivativeGain,
            MaximumCorrectionAngularAcceleration);

    const FVector SafeCartUp = CartUp.GetSafeNormal();
    const float ConstraintTiltDegrees = SafeCartUp.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(SafeCartUp, FVector::UpVector), -1.0f, 1.0f)));
    const float LimitApproachAlpha = bEnableUprightSafetyConstraint
        ? Ch4CartStabilization::CalculateAssistAlpha(
            ConstraintTiltDegrees,
            EffectiveStabilizationFullAssistDegrees,
            EffectiveMaximumTiltAngleDegrees)
        : 0.0f;
    const float EffectiveDerivativeGain = FMath::Max(StabilizationDerivativeGain, 0.0f)
        * FMath::Lerp(1.0f, FMath::Max(LimitApproachDampingMultiplier, 1.0f), LimitApproachAlpha);
    const Ch4CartStabilization::FCorrectionResult Correction = LimitApproachAlpha > 0.0f
        ? Ch4CartStabilization::CalculateCorrection(
            CartUp,
            TargetUp,
            AngularVelocityRadians,
            EffectiveStabilizationDeadZoneDegrees,
            EffectiveStabilizationFullAssistDegrees,
            StabilizationProportionalGain,
            EffectiveDerivativeGain,
            MaximumCorrectionAngularAcceleration)
        : BaseCorrection;

    LastStabilizationTargetUp = TargetUp;
    LastStabilizationTiltDegrees = Correction.TiltAngleDegrees;

    if (!Correction.bIsValid || Correction.AngularAcceleration.IsNearlyZero())
    {
        return;
    }

    CartMesh->AddTorqueInRadians(Correction.AngularAcceleration, NAME_None, true);
}

void ACartBase::InitializeStabilizationSettings()
{
    const Ch4CartStabilization::FAngleSettings AngleSettings =
        Ch4CartStabilization::SanitizeAngleSettings(
            StabilizationDeadZoneDegrees,
            StabilizationFullAssistDegrees,
            MaximumTiltAngleDegrees);
    EffectiveStabilizationDeadZoneDegrees = AngleSettings.DeadZoneDegrees;
    EffectiveStabilizationFullAssistDegrees = AngleSettings.FullAssistDegrees;
    EffectiveMaximumTiltAngleDegrees = AngleSettings.MaximumTiltDegrees;

    if (HasAuthority() && AngleSettings.bWasAdjusted)
    {
        UE_LOG(LogCh4_multiGame, Warning,
            TEXT("[CartPhysics] Invalid stabilization angle order on %s: Dead=%.2f Full=%.2f Max=%.2f; using Dead=%.2f Full=%.2f Max=%.2f"),
            *GetNameSafe(this), StabilizationDeadZoneDegrees, StabilizationFullAssistDegrees,
            MaximumTiltAngleDegrees, EffectiveStabilizationDeadZoneDegrees,
            EffectiveStabilizationFullAssistDegrees, EffectiveMaximumTiltAngleDegrees);
    }
}

void ACartBase::ConfigureUprightSafetyConstraint()
{
    if (!UprightSafetyConstraint || !CartMesh)
    {
        return;
    }

    UprightSafetyConstraint->TermComponentConstraint();
    if (!bEnableUprightSafetyConstraint)
    {
        UE_LOG(LogCh4_multiGame, Log, TEXT("[CartPhysics] %s upright safety constraint disabled"),
            *GetNameSafe(this));
        return;
    }

    const float SafeStiffness = FMath::Max(SoftAngularLimitStiffness, 0.0f);
    const float SafeDamping = FMath::Max(SoftAngularLimitDamping, 0.0f);
    const bool bUseSafeSoftLimit = bUseSoftAngularLimit && (SafeStiffness > 0.0f || SafeDamping > 0.0f);
    if (bUseSoftAngularLimit && !bUseSafeSoftLimit)
    {
        UE_LOG(LogCh4_multiGame, Warning,
            TEXT("[CartPhysics] %s soft angular limit has zero stiffness and damping; using the hard safety limit"),
            *GetNameSafe(this));
    }

    UprightSafetyConstraint->SetLinearXLimit(LCM_Free, 0.0f);
    UprightSafetyConstraint->SetLinearYLimit(LCM_Free, 0.0f);
    UprightSafetyConstraint->SetLinearZLimit(LCM_Free, 0.0f);
    UprightSafetyConstraint->SetAngularTwistLimit(ACM_Free, 0.0f);
    UprightSafetyConstraint->SetAngularSwing1Limit(ACM_Limited, EffectiveMaximumTiltAngleDegrees);
    UprightSafetyConstraint->SetAngularSwing2Limit(ACM_Limited, EffectiveMaximumTiltAngleDegrees);
    UprightSafetyConstraint->ConstraintInstance.SetSoftSwingLimitParams(
        bUseSafeSoftLimit,
        SafeStiffness,
        SafeDamping,
        FMath::Clamp(ConstraintRestitution, 0.0f, 1.0f),
        0.0f);
    UprightSafetyConstraint->SetAngularBreakable(false, 0.0f);
    UprightSafetyConstraint->SetProjectionEnabled(false);
    UprightSafetyConstraint->SetDisableCollision(true);

    FVector HorizontalForward = FVector::VectorPlaneProject(CartMesh->GetForwardVector(), FVector::UpVector).GetSafeNormal();
    if (HorizontalForward.IsNearlyZero())
    {
        HorizontalForward = FVector::ForwardVector;
    }

    UprightSafetyConstraint->SetWorldLocation(CartMesh->GetCenterOfMass());
    UprightSafetyConstraint->SetWorldRotation(FRotationMatrix::MakeFromXY(FVector::UpVector, HorizontalForward).Rotator());
    UprightSafetyConstraint->SetConstrainedComponents(CartMesh, NAME_None, nullptr, NAME_None);

    // Primary is the twist axis. Local Z against World Z keeps yaw free while both swing axes cap tilt.
    UprightSafetyConstraint->SetConstraintReferenceOrientation(
        EConstraintFrame::Frame1, FVector::UpVector, FVector::ForwardVector);
    UprightSafetyConstraint->SetConstraintReferenceOrientation(
        EConstraintFrame::Frame2, FVector::UpVector, HorizontalForward);

    UE_LOG(LogCh4_multiGame, Log,
        TEXT("[CartPhysics] %s stabilization initialized | ConstraintValid=%d | MaxTilt=%.1f | Limit=%s | SoftK=%.1f SoftD=%.1f | MaxAngularVelocity=%.1f deg/s"),
        *GetNameSafe(this), UprightSafetyConstraint->ConstraintInstance.IsValidConstraintInstance() ? 1 : 0,
        EffectiveMaximumTiltAngleDegrees, bUseSafeSoftLimit ? TEXT("Soft") : TEXT("Hard"),
        SafeStiffness, SafeDamping, FMath::Max(MaximumAngularVelocityDegrees, 0.0f));
}

void ACartBase::DrawCartPhysicsDebug() const
{
#if ENABLE_DRAW_DEBUG
    UWorld* World = GetWorld();
    if (!World || !CartMesh)
    {
        return;
    }

    const FVector CenterOfMass = CartMesh->GetCenterOfMass();
    const FVector CartUp = CartMesh->GetUpVector();
    DrawDebugSphere(World, CenterOfMass, 7.0f, 12, FColor::Yellow, false, 0.0f, 0, 1.5f);
    DrawDebugLine(World, CenterOfMass, CenterOfMass + CartUp * 100.0f, FColor::Blue, false, 0.0f, 0, 2.5f);

    if (!LastStabilizationTargetUp.IsNearlyZero())
    {
        DrawDebugLine(World, CenterOfMass, CenterOfMass + LastStabilizationTargetUp * 100.0f,
            FColor::Green, false, 0.0f, 0, 2.5f);
    }

    DrawDebugCone(World, CenterOfMass, FVector::UpVector, 80.0f,
        FMath::DegreesToRadians(EffectiveMaximumTiltAngleDegrees), FMath::DegreesToRadians(EffectiveMaximumTiltAngleDegrees),
        24, FColor::Purple, false, 0.0f, 0, 0.75f);
    DrawDebugString(World, CenterOfMass + FVector(0.0f, 0.0f, 25.0f),
        FString::Printf(TEXT("Tilt %.1f | Dead %.1f Full %.1f Max %.1f | Contacts %d"),
            LastStabilizationTiltDegrees, EffectiveStabilizationDeadZoneDegrees,
            EffectiveStabilizationFullAssistDegrees, EffectiveMaximumTiltAngleDegrees,
            SuspensionContactNormals.Num()),
        nullptr, FColor::White, 0.0f, false, 1.0f);
#endif
}


void ACartBase::ApplyPlayerForces(float DeltaTime)
{
    if (!CartMesh)
    {
        return;
    }

    for (const TPair<TObjectPtr<ACh4_PlayerCharacter>, FVector2D>& Pair : PlayerInputs)
    {
        ACh4_PlayerCharacter* Player = Pair.Key;
        const FVector2D Input = Pair.Value;

        if (!IsValid(Player) || Input.IsNearlyZero())
        {
            continue;
        }

        USceneComponent* Anchor = GetAnchorFor(Player);
        if (!Anchor)
        {
            continue;   // 잡고 있지 않으면 힘을 줄 수 없다.
        }

        // 캐릭터가 보는 방향 기준.
        const FVector Forward = Player->GetActorForwardVector();
        const FVector Right = Player->GetActorRightVector();

        // 앵커 위치마다 역할이 다르다.
        const int32 AnchorIndex = AnchorOccupants.IndexOfByKey(Player);

        float ForwardScale = 1.0f;
        float SideScale = 1.0f;

        if (AnchorIndex <= 1)                             // 손잡이: 추진
        {
            ForwardScale = HandleForwardScale;
            SideScale = -HandleSideScale;
        }
        else if (AnchorIndex == 2 || AnchorIndex == 4)    // 중간 사이드: 뒷바퀴 쪽
        {
            ForwardScale = MidForwardScale;
            SideScale = MidSideScale;
        }
        else                                              // 앞 사이드: 방향 전환
        {
            ForwardScale = FrontForwardScale;
            SideScale = FrontSideScale;
        }

        const FVector Force = (Forward * Input.Y * ForwardScale
                             + Right * Input.X * SideScale) * PushForce;

        // 잡은 앵커 위치에 힘을 준다. 위치에 따라 회전이 생긴다.
        CartMesh->AddForceAtLocation(Force, Anchor->GetComponentLocation());
    }
}

void ACartBase::SyncRootToPhysics()
{
    if (!HasAuthority() || bPreparationLocked || bCartTeleportInProgress
        || !CartRoot || !CartMesh || !CartMesh->IsSimulatingPhysics())
    {
        return;
    }

    // 물리로 움직인 메시 위치를 루트에 옮겨야 복제가 나간다.
    CartRoot->SetWorldLocationAndRotation(
        CartMesh->GetComponentLocation(),
        CartMesh->GetComponentRotation());
}

void ACartBase::InterpolateClientTransform(float DeltaTime)
{
    if (HasAuthority() || !CartRoot || !CartMesh)
    {
        return;
    }

    DisableClientPhysics();

    // 루트는 복제로 툭툭 점프한다. 메시가 그 자리를 부드럽게 따라간다.
    const FVector TargetLocation = CartRoot->GetComponentLocation();
    const FQuat TargetRotation = CartRoot->GetComponentQuat();
    const FVector CurrentLocation = CartMesh->GetComponentLocation();
    const FQuat CurrentRotation = CartMesh->GetComponentQuat();
    const float PositionError = FVector::Distance(CurrentLocation, TargetLocation);
    const float RotationErrorDegrees = FMath::RadiansToDegrees(CurrentRotation.AngularDistance(TargetRotation));

    if (PositionError > FMath::Max(ClientSmoothingSnapDistance, 0.0f)
        || RotationErrorDegrees > FMath::Clamp(ClientSmoothingSnapAngleDegrees, 0.0f, 180.0f))
    {
        CartMesh->SetWorldLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
        return;
    }

    const FVector NewLocation = FMath::VInterpTo(
        CurrentLocation, TargetLocation, DeltaTime, FMath::Max(ClientPositionInterpSpeed, 0.0f));
    const FQuat NewRotation = FMath::QInterpTo(
        CurrentRotation, TargetRotation, DeltaTime, FMath::Max(ClientRotationInterpSpeed, 0.0f));

    CartMesh->SetWorldLocationAndRotation(NewLocation, NewRotation);
}

void ACartBase::DisableClientPhysics()
{
    if (HasAuthority() || !CartMesh)
    {
        return;
    }

    if (CartMesh->IsSimulatingPhysics())
    {
        CartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    }

    // Clear the BodyInstance flag even when the client body is not initialized yet.
    // Otherwise a deferred body could begin simulating after this replication callback.
    CartMesh->SetSimulatePhysics(false);
    CartMesh->SetEnableGravity(false);
}

bool ACartBase::TeleportCartToTransform(const FTransform& Destination)
{
    if (!HasAuthority() || bCartTeleportInProgress || !CartRoot || !CartMesh
        || Destination.ContainsNaN()
        || CartRoot->Mobility != EComponentMobility::Movable
        || CartMesh->Mobility != EComponentMobility::Movable)
    {
        return false;
    }

    TGuardValue<bool> TeleportGuard(bCartTeleportInProgress, true);
    const FTransform OriginalRootTransform = CartRoot->GetComponentTransform();
    const FTransform OriginalMeshTransform = CartMesh->GetComponentTransform();
    const bool bWasSimulating = CartMesh->IsSimulatingPhysics();

    if (bWasSimulating)
    {
        CartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        CartMesh->SetSimulatePhysics(false);
    }

    SetActorTransform(Destination, false, nullptr, ETeleportType::TeleportPhysics);
    CartMesh->SetWorldLocationAndRotation(
        Destination.GetLocation(), Destination.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);

    const bool bTransformsAligned = CartRoot->GetComponentLocation().Equals(Destination.GetLocation(), 0.1f)
        && CartRoot->GetComponentQuat().Equals(Destination.GetRotation(), 0.001f)
        && CartMesh->GetComponentLocation().Equals(Destination.GetLocation(), 0.1f)
        && CartMesh->GetComponentQuat().Equals(Destination.GetRotation(), 0.001f);

    if (!bTransformsAligned)
    {
        CartRoot->SetWorldTransform(OriginalRootTransform, false, nullptr, ETeleportType::TeleportPhysics);
        CartMesh->SetWorldTransform(OriginalMeshTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }

    if (bWasSimulating)
    {
        CartMesh->SetSimulatePhysics(true);
        CartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        ConfigureUprightSafetyConstraint();
    }

    if (bTransformsAligned)
    {
        ForceNetUpdate();
    }

    if (bTransformsAligned)
    {
        UE_LOG(LogCh4_multiGame, Log,
            TEXT("[CartNetwork] Teleport Cart=%s Accepted=1 PhysicsBefore=%d PhysicsAfter=%d Root=%s Mesh=%s"),
            *GetName(), bWasSimulating ? 1 : 0, CartMesh->IsSimulatingPhysics() ? 1 : 0,
            *CartRoot->GetComponentLocation().ToString(), *CartMesh->GetComponentLocation().ToString());
    }
    else
    {
        UE_LOG(LogCh4_multiGame, Warning,
            TEXT("[CartNetwork] Teleport Cart=%s Accepted=0 PhysicsBefore=%d PhysicsAfter=%d Root=%s Mesh=%s"),
            *GetName(), bWasSimulating ? 1 : 0, CartMesh->IsSimulatingPhysics() ? 1 : 0,
            *CartRoot->GetComponentLocation().ToString(), *CartMesh->GetComponentLocation().ToString());
    }
    return bTransformsAligned;
}


// ── 앵커 조회 ──

USceneComponent* ACartBase::GetAnchorFor(const ACh4_PlayerCharacter* Player) const
{
    const int32 Index = AnchorOccupants.IndexOfByKey(Player);
    return Anchors.IsValidIndex(Index) ? Anchors[Index] : nullptr;
}

int32 ACartBase::FindClosestFreeAnchor(const ACh4_PlayerCharacter* Player) const
{
    if (!IsValid(Player))
    {
        return INDEX_NONE;
    }

    const FVector PlayerLocation = Player->GetActorLocation();

    int32 BestIndex = INDEX_NONE;
    float BestDistSq = FMath::Square(GrabDistance);

    for (int32 i = 0; i < Anchors.Num(); ++i)
    {
        // 이미 다른 사람이 잡고 있으면 건너뛴다.
        if (AnchorOccupants.IsValidIndex(i) && IsValid(AnchorOccupants[i]))
        {
            continue;
        }

        const float DistSq = FVector::DistSquared(
            Anchors[i]->GetComponentLocation(), PlayerLocation);

        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestIndex = i;
        }
    }

    return BestIndex;
}

void ACartBase::ReleaseAnchorFor(const ACh4_PlayerCharacter* Player)
{
    const int32 Index = AnchorOccupants.IndexOfByKey(Player);
    if (AnchorOccupants.IsValidIndex(Index))
    {
        AnchorOccupants[Index] = nullptr;
    }
}

void ACartBase::CleanupInvalidPlayers()
{
    for (TObjectPtr<ACh4_PlayerCharacter>& Occupant : AnchorOccupants)
    {
        if (!IsValid(Occupant))
        {
            Occupant = nullptr;
        }
    }

    for (auto It = PlayerInputs.CreateIterator(); It; ++It)
    {
        if (!IsValid(It.Key()) || !GetAnchorFor(It.Key()))
        {
            It.RemoveCurrent();
        }
    }
}

bool ACartBase::TryGrabPlayer(ACh4_PlayerCharacter* Player)
{
    if (!HasAuthority() || !IsValid(Player) || Player->IsActorBeingDestroyed()
        || Player->GetWorld() != GetWorld() || Player->GrabbedComponent || Player->GrabbedCart
        || GetAnchorFor(Player))
    {
        return false;
    }

    if (bPreparationLocked)
    {
        UE_LOG(LogCh4_multiGame, Log, TEXT("[Cart] Grab rejected: preparation lock active; Player=%s"),
            *GetNameSafe(Player));
        return false;
    }

    const int32 Index = FindClosestFreeAnchor(Player);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    AnchorOccupants[Index] = Player;

    // 서버가 위치를 옮기면 이동이 클라이언트로 복제된다.
    Player->SetActorLocation(Anchors[Index]->GetComponentLocation());
    Player->SetActorRotation(Anchors[Index]->GetComponentRotation());
    Player->SetCartGrabRagdollSuppressed(true);
    Player->SetCartGrabMovementLocked(true);
    Player->AttachToComponent(CartMesh,
        FAttachmentTransformRules::KeepWorldTransform);
    Player->GrabbedCart = this;
    Player->CartMoveInput = FVector2D::ZeroVector;
    Player->bIsBraking = false;
    Player->ForceNetUpdate();
    ForceNetUpdate();
    return true;
}

bool ACartBase::ReleasePlayer(ACh4_PlayerCharacter* Player)
{
    if (!HasAuthority() || !Player || !GetAnchorFor(Player))
    {
        return false;
    }

    ReleaseAnchorFor(Player);
    PlayerInputs.Remove(Player);

    if (IsValid(Player))
    {
        Player->CartMoveInput = FVector2D::ZeroVector;
        Player->bIsBraking = false;
        Player->GrabbedCart = nullptr;
        Player->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Player->SetCartGrabMovementLocked(false);
        Player->SetCartGrabRagdollSuppressed(false);
        Player->ForceNetUpdate();
    }
    ForceNetUpdate();
    return true;
}

bool ACartBase::SetPlayerMoveInput(ACh4_PlayerCharacter* Player, const FVector2D& Input)
{
    if (!HasAuthority() || bPreparationLocked || !IsValid(Player)
        || Player->GetWorld() != GetWorld() || !GetAnchorFor(Player))
    {
        return false;
    }

    const FVector2D ValidatedInput = Input.ContainsNaN()
        ? FVector2D::ZeroVector
        : Input.GetClampedToMaxSize(1.0f);
    PlayerInputs.Add(Player, ValidatedInput);

    const bool bBraking = ValidatedInput.Y < 0.0f;

    Player->CartMoveInput = ValidatedInput;
    Player->bIsBraking = bBraking;
    return true;
}

bool ACartBase::SetPreparationLocked(const bool bLocked)
{
    if (!HasAuthority() || !CartMesh)
    {
        return false;
    }

    if (bPreparationLocked == bLocked)
    {
        return true;
    }

    if (bLocked)
    {
        bWasSimulatingBeforePreparationLock = CartMesh->BodyInstance.bSimulatePhysics;
        if (CartMesh->IsSimulatingPhysics())
        {
            CartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            CartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        }

        const TArray<TObjectPtr<ACh4_PlayerCharacter>> Occupants = AnchorOccupants;
        for (ACh4_PlayerCharacter* Occupant : Occupants)
        {
            if (Occupant)
            {
                ReleasePlayer(Occupant);
            }
        }
        AnchorOccupants.SetNum(Anchors.Num());
        PlayerInputs.Reset();
        CartMesh->SetSimulatePhysics(false);
    }
    else if (bWasSimulatingBeforePreparationLock)
    {
        CartMesh->SetSimulatePhysics(true);
        CartMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        CartMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        ConfigureUprightSafetyConstraint();
    }

    bPreparationLocked = bLocked;
    ForceNetUpdate();
    UE_LOG(LogCh4_multiGame, Log, TEXT("[Cart] Preparation lock %s: Cart=%s Simulating=%d Collision=%d"),
        bLocked ? TEXT("enabled") : TEXT("disabled"), *GetName(), CartMesh->IsSimulatingPhysics(),
        static_cast<int32>(CartMesh->GetCollisionEnabled()));
    return true;
}
