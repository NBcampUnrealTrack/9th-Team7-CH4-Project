// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ch4_multiGameGameMode.h"

#include "Ch4_multiGame.h"
#include "GameFlow/Ch4_multiGameGameState.h"

ACh4_multiGameGameMode::ACh4_multiGameGameMode()
{
	GameStateClass = ACh4_multiGameGameState::StaticClass();
}

#if WITH_DEV_AUTOMATION_TESTS
void ACh4_multiGameGameMode::SetGameRuleConfigForTesting(const FCh4GameRuleConfig& NewGameRuleConfig)
{
	GameRuleConfig = NewGameRuleConfig;
}
#endif

bool ACh4_multiGameGameMode::InitializeCargoCount(const int32 CargoCount)
{
	return RequestCargoInitialization(CargoCount);
}

bool ACh4_multiGameGameMode::RequestCargoInitialization(const int32 InitialCargoCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Cargo initialization rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState)
	{
		return false;
	}

	if (!CanInitializeCargo(InitialCargoCount, *GameFlowState))
	{
		if (InitialCargoCount <= 0)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow] Cargo initialization rejected: invalid count %d"),
				InitialCargoCount);
		}
		else if (GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Waiting)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow] Cargo initialization rejected: current phase is %s"),
				*UEnum::GetValueAsString(GameFlowState->GetCurrentGamePhase()));
		}
		else
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow] Cargo initialization rejected: already initialized with %d Cargo"),
				GameFlowState->GetInitialCargoCount());
		}
		return false;
	}

	if (!GameFlowState->SetCargoCounts(InitialCargoCount, InitialCargoCount))
	{
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Cargo Initialized: %d"), InitialCargoCount);
	return true;
}

bool ACh4_multiGameGameMode::StartGame()
{
	return RequestGameStart();
}

bool ACh4_multiGameGameMode::RequestGameStart()
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Game start rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState)
	{
		return false;
	}

	if (!CanStartGame(*GameFlowState))
	{
		if (GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Waiting)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow] Game start rejected: current phase is %s"),
				*UEnum::GetValueAsString(GameFlowState->GetCurrentGamePhase()));
		}
		else
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow] Game start rejected: Cargo has not been initialized"));
		}
		return false;
	}

	if (!TryTransitionGamePhase(ECh4GamePhase::Playing, ECh4GameEndReason::None))
	{
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Game Started"));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
		GameFlowState->GetRemainingCargoCount(), GameFlowState->GetInitialCargoCount());
	return true;
}

bool ACh4_multiGameGameMode::UpdateRemainingCargo(const int32 NewRemainingCargo)
{
	if (!ApplyRemainingCargoCount(NewRemainingCargo))
	{
		return false;
	}

	EvaluateGameOutcome(EGameRuleEvaluationEvent::CargoChanged);
	return true;
}

bool ACh4_multiGameGameMode::NotifyCargoLost(const int32 LostCargoCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Cargo loss rejected without server authority"));
		return false;
	}

	if (LostCargoCount <= 0)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Cargo loss rejected: invalid Delta %d"),
			LostCargoCount);
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState)
	{
		return false;
	}

	if (!CanProcessCargoChange(*GameFlowState))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Cargo loss rejected: current phase is %s"),
			*UEnum::GetValueAsString(GameFlowState->GetCurrentGamePhase()));
		return false;
	}

	if (LostCargoCount > GameFlowState->GetRemainingCargoCount())
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Cargo loss rejected: Delta %d exceeds remaining Cargo %d"),
			LostCargoCount,
			GameFlowState->GetRemainingCargoCount());
		return false;
	}

	if (!ApplyRemainingCargoCount(GameFlowState->GetRemainingCargoCount() - LostCargoCount))
	{
		return false;
	}

	EvaluateGameOutcome(EGameRuleEvaluationEvent::CargoChanged);
	return true;
}

