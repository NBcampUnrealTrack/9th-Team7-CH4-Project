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
	if (!SplineComponent)
	{
		return;
	}

	if (OriginalSplinePointLocations.Num() == 0)
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

	const int32 NumPoints = FMath::Min(
		SplineComponent->GetNumberOfSplinePoints(),
		OriginalSplinePointLocations.Num());

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		FVector NewLocation = OriginalSplinePointLocations[Index];

		const float Distance = SplineComponent->GetDistanceAlongSplineAtSplinePoint(Index);

		const float WavePhase =
			ElapsedTime * WobbleSpeed - Distance * WobbleFrequency;

		const float OffsetZ =
			FMath::Sin(WavePhase) * WobbleAmplitude;

		NewLocation.Z += OffsetZ;

		SplineComponent->SetLocationAtSplinePoint(
			Index,
			NewLocation,
			ESplineCoordinateSpace::Local,
			false);
	}

	SplineComponent->UpdateSpline();

	const float SplineLength = SplineComponent->GetSplineLength();

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

		const float EndDistance = FMath::Min(
			(Index + 1) * MeshLength,
			SplineLength);

		FVector StartPos =
			SplineComponent->GetLocationAtDistanceAlongSpline(
				StartDistance,
				ESplineCoordinateSpace::Local);

		FVector StartTangent =
			SplineComponent->GetTangentAtDistanceAlongSpline(
				StartDistance,
				ESplineCoordinateSpace::Local);

		FVector EndPos =
			SplineComponent->GetLocationAtDistanceAlongSpline(
				EndDistance,
				ESplineCoordinateSpace::Local);

		FVector EndTangent =
			SplineComponent->GetTangentAtDistanceAlongSpline(
				EndDistance,
				ESplineCoordinateSpace::Local);

		StartTangent = StartTangent.GetClampedToMaxSize(MeshLength);
		EndTangent = EndTangent.GetClampedToMaxSize(MeshLength);

		SplineMeshComp->SetStartAndEnd(
			StartPos,
			StartTangent,
			EndPos,
			EndTangent,
			true);
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