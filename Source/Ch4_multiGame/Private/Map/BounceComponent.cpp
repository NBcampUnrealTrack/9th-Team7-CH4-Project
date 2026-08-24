#include "Map/BounceComponent.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"

UBounceComponent::UBounceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UBounceComponent::BeginPlay()
{
	Super::BeginPlay();
	
	UPrimitiveComponent* ParentCollision = Cast<UPrimitiveComponent>(GetAttachParent());

	if (ParentCollision)
	{
		ParentCollision->OnComponentBeginOverlap.AddDynamic(this, &UBounceComponent::OnParentBeginOverlap);
	}
}

void UBounceComponent::OnParentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp,Warning,TEXT("바운스 액터와 오버랩 발생"));
	BounceActor(OtherActor, OtherComp);
}

void UBounceComponent::BounceActor(AActor* TargetActor, UPrimitiveComponent* TargetComp)
{
	
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	
	if (!TargetActor||TargetActor==GetOwner())
	{
		return;
	}
	//튕겨낼 방향 계산 (부모 액터의 UpVector를 쓸지, 지정된 로컬 방향을 쓸지)
	FVector LaunchDirection;
	
	if (bUseComponentForwardVector)
	{
		// BounceComponent의 로컬 X축 방향
		LaunchDirection = GetForwardVector();
	}
	else
	{
		// CustomBounceDirection을 BounceComponent의 로컬 좌표 기준으로 변환
		LaunchDirection = GetComponentTransform()
			.TransformVectorNoScale(CustomBounceDirection)
			.GetSafeNormal();
	}

	
	// 대상이 플레이어 캐릭터인 경우
	ACharacter* PlayerCharacter = Cast<ACharacter>(TargetActor);
	if (PlayerCharacter)
	{
		FVector LaunchVelocity = LaunchDirection * BounceForce;
		PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
		return;
	}

	// 대상이 일반 물리 액터인 경우
	UPrimitiveComponent* PhysComp = TargetComp;
	if (!PhysComp)
	{
		PhysComp = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent());
	}

	if (PhysComp && PhysComp->IsSimulatingPhysics())
	{
		// VelocityChange = true로 설정하여 Mass(질량) 상관없이 속도 직접 부여
		float ImpulseMagnitude = BounceForce;
		FVector ImpulseVector = LaunchDirection * ImpulseMagnitude;
	
		PhysComp->AddImpulse(ImpulseVector, NAME_None, true /* bVelChange */);
	}
}


