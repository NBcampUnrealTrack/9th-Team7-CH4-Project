// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Lobby/Ch4_multiGameLobbyGameMode.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4LobbyReadyTravelRulesTest,
	"Ch4_multiGame.Lobby.ReadyTravelRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LobbyReadyTravelRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("One ready player cannot start a two-player lobby"),
		ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(1, 2, 2, false));

	TestTrue(
		TEXT("All ready players can start when the minimum is met"),
		ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(2, 2, 2, false));

	TestFalse(
		TEXT("Ready cancellation breaks the all-ready condition"),
		ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(1, 2, 2, false));

	TestFalse(
		TEXT("All present players cannot start below the minimum"),
		ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(1, 1, 2, false));

	TestFalse(
		TEXT("A travel already in progress blocks duplicate travel"),
		ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(2, 2, 2, true));

	TestFalse(
		TEXT("Invalid negative ready counts cannot start travel"),
		ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(-1, 2, 2, false));

	return true;
}

#endif
