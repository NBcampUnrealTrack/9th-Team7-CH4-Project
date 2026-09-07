#include "Player/TestPickUpActor.h"

ATestPickUpActor::ATestPickUpActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true); // 물리 시뮬레이션 위치를 클라이언트에 동기화

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetNotifyRigidBodyCollision(true);
}

