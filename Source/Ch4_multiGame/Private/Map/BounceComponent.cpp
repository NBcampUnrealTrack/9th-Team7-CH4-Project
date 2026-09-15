#include "Map/BounceComponent.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"

UBounceComponent::UBounceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UBounceComponent::BeginPlay()
{
    Super::BeginPlay();

    // 서버 권한이 있을 때만 오버랩 이벤트를 바인딩하여 
    // 불필요한 클라이언트 판정 및 중복 실행 방지
    AActor* OwnerActor = GetOwner();
    if (OwnerActor && OwnerActor->HasAuthority())
    {
        UPrimitiveComponent* ParentCollision = Cast<UPrimitiveComponent>(GetAttachParent());
        if (ParentCollision)
        {
            ParentCollision->OnComponentBeginOverlap.AddDynamic(this, &UBounceComponent::OnParentBeginOverlap);
        }
    }
}

void UBounceComponent::OnParentBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    BounceActor(OtherActor, OtherComp);
}

void UBounceComponent::BounceActor(AActor* TargetActor, UPrimitiveComponent* TargetComp)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !TargetActor || TargetActor == OwnerActor)
    {
        return;
    }

    // 서버 권한이 있는 경우에만 실행 (동기화 보장)
    if (!OwnerActor->HasAuthority())
    {
        return;
    }

    ExecuteBounce(TargetActor, TargetComp);
}

void UBounceComponent::ExecuteBounce(AActor* TargetActor, UPrimitiveComponent* TargetComp)
{
    if (!TargetActor)
    {
        return;
    }

    // 튕겨낼 방향 계산
    FVector LaunchDirection = bUseComponentForwardVector 
        ? GetForwardVector() 
        : GetComponentTransform().TransformVectorNoScale(CustomBounceDirection).GetSafeNormal();

    // 1. 플레이어 캐릭터인 경우
    ACharacter* PlayerCharacter = Cast<ACharacter>(TargetActor);
    if (PlayerCharacter)
    {
        FVector LaunchVelocity = LaunchDirection * BounceForce;
        
        // 서버에서 LaunchCharacter 실행 시 CharacterMovementComponent가 클라이언트에 위치 자동 동기화
        PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
        return;
    }

    // 2. 일반 물리 액터인 경우
    UPrimitiveComponent* PhysComp = TargetComp ? TargetComp : Cast<UPrimitiveComponent>(TargetActor->GetRootComponent());
    if (PhysComp && PhysComp->IsSimulatingPhysics())
    {
        FVector ImpulseVector = LaunchDirection * BounceForce;
        PhysComp->AddImpulse(ImpulseVector, NAME_None, true /* bVelChange */);
    }
}