// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ch4_multiGameGameMode.h"

#include "Ch4_multiGame.h"
#include "AssetRegistry/AssetData.h"
#include "Cargo/CargoActor.h"
#include "Cart/CartBase.h"
#include "Cart/CartCargoTrackerComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "Misc/AssetRegistryInterface.h"
#include "TimerManager.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Player/Ch4_multiGameGameInstance.h"

ACh4_multiGameGameMode::ACh4_multiGameGameMode()
{
	bUseSeamlessTravel = true;
	GameStateClass = ACh4_multiGameGameState::StaticClass();
	PlayerStateClass = ACh4_multiGamePlayerState::StaticClass();
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Lobby/L_Lobby.L_Lobby")));
}

bool ACh4_multiGameGameMode::RegisterGameplayCart(ACartBase* Cart)
{
	if (!HasAuthority() || !IsValid(Cart) || Cart->IsActorBeingDestroyed()
		|| Cart->GetWorld() != GetWorld())
	{
		return false;
	}
	if (GameplayCart.IsValid() && GameplayCart.Get() != Cart)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[PlayerRecovery] Cart registration rejected: existing=%s new=%s"),
			*GetNameSafe(GameplayCart.Get()), *GetNameSafe(Cart));
		return false;
	}

	GameplayCart = Cart;
	StartPlayerRecoveryChecks();
	return true;
}

ECh4PlayerRecoveryReason ACh4_multiGameGameMode::EvaluatePlayerRecovery(
	const ECh4GamePhase GamePhase,
	const FVector& PlayerLocation,
	const FVector& CartLocation,
	const float MaximumDistance,
	const float MaximumDistanceBelowCart)
{
	if (GamePhase != ECh4GamePhase::Playing)
	{
		return ECh4PlayerRecoveryReason::None;
	}
	if (PlayerLocation.ContainsNaN() || CartLocation.ContainsNaN())
	{
		return ECh4PlayerRecoveryReason::InvalidLocation;
	}
	if (FMath::IsFinite(MaximumDistanceBelowCart) && MaximumDistanceBelowCart > 0.0f
		&& PlayerLocation.Z < CartLocation.Z - MaximumDistanceBelowCart)
	{
		return ECh4PlayerRecoveryReason::BelowCart;
	}
	if (FMath::IsFinite(MaximumDistance) && MaximumDistance > 0.0f
		&& FVector::DistSquared(PlayerLocation, CartLocation) > FMath::Square(MaximumDistance))
	{
		return ECh4PlayerRecoveryReason::DistanceExceeded;
	}
	return ECh4PlayerRecoveryReason::None;
}

