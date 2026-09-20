#include "Map/RoadBase.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "PCGComponent.h"

ARoadBase::ARoadBase()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    StartPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StartPoint"));
    StartPoint->SetupAttachment(RootComponent);

    EndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
    EndPoint->SetupAttachment(RootComponent);

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("RoadSpline"));
    SplineComponent->SetupAttachment(RootComponent);

    ForwardAxis = ESplineMeshAxis::X;
    
    PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));
    
    // [핵심] PCG 컴포넌트 자체의 네트워크 복제를 차단하여
    // 클라이언트가 서버 패킷을 어설프게 받아 메쉬가 투명해지는 현상 방지
    if (PCGComponent)
    {
        PCGComponent->SetIsReplicated(false);
    }
}

void ARoadBase::BeginPlay()
{
    Super::BeginPlay();
}

void ARoadBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!SplineComponent) return;

    SplineComponent->UpdateSpline();

    for (USplineMeshComponent* Comp : SplineMeshComponents)
    {
        if (Comp)
        {
            Comp->UnregisterComponent();
            Comp->DestroyComponent();
        }
    }

    SplineMeshComponents.Empty();

    if (!MeshToUse || MeshLength <= 0.0f) return;

    const float SplineLength = SplineComponent->GetSplineLength();
    const int32 NumberOfMeshes = FMath::CeilToInt(SplineLength / MeshLength);
    const int32 LastIndex = NumberOfMeshes - 1;

    if (LastIndex < 0) return;

    for (int32 Index = 0; Index <= LastIndex; ++Index)
    {
        USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(this);
        if (!SplineMeshComp) continue;

        SplineMeshComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        SplineMeshComp->SetMobility(EComponentMobility::Movable);
        
        SplineMeshComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        SplineMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        SplineMeshComp->SetUseCCD(true);
        
        SplineMeshComp->AttachToComponent(SplineComponent, FAttachmentTransformRules::KeepRelativeTransform);
        SplineMeshComp->SetStaticMesh(MeshToUse);
        SplineMeshComp->SetForwardAxis(ForwardAxis);

        const float StartDistance = Index * MeshLength;
        const float EndDistance = FMath::Min((Index + 1) * MeshLength, SplineLength);

        FVector StartPos = SplineComponent->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
        FVector StartTangent = SplineComponent->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
        FVector EndPos = SplineComponent->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
        FVector EndTangent = SplineComponent->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);

        StartTangent = StartTangent.GetClampedToMaxSize(MeshLength);
        EndTangent = EndTangent.GetClampedToMaxSize(MeshLength);

        SplineMeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
        SplineMeshComp->UpdateMesh();
        
        SplineMeshComp->RegisterComponent();
        SplineMeshComponents.Add(SplineMeshComp);
    }
}

void ARoadBase::GenerateObstacles(int32 InRandomSeed, bool bIsForce)
{
    UPCGComponent* PCGComp = FindComponentByClass<UPCGComponent>();
    if (!PCGComp) return;

    RandomSeed = InRandomSeed;
    PCGComp->Seed = InRandomSeed;

    // 도로 위치 이동에 맞춰 Spline 좌표와 Transform 강제 갱신
    if (SplineComponent)
    {
        SplineComponent->UpdateSpline();
    }
    UpdateComponentTransforms();

    // 기존 리소스 즉시 정제
    PCGComp->CleanupLocalImmediate(true);

#if WITH_EDITOR
    // 에디터 뷰포트 미플레이(Editor World) 상태 처리
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        PCGComp->DirtyGenerated();
        PCGComp->Generate(bIsForce);
        return;
    }
#endif

    // [핵심] HasAuthority() 제약 없이 서버와 클라이언트 모두 각자의 로컬 World에서 생성 연산 수행
    PCGComp->Generate(bIsForce);
}

void ARoadBase::SetRoadComponentsStatic()
{
    TArray<UActorComponent*> Components;
    GetComponents(Components);

    for (UActorComponent* Component : Components)
    {
        if (!Component) continue;

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
        {
            SceneComponent->SetMobility(EComponentMobility::Static);
        }

        if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
        {
            PrimitiveComponent->SetCollisionObjectType(ECC_WorldStatic);
        }
    }
}