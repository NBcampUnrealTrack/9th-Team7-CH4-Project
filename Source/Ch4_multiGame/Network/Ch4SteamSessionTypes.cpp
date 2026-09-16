#include "Network/Ch4SteamSessionTypes.h"
#include "Online/OnlineSessionNames.h"

void Ch4SteamSessions::ApplyMatchState(FOnlineSessionSettings& Settings, const ECh4SteamMatchState MatchState)
{
	const bool bLobby = MatchState == ECh4SteamMatchState::Lobby;
	Settings.bShouldAdvertise = bLobby;
	Settings.bAllowJoinInProgress = bLobby;
	Settings.bAllowInvites = bLobby;
	Settings.bAllowJoinViaPresence = bLobby;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.Set(
		MatchStateKey,
		bLobby ? LobbyMatchState : PlayingMatchState,
		EOnlineDataAdvertisementType::ViaOnlineService);
}

FOnlineSessionSettings Ch4SteamSessions::BuildSettings(const ECh4SteamMatchState MatchState)
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.Set(GameIdKey, GameId, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ProtocolKey, ProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineService);
	ApplyMatchState(Settings, MatchState);
	Settings.Set(PlayerCountKey, 1, EOnlineDataAdvertisementType::ViaOnlineService);
	// Preserve UE's BuildUniqueId in addition to our explicit application protocol version.
	return Settings;
}

TSharedRef<FOnlineSessionSearch> Ch4SteamSessions::MakeSearch()
{
	TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = false;
	Search->MaxSearchResults = 100;
	Search->TimeoutInSeconds = 20.0f;
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(GameIdKey, GameId, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(ProtocolKey, ProtocolVersion, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(MatchStateKey, LobbyMatchState, EOnlineComparisonOp::Equals);
	return Search;
}

bool Ch4SteamSessions::IsLobbySession(const FOnlineSessionSettings& Settings)
{
	FString MatchState;
	return Settings.Get(MatchStateKey, MatchState) && MatchState == LobbyMatchState
		&& Settings.bShouldAdvertise && Settings.bAllowJoinInProgress;
}

bool Ch4SteamSessions::IsCompatible(const FOnlineSessionSettings& Settings)
{
	FString CandidateGame;
	int32 CandidateProtocol = 0;
	return !Settings.bIsLANMatch && !Settings.bIsDedicated
		&& Settings.bUsesPresence && Settings.bUseLobbiesIfAvailable
		&& Settings.NumPublicConnections == MaxPlayers
		&& Settings.BuildUniqueId == GetBuildUniqueId()
		&& Settings.Get(GameIdKey, CandidateGame) && CandidateGame == GameId
		&& Settings.Get(ProtocolKey, CandidateProtocol) && CandidateProtocol == ProtocolVersion
		&& IsLobbySession(Settings);
}
