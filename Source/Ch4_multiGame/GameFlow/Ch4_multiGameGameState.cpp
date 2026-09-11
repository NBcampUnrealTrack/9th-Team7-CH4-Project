// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/Ch4_multiGameGameState.h"

#include "Ch4_multiGame.h"
#include "Net/UnrealNetwork.h"

void ACh4_multiGameGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACh4_multiGameGameState, InitialCargoCount);
	DOREPLIFETIME(ACh4_multiGameGameState, RemainingCargoCount);
	DOREPLIFETIME(ACh4_multiGameGameState, FinalCargoScore);
	DOREPLIFETIME(ACh4_multiGameGameState, GameEndReason);
	DOREPLIFETIME(ACh4_multiGameGameState, CurrentGamePhase);
	DOREPLIFETIME(ACh4_multiGameGameState, PreparationEndTime);
	DOREPLIFETIME(ACh4_multiGameGameState, PreparationTotalDuration);
	DOREPLIFETIME(ACh4_multiGameGameState, GameResultSnapshot);
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

FCh4GameResult ACh4_multiGameGameState::GetGameResult() const
{
	if (GameResultSnapshot.bResultAvailable)
	{
		return GameResultSnapshot;
	}

	FCh4GameResult Result;
	Result.GamePhase = CurrentGamePhase;
	Result.GameEndReason = GameEndReason;
	Result.InitialCargoCount = InitialCargoCount;
	Result.RemainingCargoCount = RemainingCargoCount;
	Result.LostCargoCount = GetLostCargoCount();
	Result.CargoSurvivalRate = GetCargoSurvivalRate();
	Result.FinalCargoScore = FinalCargoScore;
	Result.bGameEnded = CurrentGamePhase == ECh4GamePhase::Cleared
		|| CurrentGamePhase == ECh4GamePhase::GameOver;
	Result.bSucceeded = CurrentGamePhase == ECh4GamePhase::Cleared;
	return Result;
}

bool ACh4_multiGameGameState::SetGamePhaseState(
	const ECh4GamePhase NewGamePhase,
	const ECh4GameEndReason NewEndReason,
	const int32 NewFinalCargoScore)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Rejected a client-side game phase change"));
		return false;
	}

	const int32 ValidatedFinalCargoScore = NewGamePhase == ECh4GamePhase::Cleared
		? FMath::Max(NewFinalCargoScore, 0)
		: 0;
	if (CurrentGamePhase == NewGamePhase
		&& GameEndReason == NewEndReason
		&& FinalCargoScore == ValidatedFinalCargoScore)
	{
		return false;
	}

	FinalCargoScore = ValidatedFinalCargoScore;
	GameEndReason = NewEndReason;
	CurrentGamePhase = NewGamePhase;
	if (GameResultSnapshot.bResultAvailable
		&& (NewGamePhase == ECh4GamePhase::Cleared || NewGamePhase == ECh4GamePhase::GameOver))
	{
		GameResultSnapshot.GamePhase = NewGamePhase;
		GameResultSnapshot.GameEndReason = NewEndReason;
		GameResultSnapshot.bGameEnded = true;
		GameResultSnapshot.bSucceeded = NewGamePhase == ECh4GamePhase::Cleared;
		GameResultSnapshot.FinalCargoScore = NewGamePhase == ECh4GamePhase::Cleared
			? ValidatedFinalCargoScore : 0;
		OnGameResultChanged.Broadcast(GameResultSnapshot);
	}
	OnGamePhaseChanged.Broadcast(CurrentGamePhase);
	ForceNetUpdate();
	return true;
}

bool ACh4_multiGameGameState::SetGoalResultSnapshot(
	const float ClearTimeSeconds,
	const int32 DeliveredCargoCount,
	const int32 DeliveredCargoScore,
	const bool bSucceeded,
	const float ResultDisplayEndServerTime)
{
	if (!HasAuthority() || GameResultSnapshot.bResultAvailable)
	{
		return false;
	}

	GameResultSnapshot.bResultAvailable = true;
	GameResultSnapshot.GamePhase = bSucceeded ? ECh4GamePhase::Cleared : CurrentGamePhase;
	GameResultSnapshot.GameEndReason = bSucceeded
		? ECh4GameEndReason::GoalReached : ECh4GameEndReason::CargoRuleFailed;
	GameResultSnapshot.InitialCargoCount = InitialCargoCount;
	GameResultSnapshot.RemainingCargoCount = RemainingCargoCount;
	GameResultSnapshot.LostCargoCount = GetLostCargoCount();
	GameResultSnapshot.CargoSurvivalRate = GetCargoSurvivalRate();
	GameResultSnapshot.FinalCargoScore = bSucceeded ? FMath::Max(DeliveredCargoScore, 0) : 0;
	GameResultSnapshot.ClearTimeSeconds = FMath::IsFinite(ClearTimeSeconds)
		? FMath::Max(ClearTimeSeconds, 0.0f) : 0.0f;
	GameResultSnapshot.DeliveredCargoCount = FMath::Max(DeliveredCargoCount, 0);
	GameResultSnapshot.ResultDisplayEndServerTime = FMath::IsFinite(ResultDisplayEndServerTime)
		? FMath::Max(ResultDisplayEndServerTime, 0.0f) : 0.0f;
	GameResultSnapshot.bGameEnded = bSucceeded;
	GameResultSnapshot.bSucceeded = bSucceeded;

	OnGameResultChanged.Broadcast(GameResultSnapshot);
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

void ACh4_multiGameGameState::SetPreparationTimer(float DurationSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	PreparationTotalDuration = DurationSeconds;
	PreparationEndTime = GetServerWorldTimeSeconds() + DurationSeconds;
	OnRep_PreparationEndTime();
	ForceNetUpdate();
}

void ACh4_multiGameGameState::OnRep_PreparationEndTime()
{
	OnPreparationTimerUpdated.Broadcast(GetRemainingPreparationTime(), PreparationTotalDuration);
}

void ACh4_multiGameGameState::OnRep_GameResultSnapshot()
{
	if (GameResultSnapshot.bResultAvailable)
	{
		OnGameResultChanged.Broadcast(GameResultSnapshot);
	}
}

float ACh4_multiGameGameState::GetRemainingPreparationTime() const
{
	if (PreparationEndTime <= 0.0f || CurrentGamePhase != ECh4GamePhase::Waiting)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, PreparationEndTime - GetServerWorldTimeSeconds());
}

float ACh4_multiGameGameState::GetPreparationTimeRatio() const
{
	if (PreparationTotalDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(GetRemainingPreparationTime() / PreparationTotalDuration, 0.0f, 1.0f);
}
