// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ch4_multiGameGameMode.h"

#include "Ch4_multiGame.h"
#include "Cargo/CargoActor.h"
#include "Cart/CartCargoTrackerComponent.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "Player/Ch4_multiGamePlayerState.h"

ACh4_multiGameGameMode::ACh4_multiGameGameMode()
{
	GameStateClass = ACh4_multiGameGameState::StaticClass();
	PlayerStateClass = ACh4_multiGamePlayerState::StaticClass();
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Lobby/L_Lobby.L_Lobby")));
}

#if WITH_DEV_AUTOMATION_TESTS
void ACh4_multiGameGameMode::SetGameRuleConfigForTesting(const FCh4GameRuleConfig& NewGameRuleConfig)
{
	GameRuleConfig = NewGameRuleConfig;
}

void ACh4_multiGameGameMode::SetDeliveryScoreSummaryForTesting(
	AActor* TargetActor,
	const FCh4DeliveryScoreSummary& NewDeliveryScoreSummary)
{
	DeliveryScoreTargetOverrideForTesting = TargetActor;
	DeliveryScoreSummaryOverrideForTesting = NewDeliveryScoreSummary;
}
#endif

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

bool ACh4_multiGameGameMode::RequestGameStart()
{
	if (bPreparationTransitionPending)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Game start rejected while the preparation load is moving"));
		return false;
	}
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
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Legacy Cargo update rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState)
	{
		return false;
	}

	const int32 CurrentCargoCount = GameFlowState->GetRemainingCargoCount();
	if (NewRemainingCargo < 0 || NewRemainingCargo > CurrentCargoCount)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Legacy Cargo update rejected: count %d is outside [0, %d]"),
			NewRemainingCargo,
			CurrentCargoCount);
		return false;
	}

	return NotifyCargoLost(CurrentCargoCount - NewRemainingCargo);
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

bool ACh4_multiGameGameMode::NotifyGoalReached(AActor* ReachingActor)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Goal notification rejected without server authority"));
		return false;
	}

	if (!IsValid(ReachingActor)
		|| ReachingActor->IsActorBeingDestroyed()
		|| ReachingActor->GetWorld() != GetWorld())
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
		*GetNameSafe(ReachingActor));

	const FCh4DeliveryScoreSummary DeliveryScoreSummary = GetValidatedDeliveryScoreSummary(ReachingActor);
	if (DeliveryScoreSummary.bHasScoreData
		&& DeliveryScoreSummary.DeliveredCargoCount != GameFlowState->GetRemainingCargoCount())
	{
		UE_LOG(LogCh4_multiGame, Verbose,
			TEXT("[GameFlow] Delivered Cargo count differs from GameState Remaining Cargo: Delivered=%d, Remaining=%d"),
			DeliveryScoreSummary.DeliveredCargoCount,
			GameFlowState->GetRemainingCargoCount());
	}

	return EvaluateGameOutcome(
		EGameRuleEvaluationEvent::GoalReached,
		DeliveryScoreSummary.DeliveredCargoScore,
		DeliveryScoreSummary.bHasScoreData ? DeliveryScoreSummary.DeliveredCargoCount : INDEX_NONE);
}