void ACh4_multiGameGameMode::BuildPlayerRecoveryCandidates(
	const FVector& CartLocation,
	const FRotator& CartRotation,
	const float BehindDistance,
	const float HeightOffset,
	const float LateralOffset,
	TArray<FVector>& OutCandidates)
{
	OutCandidates.Reset(6);
	const float CartYaw = FMath::IsFinite(CartRotation.Yaw) ? CartRotation.Yaw : 0.0f;
	const FRotationMatrix YawRotation(FRotator(0.0f, CartYaw, 0.0f));
	const FVector Forward = YawRotation.GetUnitAxis(EAxis::X);
	const FVector Right = YawRotation.GetUnitAxis(EAxis::Y);
	const float SafeBehindDistance = FMath::Max(BehindDistance, 0.0f);
	const float SafeHeightOffset = FMath::Max(HeightOffset, 0.0f);
	const float SafeLateralOffset = FMath::Max(LateralOffset, 0.0f);
	const FVector ElevatedCartLocation = CartLocation + FVector::UpVector * SafeHeightOffset;
	const FVector Behind = ElevatedCartLocation - Forward * SafeBehindDistance;
	const FVector FarBehind = ElevatedCartLocation
		- Forward * (SafeBehindDistance + SafeLateralOffset);

	OutCandidates.Add(Behind);
	OutCandidates.Add(Behind - Right * SafeLateralOffset);
	OutCandidates.Add(Behind + Right * SafeLateralOffset);
	OutCandidates.Add(FarBehind);
	OutCandidates.Add(FarBehind - Right * SafeLateralOffset);
	OutCandidates.Add(FarBehind + Right * SafeLateralOffset);
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

void ACh4_multiGameGameMode::SetResultDisplayDurationForTesting(const float NewDurationSeconds)
{
	ResultDisplayDurationSeconds = NewDurationSeconds;
}

void ACh4_multiGameGameMode::SetGameplayTimesForTesting(
	const float StartServerTimeSeconds,
	const float GoalServerTimeSeconds)
{
	bGameplayStartTimeRecorded = true;
	GameplayStartServerTimeSeconds = StartServerTimeSeconds;
	GoalServerTimeOverrideForTesting = GoalServerTimeSeconds;
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
	bGameplayStartTimeRecorded = true;
	GameplayStartServerTimeSeconds = GameFlowState->GetServerWorldTimeSeconds();

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Game Started: ServerTime=%.2f"),
		GameplayStartServerTimeSeconds);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
		GameFlowState->GetRemainingCargoCount(), GameFlowState->GetInitialCargoCount());
	StartPlayerRecoveryChecks();
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
	if (GameFlowState->HasGameResultSnapshot())
	{
		UE_LOG(LogCh4_multiGame, Verbose, TEXT("[GameFlow] Duplicate Goal notification reused the frozen result"));
		return true;
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
	if (DeliveryScoreSummary.bHasScoreData && DeliveryScoreSummary.DeliveredCargoCount == 0)
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Goal reached with empty cart"));
		if (!CaptureGoalResultSnapshot(false, 0, 0))
		{
			return false;
		}
		return ScheduleEmptyCartFailure();
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

	const int32 CargoCountAtGoal = DeliveredCargoCount == INDEX_NONE
		? GameFlowState->GetRemainingCargoCount() : DeliveredCargoCount;
	if (!EndGameAsClear(ECh4GameEndReason::GoalReached, FinalCargoScore))
	{
		return false;
	}
	if (!CaptureGoalResultSnapshot(true, CargoCountAtGoal, FinalCargoScore))
	{
		UE_LOG(LogCh4_multiGame, Error, TEXT("[GameFlow] Cleared without capturing the Goal result snapshot"));
		return false;
	}
	ScheduleReturnToLobby();
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

	const bool bTransitioned = GameFlowState->SetGamePhaseState(NewPhase, EndReason, FinalCargoScore);
	if (bTransitioned && (NewPhase == ECh4GamePhase::Cleared || NewPhase == ECh4GamePhase::GameOver))
	{
		StopPlayerRecoveryChecks();
		ClearEmptyCartFailure();
	}
	return bTransitioned;
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
	return true;
}

bool ACh4_multiGameGameMode::EndGameAsGameOver(const ECh4GameEndReason EndReason)
{
	if (TryTransitionGamePhase(ECh4GamePhase::GameOver, EndReason))
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME OVER"));
		return true;
	}
	return false;
}

float ACh4_multiGameGameMode::GetValidatedResultDisplayDuration() const
{
	if (!FMath::IsFinite(ResultDisplayDurationSeconds))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Invalid result display duration; using the next server tick"));
		return 0.0f;
	}
	return FMath::Max(ResultDisplayDurationSeconds, 0.0f);
}

bool ACh4_multiGameGameMode::CaptureGoalResultSnapshot(
	const bool bSucceeded,
	const int32 DeliveredCargoCount,
	const int32 DeliveredCargoScore)
{
	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!HasAuthority() || !GameFlowState || GameFlowState->HasGameResultSnapshot())
	{
		return false;
	}

	float GoalServerTimeSeconds = GameFlowState->GetServerWorldTimeSeconds();
#if WITH_DEV_AUTOMATION_TESTS
	if (GoalServerTimeOverrideForTesting.IsSet())
	{
		GoalServerTimeSeconds = GoalServerTimeOverrideForTesting.GetValue();
	}
#endif
	const float ClearTimeSeconds = bGameplayStartTimeRecorded
		? FMath::Max(GoalServerTimeSeconds - GameplayStartServerTimeSeconds, 0.0f)
		: 0.0f;
	if (!bGameplayStartTimeRecorded)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow] Goal result captured without a recorded RequestGameStart time"));
	}
	const float ResultDisplayEndServerTime = GoalServerTimeSeconds + GetValidatedResultDisplayDuration();
	const bool bCaptured = GameFlowState->SetGoalResultSnapshot(
		ClearTimeSeconds,
		DeliveredCargoCount,
		DeliveredCargoScore,
		bSucceeded,
		ResultDisplayEndServerTime);
	if (bCaptured)
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[GameFlow] Result captured: Success=%s ClearTime=%.2f Cargo=%d Score=%d DisplayEnd=%.2f"),
			bSucceeded ? TEXT("true") : TEXT("false"),
			ClearTimeSeconds,
			FMath::Max(DeliveredCargoCount, 0),
			bSucceeded ? FMath::Max(DeliveredCargoScore, 0) : 0,
			ResultDisplayEndServerTime);
	}
	return bCaptured;
}

