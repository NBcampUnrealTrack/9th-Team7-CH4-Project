#include "Public/Map/LevelFloorBase.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/Character.h"

ALevelFloorBase::ALevelFloorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// 루트 컴포넌트 생성
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComp);

	// 시작점 생성
	StartPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StartPoint"));
	StartPoint->SetupAttachment(RootComponent);

	// 끝점 생성
	EndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
	EndPoint->SetupAttachment(RootComponent);

	// 충돌 박스 생성
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);

	// 스플라인 생성
	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("FloorSplineComponent"));
	SplineComponent->SetupAttachment(RootComponent);

	// 스플라인 메시의 진행 방향
	ForwardAxis = ESplineMeshAxis::X;
}

void ALevelFloorBase::BeginPlay()
{
	Super::BeginPlay();

	CollisionBox->OnComponentBeginOverlap.AddDynamic(
		this,
		&ALevelFloorBase::OnCollisionBoxBeginOverlap
	);
}

void ALevelFloorBase::OnCollisionBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);

	if (!PlayerCharacter)
	{
		return;
	}
}

void ALevelFloorBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!SplineComponent)
    {
       return;
    }

    // 스플라인 내부 데이터 갱신
    SplineComponent->UpdateSpline();

    // 1. 기존에 생성된 스플라인 메시 제거
    for (USplineMeshComponent* Comp : SplineMeshComponents)
    {
       if (Comp)
       {
          Comp->UnregisterComponent();
          Comp->DestroyComponent();
       }
    }
    SplineMeshComponents.Empty();

    // 사용할 메시가 없거나 MeshLength가 0 이하이면 종료 (0 나누기 예방)
    if (!MeshToUse || MeshLength <= 0.0f)
    {
       return;
    }

    // 2. [Get Spline Length] 스플라인 전체 길이 가져오기
    const float SplineLength = SplineComponent->GetSplineLength();

    // 3. [Get Spline Length / Mesh Length -> Truncate - 1] 생성할 메시 개수 계산
    const int32 NumberOfMeshes = FMath::TruncToInt(SplineLength / MeshLength);
    const int32 LastIndex = NumberOfMeshes - 1;

    // 생성할 메시가 없으면 종료
    if (LastIndex < 0)
    {
       return;
    }

    // 4. [For Loop] (Index: 0 ~ LastIndex)
    for (int32 Index = 0; Index <= LastIndex; ++Index)
    {
       // [Add Spline Mesh Component]
       USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(this);

       if (!SplineMeshComp)
       {
          continue;
       }

       SplineMeshComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
       SplineMeshComp->SetMobility(EComponentMobility::Movable);
    	
    	// 1. 충돌 프로필 설정 (BlockAll / WorldStatic / BlockAllDynamic 등)
    	SplineMeshComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

    	// 2. 복잡한 메쉬 구조를 단순 충돌체로 연산하도록 설정 (선택 사항)
    	SplineMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

       SplineMeshComp->AttachToComponent(
          SplineComponent,
          FAttachmentTransformRules::KeepRelativeTransform
       );

       // [Set Static Mesh]
       SplineMeshComp->SetStaticMesh(MeshToUse);
       SplineMeshComp->SetForwardAxis(ForwardAxis);

       // --- 거리 계산 (Start / End Distance) ---
       // Start Distance = Index * MeshLength
       const float StartDistance = Index * MeshLength;
       // End Distance = (Index + 1) * MeshLength
       const float EndDistance = (Index + 1) * MeshLength;

       // --- [Get Location and Tangent at Distance Along Spline] ---
       // 시작 지점 위치 & 탄젠트 (Local 공간)
       FVector StartPos = SplineComponent->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
       FVector StartTangent = SplineComponent->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);

       // 끝 지점 위치 & 탄젠트 (Local 공간)
       FVector EndPos = SplineComponent->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
       FVector EndTangent = SplineComponent->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);

       // --- [Clamp Vector Size] 탄젠트 벡터 크기를 MeshLength로 제한 ---
       StartTangent = StartTangent.GetClampedToMaxSize(MeshLength);
       EndTangent = EndTangent.GetClampedToMaxSize(MeshLength);

       // [Set Start and End]
       SplineMeshComp->SetStartAndEnd(
          StartPos,
          StartTangent,
          EndPos,
          EndTangent,
          true
       );

       SplineMeshComp->RegisterComponent();
       SplineMeshComponents.Add(SplineMeshComp);
    }
}