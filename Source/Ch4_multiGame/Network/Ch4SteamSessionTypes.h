#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Ch4SteamSessionTypes.generated.h"

UENUM(BlueprintType)
enum class ECh4SteamSessionOperation : uint8
{
	Idle,
	Creating,
	Finding,
	Joining,
	Destroying,
	Travelling
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCh4SteamSessionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCh4SteamSessionComplete, bool, bSucceeded, FText, Message);

enum class ECh4SteamMatchState : uint8
{
	Lobby,
	Playing
};

/** Shared settings/filter policy. No Steam SDK calls are made here. */
namespace Ch4SteamSessions
{
	inline const FName GameIdKey(TEXT("CH4_GAME_ID"));
	inline const FName ProtocolKey(TEXT("CH4_PROTOCOL"));
	inline const FName MatchStateKey(TEXT("CH4_MATCH_STATE"));
	inline const FName PlayerCountKey(TEXT("CH4_PLAYER_COUNT"));
	inline const FString GameId(TEXT("Ch4MultiGame"));
	inline const FString LobbyMatchState(TEXT("Lobby"));
	inline const FString PlayingMatchState(TEXT("Playing"));
	inline constexpr int32 ProtocolVersion = 1;
	inline constexpr int32 MaxPlayers = 4;
	inline constexpr const TCHAR* NetDriverPath = TEXT("/Script/SteamSockets.SteamSocketsNetDriver");

	FOnlineSessionSettings BuildSettings(ECh4SteamMatchState MatchState = ECh4SteamMatchState::Lobby);
	void ApplyMatchState(FOnlineSessionSettings& Settings, ECh4SteamMatchState MatchState);
	TSharedRef<FOnlineSessionSearch> MakeSearch();
	bool IsLobbySession(const FOnlineSessionSettings& Settings);
	bool IsCompatible(const FOnlineSessionSettings& Settings);
}
