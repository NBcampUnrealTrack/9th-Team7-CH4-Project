// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Ch4_multiGameGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"

namespace Ch4GameFlowTests
{
	struct FGameFlowTestWorld
	{
		UWorld* World = nullptr;
		ACh4_multiGameGameMode* GameMode = nullptr;
		ACh4_multiGameGameState* GameState = nullptr;

		bool Initialize()
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

			UClass* GameModeClass = LoadClass<ACh4_multiGameGameMode>(
				nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));

			if (GameModeClass)
			{
				GameMode = World->SpawnActor<ACh4_multiGameGameMode>(GameModeClass);
			}

			return GameMode && GameState;
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
		TestFalse(TEXT("Game cannot start before cargo initialization"), ClearFlow.GameMode->StartGame());
		TestTrue(TEXT("Cargo initializes to 20"), ClearFlow.GameMode->InitializeCargoCount(20));
		TestTrue(TEXT("Game starts after cargo initialization"), ClearFlow.GameMode->StartGame());
		TestTrue(TEXT("Cargo updates from 20 to 18"), ClearFlow.GameMode->UpdateRemainingCargo(18));
		TestTrue(TEXT("Cargo updates from 18 to 13"), ClearFlow.GameMode->UpdateRemainingCargo(13));
		TestTrue(TEXT("Final delivery succeeds with cargo remaining"), ClearFlow.GameMode->TryCompleteGame());
		TestEqual(TEXT("Clear flow ends as Cleared"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Cleared));
		TestEqual(TEXT("Clear flow keeps 13 cargo"), ClearFlow.GameState->GetRemainingCargoCount(), 13);
		TestEqual(TEXT("Clear flow lost cargo is calculated"), ClearFlow.GameState->GetLostCargoCount(), 7);
		TestTrue(TEXT("Clear flow survival rate is 65 percent"), FMath::IsNearlyEqual(ClearFlow.GameState->GetCargoSurvivalRate(), 0.65f));
		TestFalse(TEXT("Cargo changes are ignored after clear"), ClearFlow.GameMode->UpdateRemainingCargo(0));
		TestEqual(TEXT("Cleared cannot become GameOver"), static_cast<uint8>(ClearFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::Cleared));
	}

	{
		FGameFlowTestWorld GameOverFlow;
		if (!GameOverFlow.Initialize())
		{
			AddError(TEXT("Failed to initialize the game-over test world."));
			return false;
		}

		TestTrue(TEXT("Game-over cargo initializes to 3"), GameOverFlow.GameMode->InitializeCargoCount(3));
		TestTrue(TEXT("Game-over flow starts"), GameOverFlow.GameMode->StartGame());
		TestTrue(TEXT("Cargo updates from 3 to 2"), GameOverFlow.GameMode->UpdateRemainingCargo(2));
		TestTrue(TEXT("Cargo updates from 2 to 1"), GameOverFlow.GameMode->UpdateRemainingCargo(1));
		TestTrue(TEXT("Cargo updates from 1 to 0"), GameOverFlow.GameMode->UpdateRemainingCargo(0));
		TestEqual(TEXT("Zero cargo ends as GameOver"), static_cast<uint8>(GameOverFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::GameOver));
		TestFalse(TEXT("Final delivery is ignored after GameOver"), GameOverFlow.GameMode->TryCompleteGame());
		TestEqual(TEXT("GameOver cannot become Cleared"), static_cast<uint8>(GameOverFlow.GameState->GetCurrentGamePhase()), static_cast<uint8>(ECh4GamePhase::GameOver));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
