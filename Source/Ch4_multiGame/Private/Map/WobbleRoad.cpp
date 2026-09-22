#include "Map/WobbleRoad.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

AWobbleRoad::AWobbleRoad()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;
}

void AWobbleRoad::BeginPlay()
{
	Super::BeginPlay();

	if (!SplineComponent)
	{
		return;
	}

	const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();

	OriginalSplinePointLocations.Empty();
	OriginalSplinePointLocations.Reserve(NumPoints);

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		OriginalSplinePointLocations.Add(
			SplineComponent->GetLocationAtSplinePoint(
				Index,
				ESplineCoordinateSpace::Local));
	}

	if (HasAuthority() && bEnableWobble)
	{
		if (AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			WobbleStartServerTime = GameState->GetServerWorldTimeSeconds();
		}
	}
}

void AWobbleRoad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnableWobble)
	{
		return;
	}

	UpdateWobble();
}

void AWobbleRoad::UpdateWobble()
{
    if (!SplineComponent || OriginalSplinePointLocations.Num() == 0)
    {
       return;
    }

    AGameStateBase* GameState = GetWorld()->GetGameState();
    if (!GameState)
    {
       return;
    }

    const float CurrentServerTime = GameState->GetServerWorldTimeSeconds();
    const float ElapsedTime = CurrentServerTime - WobbleStartServerTime;
    const float SplineLength = SplineComponent->GetSplineLength();

    // 1. 스플라인 제어점 위치를 바꾸는 대신, 원래 스플라인에서 위치/탄젠트를 계산합니다.
    for (int32 Index = 0; Index < SplineMeshComponents.Num(); ++Index)
    {
       USplineMeshComponent* SplineMeshComp = SplineMeshComponents[Index];
       if (!SplineMeshComp)
       {
          continue;
       }

       const float StartDistance = Index * MeshLength;
       if (StartDistance >= SplineLength)
       {
          continue;
       }

       const float EndDistance = FMath::Min((Index + 1) * MeshLength, SplineLength);

       // 원래 스플라인 선상의 기준 위치 및 탄젠트 구하기
       FVector StartPos = SplineComponent->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
       FVector StartTangent = SplineComponent->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
       FVector EndPos = SplineComponent->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);
       FVector EndTangent = SplineComponent->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);

       // 2. 거리(Distance) 기반으로 오프셋 Z 계산
       const float StartPhase = ElapsedTime * WobbleSpeed - StartDistance * WobbleFrequency;
       const float EndPhase = ElapsedTime * WobbleSpeed - EndDistance * WobbleFrequency;

       const float StartOffsetZ = FMath::Sin(StartPhase) * WobbleAmplitude;
       const float EndOffsetZ = FMath::Sin(EndPhase) * WobbleAmplitude;

       // 높이 적용
       StartPos.Z += StartOffsetZ;
       EndPos.Z += EndOffsetZ;

       // 3. 파형의 기울기(미분값)를 탄젠트 Z에 반영하여 메시 연결 부위가 자연스럽게 기울어지도록 보정
       const float StartSlope = FMath::Cos(StartPhase) * WobbleAmplitude * WobbleFrequency;
       const float EndSlope = FMath::Cos(EndPhase) * WobbleAmplitude * WobbleFrequency;

       StartTangent.Z += StartSlope;
       EndTangent.Z += EndSlope;

       StartTangent = StartTangent.GetClampedToMaxSize(MeshLength);
       EndTangent = EndTangent.GetClampedToMaxSize(MeshLength);

       // 4. 메시 업데이트 (UpdateMesh = true)
       SplineMeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
    }
}

void AWobbleRoad::ResetSplineToOriginal()
{
	if (!SplineComponent)
	{
		return;
	}

	const int32 NumPoints = FMath::Min(
		SplineComponent->GetNumberOfSplinePoints(),
		OriginalSplinePointLocations.Num());

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		SplineComponent->SetLocationAtSplinePoint(
			Index,
			OriginalSplinePointLocations[Index],
			ESplineCoordinateSpace::Local,
			false);
	}

	SplineComponent->UpdateSpline();
}

void AWobbleRoad::OnRep_EnableWobble()
{
	if (!bEnableWobble)
	{
		ResetSplineToOriginal();
	}
}

void AWobbleRoad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWobbleRoad, bEnableWobble);
	DOREPLIFETIME(AWobbleRoad, WobbleStartServerTime);
}