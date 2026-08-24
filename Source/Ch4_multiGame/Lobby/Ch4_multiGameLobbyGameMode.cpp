// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameLobbyGameMode.h"

#include "Ch4_multiGame.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "IPAddress.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACh4_multiGameLobbyGameMode::ACh4_multiGameLobbyGameMode()
{
	GameStateClass = ACh4_multiGameLobbyGameState::StaticClass();
	PlayerStateClass = ACh4_multiGameLobbyPlayerState::StaticClass();
	PlayerControllerClass = ACh4_multiGameLobbyPlayerController::StaticClass();
	bUseSeamlessTravel = false;

	static ConstructorHelpers::FClassFinder<APawn> ThirdPersonPawnClass(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (ThirdPersonPawnClass.Succeeded())
	{
		DefaultPawnClass = ThirdPersonPawnClass.Class;
	}
}

void ACh4_multiGameLobbyGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MaxLobbyPlayers = FMath::Max(MaxLobbyPlayers, 2);
	MinPlayersToStart = FMath::Clamp(MinPlayersToStart, 2, MaxLobbyPlayers);
	if (GameSession)
	{
		// AGameSession::ApproveLogin uses this value before Login/PostLogin.
		GameSession->MaxPlayers = MaxLobbyPlayers;
	}

	const bool bIsListenServer = GetNetMode() == NM_ListenServer;
	const int32 ListenPort = GetListenPort();
	const FString NetworkMode = bIsListenServer ? TEXT("LISTEN SERVER") : TEXT("STANDALONE");
	const FString StartupMessage = bIsListenServer
		? FString::Printf(
			TEXT("%s READY | Port: %d | Client: open <HostHamachiIP>:%d"),
			*NetworkMode,
			ListenPort,
			ListenPort)
		: TEXT("STANDALONE ONLY | Clients cannot join | Run: open L_Lobby?listen");

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] %s | Map: %s | Players: %d-%d"),
		*StartupMessage,
		*MapName,
		MinPlayersToStart,
		MaxLobbyPlayers);

	if (bIsListenServer)
	{
		const UNetDriver* NetDriver = GetWorld() ? GetWorld()->GetNetDriver() : nullptr;
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Listen Server started"));
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Listening Port: %d"), ListenPort);
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] GameNetDriver: %s | DriverClass: %s"),
			*GetNameSafe(NetDriver),
			NetDriver ? *NetDriver->GetClass()->GetPathName() : TEXT("Unavailable"));
	}
	ShowServerDebugStatus(StartupMessage, bIsListenServer ? FColor::Green : FColor::Red, 30.0f);
}

void ACh4_multiGameLobbyGameMode::InitGameState()
{
	Super::InitGameState();
	UpdateLobbyPlayerCount(GetNumPlayers());
}

void ACh4_multiGameLobbyGameMode::StartPlay()
{
	Super::StartPlay();

	int32 PlayerStartCount = 0;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		++PlayerStartCount;
	}

	const bool bHasEnoughPlayerStarts = PlayerStartCount >= MaxLobbyPlayers;
	if (bHasEnoughPlayerStarts)
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] PlayerStarts: %d / %d | Distinct four-player spawns: READY"),
			PlayerStartCount,
			MaxLobbyPlayers);
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] PlayerStarts: %d / %d | Distinct four-player spawns: CHECK MAP"),
			PlayerStartCount,
			MaxLobbyPlayers);
	}
}

void ACh4_multiGameLobbyGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	const int32 CurrentPlayers = GetNumPlayers();
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] PreLogin request | Current Players: %d / %d | Traveling: %s"),
		CurrentPlayers,
		MaxLobbyPlayers,
		bTravelStarted ? TEXT("YES") : TEXT("NO"));

	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (ErrorMessage.IsEmpty() && GetNumPlayers() >= MaxLobbyPlayers)
	{
		ErrorMessage = FString::Printf(TEXT("Lobby is full (%d/%d)."), GetNumPlayers(), MaxLobbyPlayers);
	}
	else if (ErrorMessage.IsEmpty() && bTravelStarted)
	{
		ErrorMessage = TEXT("Lobby is already traveling to the game map.");
	}

	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] Player Rejected | Reason: %s | Players: %d / %d"),
			*ErrorMessage,
			GetNumPlayers(),
			MaxLobbyPlayers);
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] Player Accepted | Players before Login: %d / %d"),
			GetNumPlayers(),
			MaxLobbyPlayers);
	}
}

