// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Ch4_multiGameGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowDebugDriver.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFlow/GameFlowTargetInterface.h"
#include "UObject/UnrealType.h"

namespace Ch4GameFlowTests
{
	struct FGameFlowTestWorld
	{
		UWorld* World = nullptr;
		ACh4_multiGameGameMode* GameMode = nullptr;
		ACh4_multiGameGameState* GameState = nullptr;
		IGameFlowRuleInterface* GameRule = nullptr;

		bool Initialize(const FCh4GameRuleConfig* RuleConfig = nullptr)
		{
			if (!GEngine)
			{
				return false;
			}

			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
			{
				return false;
			}

			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);

			GameState = World->SpawnActor<ACh4_multiGameGameState>();
			World->SetGameState(GameState);

			GameMode = World->SpawnActor<ACh4_multiGameGameMode>();
			GameRule = Cast<IGameFlowRuleInterface>(GameMode);
			if (GameMode && RuleConfig)
			{
				GameMode->SetGameRuleConfigForTesting(*RuleConfig);
			}

			return GameMode && GameState && GameRule;
		}

		AActor* SpawnGoalActor() const
		{
			return World ? World->SpawnActor<AActor>() : nullptr;
		}

		~FGameFlowTestWorld()
		{
			if (World)
			{
				World->SetGameState(nullptr);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TestStateInvariants(
		FAutomationTestBase& Test,
		const FString& Context,
		const ACh4_multiGameGameState& GameState)
	{
		const int32 InitialCargoCount = GameState.GetInitialCargoCount();
		const int32 RemainingCargoCount = GameState.GetRemainingCargoCount();
		const int32 LostCargoCount = GameState.GetLostCargoCount();
		const float SurvivalRate = GameState.GetCargoSurvivalRate();
		const int32 FinalCargoScore = GameState.GetFinalCargoScore();

		Test.TestTrue(Context + TEXT(": Initial Cargo is non-negative"), InitialCargoCount >= 0);
		Test.TestTrue(Context + TEXT(": Remaining Cargo is non-negative"), RemainingCargoCount >= 0);
		Test.TestTrue(Context + TEXT(": Remaining Cargo does not exceed Initial Cargo"), RemainingCargoCount <= InitialCargoCount);
		Test.TestTrue(Context + TEXT(": Lost Cargo is non-negative"), LostCargoCount >= 0);
		Test.TestTrue(Context + TEXT(": Lost Cargo does not exceed Initial Cargo"), LostCargoCount <= InitialCargoCount);
		Test.TestEqual(Context + TEXT(": Lost Cargo equals Initial minus Remaining"), LostCargoCount, InitialCargoCount - RemainingCargoCount);
		Test.TestTrue(Context + TEXT(": Survival Rate is at least zero"), SurvivalRate >= 0.0f);
		Test.TestTrue(Context + TEXT(": Survival Rate is at most one"), SurvivalRate <= 1.0f);
		Test.TestTrue(Context + TEXT(": Final Cargo Score is non-negative"), FinalCargoScore >= 0);

		const ECh4GamePhase GamePhase = GameState.GetCurrentGamePhase();
		const ECh4GameEndReason EndReason = GameState.GetGameEndReason();
		const bool bEndReasonMatchesPhase =
			((GamePhase == ECh4GamePhase::Waiting || GamePhase == ECh4GamePhase::Playing)
				&& EndReason == ECh4GameEndReason::None)
			|| (GamePhase == ECh4GamePhase::Cleared
				&& EndReason == ECh4GameEndReason::GoalReached)
			|| (GamePhase == ECh4GamePhase::GameOver
				&& EndReason == ECh4GameEndReason::CargoRuleFailed);
		Test.TestTrue(Context + TEXT(": Phase and End Reason are consistent"), bEndReasonMatchesPhase);
		if (GamePhase != ECh4GamePhase::Cleared)
		{
			Test.TestEqual(Context + TEXT(": Non-cleared phase keeps Final Cargo Score zero"), FinalCargoScore, 0);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4GameFlowStateTransitionsTest,
	"Ch4_multiGame.GameFlow.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4GameFlowStateTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameFlowTests;

	{
		FGameFlowTestWorld ClearFlow;
		if (!ClearFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the clear-flow test world."));
			return false;
		}
		AActor* GoalActor = ClearFlow.SpawnGoalActor();
		TestNotNull(TEXT("Clear-flow goal Actor is valid"), GoalActor);

		TestEqual(TEXT("Initial phase is Waiting"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Waiting));
		TestEqual(TEXT("Initial end reason is None"), static_cast<uint8>(ClearFlow.GameState->GetGameEndReason()), static_cast<uint8>(ECh4GameEndReason::None));
		TestFalse(TEXT("Game cannot start before cargo initialization"), ClearFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Zero cargo cannot be initialized"), ClearFlow.GameRule->RequestCargoInitialization(0));
		TestFalse(TEXT("Negative cargo cannot be initialized"), ClearFlow.GameRule->RequestCargoInitialization(-3));
		TestFalse(TEXT("Negative cargo loss is rejected"), ClearFlow.GameRule->NotifyCargoLost(-1));
		TestFalse(TEXT("Goal is ignored while Waiting"), ClearFlow.GameRule->NotifyGoalReached(GoalActor));
		TestTrue(TEXT("Cargo initializes to 20"), ClearFlow.GameRule->RequestCargoInitialization(20));
		TestTrue(TEXT("Game starts after cargo initialization"), ClearFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Cargo loss updates from 20 to 18"), ClearFlow.GameRule->NotifyCargoLost(2));
		TestTrue(TEXT("Cargo updates from 18 to 13"), ClearFlow.GameMode->UpdateRemainingCargo(13));
		TestTrue(TEXT("Final delivery succeeds with cargo remaining"), ClearFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Clear flow ends as Cleared"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Cleared));
		TestEqual(TEXT("Clear flow keeps 13 cargo"), ClearFlow.GameState->GetRemainingCargoCount(), 13);
		TestEqual(TEXT("Clear flow lost cargo is calculated"), ClearFlow.GameState->GetLostCargoCount(), 7);
		TestTrue(TEXT("Clear flow survival rate is 65 percent"), FMath::IsNearlyEqual(ClearFlow.GameState->GetCargoSurvivalRate(), 0.65f));
		const FCh4GameResult ClearResult = ClearFlow.GameState->GetGameResult();
		TestTrue(TEXT("Clear result is complete"), ClearResult.bGameEnded);
		TestTrue(TEXT("Clear result succeeds"), ClearResult.bSucceeded);
		TestEqual(TEXT("Clear result records GoalReached"), static_cast<uint8>(ClearResult.GameEndReason), static_cast<uint8>(ECh4GameEndReason::GoalReached));
		TestEqual(TEXT("Clear result contains lost cargo"), ClearResult.LostCargoCount, 7);
		TestFalse(TEXT("Cargo changes are ignored after clear"), ClearFlow.GameMode->UpdateRemainingCargo(0));
		TestFalse(TEXT("Cargo loss is ignored after clear"), ClearFlow.GameRule->NotifyCargoLost(1));
		TestFalse(TEXT("Goal is ignored after clear"), ClearFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Cleared cannot become GameOver"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Cleared));
	}

	{
		FGameFlowTestWorld GameOverFlow;
		if (!GameOverFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the game-over test world."));
			return false;
		}
		AActor* GoalActor = GameOverFlow.SpawnGoalActor();
		TestNotNull(TEXT("Game-over goal Actor is valid"), GoalActor);

		TestTrue(TEXT("Game-over cargo initializes to 3"), GameOverFlow.GameRule->RequestCargoInitialization(3));
		TestTrue(TEXT("Game-over flow starts"), GameOverFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Cargo loss updates from 3 to 2"), GameOverFlow.GameRule->NotifyCargoLost(1));
		TestTrue(TEXT("Cargo loss updates from 2 to 1"), GameOverFlow.GameRule->NotifyCargoLost(1));
		TestTrue(TEXT("Cargo loss updates from 1 to 0"), GameOverFlow.GameRule->NotifyCargoLost(1));
		TestEqual(TEXT("Zero cargo ends as GameOver"), static_cast<uint8>(GameOverFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::GameOver));
		const FCh4GameResult GameOverResult = GameOverFlow.GameState->GetGameResult();
		TestTrue(TEXT("Game-over result is complete"), GameOverResult.bGameEnded);
		TestFalse(TEXT("Game-over result fails"), GameOverResult.bSucceeded);
		TestEqual(TEXT("Game-over result records CargoRuleFailed"), static_cast<uint8>(GameOverResult.GameEndReason), static_cast<uint8>(ECh4GameEndReason::CargoRuleFailed));
		TestFalse(TEXT("Cargo loss is ignored after GameOver"), GameOverFlow.GameRule->NotifyCargoLost(1));
		TestFalse(TEXT("Final delivery is ignored after GameOver"), GameOverFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("GameOver cannot become Cleared"), static_cast<uint8>(GameOverFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::GameOver));
	}

	{
		FCh4GameRuleConfig MinimumCountConfig;
		MinimumCountConfig.MinimumCargoCountToClear = 5;

		FGameFlowTestWorld MinimumCountFlow;
		if (!MinimumCountFlow.Initialize(&MinimumCountConfig))
		{
			AddError(TEXT("Failed to initialize the minimum-count test world."));
			return false;
		}
		AActor* GoalActor = MinimumCountFlow.SpawnGoalActor();
		TestNotNull(TEXT("Minimum-count goal Actor is valid"), GoalActor);

		TestTrue(TEXT("Minimum-count cargo initializes to 10"), MinimumCountFlow.GameRule->RequestCargoInitialization(10));
		TestTrue(TEXT("Minimum-count flow starts"), MinimumCountFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Minimum-count flow loses 6 cargo"), MinimumCountFlow.GameRule->NotifyCargoLost(6));
		TestFalse(TEXT("Goal does not clear below minimum cargo count"), MinimumCountFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Insufficient cargo keeps the game Playing"), static_cast<uint8>(MinimumCountFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Playing));
		TestEqual(TEXT("Insufficient cargo has no end reason"), static_cast<uint8>(MinimumCountFlow.GameState->GetGameEndReason()), static_cast<uint8>(ECh4GameEndReason::None));
	}

	{
		FCh4GameRuleConfig SurvivalRateConfig;
		SurvivalRateConfig.MinimumCargoSurvivalRateToClear = 0.75f;

		FGameFlowTestWorld SurvivalRateFlow;
		if (!SurvivalRateFlow.Initialize(&SurvivalRateConfig))
		{
			AddError(TEXT("Failed to initialize the survival-rate test world."));
			return false;
		}
		AActor* GoalActor = SurvivalRateFlow.SpawnGoalActor();
		TestNotNull(TEXT("Survival-rate goal Actor is valid"), GoalActor);

		TestTrue(TEXT("Survival-rate cargo initializes to 10"), SurvivalRateFlow.GameRule->RequestCargoInitialization(10));
		TestTrue(TEXT("Survival-rate flow starts"), SurvivalRateFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Survival-rate flow loses 3 cargo"), SurvivalRateFlow.GameRule->NotifyCargoLost(3));
		TestFalse(TEXT("Goal does not clear below minimum survival rate"), SurvivalRateFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Insufficient survival rate keeps the game Playing"), static_cast<uint8>(SurvivalRateFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Playing));
	}

	{
		FCh4GameRuleConfig AnyLossFailureConfig;
		AnyLossFailureConfig.bFailOnAnyCargoLost = true;

		FGameFlowTestWorld AnyLossFailureFlow;
		if (!AnyLossFailureFlow.Initialize(&AnyLossFailureConfig))
		{
			AddError(TEXT("Failed to initialize the any-loss failure test world."));
			return false;
		}

		TestTrue(TEXT("Any-loss cargo initializes to 5"), AnyLossFailureFlow.GameRule->RequestCargoInitialization(5));
		TestTrue(TEXT("Any-loss flow starts"), AnyLossFailureFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Any-loss flow processes one lost cargo"), AnyLossFailureFlow.GameRule->NotifyCargoLost(1));
		TestEqual(TEXT("Any cargo loss can be configured as GameOver"), static_cast<uint8>(AnyLossFailureFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::GameOver));
		TestEqual(TEXT("Any-loss failure records CargoRuleFailed"), static_cast<uint8>(AnyLossFailureFlow.GameState->GetGameEndReason()), static_cast<uint8>(ECh4GameEndReason::CargoRuleFailed));
	}

	{
		FCh4GameRuleConfig EmptyCargoAllowedConfig;
		EmptyCargoAllowedConfig.bFailWhenCargoEmpty = false;

		FGameFlowTestWorld EmptyCargoAllowedFlow;
		if (!EmptyCargoAllowedFlow.Initialize(&EmptyCargoAllowedConfig))
		{
			AddError(TEXT("Failed to initialize the empty-cargo policy test world."));
			return false;
		}
		AActor* GoalActor = EmptyCargoAllowedFlow.SpawnGoalActor();
		TestNotNull(TEXT("Empty-policy goal Actor is valid"), GoalActor);

		TestTrue(TEXT("Empty-policy cargo initializes to 1"), EmptyCargoAllowedFlow.GameRule->RequestCargoInitialization(1));
		TestTrue(TEXT("Empty-policy flow starts"), EmptyCargoAllowedFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Empty-policy flow processes the final cargo loss"), EmptyCargoAllowedFlow.GameRule->NotifyCargoLost(1));
		TestEqual(TEXT("Disabled empty-cargo failure keeps Playing"), static_cast<uint8>(EmptyCargoAllowedFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Playing));
		TestFalse(TEXT("Zero cargo still cannot satisfy the default clear requirement"), EmptyCargoAllowedFlow.GameRule->NotifyGoalReached(GoalActor));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4GameFlowScoreTest,
	"Ch4_multiGame.GameFlow.Score",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4GameFlowScoreTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameFlowTests;

	{
		FGameFlowTestWorld ProviderlessFlow;
		if (!ProviderlessFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the providerless score test world."));
			return false;
		}

		AActor* GoalActor = ProviderlessFlow.SpawnGoalActor();
		TestNotNull(TEXT("Providerless goal Actor is valid"), GoalActor);
		TestEqual(TEXT("Initial Final Cargo Score is zero"), ProviderlessFlow.GameState->GetFinalCargoScore(), 0);
		TestTrue(TEXT("Providerless flow initializes"), ProviderlessFlow.GameRule->RequestCargoInitialization(2));
		TestTrue(TEXT("Providerless flow starts"), ProviderlessFlow.GameRule->RequestGameStart());
		TestEqual(TEXT("Playing Final Cargo Score remains zero"), ProviderlessFlow.GameState->GetFinalCargoScore(), 0);
		TestTrue(TEXT("Goal without a score provider still clears"), ProviderlessFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Providerless clear finalizes zero score"), ProviderlessFlow.GameState->GetFinalCargoScore(), 0);
		TestEqual(TEXT("Providerless result contains zero score"),
			ProviderlessFlow.GameState->GetGameResult().FinalCargoScore, 0);
	}

	{
		FGameFlowTestWorld ScoredFlow;
		if (!ScoredFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the scored GameFlow test world."));
			return false;
		}

		AActor* GoalActor = ScoredFlow.SpawnGoalActor();
		TestNotNull(TEXT("Scored goal Actor is valid"), GoalActor);
		FCh4DeliveryScoreSummary ScoreSummary;
		ScoreSummary.bHasScoreData = true;
		ScoreSummary.DeliveredCargoCount = 2;
		ScoreSummary.DeliveredCargoScore = 600;
		ScoredFlow.GameMode->SetDeliveryScoreSummaryForTesting(GoalActor, ScoreSummary);

		TestTrue(TEXT("Scored flow initializes"), ScoredFlow.GameRule->RequestCargoInitialization(2));
		TestTrue(TEXT("Scored flow starts"), ScoredFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Score provider goal clears"), ScoredFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Cleared flow finalizes provided score"), ScoredFlow.GameState->GetFinalCargoScore(), 600);
		TestEqual(TEXT("Game result contains finalized score"),
			ScoredFlow.GameState->GetGameResult().FinalCargoScore, 600);

		ScoreSummary.DeliveredCargoScore = 900;
		ScoredFlow.GameMode->SetDeliveryScoreSummaryForTesting(GoalActor, ScoreSummary);
		TestFalse(TEXT("Duplicate goal after Cleared is rejected"), ScoredFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Cleared score is terminal and immutable"), ScoredFlow.GameState->GetFinalCargoScore(), 600);
	}

	{
		FGameFlowTestWorld InvalidSummaryFlow;
		if (!InvalidSummaryFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the invalid-summary test world."));
			return false;
		}

		AActor* GoalActor = InvalidSummaryFlow.SpawnGoalActor();
		FCh4DeliveryScoreSummary InvalidSummary;
		InvalidSummary.bHasScoreData = true;
		InvalidSummary.DeliveredCargoCount = -2;
		InvalidSummary.DeliveredCargoScore = -100;
		InvalidSummaryFlow.GameMode->SetDeliveryScoreSummaryForTesting(GoalActor, InvalidSummary);
		TestTrue(TEXT("Invalid-summary flow initializes"), InvalidSummaryFlow.GameRule->RequestCargoInitialization(1));
		TestTrue(TEXT("Invalid-summary flow starts"), InvalidSummaryFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("A negative delivered count clamps to zero and cannot clear"),
			InvalidSummaryFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Rejected delivered count preserves Playing"), InvalidSummaryFlow.GameState->GetCurrentGamePhase(), ECh4GamePhase::Playing);
		InvalidSummary.DeliveredCargoCount = 1;
		InvalidSummaryFlow.GameMode->SetDeliveryScoreSummaryForTesting(GoalActor, InvalidSummary);
		TestTrue(TEXT("A sufficient delivered count can clear with a clamped zero score"), InvalidSummaryFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Negative provided score clamps to zero"),
			InvalidSummaryFlow.GameState->GetFinalCargoScore(), 0);
	}

	{
		FGameFlowTestWorld GameOverScoreFlow;
		if (!GameOverScoreFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the GameOver score test world."));
			return false;
		}

		TestTrue(TEXT("GameOver score flow initializes"), GameOverScoreFlow.GameRule->RequestCargoInitialization(1));
		TestTrue(TEXT("GameOver score flow starts"), GameOverScoreFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Final Cargo loss is processed"), GameOverScoreFlow.GameRule->NotifyCargoLost(1));
		TestEqual(TEXT("GameOver keeps Final Cargo Score zero"),
			GameOverScoreFlow.GameState->GetFinalCargoScore(), 0);
		TestEqual(TEXT("GameOver result contains zero score"),
			GameOverScoreFlow.GameState->GetGameResult().FinalCargoScore, 0);
	}

	const FProperty* FinalScoreProperty =
		FindFProperty<FProperty>(ACh4_multiGameGameState::StaticClass(), TEXT("FinalCargoScore"));
	TestTrue(TEXT("FinalCargoScore is replicated"),
		FinalScoreProperty && FinalScoreProperty->HasAnyPropertyFlags(CPF_Net));
	TestNull(TEXT("FinalCargoScore has no Blueprint setter"),
		ACh4_multiGameGameState::StaticClass()->FindFunctionByName(TEXT("SetFinalCargoScore")));
	TestNotNull(TEXT("GameFlow target exposes an optional score summary function"),
		UGameFlowTargetInterface::StaticClass()->FindFunctionByName(TEXT("GetDeliveryScoreSummary")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4GameFlowContractInvariantsTest,
	"Ch4_multiGame.GameFlow.ContractInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4GameFlowContractInvariantsTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameFlowTests;

	{
		FGameFlowTestWorld ClearContractFlow;
		if (!ClearContractFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the clear contract test world."));
			return false;
		}

		AActor* GoalActor = ClearContractFlow.SpawnGoalActor();
		TestNotNull(TEXT("Contract goal Actor is valid"), GoalActor);
		TestStateInvariants(*this, TEXT("Initial Waiting state"), *ClearContractFlow.GameState);

		TestFalse(TEXT("Start before initialization is rejected"), ClearContractFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Valid Goal is rejected while Waiting"), ClearContractFlow.GameRule->NotifyGoalReached(GoalActor));
		TestFalse(TEXT("Null Goal Actor is rejected"), ClearContractFlow.GameRule->NotifyGoalReached(nullptr));
		TestFalse(TEXT("Zero Cargo initialization is rejected"), ClearContractFlow.GameRule->RequestCargoInitialization(0));
		TestFalse(TEXT("Negative Cargo initialization is rejected"), ClearContractFlow.GameRule->RequestCargoInitialization(-1));

		TestTrue(TEXT("Valid Cargo initialization succeeds once"), ClearContractFlow.GameRule->RequestCargoInitialization(20));
		TestFalse(TEXT("Same-count duplicate initialization is rejected"), ClearContractFlow.GameRule->RequestCargoInitialization(20));
		TestFalse(TEXT("Different-count duplicate initialization is rejected"), ClearContractFlow.GameRule->RequestCargoInitialization(30));
		TestEqual(TEXT("Duplicate initialization preserves Initial Cargo"), ClearContractFlow.GameState->GetInitialCargoCount(), 20);
		TestEqual(TEXT("Duplicate initialization preserves Remaining Cargo"), ClearContractFlow.GameState->GetRemainingCargoCount(), 20);
		TestStateInvariants(*this, TEXT("Initialized Waiting state"), *ClearContractFlow.GameState);

		TestTrue(TEXT("Valid start succeeds"), ClearContractFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Duplicate start is rejected"), ClearContractFlow.GameRule->RequestGameStart());
		TestStateInvariants(*this, TEXT("Playing state before Cargo loss"), *ClearContractFlow.GameState);

		TestFalse(TEXT("Zero Cargo loss Delta is rejected"), ClearContractFlow.GameRule->NotifyCargoLost(0));
		TestFalse(TEXT("Negative Cargo loss Delta is rejected"), ClearContractFlow.GameRule->NotifyCargoLost(-1));
		TestFalse(TEXT("Cargo loss Delta above Remaining is rejected"), ClearContractFlow.GameRule->NotifyCargoLost(21));
		TestEqual(TEXT("Rejected Cargo loss preserves Remaining Cargo"), ClearContractFlow.GameState->GetRemainingCargoCount(), 20);
		TestFalse(TEXT("Negative legacy absolute Cargo update is rejected"), ClearContractFlow.GameMode->UpdateRemainingCargo(-1));
		TestFalse(TEXT("Legacy absolute Cargo above Initial is rejected"), ClearContractFlow.GameMode->UpdateRemainingCargo(21));
		TestFalse(TEXT("Unchanged legacy absolute Cargo update is rejected"), ClearContractFlow.GameMode->UpdateRemainingCargo(20));
		TestEqual(TEXT("Rejected legacy updates preserve Remaining Cargo"), ClearContractFlow.GameState->GetRemainingCargoCount(), 20);

		AActor* DestroyedGoalActor = ClearContractFlow.SpawnGoalActor();
		TestNotNull(TEXT("Destroyed-goal test Actor was spawned"), DestroyedGoalActor);
		if (DestroyedGoalActor)
		{
			TestTrue(TEXT("Goal test Actor can be destroyed"), DestroyedGoalActor->Destroy());
			TestFalse(TEXT("Destroyed Goal Actor is rejected"), ClearContractFlow.GameRule->NotifyGoalReached(DestroyedGoalActor));
		}

		TestTrue(TEXT("Positive Cargo loss Delta is processed"), ClearContractFlow.GameRule->NotifyCargoLost(2));
		TestEqual(TEXT("Positive Cargo loss updates Remaining Cargo"), ClearContractFlow.GameState->GetRemainingCargoCount(), 18);
		TestFalse(TEXT("Legacy absolute Cargo update cannot increase Cargo"), ClearContractFlow.GameMode->UpdateRemainingCargo(19));
		TestFalse(TEXT("Same-value legacy update is rejected without rebroadcast"), ClearContractFlow.GameMode->UpdateRemainingCargo(18));
		TestStateInvariants(*this, TEXT("Playing state after Cargo loss"), *ClearContractFlow.GameState);

		TestTrue(TEXT("Valid Goal clears the Playing match"), ClearContractFlow.GameRule->NotifyGoalReached(GoalActor));
		TestStateInvariants(*this, TEXT("Cleared terminal state"), *ClearContractFlow.GameState);
		const int32 ClearedRemainingCargo = ClearContractFlow.GameState->GetRemainingCargoCount();

		TestFalse(TEXT("Initialization is rejected after Cleared"), ClearContractFlow.GameRule->RequestCargoInitialization(99));
		TestFalse(TEXT("Start is rejected after Cleared"), ClearContractFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Cargo loss is rejected after Cleared"), ClearContractFlow.GameRule->NotifyCargoLost(1));
		TestFalse(TEXT("Legacy Cargo update is rejected after Cleared"), ClearContractFlow.GameMode->UpdateRemainingCargo(0));
		TestFalse(TEXT("Goal is rejected after Cleared"), ClearContractFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("Cleared remains Cleared"), static_cast<uint8>(ClearContractFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Cleared));
		TestEqual(TEXT("Cleared keeps GoalReached reason"), static_cast<uint8>(ClearContractFlow.GameState->GetGameEndReason()), static_cast<uint8>(ECh4GameEndReason::GoalReached));
		TestEqual(TEXT("Cleared terminal Cargo is immutable"), ClearContractFlow.GameState->GetRemainingCargoCount(), ClearedRemainingCargo);
		TestStateInvariants(*this, TEXT("Cleared state after rejected events"), *ClearContractFlow.GameState);
	}

	{
		FGameFlowTestWorld GameOverContractFlow;
		if (!GameOverContractFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the GameOver contract test world."));
			return false;
		}

		AActor* GoalActor = GameOverContractFlow.SpawnGoalActor();
		TestNotNull(TEXT("GameOver contract goal Actor is valid"), GoalActor);
		TestTrue(TEXT("GameOver contract Cargo initializes"), GameOverContractFlow.GameRule->RequestCargoInitialization(3));
		TestTrue(TEXT("GameOver contract flow starts"), GameOverContractFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Oversized Delta is rejected before GameOver"), GameOverContractFlow.GameRule->NotifyCargoLost(4));
		TestEqual(TEXT("Oversized Delta leaves Remaining Cargo unchanged"), GameOverContractFlow.GameState->GetRemainingCargoCount(), 3);
		TestTrue(TEXT("Exact Remaining Delta reaches zero"), GameOverContractFlow.GameRule->NotifyCargoLost(3));
		TestStateInvariants(*this, TEXT("GameOver terminal state"), *GameOverContractFlow.GameState);

		TestFalse(TEXT("Initialization is rejected after GameOver"), GameOverContractFlow.GameRule->RequestCargoInitialization(5));
		TestFalse(TEXT("Start is rejected after GameOver"), GameOverContractFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Cargo loss is rejected after GameOver"), GameOverContractFlow.GameRule->NotifyCargoLost(1));
		TestFalse(TEXT("Legacy Cargo update is rejected after GameOver"), GameOverContractFlow.GameMode->UpdateRemainingCargo(1));
		TestFalse(TEXT("Goal is rejected after GameOver"), GameOverContractFlow.GameRule->NotifyGoalReached(GoalActor));
		TestEqual(TEXT("GameOver remains GameOver"), static_cast<uint8>(GameOverContractFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::GameOver));
		TestEqual(TEXT("GameOver keeps CargoRuleFailed reason"), static_cast<uint8>(GameOverContractFlow.GameState->GetGameEndReason()), static_cast<uint8>(ECh4GameEndReason::CargoRuleFailed));
		TestEqual(TEXT("GameOver Remaining Cargo stays zero"), GameOverContractFlow.GameState->GetRemainingCargoCount(), 0);
		TestStateInvariants(*this, TEXT("GameOver state after rejected events"), *GameOverContractFlow.GameState);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4GameFlowDebugStartupTest,
	"Ch4_multiGame.GameFlow.DebugStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4GameFlowDebugStartupTest::RunTest(const FString& Parameters)
{
	using namespace Ch4GameFlowTests;

	{
		FGameFlowTestWorld NoDriverFlow;
		if (!NoDriverFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the no-driver test world."));
			return false;
		}

		TestEqual(TEXT("No debug driver leaves the game Waiting"),
			static_cast<uint8>(NoDriverFlow.GameState->GetCurrentGamePhase()),
			static_cast<uint8>(ECh4GamePhase::Waiting));
		TestEqual(TEXT("No debug driver leaves Cargo uninitialized"),
			NoDriverFlow.GameState->GetInitialCargoCount(), 0);
	}

	{
		FGameFlowTestWorld DisabledDriverFlow;
		if (!DisabledDriverFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the disabled-driver test world."));
			return false;
		}

		AGameFlowDebugDriver* DebugDriver = DisabledDriverFlow.World->SpawnActor<AGameFlowDebugDriver>();
		if (!DebugDriver)
		{
			AddError(TEXT("Failed to spawn the disabled debug driver."));
			return false;
		}

		DebugDriver->ConfigureForTesting(false, 20);
		TestFalse(TEXT("Disabled debug startup makes no request"), DebugDriver->RunDebugStartupForTesting());
		TestEqual(TEXT("Disabled debug startup leaves the game Waiting"),
			static_cast<uint8>(DisabledDriverFlow.GameState->GetCurrentGamePhase()),
			static_cast<uint8>(ECh4GamePhase::Waiting));
	}

	{
		FGameFlowTestWorld EnabledDriverFlow;
		if (!EnabledDriverFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the enabled-driver test world."));
			return false;
		}

		AGameFlowDebugDriver* DebugDriver = EnabledDriverFlow.World->SpawnActor<AGameFlowDebugDriver>();
		if (!DebugDriver)
		{
			AddError(TEXT("Failed to spawn the enabled debug driver."));
			return false;
		}

		DebugDriver->ConfigureForTesting(true, 20, EnabledDriverFlow.GameRule);
		TestTrue(TEXT("Enabled server debug startup succeeds"), DebugDriver->RunDebugStartupForTesting());
		TestEqual(TEXT("Enabled debug startup enters Playing"),
			static_cast<uint8>(EnabledDriverFlow.GameState->GetCurrentGamePhase()),
			static_cast<uint8>(ECh4GamePhase::Playing));
		TestEqual(TEXT("Enabled debug startup initializes 20 Cargo"),
			EnabledDriverFlow.GameState->GetInitialCargoCount(), 20);
		TestEqual(TEXT("Enabled debug startup keeps 20 Cargo"),
			EnabledDriverFlow.GameState->GetRemainingCargoCount(), 20);
	}

	{
		FGameFlowTestWorld InvalidCargoFlow;
		if (!InvalidCargoFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the invalid-cargo test world."));
			return false;
		}

		AGameFlowDebugDriver* DebugDriver = InvalidCargoFlow.World->SpawnActor<AGameFlowDebugDriver>();
		if (!DebugDriver)
		{
			AddError(TEXT("Failed to spawn the invalid-cargo debug driver."));
			return false;
		}

		DebugDriver->ConfigureForTesting(true, 0);
		TestFalse(TEXT("Zero debug Cargo cannot start the game"), DebugDriver->RunDebugStartupForTesting());
		TestEqual(TEXT("Invalid debug Cargo leaves the game Waiting"),
			static_cast<uint8>(InvalidCargoFlow.GameState->GetCurrentGamePhase()),
			static_cast<uint8>(ECh4GamePhase::Waiting));
	}

	TestFalse(TEXT("A client cannot attempt debug startup"),
		AGameFlowDebugDriver::CanAttemptDebugStartupForTesting(true, false, 20));
	TestTrue(TEXT("An enabled server can attempt valid debug startup"),
		AGameFlowDebugDriver::CanAttemptDebugStartupForTesting(true, true, 20));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
