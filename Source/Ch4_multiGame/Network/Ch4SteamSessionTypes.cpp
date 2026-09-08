#include "Network/Ch4SteamSessionTypes.h"
#include "Online/OnlineSessionNames.h"

FOnlineSessionSettings Ch4SteamSessions::BuildSettings()
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = false;
	Settings.Set(GameIdKey, GameId, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(ProtocolKey, ProtocolVersion, EOnlineDataAdvertisementType::ViaOnlineService);
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
	return Search;
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
		&& Settings.Get(ProtocolKey, CandidateProtocol) && CandidateProtocol == ProtocolVersion;
}
