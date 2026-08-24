// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/Ch4_multiGameGameState.h"

#include "Ch4_multiGame.h"
#include "Net/UnrealNetwork.h"

void ACh4_multiGameGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACh4_multiGameGameState, InitialCargoCount);
	DOREPLIFETIME(ACh4_multiGameGameState, RemainingCargoCount);
	DOREPLIFETIME(ACh4_multiGameGameState, CurrentGamePhase);
}

int32 ACh4_multiGameGameState::GetLostCargoCount() const
{
	return FMath::Max(InitialCargoCount - RemainingCargoCount, 0);
}

float ACh4_multiGameGameState::GetCargoSurvivalRate() const
{
	if (InitialCargoCount <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(RemainingCargoCount) / static_cast<float>(InitialCargoCount), 0.0f, 1.0f);
}

bool ACh4_multiGameGameState::SetCurrentGamePhase(const ECh4GamePhase NewGamePhase)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Rejected a client-side game phase change"));
		return false;
	}

	if (CurrentGamePhase == NewGamePhase)
	{
		return false;
	}

	CurrentGamePhase = NewGamePhase;
	OnGamePhaseChanged.Broadcast(CurrentGamePhase);
	ForceNetUpdate();
	return true;
}

bool ACh4_multiGameGameState::SetCargoCounts(const int32 NewInitialCargoCount, const int32 NewRemainingCargoCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Rejected a client-side cargo initialization"));
		return false;
	}

	const int32 ValidatedInitialCount = FMath::Max(NewInitialCargoCount, 0);
	const int32 ValidatedRemainingCount = FMath::Clamp(NewRemainingCargoCount, 0, ValidatedInitialCount);

	if (InitialCargoCount == ValidatedInitialCount && RemainingCargoCount == ValidatedRemainingCount)
	{
		return false;
	}

	InitialCargoCount = ValidatedInitialCount;
	RemainingCargoCount = ValidatedRemainingCount;
	BroadcastCargoCountChanged();
	ForceNetUpdate();
	return true;
}

bool ACh4_multiGameGameState::SetRemainingCargoCount(const int32 NewRemainingCargoCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Rejected a client-side cargo count change"));
		return false;
	}

	const int32 ValidatedRemainingCount = FMath::Clamp(NewRemainingCargoCount, 0, InitialCargoCount);
	if (RemainingCargoCount == ValidatedRemainingCount)
	{
		return false;
	}

	RemainingCargoCount = ValidatedRemainingCount;
	BroadcastCargoCountChanged();
	ForceNetUpdate();
	return true;
}

void ACh4_multiGameGameState::OnRep_CurrentGamePhase()
{
	OnGamePhaseChanged.Broadcast(CurrentGamePhase);
}

void ACh4_multiGameGameState::OnRep_CargoCounts()
{
	BroadcastCargoCountChanged();
}

void ACh4_multiGameGameState::BroadcastCargoCountChanged()
{
	OnCargoCountChanged.Broadcast(RemainingCargoCount, InitialCargoCount);
}
