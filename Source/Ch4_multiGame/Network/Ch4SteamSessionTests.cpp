#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameEngine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Network/Ch4SteamSessionTypes.h"
#include "Online/OnlineSessionNames.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "UI/MainMenu/Ch4MainMenuTypes.h"

namespace Ch4SteamTests
{
	/** In-memory search metadata only; never sent to an online interface. */
	class FSessionInfo final : public FOnlineSessionInfo
	{
	public:
		FUniqueNetIdStringRef Id = FUniqueNetIdString::Create(FString(TEXT("fixture")), FName(TEXT("TEST")));
		virtual const uint8* GetBytes() const override { return Id->GetBytes(); }
		virtual int32 GetSize() const override { return Id->GetSize(); }
		virtual bool IsValid() const override { return true; }
		virtual FString ToString() const override { return TEXT("fixture"); }
		virtual FString ToDebugString() const override { return ToString(); }
		virtual const FUniqueNetId& GetSessionId() const override { return *Id; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4SteamSessionSettingsTest, "Ch4_multiGame.Steam.SessionSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4SteamSessionSettingsTest::RunTest(const FString& Parameters)
{
	const FOnlineSessionSettings Settings = Ch4SteamSessions::BuildSettings();
	TestEqual(TEXT("Four total public slots including host"), Settings.NumPublicConnections, 4);
	TestEqual(TEXT("No private slots"), Settings.NumPrivateConnections, 0);
	TestFalse(TEXT("Internet session"), Settings.bIsLANMatch);
	TestFalse(TEXT("Listen server"), Settings.bIsDedicated);
	TestTrue(TEXT("Advertised presence lobby with invites and join-in-progress"),
		Settings.bShouldAdvertise && Settings.bUsesPresence && Settings.bUseLobbiesIfAvailable
		&& Settings.bAllowInvites && Settings.bAllowJoinViaPresence && Settings.bAllowJoinInProgress);
	TestFalse(TEXT("No new lobby voice room"), Settings.bUseLobbiesVoiceChatIfAvailable);
	TestEqual(TEXT("Engine build compatibility remains enabled"), Settings.BuildUniqueId, GetBuildUniqueId());
	TestTrue(TEXT("Our settings satisfy the receive-side filter"), Ch4SteamSessions::IsCompatible(Settings));
	FString MatchState;
	TestTrue(TEXT("New rooms advertise the Lobby match state"),
		Settings.Get(Ch4SteamSessions::MatchStateKey, MatchState)
		&& MatchState == Ch4SteamSessions::LobbyMatchState);
	const TSharedRef<FOnlineSessionSearch> Search = Ch4SteamSessions::MakeSearch();
	TestFalse(TEXT("Internet search"), Search->bIsLanQuery);
	TestEqual(TEXT("Bounded result count"), Search->MaxSearchResults, 100);
	bool bLobbySearch = false;
	TestTrue(TEXT("UE 5.8 SEARCH_LOBBIES selects Steam lobby discovery"), Search->QuerySettings.Get(SEARCH_LOBBIES, bLobbySearch) && bLobbySearch);
	FString GameId;
	int32 Protocol = 0;
	TestTrue(TEXT("Game ID is filtered before the Steam result cap"), Search->QuerySettings.Get(Ch4SteamSessions::GameIdKey, GameId) && GameId == Ch4SteamSessions::GameId);
	TestTrue(TEXT("Protocol is filtered at the backend too"), Search->QuerySettings.Get(Ch4SteamSessions::ProtocolKey, Protocol) && Protocol == Ch4SteamSessions::ProtocolVersion);
	MatchState.Reset();
	TestTrue(TEXT("Backend search only requests Lobby rooms"),
		Search->QuerySettings.Get(Ch4SteamSessions::MatchStateKey, MatchState)
		&& MatchState == Ch4SteamSessions::LobbyMatchState);

	FOnlineSessionSettings PlayingSettings = Settings;
	Ch4SteamSessions::ApplyMatchState(PlayingSettings, ECh4SteamMatchState::Playing);
	TestFalse(TEXT("Gameplay rooms are not advertised or joinable"),
		PlayingSettings.bShouldAdvertise || PlayingSettings.bAllowJoinInProgress
		|| PlayingSettings.bAllowInvites || PlayingSettings.bAllowJoinViaPresence);
	TestFalse(TEXT("Gameplay rooms fail the receive-side Lobby filter"),
		Ch4SteamSessions::IsCompatible(PlayingSettings));
	Ch4SteamSessions::ApplyMatchState(PlayingSettings, ECh4SteamMatchState::Lobby);
	TestTrue(TEXT("Returning to Lobby restores advertisement and join policy"),
		Ch4SteamSessions::IsCompatible(PlayingSettings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4SteamSessionFilterTest, "Ch4_multiGame.Steam.CompatibilityFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4SteamSessionFilterTest::RunTest(const FString& Parameters)
{
	FOnlineSessionSettings Settings = Ch4SteamSessions::BuildSettings();
	Settings.Set(Ch4SteamSessions::GameIdKey, FString(TEXT("Another480Game")), EOnlineDataAdvertisementType::ViaOnlineService);
	TestFalse(TEXT("Unrelated AppID 480 game is hidden"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings();
	Settings.Settings.Remove(Ch4SteamSessions::GameIdKey);
	TestFalse(TEXT("Missing game tag is hidden"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings();
	Settings.Set(Ch4SteamSessions::ProtocolKey, 999, EOnlineDataAdvertisementType::ViaOnlineService);
	TestFalse(TEXT("Different application protocol is hidden"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings();
	Settings.BuildUniqueId ^= 1;
	TestFalse(TEXT("Different engine build is hidden"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings();
	Settings.bUseLobbiesIfAvailable = false;
	TestFalse(TEXT("Mismatched presence/lobby settings cannot be joined"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings();
	Settings.bIsLANMatch = true;
	TestFalse(TEXT("Legacy LAN room is not a Steam room"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings();
	Settings.Settings.Remove(Ch4SteamSessions::MatchStateKey);
	TestFalse(TEXT("Rooms without an explicit match state are hidden"), Ch4SteamSessions::IsCompatible(Settings));
	Settings = Ch4SteamSessions::BuildSettings(ECh4SteamMatchState::Playing);
	TestFalse(TEXT("Playing rooms are hidden even if their other compatibility tags match"),
		Ch4SteamSessions::IsCompatible(Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4SteamSessionGuardsTest, "Ch4_multiGame.Steam.OperationGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4SteamSessionGuardsTest::RunTest(const FString& Parameters)
{
	// No Init(), Steam identity, online backend call or map travel is performed in this fixture.
	UCh4_multiGameGameInstance* GI = NewObject<UCh4_multiGameGameInstance>();
	TestFalse(TEXT("Missing world/interface rejects Host safely"), GI->HostSteamGame());
	TestFalse(TEXT("Missing world/interface rejects Find safely"), GI->FindSteamGames());
	TestFalse(TEXT("Invalid room rejects Join safely"), GI->JoinSteamGame(nullptr));
	TestFalse(TEXT("Validation failures do not leave the UI busy"), GI->IsSteamSessionBusy());
	bool bDirectIPNoSessionCompletion = false;
	TestTrue(TEXT("Direct-IP/no-session Gameplay availability is a successful no-op"),
		GI->SetSteamSessionGameplayAvailability(
			[&bDirectIPNoSessionCompletion](const bool bSucceeded)
			{
				bDirectIPNoSessionCompletion = bSucceeded;
			}));
	TestTrue(TEXT("Direct-IP/no-session completion is immediate"), bDirectIPNoSessionCompletion);
	FOnlineSessionSearchResult RoomResult;
	RoomResult.Session.OwningUserId = FUniqueNetIdString::Create(FString(TEXT("fixture-owner")), FName(TEXT("TEST")));
	RoomResult.Session.SessionInfo = MakeShared<Ch4SteamTests::FSessionInfo>();
	RoomResult.Session.SessionSettings = Ch4SteamSessions::BuildSettings();
	RoomResult.Session.NumOpenPublicConnections = 3;
	GI->SteamSearch = Ch4SteamSessions::MakeSearch();
	FOnlineSessionSearchResult Unrelated = RoomResult;
	Unrelated.Session.SessionSettings.Set(Ch4SteamSessions::GameIdKey, FString(TEXT("AnotherGame")), EOnlineDataAdvertisementType::ViaOnlineService);
	FOnlineSessionSearchResult PlayingRoom = RoomResult;
	Ch4SteamSessions::ApplyMatchState(PlayingRoom.Session.SessionSettings, ECh4SteamMatchState::Playing);
	TestFalse(TEXT("A stale Playing result is rejected with no backend call"), GI->BeginSteamJoin(PlayingRoom));
	TestTrue(TEXT("Stale Playing join reports that the game already started"),
		GI->GetSteamSessionStatus().ToString().Contains(TEXT("Game already started")));
	GI->SteamSearch->SearchResults = {Unrelated, RoomResult, PlayingRoom, RoomResult};
	GI->SteamOperation = ECh4SteamSessionOperation::Finding;
	GI->HandleSteamFindComplete(true);
	TestEqual(TEXT("Only compatible rooms reach the existing UI"), GI->SteamRooms.Num(), 2);
	TestEqual(TEXT("Native search cache is compacted with the visible list"), GI->SteamSearch->SearchResults.Num(), 2);
	if (GI->SteamRooms.Num() == 2)
	{
		TestEqual(TEXT("First visible room keeps index zero after an earlier result was removed"), GI->SteamRooms[0]->SearchResultIndex, 0);
		TestEqual(TEXT("Second visible room keeps index one"), GI->SteamRooms[1]->SearchResultIndex, 1);
		TestEqual(TEXT("Displayed player count includes the host"), GI->SteamRooms[0]->CurrentPlayers, 1);
	}
	GI->SteamOperation = ECh4SteamSessionOperation::Finding;
	TestFalse(TEXT("Create during Find is rejected"), GI->HostSteamGame());
	TestFalse(TEXT("Duplicate Find is rejected"), GI->FindSteamGames());
	TestFalse(TEXT("Join during Find is rejected"), GI->JoinSteamGame(nullptr));
	TestEqual(TEXT("Rejected duplicates preserve the active operation"), GI->SteamOperation, ECh4SteamSessionOperation::Finding);
	TestTrue(TEXT("Leave waits for the outstanding operation"), GI->DestroySteamSession());
	TestTrue(TEXT("Leave does not release the search lifetime early"), GI->bSteamLeaveRequested);
	TestEqual(TEXT("Search still owns completion"), GI->SteamOperation, ECh4SteamSessionOperation::Finding);
	TestFalse(TEXT("Duplicate Leave is rejected"), GI->DestroySteamSession());
	GI->HandleSteamFindComplete(false);
	TestFalse(TEXT("Missing menu produces a safe completed failure"), GI->IsSteamSessionBusy());
	GI->SteamOperation = ECh4SteamSessionOperation::Joining;
	GI->HandleSteamJoinComplete(FName(TEXT("UnrelatedSession")), EOnJoinSessionCompleteResult::Success);
	TestEqual(TEXT("Callbacks for other session names are ignored"), GI->SteamOperation, ECh4SteamSessionOperation::Joining);
	GI->HandleSteamJoinComplete(NAME_GameSession, EOnJoinSessionCompleteResult::SessionIsFull);
	TestFalse(TEXT("Failed join completes without attempting travel"), GI->IsSteamSessionBusy());
	GI->SteamOperation = ECh4SteamSessionOperation::Joining;
	GI->HandleSteamJoinComplete(NAME_GameSession, EOnJoinSessionCompleteResult::Success);
	TestFalse(TEXT("Join success without interface/controller cannot travel"), GI->IsSteamSessionBusy());
	GI->SteamOperation = ECh4SteamSessionOperation::Destroying;
	GI->bSteamRehostAfterDestroy = true;
	GI->HandleSteamDestroyComplete(NAME_GameSession, false);
	TestFalse(TEXT("Failed Destroy never creates a replacement room"), GI->IsSteamSessionBusy());
	TestFalse(TEXT("Failed Destroy clears the rehost request"), GI->bSteamRehostAfterDestroy);
	GI->HandleSteamCreateComplete(NAME_GameSession, true);
	TestFalse(TEXT("Stale Create completion is ignored"), GI->IsSteamSessionBusy());
	GI->bSteamShuttingDown = true;
	TestFalse(TEXT("Shutdown rejects Leave"), GI->DestroySteamSession());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4SteamDriverConfigTest, "Ch4_multiGame.Steam.DriverConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4SteamDriverConfigTest::RunTest(const FString& Parameters)
{
	const UGameEngine* EngineDefaults = GetDefault<UGameEngine>();
	int32 GameDrivers = 0;
	bool bBeaconRetained = false;
	bool bDemoRetained = false;
	for (const FNetDriverDefinition& Definition : EngineDefaults->NetDriverDefinitions)
	{
		if (Definition.DefName == NAME_GameNetDriver)
		{
			++GameDrivers;
			TestEqual(TEXT("One SteamSockets transport"), Definition.DriverClassName, FName(Ch4SteamSessions::NetDriverPath));
			TestEqual(TEXT("No silent fallback to a different transport"), Definition.DriverClassNameFallback, Definition.DriverClassName);
		}
		bBeaconRetained |= Definition.DefName == FName(TEXT("BeaconNetDriver"));
		bDemoRetained |= Definition.DefName == FName(TEXT("DemoNetDriver"));
	}
	TestEqual(TEXT("Exactly one inherited GameNetDriver definition"), GameDrivers, 1);
	TestTrue(TEXT("Beacon and replay driver definitions are preserved"), bBeaconRetained && bDemoRetained);
	FConfigFile ProjectConfig;
	ProjectConfig.Read(FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini"));
	FString Service;
	int32 AppId = 0;
	TestTrue(TEXT("Steam is the configured production OSS"), ProjectConfig.GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), Service) && Service == TEXT("Steam"));
	TestTrue(TEXT("Development App ID is 480"), ProjectConfig.GetInt(TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), AppId) && AppId == 480);
	return true;
}

#endif
