#include "Cart/CartBase.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
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

    CartMesh->SetSimulatePhysics(true);
    CartMesh->SetMassOverrideInKg(NAME_None, 220.0f, true);
    CartMesh->SetLinearDamping(0.5f);
    CartMesh->SetAngularDamping(3.0f);
    CartMesh->SetCenterOfMass(FVector(-20.0f, 0.0f, 0.0f));

    // 기획: 카트는 절대 전복되지 않는다. Yaw만 남기고 Roll/Pitch를 잠근다.
    CartMesh->BodyInstance.bLockXRotation = true;
    CartMesh->BodyInstance.bLockYRotation = true;

    CartMesh->SetCollisionObjectType(ECC_PhysicsBody);
    CartMesh->SetCollisionResponseToAllChannels(ECR_Block);

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
}

void ACartBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 물리는 서버에서만. 클라는 복제된 위치를 받는다.
    if (!HasAuthority())
    {
        return;
    }

    ApplySuspension(DeltaTime);
    ApplyGrip(DeltaTime);
    ApplyPlayerForces(DeltaTime);
}

void ACartBase::ApplySuspension(float DeltaTime)
{
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

        if (!bHit)
        {
            continue;   // 바퀴가 떠 있으면 힘을 주지 않는다.
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

        // 카트 기준 방향. 누가 어디에 서 있든 같은 입력은 같은 결과를 낸다.
        const FVector Forward = CartMesh->GetForwardVector();
        const FVector Right = CartMesh->GetRightVector();

        FVector Force = (Forward * Input.Y + Right * Input.X) * PushForce;

        // 뒤로 미는 입력은 브레이크로 취급한다.
        if (Input.Y < 0.0f)
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
}