bool ACh4_multiGameGameMode::ScheduleEmptyCartFailure()
{
	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!HasAuthority() || !GetWorld() || !GameFlowState
		|| GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return false;
	}
	if (bEmptyCartFailurePending)
	{
		return true;
	}

	const float DelaySeconds = GetValidatedResultDisplayDuration();

	bEmptyCartFailurePending = true;
	if (DelaySeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(EmptyCartFailureTimer, this,
			&ACh4_multiGameGameMode::OnEmptyCartFailureTimer, DelaySeconds, false);
	}
	else
	{
		EmptyCartFailureTimer = GetWorldTimerManager().SetTimerForNextTick(
			this, &ACh4_multiGameGameMode::OnEmptyCartFailureTimer);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Empty cart failure scheduled: %.2fs"), DelaySeconds);
	return true;
}

void ACh4_multiGameGameMode::OnEmptyCartFailureTimer()
{
	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!bEmptyCartFailurePending || !HasAuthority() || !GameFlowState
		|| GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		ClearEmptyCartFailure();
		return;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Empty cart failure triggered"));
	if (EndGameAsGameOver(ECh4GameEndReason::CargoRuleFailed))
	{
		// Empty results already spent the shared display duration in Playing.
		// Travel immediately after GameOver so no second result timer is introduced.
		StartLobbyTravel();
	}
}

void ACh4_multiGameGameMode::ClearEmptyCartFailure()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(EmptyCartFailureTimer);
	}
	bEmptyCartFailurePending = false;
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
	FAssetData MapAsset;
	const IAssetRegistryInterface* AssetRegistry = IAssetRegistryInterface::GetPtr();
	if (!FPackageName::IsValidLongPackageName(Package)
		|| !FPackageName::DoesPackageExist(Package)
		|| !AssetRegistry
		|| AssetRegistry->TryGetAssetByObjectPath(LobbyMap.ToSoftObjectPath(), MapAsset) != UE::AssetRegistry::EExists::Exists
		|| MapAsset.AssetClassPath != UWorld::StaticClass()->GetClassPathName())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Lobby return rejected: configure a valid Lobby World asset"));
		return false;
	}
	OutURL = Package;
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
	const float DelaySeconds = GetValidatedResultDisplayDuration();
	bReturnToLobbyScheduled = true;
	if (DelaySeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ReturnToLobbyTimer, this,
			&ACh4_multiGameGameMode::OnReturnToLobbyTimer, DelaySeconds, false);
	}
	else
	{
		ReturnToLobbyTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ACh4_multiGameGameMode::OnReturnToLobbyTimer);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Lobby return scheduled in %.2f seconds"), DelaySeconds);
	return true;
}

void ACh4_multiGameGameMode::OnReturnToLobbyTimer()
{
	ReturnToLobby();
}

bool ACh4_multiGameGameMode::ReturnToLobby()
{
	const ACh4_multiGameGameState* State = GetGameFlowGameState();
	if (!State
		|| (State->GetCurrentGamePhase() != ECh4GamePhase::Cleared
			&& (State->GetCurrentGamePhase() != ECh4GamePhase::GameOver
				|| !State->HasGameResultSnapshot())))
	{
		return false;
	}
	return StartLobbyTravel();
}

bool ACh4_multiGameGameMode::ReturnToLobbyFromPreparation()
{
	const ACh4_multiGameGameState* State = GetGameFlowGameState();
	if (!State || State->GetCurrentGamePhase() != ECh4GamePhase::Waiting
		|| State->GetInitialCargoCount() != 0 || bUsesPreparationCargoRoster
		|| bPreparationTransitionPending)
	{
		return false;
	}
	return StartLobbyTravel();
}

