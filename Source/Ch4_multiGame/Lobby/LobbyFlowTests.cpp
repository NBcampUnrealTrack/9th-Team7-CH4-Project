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
	const TSoftObjectPtr<UWorld> ForestMap(
		FSoftObjectPath(TEXT("/Game/Map/Level/ForestLevel.ForestLevel")));
	const TSoftObjectPtr<UWorld> SpaceMap(
		FSoftObjectPath(TEXT("/Game/Map/Level/SpaceLevel.SpaceLevel")));
	const TSoftObjectPtr<UWorld> MainMap(
		FSoftObjectPath(TEXT("/Game/Map/Level/MainLevel.MainLevel")));
	const TSoftObjectPtr<UWorld> MissingMap(
		FSoftObjectPath(TEXT("/Game/Map/Level/MapThatDoesNotExist.MapThatDoesNotExist")));
	const TSoftObjectPtr<UWorld> NonWorldAsset(
		FSoftObjectPath(TEXT("/Game/Input/Lobby/IA_LobbyReady.IA_LobbyReady")));
	FString SelectedMapPackage;
	const ACh4_multiGameLobbyGameMode* LobbyGameModeDefaults =
		GetDefault<ACh4_multiGameLobbyGameMode>();
	TestNotNull(TEXT("Lobby GameMode defaults are available"), LobbyGameModeDefaults);
	if (!LobbyGameModeDefaults)
	{
		return false;
	}
	TestEqual(
		TEXT("The default gameplay map pool contains one entry"),
		LobbyGameModeDefaults->GameplayMaps.Num(),
		1);
	TestTrue(
		TEXT("The default gameplay map entry is ForestLevel"),
		LobbyGameModeDefaults->GameplayMaps.Num() == 1
			&& LobbyGameModeDefaults->GameplayMaps[0] == ForestMap);

	TestFalse(
		TEXT("An empty gameplay map pool cannot select a travel target"),
		ACh4_multiGameLobbyGameMode::TrySelectRandomGameplayMap({}, SelectedMapPackage));

	TestTrue(
		TEXT("A single valid ForestLevel entry can be selected"),
		ACh4_multiGameLobbyGameMode::TrySelectRandomGameplayMap({ForestMap}, SelectedMapPackage));
	TestEqual(
		TEXT("A single-entry pool always selects ForestLevel"),
		SelectedMapPackage,
		FString(TEXT("/Game/Map/Level/ForestLevel")));

	const TArray<TSoftObjectPtr<UWorld>> ThreeValidMaps = {ForestMap, SpaceMap, MainMap};
	TestTrue(
		TEXT("A pool with three valid maps can select a travel target"),
		ACh4_multiGameLobbyGameMode::TrySelectRandomGameplayMap(
			ThreeValidMaps,
			SelectedMapPackage));
	TestTrue(
		TEXT("The random selection belongs to the configured valid pool"),
		SelectedMapPackage == TEXT("/Game/Map/Level/ForestLevel")
			|| SelectedMapPackage == TEXT("/Game/Map/Level/SpaceLevel")
			|| SelectedMapPackage == TEXT("/Game/Map/Level/MainLevel"));

	TestTrue(
		TEXT("Null and missing entries are ignored when ForestLevel is valid"),
		ACh4_multiGameLobbyGameMode::TrySelectRandomGameplayMap(
			{TSoftObjectPtr<UWorld>(), MissingMap, ForestMap},
			SelectedMapPackage));
	TestEqual(
		TEXT("Filtering invalid entries leaves ForestLevel as the only selection"),
		SelectedMapPackage,
		FString(TEXT("/Game/Map/Level/ForestLevel")));

	TestFalse(
		TEXT("Null, missing, and non-World assets produce no travel target"),
		ACh4_multiGameLobbyGameMode::TrySelectRandomGameplayMap(
			{TSoftObjectPtr<UWorld>(), MissingMap, NonWorldAsset},
			SelectedMapPackage));
	TestTrue(
		TEXT("Failed selection clears the output package"),
		SelectedMapPackage.IsEmpty());

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
