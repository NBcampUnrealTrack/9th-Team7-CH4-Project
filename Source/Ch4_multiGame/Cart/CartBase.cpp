#include "Cart/CartBase.h"

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

    // ── 카트 본체 ──
    CartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartMesh"));
    SetRootComponent(CartMesh);

    UprightSafetyConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("UprightSafetyConstraint"));
    UprightSafetyConstraint->SetupAttachment(CartMesh);
    
    CartMesh->SetLinearDamping(0.5f);
    CartMesh->SetAngularDamping(3.0f);

    CartMesh->SetCollisionObjectType(ECC_PhysicsBody);
    CartMesh->SetCollisionResponseToAllChannels(ECR_Block);

    CartMesh->SetSimulatePhysics(true);
    
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
}

void ACartBase::BeginPlay()
{
    Super::BeginPlay();

    if (!CartMesh)
    {
        return;
    }

    if (HasAuthority())
    {
        CartMesh->SetSimulatePhysics(true);
        CartMesh->SetMassOverrideInKg(NAME_None, 220.0f, true);

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
        // 클라이언트는 서버가 복제한 위치를 그대로 따른다.
        CartMesh->SetSimulatePhysics(false);
    }
}

void ACartBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority() || !CartMesh || !CartMesh->IsSimulatingPhysics())
    {
        return;
    }

    ApplySuspension(DeltaTime);
    ApplyGrip(DeltaTime);
    ApplyUprightStabilization(DeltaTime);
    ApplyPlayerForces(DeltaTime);

    if (bDrawCartPhysicsDebug)
    {
        DrawCartPhysicsDebug();
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
        SuspensionContactNormals, 2, 0.2f, TargetUp);

    if (!bHasGroundReference)
    {
        const Ch4CartStabilization::FCorrectionResult WorldUpResult =
            Ch4CartStabilization::CalculateCorrection(
                CartUp,
                FVector::UpVector,
                AngularVelocityRadians,
                StabilizationDeadZoneDegrees,
                StabilizationFullAssistDegrees,
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

    const Ch4CartStabilization::FCorrectionResult Correction =
        Ch4CartStabilization::CalculateCorrection(
            CartUp,
            TargetUp,
            AngularVelocityRadians,
            StabilizationDeadZoneDegrees,
            StabilizationFullAssistDegrees,
            StabilizationProportionalGain,
            StabilizationDerivativeGain,
            MaximumCorrectionAngularAcceleration);

    LastStabilizationTargetUp = TargetUp;
    LastStabilizationTiltDegrees = Correction.TiltAngleDegrees;

    if (!Correction.bIsValid || Correction.AngularAcceleration.IsNearlyZero())
    {
        return;
    }

    CartMesh->AddTorqueInRadians(Correction.AngularAcceleration, NAME_None, true);
}

void ACartBase::ConfigureUprightSafetyConstraint()
{
    if (!UprightSafetyConstraint || !CartMesh)
    {
        return;
    }

    const float SafeMaximumTilt = FMath::Clamp(MaximumTiltAngleDegrees, 1.0f, 89.0f);

    UprightSafetyConstraint->TermComponentConstraint();
    UprightSafetyConstraint->SetLinearXLimit(LCM_Free, 0.0f);
    UprightSafetyConstraint->SetLinearYLimit(LCM_Free, 0.0f);
    UprightSafetyConstraint->SetLinearZLimit(LCM_Free, 0.0f);
    UprightSafetyConstraint->SetAngularTwistLimit(ACM_Free, 0.0f);
    UprightSafetyConstraint->SetAngularSwing1Limit(ACM_Limited, SafeMaximumTilt);
    UprightSafetyConstraint->SetAngularSwing2Limit(ACM_Limited, SafeMaximumTilt);
    UprightSafetyConstraint->ConstraintInstance.SetSoftSwingLimitParams(false, 0.0f, 0.0f, 0.0f, 0.0f);
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

    UE_LOG(LogTemp, Log,
        TEXT("[CartPhysics] %s stabilization initialized | ConstraintValid=%d | MaxTilt=%.1f | MaxAngularVelocity=%.1f deg/s"),
        *GetNameSafe(this), UprightSafetyConstraint->ConstraintInstance.IsValidConstraintInstance() ? 1 : 0,
        SafeMaximumTilt, FMath::Max(MaximumAngularVelocityDegrees, 0.0f));
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

    const float SafeMaximumTilt = FMath::Clamp(MaximumTiltAngleDegrees, 1.0f, 89.0f);
    DrawDebugCone(World, CenterOfMass, FVector::UpVector, 80.0f,
        FMath::DegreesToRadians(SafeMaximumTilt), FMath::DegreesToRadians(SafeMaximumTilt),
        24, FColor::Purple, false, 0.0f, 0, 0.75f);
    DrawDebugString(World, CenterOfMass + FVector(0.0f, 0.0f, 25.0f),
        FString::Printf(TEXT("Tilt %.1f / Limit %.1f | Contacts %d"),
            LastStabilizationTiltDegrees, SafeMaximumTilt, SuspensionContactNormals.Num()),
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

        // 캐릭터가 보는 방향 기준. BP 버전과 동일하게.
        const FVector Forward = Player->GetActorForwardVector();
        const FVector Right = Player->GetActorRightVector();

        FVector Force = (Forward * Input.Y + Right * Input.X) * PushForce;

        // 손잡이 쪽(앵커 0, 1)에서 뒤로 당기면 브레이크로 동작한다.
        const int32 AnchorIndex = AnchorOccupants.IndexOfByKey(Player);
        if (Input.Y < 0.0f && AnchorIndex <= 1)
        {
            const FVector Velocity = CartMesh->GetPhysicsLinearVelocity();
            Force = -Velocity.GetSafeNormal() * BrakeForce;
        }

        CartMesh->AddForce(Force);
    }
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

// ── RPC ──

void ACartBase::ServerRequestGrab_Implementation(ACh4_PlayerCharacter* Player)
{
    if (!IsValid(Player) || GetAnchorFor(Player))
    {
        return;   // 이미 잡고 있으면 무시.
    }

    const int32 Index = FindClosestFreeAnchor(Player);
    if (Index == INDEX_NONE)
    {
        return;   // 빈 앵커가 사거리 안에 없다.
    }

    AnchorOccupants[Index] = Player;

    // 서버가 위치를 옮기면 이동이 클라이언트로 복제된다.
    Player->SetActorLocation(Anchors[Index]->GetComponentLocation());
    Player->SetActorRotation(Anchors[Index]->GetComponentRotation());
    Player->AttachToComponent(CartMesh,
        FAttachmentTransformRules::KeepWorldTransform);
}

void ACartBase::ServerRequestRelease_Implementation(ACh4_PlayerCharacter* Player)
{
    if (!IsValid(Player) || !GetAnchorFor(Player))
    {
        return;
    }

    ReleaseAnchorFor(Player);
    PlayerInputs.Remove(Player);

    Player->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

void ACartBase::ServerSetMoveInput_Implementation(ACh4_PlayerCharacter* Player, FVector2D Input)
{
    if (!IsValid(Player) || !GetAnchorFor(Player))
    {
        return;   // 잡고 있지 않은 플레이어의 입력은 버린다.
    }

    PlayerInputs.Add(Player, Input);
    
    Player->CartMoveInput = Input;
}