bool ACh4_multiGameGameMode::StartLobbyTravel()
{
	if (!HasAuthority() || !GetWorld() || bLobbyTravelStarted)
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
	if (const auto* GI = GetGameInstance<UCh4_multiGameGameInstance>())
	{
		GI->LogMatchTravel(GetWorld(), URL, bUseSeamlessTravel);
	}
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

void ACh4_multiGameGameMode::StartPlayerRecoveryChecks()
{
	const ACh4_multiGameGameState* State = GetGameFlowGameState();
	if (!bEnablePlayerCartRecovery || !HasAuthority() || !GetWorld() || !GameplayCart.IsValid()
		|| !State || State->GetCurrentGamePhase() != ECh4GamePhase::Playing
		|| GetWorldTimerManager().IsTimerActive(PlayerRecoveryTimer))
	{
		return;
	}
	if (!FMath::IsFinite(PlayerRecoveryCheckIntervalSeconds) || PlayerRecoveryCheckIntervalSeconds < 0.1f
		|| !FMath::IsFinite(MaximumPlayerCartDistance) || MaximumPlayerCartDistance <= 0.0f
		|| !FMath::IsFinite(MaximumVerticalDistanceBelowCart) || MaximumVerticalDistanceBelowCart <= 0.0f)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[PlayerRecovery] Disabled by invalid interval or distance settings on %s"), *GetName());
		return;
	}

	GetWorldTimerManager().SetTimer(
		PlayerRecoveryTimer,
		this,
		&ACh4_multiGameGameMode::CheckPlayersForRecovery,
		PlayerRecoveryCheckIntervalSeconds,
		true,
		PlayerRecoveryCheckIntervalSeconds);
}

void ACh4_multiGameGameMode::StopPlayerRecoveryChecks()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PlayerRecoveryTimer);
	}
	RecoveryLocationFailureWarnings.Reset();
}

const TCHAR* ACh4_multiGameGameMode::GetPlayerRecoveryReasonName(const ECh4PlayerRecoveryReason Reason)
{
	switch (Reason)
	{
	case ECh4PlayerRecoveryReason::DistanceExceeded:
		return TEXT("DistanceExceeded");
	case ECh4PlayerRecoveryReason::BelowCart:
		return TEXT("BelowCart");
	case ECh4PlayerRecoveryReason::InvalidLocation:
		return TEXT("InvalidLocation");
	default:
		return TEXT("None");
	}
}

bool ACh4_multiGameGameMode::FindSafePlayerRecoveryLocation(
	const ACh4_PlayerCharacter& Player,
	FVector& OutLocation) const
{
	const ACartBase* Cart = GameplayCart.Get();
	const UCapsuleComponent* Capsule = Player.GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = Player.GetCharacterMovement();
	UWorld* World = GetWorld();
	if (!World || !IsValid(Cart) || !Capsule || !Movement
		|| !FMath::IsFinite(RecoveryBehindCartDistance) || !FMath::IsFinite(RecoveryHeightOffset)
		|| !FMath::IsFinite(RecoveryGroundTraceHeight) || !FMath::IsFinite(RecoveryGroundTraceDepth))
	{
		return false;
	}

	float CapsuleRadius = 0.0f;
	float CapsuleHalfHeight = 0.0f;
	Capsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	const float LateralOffset = FMath::Max(CapsuleRadius * 2.0f + 50.0f, 150.0f);
	TArray<FVector> Candidates;
	BuildPlayerRecoveryCandidates(
		Cart->GetActorLocation(),
		Cart->GetActorRotation(),
		RecoveryBehindCartDistance,
		RecoveryHeightOffset,
		LateralOffset,
		Candidates);

	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(PlayerRecoveryGroundTrace), false, &Player);
	GroundParams.AddIgnoredActor(Cart);
	FCollisionQueryParams SpaceParams(SCENE_QUERY_STAT(PlayerRecoveryCapsuleSpace), false, &Player);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	const float MinimumGroundNormalZ = Movement->GetWalkableFloorZ();
	constexpr float CapsuleGroundSafetyOffset = 5.0f;

	for (const FVector& Candidate : Candidates)
	{
		if (Candidate.ContainsNaN())
		{
			continue;
		}
		FHitResult GroundHit;
		const FVector TraceStart = Candidate + FVector::UpVector * FMath::Max(RecoveryGroundTraceHeight, 0.0f);
		const FVector TraceEnd = Candidate - FVector::UpVector * FMath::Max(RecoveryGroundTraceDepth, 1.0f);
		if (!World->LineTraceSingleByChannel(
			GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundParams)
			|| !GroundHit.bBlockingHit || GroundHit.ImpactNormal.Z < MinimumGroundNormalZ
			|| !IsValid(GroundHit.GetComponent()) || GroundHit.GetComponent()->IsSimulatingPhysics())
		{
			continue;
		}

		const FVector CapsuleLocation = GroundHit.ImpactPoint
			+ FVector::UpVector * (CapsuleHalfHeight + CapsuleGroundSafetyOffset);
		if (CapsuleLocation.ContainsNaN()
			|| World->OverlapBlockingTestByChannel(
				CapsuleLocation,
				FQuat::Identity,
				Capsule->GetCollisionObjectType(),
				CapsuleShape,
				SpaceParams))
		{
			continue;
		}

		OutLocation = CapsuleLocation;
		return true;
	}
	return false;
}