FCh4DeliveryScoreSummary ACh4_multiGameGameMode::GetValidatedDeliveryScoreSummary(AActor* ReachingActor) const
{
	FCh4DeliveryScoreSummary RawSummary;
#if WITH_DEV_AUTOMATION_TESTS
	if (DeliveryScoreTargetOverrideForTesting.Get() == ReachingActor)
	{
		RawSummary = DeliveryScoreSummaryOverrideForTesting;
	}
	else
#endif
	if (IsValid(ReachingActor)
		&& ReachingActor->GetClass()->ImplementsInterface(UGameFlowTargetInterface::StaticClass()))
	{
		RawSummary = IGameFlowTargetInterface::Execute_GetDeliveryScoreSummary(ReachingActor);
	}

	// An actual Cart with a native tracker must never silently use the providerless debug clear rule.
	// This only queries the reaching actor's component, never all Cargo in the world.
	if (!RawSummary.bHasScoreData && IsValid(ReachingActor))
	{
		if (const UCartCargoTrackerComponent* Tracker = ReachingActor->FindComponentByClass<UCartCargoTrackerComponent>())
		{
			RawSummary = Tracker->BuildDeliveryScoreSummary();
		}
	}

	if (!RawSummary.bHasScoreData)
	{
		if (RawSummary.DeliveredCargoCount != 0 || RawSummary.DeliveredCargoScore != 0)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow] Ignored delivery score values because the target reported no score data: Count=%d, Score=%d"),
				RawSummary.DeliveredCargoCount,
				RawSummary.DeliveredCargoScore);
		}
		return FCh4DeliveryScoreSummary();
	}

	FCh4DeliveryScoreSummary ValidatedSummary = RawSummary;
	ValidatedSummary.DeliveredCargoCount = FMath::Max(RawSummary.DeliveredCargoCount, 0);
	ValidatedSummary.DeliveredCargoScore = FMath::Max(RawSummary.DeliveredCargoScore, 0);
	if (ValidatedSummary.DeliveredCargoCount != RawSummary.DeliveredCargoCount
		|| ValidatedSummary.DeliveredCargoScore != RawSummary.DeliveredCargoScore)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Clamped invalid delivery score summary: Count=%d->%d, Score=%d->%d"),
			RawSummary.DeliveredCargoCount,
			ValidatedSummary.DeliveredCargoCount,
			RawSummary.DeliveredCargoScore,
			ValidatedSummary.DeliveredCargoScore);
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[GameFlow] Delivery score summary received: Cargo=%d, Score=%d"),
		ValidatedSummary.DeliveredCargoCount,
		ValidatedSummary.DeliveredCargoScore);
	return ValidatedSummary;
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

bool ACh4_multiGameGameMode::CanCompleteGame(
	const ACh4_multiGameGameState& GameFlowState, const int32 DeliveredCargoCount) const
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
	const int32 CargoCountAtGoal = DeliveredCargoCount == INDEX_NONE
		? GameFlowState.GetRemainingCargoCount() : DeliveredCargoCount;
	return CargoCountAtGoal >= RequiredCargoCount
		&& GameFlowState.GetCargoSurvivalRate() + UE_KINDA_SMALL_NUMBER >= RequiredSurvivalRate;
}

bool ACh4_multiGameGameMode::EvaluateGameOutcome(
	const EGameRuleEvaluationEvent EvaluationEvent,
	const int32 FinalCargoScore,
	const int32 DeliveredCargoCount)
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

	if (!CanCompleteGame(*GameFlowState, DeliveredCargoCount))
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[GameFlow] Goal reached but clear requirements were not met: CargoAtGoal=%d/%d, Survival=%.3f/%.3f"),
			DeliveredCargoCount == INDEX_NONE ? GameFlowState->GetRemainingCargoCount() : DeliveredCargoCount,
			FMath::Max(GameRuleConfig.MinimumCargoCountToClear, 0),
			GameFlowState->GetCargoSurvivalRate(),
			FMath::Clamp(GameRuleConfig.MinimumCargoSurvivalRateToClear, 0.0f, 1.0f));
		return false;
	}

	return EndGameAsClear(ECh4GameEndReason::GoalReached, FinalCargoScore);
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
	const ECh4GameEndReason EndReason,
	const int32 FinalCargoScore)
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

	return GameFlowState->SetGamePhaseState(NewPhase, EndReason, FinalCargoScore);
}

bool ACh4_multiGameGameMode::EndGameAsClear(
	const ECh4GameEndReason EndReason,
	const int32 FinalCargoScore)
{
	if (!TryTransitionGamePhase(ECh4GamePhase::Cleared, EndReason, FinalCargoScore))
	{
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME CLEARED"));
	if (GameFlowState)
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
			GameFlowState->GetRemainingCargoCount(), GameFlowState->GetInitialCargoCount());
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Final Cargo Score: %d"),
			GameFlowState->GetFinalCargoScore());
	}
	if (bAutoReturnToLobbyOnClear)
	{
		ScheduleReturnToLobby();
	}
	return true;
}

void ACh4_multiGameGameMode::EndGameAsGameOver(const ECh4GameEndReason EndReason)
{
	if (TryTransitionGamePhase(ECh4GamePhase::GameOver, EndReason))
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME OVER"));
	}
}