bool ACh4_multiGameGameMode::ApplyRemainingCargoCount(const int32 NewRemainingCargo)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] UpdateRemainingCargo rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState)
	{
		return false;
	}

	if (!CanProcessCargoChange(*GameFlowState))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Cargo update rejected: current phase is %s"),
			*UEnum::GetValueAsString(GameFlowState->GetCurrentGamePhase()));
		return false;
	}

	const int32 PreviousCargoCount = GameFlowState->GetRemainingCargoCount();
	if (NewRemainingCargo < 0 || NewRemainingCargo > GameFlowState->GetInitialCargoCount())
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Cargo update rejected: count %d is outside [0, %d]"),
			NewRemainingCargo,
			GameFlowState->GetInitialCargoCount());
		return false;
	}

	if (NewRemainingCargo == PreviousCargoCount)
	{
		UE_LOG(LogCh4_multiGame, Verbose,
			TEXT("[GameFlow] Cargo update ignored: count is already %d"),
			NewRemainingCargo);
		return false;
	}

	if (!GameFlowState->SetRemainingCargoCount(NewRemainingCargo))
	{
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Cargo Changed: %d -> %d"), PreviousCargoCount, NewRemainingCargo);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
		NewRemainingCargo, GameFlowState->GetInitialCargoCount());

	return true;
}

bool ACh4_multiGameGameMode::TryCompleteGame()
{
	return ProcessGoalReached(nullptr, false);
}

bool ACh4_multiGameGameMode::NotifyGoalReached(AActor* ReachingActor)
{
	return ProcessGoalReached(ReachingActor, true);
}

bool ACh4_multiGameGameMode::ProcessGoalReached(
	AActor* ReachingActor,
	const bool bRequireValidReachingActor)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Goal notification rejected without server authority"));
		return false;
	}

	if (bRequireValidReachingActor
		&& (!IsValid(ReachingActor)
			|| ReachingActor->IsActorBeingDestroyed()
			|| ReachingActor->GetWorld() != GetWorld()))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Goal notification rejected: reaching Actor is null, invalid, or belongs to another World"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Goal notification rejected: current phase is %s"),
			GameFlowState
				? *UEnum::GetValueAsString(GameFlowState->GetCurrentGamePhase())
				: TEXT("Unavailable"));
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Final Delivery Zone Reached by %s"),
		IsValid(ReachingActor) ? *GetNameSafe(ReachingActor) : TEXT("Legacy Compatibility Request"));

	return EvaluateGameOutcome(EGameRuleEvaluationEvent::GoalReached);
}

ACh4_multiGameGameState* ACh4_multiGameGameMode::GetGameFlowGameState() const
{
	ACh4_multiGameGameState* GameFlowState = GetWorld()
		? GetWorld()->GetGameState<ACh4_multiGameGameState>()
		: nullptr;
	if (!GameFlowState)
	{
		UE_LOG(LogCh4_multiGame, Error, TEXT("[GameFlow] ACh4_multiGameGameState is not active. Check the GameMode GameStateClass."));
	}

	return GameFlowState;
}

bool ACh4_multiGameGameMode::CanInitializeCargo(
	const int32 InitialCargoCount,
	const ACh4_multiGameGameState& GameFlowState) const
{
	return GameFlowState.GetCurrentGamePhase() == ECh4GamePhase::Waiting
		&& InitialCargoCount > 0
		&& GameFlowState.GetInitialCargoCount() == 0
		&& GameFlowState.GetRemainingCargoCount() == 0;
}

bool ACh4_multiGameGameMode::CanStartGame(const ACh4_multiGameGameState& GameFlowState) const
{
	return GameFlowState.GetCurrentGamePhase() == ECh4GamePhase::Waiting
		&& GameFlowState.GetInitialCargoCount() > 0
		&& GameFlowState.GetRemainingCargoCount() > 0;
}

bool ACh4_multiGameGameMode::CanProcessCargoChange(const ACh4_multiGameGameState& GameFlowState) const
{
	return GameFlowState.GetCurrentGamePhase() == ECh4GamePhase::Playing;
}

bool ACh4_multiGameGameMode::ShouldFailGame(const ACh4_multiGameGameState& GameFlowState) const
{
	const bool bCargoEmptyFailure = GameRuleConfig.bFailWhenCargoEmpty
		&& GameFlowState.GetRemainingCargoCount() <= 0;
	const bool bAnyCargoLostFailure = GameRuleConfig.bFailOnAnyCargoLost
		&& GameFlowState.GetLostCargoCount() > 0;
	return bCargoEmptyFailure || bAnyCargoLostFailure;
}