void ACh4_multiGameLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority() || !IsValid(NewPlayer))
	{
		return;
	}

	UpdateLobbyPlayerCount(GetNumPlayers());
	const FString PlayerLabel = GetPlayerLogLabel(NewPlayer);
	const FString PawnLabel = GetNameSafe(NewPlayer->GetPawn());
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Player Joined: %s | Pawn: %s | Possessed: %s"),
		*PlayerLabel,
		*PawnLabel,
		NewPlayer->GetPawn() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Players: %d / %d"), GetNumPlayers(), MaxLobbyPlayers);
	ShowServerDebugStatus(
		FString::Printf(
			TEXT("PLAYER JOINED\nPlayers: %d / %d\nPawn: %s"),
			GetNumPlayers(),
			MaxLobbyPlayers,
			NewPlayer->GetPawn() ? TEXT("OK") : TEXT("MISSING")),
		NewPlayer->GetPawn() ? FColor::Green : FColor::Red,
		12.0f);
}

void ACh4_multiGameLobbyGameMode::Logout(AController* Exiting)
{
	const FString PlayerLabel = GetPlayerLogLabel(Exiting);
	const bool bWasPlayerController = IsValid(Cast<APlayerController>(Exiting));
	const int32 RemainingPlayerCount = bWasPlayerController
		? FMath::Max(GetNumPlayers() - 1, 0)
		: GetNumPlayers();

	Super::Logout(Exiting);

	if (!HasAuthority() || !bWasPlayerController)
	{
		return;
	}

	UpdateLobbyPlayerCount(RemainingPlayerCount);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Player Left: %s"), *PlayerLabel);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Players: %d / %d"), RemainingPlayerCount, MaxLobbyPlayers);
	ShowServerDebugStatus(
		FString::Printf(TEXT("PLAYER LEFT\nPlayers: %d / %d"), RemainingPlayerCount, MaxLobbyPlayers),
		FColor::Yellow,
		10.0f);

	// PlayerArray cleanup finishes after Logout. Recheck on the next event-loop turn
	// so a departing non-ready player cannot remain in the Ready count.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			this,
			&ACh4_multiGameLobbyGameMode::CheckAllPlayersReady);
	}
}

void ACh4_multiGameLobbyGameMode::HandlePlayerReady(APlayerController* RequestingPlayer)
{
	if (!HasAuthority() || bTravelStarted || !IsValid(RequestingPlayer))
	{
		return;
	}

	ACh4_multiGameLobbyPlayerState* LobbyPlayerState =
		RequestingPlayer->GetPlayerState<ACh4_multiGameLobbyPlayerState>();
	if (!IsValid(LobbyPlayerState))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] Ready request rejected: %s has no Lobby PlayerState"),
			*GetPlayerLogLabel(RequestingPlayer));
		return;
	}

	if (!LobbyPlayerState->SetReady())
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] Duplicate Ready request ignored: %s"),
			*GetPlayerLogLabel(RequestingPlayer));
		return;
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Player Ready: %s"),
		*GetPlayerLogLabel(RequestingPlayer));
	CheckAllPlayersReady();
}

ACh4_multiGameLobbyGameState* ACh4_multiGameLobbyGameMode::GetLobbyGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ACh4_multiGameLobbyGameState>() : nullptr;
}

void ACh4_multiGameLobbyGameMode::UpdateLobbyPlayerCount(const int32 NewPlayerCount)
{
	if (ACh4_multiGameLobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		LobbyGameState->SetPlayerCounts(NewPlayerCount, MaxLobbyPlayers);
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] ACh4_multiGameLobbyGameState is not active. Check the Lobby GameMode assignment."));
	}
}

