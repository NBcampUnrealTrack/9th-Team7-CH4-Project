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
        SplineMeshComp->RegisterComponent();
        SplineMeshComponents.Add(SplineMeshComp);
    }
}

void ARoadBase::GenerateObstacles(int32 InRandomSeed)
{
    UPCGComponent* PCGComp = FindComponentByClass<UPCGComponent>();
    if (!PCGComp) return;

    PCGComp->Seed = InRandomSeed;

    // 도로 위치 이동에 맞춰 Spline 좌표와 Transform 강제 갱신
    if (SplineComponent)
    {
        SplineComponent->UpdateSpline();
    }
    UpdateComponentTransforms();

    // 기존에 생성되어 있던 PCG 리소스 정리
    PCGComp->CleanupLocalImmediate(true);

#if WITH_EDITOR
    // 에디터 뷰포트 미플레이(Editor World) 상태일 때 강제 갱신 처리
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        PCGComp->DirtyGenerated();
        PCGComp->Generate(true);
        return;
    }
#endif

    // 런타임 게임 중 (서버) 실행
    PCGComp->Generate(true);
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