bool ACh4_multiGameGameMode::RecoverPlayer(
	ACh4_PlayerCharacter& Player,
	const ECh4PlayerRecoveryReason Reason)
{
	ACartBase* Cart = GameplayCart.Get();
	if (!HasAuthority() || !IsValid(Cart))
	{
		return false;
	}
	const TWeakObjectPtr<ACh4_PlayerCharacter> PlayerKey(&Player);
	const FVector OriginalPlayerLocation = Player.GetActorLocation();
	const FVector CartLocation = Cart->GetActorLocation();
	const double OriginalDistance = OriginalPlayerLocation.ContainsNaN() || CartLocation.ContainsNaN()
		? -1.0
		: FVector::Distance(OriginalPlayerLocation, CartLocation);

	FVector Destination;
	if (!FindSafePlayerRecoveryLocation(Player, Destination))
	{
		if (!RecoveryLocationFailureWarnings.Contains(PlayerKey))
		{
			RecoveryLocationFailureWarnings.Add(PlayerKey);
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[PlayerRecovery] No safe ground/capsule candidate: Player=%s Reason=%s"),
				*GetNameSafe(&Player), GetPlayerRecoveryReasonName(Reason));
		}
		return false;
	}
	if (!Player.ReleaseGrabsForRecovery())
	{
		if (!RecoveryLocationFailureWarnings.Contains(PlayerKey))
		{
			RecoveryLocationFailureWarnings.Add(PlayerKey);
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[PlayerRecovery] Grab release failed; teleport deferred: Player=%s"),
				*GetNameSafe(&Player));
		}
		return false;
	}

	if (UCharacterMovementComponent* Movement = Player.GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->ClearAccumulatedForces();
	}
	Player.ConsumeMovementInputVector();
	const FRotator DestinationRotation(0.0f, Cart->GetActorRotation().Yaw, 0.0f);
	if (!Player.TeleportTo(Destination, DestinationRotation, false, true))
	{
		if (!RecoveryLocationFailureWarnings.Contains(PlayerKey))
		{
			RecoveryLocationFailureWarnings.Add(PlayerKey);
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[PlayerRecovery] Teleport failed: Player=%s Destination=%s"),
				*GetNameSafe(&Player), *Destination.ToString());
		}
		return false;
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Player.GetController()))
	{
		PlayerController->SetControlRotation(DestinationRotation);
		PlayerController->ClientSetRotation(DestinationRotation, true);
	}
	Player.ForceNetUpdate();
	RecoveryLocationFailureWarnings.Remove(PlayerKey);
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[PlayerRecovery] Player=%s Distance=%.1f Reason=%s Destination=%s"),
		*GetNameSafe(&Player), OriginalDistance, GetPlayerRecoveryReasonName(Reason), *Destination.ToString());
	return true;
}

void ACh4_multiGameGameMode::CheckPlayersForRecovery()
{
	ACartBase* Cart = GameplayCart.Get();
	const ACh4_multiGameGameState* State = GetGameFlowGameState();
	if (!bEnablePlayerCartRecovery || !HasAuthority() || !IsValid(Cart) || !State
		|| State->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	for (auto It = RecoveryLocationFailureWarnings.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		ACh4_PlayerCharacter* Player = IsValid(PlayerController)
			? Cast<ACh4_PlayerCharacter>(PlayerController->GetPawn())
			: nullptr;
		if (!IsValid(Player) || Player->IsActorBeingDestroyed())
		{
			continue;
		}

		const ECh4PlayerRecoveryReason Reason = EvaluatePlayerRecovery(
			State->GetCurrentGamePhase(),
			Player->GetActorLocation(),
			Cart->GetActorLocation(),
			MaximumPlayerCartDistance,
			MaximumVerticalDistanceBelowCart);
		if (Reason == ECh4PlayerRecoveryReason::None)
		{
			RecoveryLocationFailureWarnings.Remove(TWeakObjectPtr<ACh4_PlayerCharacter>(Player));
			continue;
		}
		RecoverPlayer(*Player, Reason);
	}
}

void ACh4_multiGameGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlayerRecoveryChecks();
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTimer);
	ClearEmptyCartFailure();
	bReturnToLobbyScheduled = false;
	Super::EndPlay(EndPlayReason);
}