bool ACh4_multiGameGameMode::InitializePreparationCargo(const TArray<ACargoActor*>& CargoSnapshot)
{
	if (!HasAuthority() || bUsesPreparationCargoRoster || CargoSnapshot.IsEmpty())
	{
		return false;
	}
	TSet<TWeakObjectPtr<AActor>> NewRoster;
	for (ACargoActor* Cargo : CargoSnapshot)
	{
		if (!IsValid(Cargo) || Cargo->IsActorBeingDestroyed() || Cargo->IsLost() || Cargo->GetWorld() != GetWorld())
		{
			return false;
		}
		NewRoster.Add(Cargo);
	}
	if (NewRoster.Num() != CargoSnapshot.Num())
	{
		return false;
	}
	// Install the roster before initialization broadcasts; roll back only if initialization is rejected.
	PreparationCargoRoster = MoveTemp(NewRoster);
	bUsesPreparationCargoRoster = true;
	bPreparationTransitionPending = true;
	if (!RequestCargoInitialization(PreparationCargoRoster.Num()))
	{
		PreparationCargoRoster.Reset();
		bUsesPreparationCargoRoster = false;
		bPreparationTransitionPending = false;
		return false;
	}
	return true;
}

bool ACh4_multiGameGameMode::IsCargoPartOfMatch(const AActor* CargoActor) const
{
	return !bUsesPreparationCargoRoster
		|| PreparationCargoRoster.Contains(TWeakObjectPtr<AActor>(const_cast<AActor*>(CargoActor)));
}

bool ACh4_multiGameGameMode::FinishPreparationTransition()
{
	if (!HasAuthority() || !bPreparationTransitionPending)
	{
		return false;
	}
	bPreparationTransitionPending = false;
	return RequestGameStart();
}

bool ACh4_multiGameGameMode::GetLobbyTravelURL(FString& OutURL) const
{
	const FString Package = LobbyMap.ToSoftObjectPath().GetLongPackageName();
	FString Filename;
	if (!FPackageName::IsValidLongPackageName(Package)
		|| !FPackageName::DoesPackageExist(Package, &Filename)
		|| !FPaths::GetExtension(Filename, true).Equals(FPackageName::GetMapPackageExtension(), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Lobby return rejected: configure a valid Lobby World asset"));
		return false;
	}
	OutURL = Package;
	if (GetNetMode() == NM_ListenServer)
	{
		OutURL += TEXT("?listen");
	}
	return true;
}

bool ACh4_multiGameGameMode::ScheduleReturnToLobby()
{
	if (!HasAuthority() || !GetWorld() || bReturnToLobbyScheduled || bLobbyTravelStarted)
	{
		return false;
	}
	const ACh4_multiGameGameState* State = GetGameFlowGameState();
	FString URL;
	if (!State || State->GetCurrentGamePhase() != ECh4GamePhase::Cleared || !GetLobbyTravelURL(URL))
	{
		return false;
	}
	if (!FMath::IsFinite(ReturnToLobbyDelaySeconds) || ReturnToLobbyDelaySeconds < 0.0f)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Lobby return rejected: invalid return delay"));
		return false;
	}
	bReturnToLobbyScheduled = true;
	if (ReturnToLobbyDelaySeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ReturnToLobbyTimer, this,
			&ACh4_multiGameGameMode::OnReturnToLobbyTimer, ReturnToLobbyDelaySeconds, false);
	}
	else
	{
		ReturnToLobbyTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ACh4_multiGameGameMode::OnReturnToLobbyTimer);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Lobby return scheduled in %.2f seconds"), ReturnToLobbyDelaySeconds);
	return true;
}

void ACh4_multiGameGameMode::OnReturnToLobbyTimer()
{
	ReturnToLobby();
}

bool ACh4_multiGameGameMode::ReturnToLobby()
{
	if (!HasAuthority() || !GetWorld() || bLobbyTravelStarted)
	{
		return false;
	}
	const ACh4_multiGameGameState* State = GetGameFlowGameState();
	if (!State || State->GetCurrentGamePhase() != ECh4GamePhase::Cleared)
	{
		return false;
	}
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTimer);
	bReturnToLobbyScheduled = false;
	FString URL;
	if (!GetLobbyTravelURL(URL))
	{
		return false;
	}
	bLobbyTravelStarted = true;
#if WITH_DEV_AUTOMATION_TESTS
	const bool bAccepted = LobbyTravelForTesting ? LobbyTravelForTesting(URL) : GetWorld()->ServerTravel(URL, true);
#else
	const bool bAccepted = GetWorld()->ServerTravel(URL, true);
#endif
	if (!bAccepted)
	{
		bLobbyTravelStarted = false;
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] ServerTravel to Lobby was rejected; manual retry is allowed"));
		return false;
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] ServerTravel to Lobby: %s"), *URL);
	return true;
}

void ACh4_multiGameGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTimer);
	bReturnToLobbyScheduled = false;
	Super::EndPlay(EndPlayReason);
}
