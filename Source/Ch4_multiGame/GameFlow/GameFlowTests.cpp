// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Ch4_multiGameGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFlow/GameFlowRuleInterface.h"

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

		TestEqual(TEXT("Initial phase is Waiting"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Waiting));
		TestEqual(TEXT("Initial end reason is None"), static_cast<uint8>(ClearFlow.GameState->GetGameEndReason()), static_cast<uint8>(ECh4GameEndReason::None));
		TestFalse(TEXT("Game cannot start before cargo initialization"), ClearFlow.GameRule->RequestGameStart());
		TestFalse(TEXT("Zero cargo cannot be initialized"), ClearFlow.GameRule->RequestCargoInitialization(0));
		TestFalse(TEXT("Negative cargo cannot be initialized"), ClearFlow.GameRule->RequestCargoInitialization(-3));
		TestFalse(TEXT("Negative cargo loss is rejected"), ClearFlow.GameRule->NotifyCargoLost(-1));
		TestFalse(TEXT("Goal is ignored while Waiting"), ClearFlow.GameRule->NotifyGoalReached(nullptr));
		TestTrue(TEXT("Cargo initializes to 20"), ClearFlow.GameRule->RequestCargoInitialization(20));
		TestTrue(TEXT("Game starts after cargo initialization"), ClearFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Cargo loss updates from 20 to 18"), ClearFlow.GameRule->NotifyCargoLost(2));
		TestTrue(TEXT("Cargo updates from 18 to 13"), ClearFlow.GameMode->UpdateRemainingCargo(13));
		TestTrue(TEXT("Final delivery succeeds with cargo remaining"), ClearFlow.GameRule->NotifyGoalReached(nullptr));
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
		TestFalse(TEXT("Goal is ignored after clear"), ClearFlow.GameRule->NotifyGoalReached(nullptr));
		TestEqual(TEXT("Cleared cannot become GameOver"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Cleared));
	}

	{
		FGameFlowTestWorld GameOverFlow;
		if (!GameOverFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the game-over test world."));
			return false;
		}

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
		TestFalse(TEXT("Final delivery is ignored after GameOver"), GameOverFlow.GameRule->NotifyGoalReached(nullptr));
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

		TestTrue(TEXT("Minimum-count cargo initializes to 10"), MinimumCountFlow.GameRule->RequestCargoInitialization(10));
		TestTrue(TEXT("Minimum-count flow starts"), MinimumCountFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Minimum-count flow loses 6 cargo"), MinimumCountFlow.GameRule->NotifyCargoLost(6));
		TestFalse(TEXT("Goal does not clear below minimum cargo count"), MinimumCountFlow.GameRule->NotifyGoalReached(nullptr));
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

		TestTrue(TEXT("Survival-rate cargo initializes to 10"), SurvivalRateFlow.GameRule->RequestCargoInitialization(10));
		TestTrue(TEXT("Survival-rate flow starts"), SurvivalRateFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Survival-rate flow loses 3 cargo"), SurvivalRateFlow.GameRule->NotifyCargoLost(3));
		TestFalse(TEXT("Goal does not clear below minimum survival rate"), SurvivalRateFlow.GameRule->NotifyGoalReached(nullptr));
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

		TestTrue(TEXT("Empty-policy cargo initializes to 1"), EmptyCargoAllowedFlow.GameRule->RequestCargoInitialization(1));
		TestTrue(TEXT("Empty-policy flow starts"), EmptyCargoAllowedFlow.GameRule->RequestGameStart());
		TestTrue(TEXT("Empty-policy flow processes the final cargo loss"), EmptyCargoAllowedFlow.GameRule->NotifyCargoLost(1));
		TestEqual(TEXT("Disabled empty-cargo failure keeps Playing"), static_cast<uint8>(EmptyCargoAllowedFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Playing));
		TestFalse(TEXT("Zero cargo still cannot satisfy the default clear requirement"), EmptyCargoAllowedFlow.GameRule->NotifyGoalReached(nullptr));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
