#include "Map/RoadBase.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

ARoadBase::ARoadBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// 루트 컴포넌트
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComp);

	// 시작점
	StartPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StartPoint"));
	StartPoint->SetupAttachment(RootComponent);

	// 끝점
	EndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
	EndPoint->SetupAttachment(RootComponent);

	// 도로 Spline
	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("RoadSpline"));
	SplineComponent->SetupAttachment(RootComponent);

	// Spline Mesh 진행 방향
	ForwardAxis = ESplineMeshAxis::X;

	// 배경 배치 영역 가이드(지울예정)
	BackgroundBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("BackgroundBounds"));
	BackgroundBounds->SetupAttachment(RootComponent);
	BackgroundBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 모든 환경에서 사용할 기본 배경 영역(지울예정)
	BackgroundBounds->SetBoxExtent(FVector(20000.0f, 20000.0f, 48000.0f));
}

void ARoadBase::BeginPlay()
{
	Super::BeginPlay();

	//(지울예정)
	DrawBackgroundGuides();
}

void ARoadBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BackgroundBounds->SetVisibility(bShowBackgroundBounds);

	//(지울예정)
	DrawBackgroundGuides();

	if (!SplineComponent)
	{
		return;
	}

	// Spline 데이터 갱신
	SplineComponent->UpdateSpline();

	// 기존 Spline Mesh 제거
	for (USplineMeshComponent* Comp : SplineMeshComponents)
	{
		if (Comp)
		{
			Comp->UnregisterComponent();
			Comp->DestroyComponent();
		}
	}

	SplineMeshComponents.Empty();

	// 메시가 없거나 길이가 잘못되었으면 종료
	if (!MeshToUse || MeshLength <= 0.0f)
	{
		return;
	}

	// Spline 전체 길이
	const float SplineLength = SplineComponent->GetSplineLength();

	// 생성할 Mesh 개수
	// 기존 TruncToInt는 남는 구간을 버리기 때문에
	// CeilToInt로 올림하여 마지막 남는 구간도 생성
	const int32 NumberOfMeshes = FMath::CeilToInt(SplineLength / MeshLength);

	const int32 LastIndex = NumberOfMeshes - 1;

	if (LastIndex < 0)
	{
		return;
	}

	// Spline을 MeshLength 단위로 나눠서 Mesh 생성
	for (int32 Index = 0; Index <= LastIndex; ++Index)
	{
		USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(this);

		if (!SplineMeshComp)
		{
			continue;
		}

		SplineMeshComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;

		SplineMeshComp->SetMobility(EComponentMobility::Movable);

		// 충돌 설정
		SplineMeshComp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

		SplineMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// Spline에 부착
		SplineMeshComp->AttachToComponent(
			SplineComponent,
			FAttachmentTransformRules::KeepRelativeTransform);

		// 메시 설정
		SplineMeshComp->SetStaticMesh(MeshToUse);
		SplineMeshComp->SetForwardAxis(ForwardAxis);

		// 시작 / 끝 거리
		const float StartDistance = Index * MeshLength;

		// 마지막 Mesh가 Spline을 넘어가지 않도록
		// Spline의 실제 길이를 최대값으로 사용
		const float EndDistance =
			FMath::Min((Index + 1) * MeshLength, SplineLength);

		// 시작 위치
		FVector StartPos =
			SplineComponent->GetLocationAtDistanceAlongSpline(
				StartDistance,
				ESplineCoordinateSpace::Local);

		// 시작 탄젠트
		FVector StartTangent =
			SplineComponent->GetTangentAtDistanceAlongSpline(
				StartDistance,
				ESplineCoordinateSpace::Local);

		// 끝 위치
		FVector EndPos =
			SplineComponent->GetLocationAtDistanceAlongSpline(
				EndDistance,
				ESplineCoordinateSpace::Local);

		// 끝 탄젠트
		FVector EndTangent =
			SplineComponent->GetTangentAtDistanceAlongSpline(
				EndDistance,
				ESplineCoordinateSpace::Local);

		// 탄젠트 크기 제한
		StartTangent = StartTangent.GetClampedToMaxSize(MeshLength);

		EndTangent = EndTangent.GetClampedToMaxSize(MeshLength);

		// Spline Mesh 시작 / 끝 설정
		SplineMeshComp->SetStartAndEnd(
			StartPos,
			StartTangent,
			EndPos,
			EndTangent,
			true);

		// 컴포넌트 등록
		SplineMeshComp->RegisterComponent();

		// 배열에 저장
		SplineMeshComponents.Add(SplineMeshComp);
	}
}

void ARoadBase::DrawBackgroundGuides()
{
	FlushPersistentDebugLines(GetWorld());

	if (!BackgroundBounds || DivisionCount <= 1 || !bShowBackgroundGuides)
	{
		return;
	}

	const FVector BoundsCenter = BackgroundBounds->GetComponentLocation();
	const FVector BoundsExtent = BackgroundBounds->GetScaledBoxExtent();

	const float MinZ = BoundsCenter.Z - BoundsExtent.Z;
	const float MaxZ = BoundsCenter.Z + BoundsExtent.Z;
	const float HalfWidth = BoundsExtent.X;

	const float DivisionHeight = (MaxZ - MinZ) / DivisionCount;

	for (int32 Index = 1; Index < DivisionCount; ++Index)
	{
		const float Z = MinZ + DivisionHeight * Index;

		const FVector LineStart(
			BoundsCenter.X - HalfWidth,
			BoundsCenter.Y,
			Z);

		const FVector LineEnd(
			BoundsCenter.X + HalfWidth,
			BoundsCenter.Y,
			Z);

		DrawDebugLine(
			GetWorld(),
			LineStart,
			LineEnd,
			FColor::Red,
			true,
			-1.0f,
			0,
			10.0f);
	}
}

void ARoadBase::SetRoadComponentsStatic()
{
	TArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

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