void ACh4_multiGameLobbyGameMode::CheckAllPlayersReady()
{
	if (!HasAuthority() || bTravelStarted)
	{
		return;
	}

	int32 ReadyPlayers = 0;
	int32 TotalPlayers = 0;
	GetReadyPlayerCounts(ReadyPlayers, TotalPlayers);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Ready Players: %d / %d"),
		ReadyPlayers,
		TotalPlayers);
	const bool bHasMinimumPlayers = TotalPlayers >= MinPlayersToStart;
	ShowServerDebugStatus(
		FString::Printf(TEXT("READY PLAYERS: %d / %d"), ReadyPlayers, TotalPlayers),
		bHasMinimumPlayers && ReadyPlayers == TotalPlayers ? FColor::Green : FColor::Cyan,
		8.0f);

	if (!bHasMinimumPlayers)
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] Waiting for players before travel: %d / %d minimum"),
			TotalPlayers,
			MinPlayersToStart);
		return;
	}

	if (ReadyPlayers == TotalPlayers)
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] All Players Ready"));
		StartGameTravel();
	}
}

void ACh4_multiGameLobbyGameMode::GetReadyPlayerCounts(
	int32& OutReadyPlayers,
	int32& OutTotalPlayers) const
{
	OutReadyPlayers = 0;
	OutTotalPlayers = 0;

	const ACh4_multiGameLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		const ACh4_multiGameLobbyPlayerState* LobbyPlayerState =
			Cast<ACh4_multiGameLobbyPlayerState>(PlayerState);
		if (!IsValid(LobbyPlayerState) || LobbyPlayerState->IsInactive())
		{
			continue;
		}

		++OutTotalPlayers;
		if (LobbyPlayerState->IsReady())
		{
			++OutReadyPlayers;
		}
	}
}

void ACh4_multiGameLobbyGameMode::StartGameTravel()
{
	if (!HasAuthority() || bTravelStarted)
	{
		return;
	}

	const FString MapPackage = GameplayMapPackage.ToString();
	if (!FPackageName::IsValidLongPackageName(MapPackage) ||
		!FPackageName::DoesPackageExist(MapPackage))
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] Travel aborted: gameplay map package does not exist: %s"),
			*MapPackage);
		ShowServerDebugStatus(TEXT("TRAVEL FAILED\nGameplay map is missing"), FColor::Red, 15.0f);
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	bTravelStarted = true;
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Traveling to %s"), *MapPackage);
	ShowServerDebugStatus(
		FString::Printf(TEXT("ALL PLAYERS READY\nTraveling to %s"), *MapPackage),
		FColor::Green,
		10.0f);

	// Use an absolute URL so lobby-only options (especially ?game=LobbyGameMode)
	// cannot leak into the gameplay map. Keep ?listen so non-seamless clients can
	// reconnect to the same Listen Server after the map switch.
	const FString TravelURL = MapPackage + TEXT("?listen");
	if (!World->ServerTravel(TravelURL, true))
	{
		bTravelStarted = false;
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] ServerTravel failed for %s"),
			*MapPackage);
		ShowServerDebugStatus(TEXT("SERVER TRAVEL FAILED\nCheck Output Log"), FColor::Red, 15.0f);
	}
}

void ACh4_multiGameLobbyGameMode::ShowServerDebugStatus(
	const FString& EventMessage,
	const FColor& Color,
	const float Duration) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, FString::Printf(
			TEXT("[LOBBY SERVER]\n%s"),
			*EventMessage));
	}
}

int32 ACh4_multiGameLobbyGameMode::GetListenPort() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UNetDriver* NetDriver = World->GetNetDriver())
		{
			if (const TSharedPtr<const FInternetAddr> LocalAddress = NetDriver->GetLocalAddr())
			{
				return LocalAddress->GetPort();
			}
		}

		return World->URL.Port;
	}

	return 0;
}

FString ACh4_multiGameLobbyGameMode::GetPlayerLogLabel(const AController* Controller) const
{
	if (!IsValid(Controller))
	{
		return TEXT("UnknownPlayer");
	}

	if (const APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>())
	{
		const FString PlayerName = PlayerState->GetPlayerName();
		return PlayerName.IsEmpty()
			? GetNameSafe(Controller)
			: FString::Printf(TEXT("%s (%s)"), *PlayerName, *GetNameSafe(Controller));
	}

	return GetNameSafe(Controller);
}