bool ACh4_multiGameGameMode::CanCompleteGame(const ACh4_multiGameGameState& GameFlowState) const
{
	if (GameFlowState.GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return false;
	}

	const int32 RequiredCargoCount = FMath::Max(GameRuleConfig.MinimumCargoCountToClear, 0);
	const float RequiredSurvivalRate = FMath::Clamp(
		GameRuleConfig.MinimumCargoSurvivalRateToClear,
		0.0f,
		1.0f);
	return GameFlowState.GetRemainingCargoCount() >= RequiredCargoCount
		&& GameFlowState.GetCargoSurvivalRate() + UE_KINDA_SMALL_NUMBER >= RequiredSurvivalRate;
}

bool ACh4_multiGameGameMode::EvaluateGameOutcome(const EGameRuleEvaluationEvent EvaluationEvent)
{
	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return false;
	}

	if (ShouldFailGame(*GameFlowState))
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[GameFlow] Cargo rule failed: Remaining=%d, Lost=%d"),
			GameFlowState->GetRemainingCargoCount(),
			GameFlowState->GetLostCargoCount());
		EndGameAsGameOver(ECh4GameEndReason::CargoRuleFailed);
		return false;
	}

	if (EvaluationEvent != EGameRuleEvaluationEvent::GoalReached)
	{
		return false;
	}

	if (!CanCompleteGame(*GameFlowState))
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[GameFlow] Goal reached but clear requirements were not met: Cargo=%d/%d, Survival=%.3f/%.3f"),
			GameFlowState->GetRemainingCargoCount(),
			FMath::Max(GameRuleConfig.MinimumCargoCountToClear, 0),
			GameFlowState->GetCargoSurvivalRate(),
			FMath::Clamp(GameRuleConfig.MinimumCargoSurvivalRateToClear, 0.0f, 1.0f));
		return false;
	}

	EndGameAsClear(ECh4GameEndReason::GoalReached);
	return true;
}

bool ACh4_multiGameGameMode::IsGamePhaseTransitionAllowed(
	const ECh4GamePhase CurrentPhase,
	const ECh4GamePhase NewPhase) const
{
	return (CurrentPhase == ECh4GamePhase::Waiting && NewPhase == ECh4GamePhase::Playing)
		|| (CurrentPhase == ECh4GamePhase::Playing
			&& (NewPhase == ECh4GamePhase::Cleared || NewPhase == ECh4GamePhase::GameOver));
}

bool ACh4_multiGameGameMode::IsGameEndReasonValidForPhase(
	const ECh4GamePhase GamePhase,
	const ECh4GameEndReason EndReason) const
{
	switch (GamePhase)
	{
	case ECh4GamePhase::Waiting:
	case ECh4GamePhase::Playing:
		return EndReason == ECh4GameEndReason::None;
	case ECh4GamePhase::Cleared:
		return EndReason == ECh4GameEndReason::GoalReached;
	case ECh4GamePhase::GameOver:
		return EndReason == ECh4GameEndReason::CargoRuleFailed;
	default:
		return false;
	}
}

bool ACh4_multiGameGameMode::TryTransitionGamePhase(
	const ECh4GamePhase NewPhase,
	const ECh4GameEndReason EndReason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Phase transition rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState)
	{
		return false;
	}

	const ECh4GamePhase CurrentPhase = GameFlowState->GetCurrentGamePhase();
	if (!IsGamePhaseTransitionAllowed(CurrentPhase, NewPhase))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Invalid phase transition rejected: %d -> %d"),
			static_cast<int32>(CurrentPhase),
			static_cast<int32>(NewPhase));
		return false;
	}

	if (!IsGameEndReasonValidForPhase(NewPhase, EndReason))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Phase transition rejected: %s is inconsistent with %s"),
			*UEnum::GetValueAsString(EndReason),
			*UEnum::GetValueAsString(NewPhase));
		return false;
	}

	return GameFlowState->SetGamePhaseState(NewPhase, EndReason);
}

void ACh4_multiGameGameMode::EndGameAsClear(const ECh4GameEndReason EndReason)
{
	if (!TryTransitionGamePhase(ECh4GamePhase::Cleared, EndReason))
	{
		return;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME CLEARED"));
	if (GameFlowState)
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
			GameFlowState->GetRemainingCargoCount(), GameFlowState->GetInitialCargoCount());
	}
}

void ACh4_multiGameGameMode::EndGameAsGameOver(const ECh4GameEndReason EndReason)
{
	if (TryTransitionGamePhase(ECh4GamePhase::GameOver, EndReason))
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME OVER"));
	}
}
