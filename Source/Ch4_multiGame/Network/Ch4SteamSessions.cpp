#include "Player/Ch4_multiGameGameInstance.h"

#include "Ch4_multiGame.h"
#include "Ch4_multiGameGameMode.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UI/MainMenu/Ch4MainMenuTypes.h"

#define LOCTEXT_NAMESPACE "Ch4SteamSessions"

namespace
{
	const TCHAR* GetSteamMatchStateLabel(const ECh4SteamMatchState MatchState)
	{
		return MatchState == ECh4SteamMatchState::Lobby ? TEXT("Lobby") : TEXT("Playing");
	}
}

void UCh4_multiGameGameInstance::LogMatchTravel(const UWorld* World, const FString& Destination, bool bSeamless) const
{
	const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
	const FString NetMode = World ? ToString(World->GetNetMode()) : TEXT("None");
	const TCHAR* SessionState = SteamSessionInterface.IsValid()
		? EOnlineSessionState::ToString(SteamSessionInterface->GetSessionState(NAME_GameSession)) : TEXT("Unavailable");
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SteamTravel] Current=%s Destination=%s NetMode=%s NetDriverClass=%s NetDriver=%s Seamless=%s SessionState=%s"),
		World ? *World->GetPackage()->GetName() : TEXT("None"), *Destination,
		*NetMode,
		Driver ? *Driver->GetClass()->GetPathName() : TEXT("None"), *GetNameSafe(Driver),
		bSeamless ? TEXT("true") : TEXT("false"), SessionState);
}

void UCh4_multiGameGameInstance::InitializeSteamSessions()
{
	SteamPostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandleSteamPostLoadMap);
#if !UE_BUILD_SHIPPING
	// An explicit offline/debug launch selects one IP driver for the entire process.
	// Steam production never falls back to a raw IP connection.
	if (FParse::Param(FCommandLine::Get(), TEXT("Ch4DirectIP"))
		&& FParse::Param(FCommandLine::Get(), TEXT("nosteam")) && GEngine)
	{
		for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
		{
			if (Definition.DefName == NAME_GameNetDriver)
			{
				Definition.DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
				Definition.DriverClassNameFallback = Definition.DriverClassName;
			}
		}
		bDirectIPDebugEnabled = true;
		UE_LOG(LogCh4_multiGame, Log, TEXT("[NetworkDebug] Explicit Direct IP mode: IpNetDriver; Steam Host/Find/Join disabled"));
		return;
	}
#endif
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] OSS initialized: %s"), OSS ? *OSS->GetSubsystemName().ToString() : TEXT("Unavailable"));
	if (OSS && OSS->GetSubsystemName() == FName(TEXT("STEAM")))
	{
		SteamSessionInterface = OSS->GetSessionInterface();
		if (SteamSessionInterface.IsValid())
		{
			SteamInviteHandle = SteamSessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSteamInviteAccepted));
		}
		const IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
		UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Local user logged in: %s"),
			Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn ? TEXT("YES") : TEXT("NO"));
	}
}

void UCh4_multiGameGameInstance::ClearSteamOperationDelegates()
{
	if (SteamSessionInterface.IsValid())
	{
		SteamSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(SteamCreateHandle);
		SteamSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(SteamFindHandle);
		SteamSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(SteamJoinHandle);
		SteamSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(SteamDestroyHandle);
	}
	SteamCreateHandle.Reset();
	SteamFindHandle.Reset();
	SteamJoinHandle.Reset();
	SteamDestroyHandle.Reset();
}

void UCh4_multiGameGameInstance::ClearSteamUpdateDelegate()
{
	if (SteamSessionInterface.IsValid())
	{
		SteamSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(SteamUpdateHandle);
	}
	SteamUpdateHandle.Reset();
	bSteamUpdateInProgress = false;
	PendingSteamUpdateCompletion = {};
}

void UCh4_multiGameGameInstance::ShutdownSteamSessions()
{
	bSteamShuttingDown = true;
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(SteamPostLoadHandle);
	ClearSteamOperationDelegates();
	ClearSteamUpdateDelegate();
	if (SteamSessionInterface.IsValid())
	{
		SteamSessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SteamInviteHandle);
		if (SteamSearch.IsValid() && SteamSearch->SearchState == EOnlineAsyncTaskState::InProgress)
		{
			SteamSessionInterface->CancelFindSessions();
		}
		if (HasActiveSteamSession() && SteamSessionInterface->GetSessionState(NAME_GameSession) != EOnlineSessionState::Destroying)
		{
			// Best effort at process shutdown; normal menu exit waits for the completion delegate.
			SteamSessionInterface->DestroySession(NAME_GameSession);
		}
	}
	SteamSessionInterface.Reset();
	SteamSearch.Reset();
	SteamRooms.Reset();
}

bool UCh4_multiGameGameInstance::HasActiveSteamSession() const
{
	return SteamSessionInterface.IsValid() && SteamSessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

bool UCh4_multiGameGameInstance::SetSteamSessionGameplayAvailability(
	FCh4SteamSessionAvailabilityCompletion Completion)
{
	return UpdateSteamSessionMatchState(ECh4SteamMatchState::Playing, MoveTemp(Completion));
}

bool UCh4_multiGameGameInstance::RestoreSteamSessionLobbyAvailability()
{
	return UpdateSteamSessionMatchState(ECh4SteamMatchState::Lobby);
}

bool UCh4_multiGameGameInstance::UpdateSteamSessionMatchState(
	const ECh4SteamMatchState MatchState,
	FCh4SteamSessionAvailabilityCompletion Completion)
{
	// Direct-IP and local automation have no online room to update. Match travel remains unchanged.
	if (bDirectIPDebugEnabled || !HasActiveSteamSession())
	{
		if (Completion) Completion(true);
		return true;
	}
	if (bSteamShuttingDown || !GetWorld() || GetWorld()->GetNetMode() != NM_ListenServer)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[SteamSession] MatchState update rejected: only the listen-server host may update the room"));
		if (Completion) Completion(false);
		return false;
	}
	if (bSteamUpdateInProgress)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[SteamSession] MatchState update rejected: another UpdateSession request is in progress"));
		if (Completion) Completion(false);
		return false;
	}

	FNamedOnlineSession* Session = SteamSessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session)
	{
		if (Completion) Completion(false);
		return false;
	}

	FString CurrentState;
	Session->SessionSettings.Get(Ch4SteamSessions::MatchStateKey, CurrentState);
	const bool bLobbyTarget = MatchState == ECh4SteamMatchState::Lobby;
	const bool bPolicyAlreadyApplied = CurrentState == GetSteamMatchStateLabel(MatchState)
		&& Session->SessionSettings.bShouldAdvertise == bLobbyTarget
		&& Session->SessionSettings.bAllowJoinInProgress == bLobbyTarget
		&& Session->SessionSettings.bAllowInvites == bLobbyTarget
		&& Session->SessionSettings.bAllowJoinViaPresence == bLobbyTarget;
	if (bPolicyAlreadyApplied)
	{
		if (Completion) Completion(true);
		return true;
	}

	FOnlineSessionSettings UpdatedSettings = Session->SessionSettings;
	Ch4SteamSessions::ApplyMatchState(UpdatedSettings, MatchState);
	PendingSteamMatchState = MatchState;
	PendingSteamUpdateCompletion = MoveTemp(Completion);
	bSteamUpdateInProgress = true;
	SteamUpdateHandle = SteamSessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleSteamUpdateComplete));

	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] MatchState: %s -> %s"),
		CurrentState.IsEmpty() ? TEXT("Unknown") : *CurrentState,
		GetSteamMatchStateLabel(MatchState));
	const bool bAccepted = SteamSessionInterface->UpdateSession(NAME_GameSession, UpdatedSettings, true);
	if (!bAccepted)
	{
		FCh4SteamSessionAvailabilityCompletion FailedCompletion = MoveTemp(PendingSteamUpdateCompletion);
		ClearSteamUpdateDelegate();
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[SteamSession] UpdateSession rejected the %s MatchState request"),
			GetSteamMatchStateLabel(MatchState));
		if (FailedCompletion) FailedCompletion(false);
	}
	return bAccepted;
}

void UCh4_multiGameGameInstance::HandleSteamUpdateComplete(FName SessionName, const bool bSucceeded)
{
	if (bSteamShuttingDown || SessionName != NAME_GameSession || !bSteamUpdateInProgress)
	{
		return;
	}

	const ECh4SteamMatchState CompletedState = PendingSteamMatchState;
	FCh4SteamSessionAvailabilityCompletion Completion = MoveTemp(PendingSteamUpdateCompletion);
	ClearSteamUpdateDelegate();
	if (bSucceeded)
	{
		const bool bLobby = CompletedState == ECh4SteamMatchState::Lobby;
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[SteamSession] MatchState update complete: %s | Advertising=%s JoinInProgress=%s Invites=%s PresenceJoin=%s"),
			GetSteamMatchStateLabel(CompletedState),
			bLobby ? TEXT("enabled") : TEXT("disabled"),
			bLobby ? TEXT("enabled") : TEXT("disabled"),
			bLobby ? TEXT("enabled") : TEXT("disabled"),
			bLobby ? TEXT("enabled") : TEXT("disabled"));
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[SteamSession] MatchState update failed: %s"),
			GetSteamMatchStateLabel(CompletedState));
	}
	if (Completion) Completion(bSucceeded);
}

TArray<UCh4RoomEntryData*> UCh4_multiGameGameInstance::GetSteamRooms() const
{
	TArray<UCh4RoomEntryData*> Result;
	for (UCh4RoomEntryData* Room : SteamRooms) Result.Add(Room);
	return Result;
}

void UCh4_multiGameGameInstance::UpdateSteamSessionPlayerCount(int32 NewPlayerCount)
{
	if (bSteamShuttingDown || !SteamSessionInterface.IsValid() || !bSteamTravelIsHost)
	{
		return;
	}

	FNamedOnlineSession* Session = SteamSessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session)
	{
		return;
	}

	const int32 MaxPlayers = Session->SessionSettings.NumPublicConnections;
	const int32 ClampedPlayerCount = FMath::Clamp(NewPlayerCount, 1, MaxPlayers);
	const int32 NewOpenSlots = FMath::Clamp(MaxPlayers - ClampedPlayerCount, 0, MaxPlayers);

	int32 CurrentAdvertised = 0;
	const bool bHasAdvertised = Session->SessionSettings.Get(Ch4SteamSessions::PlayerCountKey, CurrentAdvertised);

	if (!bHasAdvertised || CurrentAdvertised != ClampedPlayerCount || Session->NumOpenPublicConnections != NewOpenSlots)
	{
		Session->SessionSettings.Set(Ch4SteamSessions::PlayerCountKey, ClampedPlayerCount, EOnlineDataAdvertisementType::ViaOnlineService);
		Session->NumOpenPublicConnections = NewOpenSlots;
		SteamSessionInterface->UpdateSession(NAME_GameSession, Session->SessionSettings, true);
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[SteamSession] Updated advertised player count: %d/%d (NumOpenPublicConnections=%d)"),
			ClampedPlayerCount, MaxPlayers, NewOpenSlots);
	}
}

void UCh4_multiGameGameInstance::SetSteamOperation(ECh4SteamSessionOperation Operation, const FText& Message)
{
	SteamOperation = Operation;
	SteamSessionStatus = Message;
	OnSteamSessionChanged.Broadcast();
}

void UCh4_multiGameGameInstance::CompleteSteamOperation(bool bSucceeded, const FText& Message)
{
	ClearSteamOperationDelegates();
	bSteamRehostAfterDestroy = false;
	bSteamLeaveRequested = false;
	SteamPendingFailure = FText::GetEmpty();
	SetSteamOperation(ECh4SteamSessionOperation::Idle, Message);
	OnSteamSessionComplete.Broadcast(bSucceeded, Message);
}

bool UCh4_multiGameGameInstance::RejectSteamRequest(const FString& Message)
{
	UE_LOG(LogCh4_multiGame, Warning, TEXT("[SteamSession] Request rejected: %s"), *Message);
	SteamSessionStatus = FText::FromString(Message);
	OnSteamSessionChanged.Broadcast();
	OnSteamSessionComplete.Broadcast(false, SteamSessionStatus);
	return false;
}

bool UCh4_multiGameGameInstance::EnsureSteamReady()
{
	if (bSteamShuttingDown || bDirectIPDebugEnabled || !GetWorld()
		|| GetWorld()->WorldType == EWorldType::PIE || !GetFirstGamePlayer())
	{
		return RejectSteamRequest(TEXT("Steam requires a local player in a standalone game process. Direct IP debug mode and PIE are not supported."));
	}
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS || OSS->GetSubsystemName() != FName(TEXT("STEAM")))
	{
		return RejectSteamRequest(TEXT("Steam OSS is unavailable. Start Steam and sign in, then restart the game. No IP fallback was used."));
	}
	const IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	const FUniqueNetIdPtr LocalId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	if (!Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn || !LocalId.IsValid() || !LocalId->IsValid())
	{
		return RejectSteamRequest(TEXT("The local Steam user is not logged in."));
	}
	if (!SteamSessionInterface.IsValid())
	{
		SteamSessionInterface = OSS->GetSessionInterface();
		if (SteamSessionInterface.IsValid())
		{
			SteamInviteHandle = SteamSessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSteamInviteAccepted));
		}
	}
	if (!SteamSessionInterface.IsValid()) return RejectSteamRequest(TEXT("Steam SessionInterface is unavailable."));
	const FNetDriverDefinition* Definition = GEngine ? GEngine->NetDriverDefinitions.FindByPredicate(
		[](const FNetDriverDefinition& Item) { return Item.DefName == NAME_GameNetDriver; }) : nullptr;
	UClass* DriverClass = LoadClass<UNetDriver>(nullptr, Ch4SteamSessions::NetDriverPath);
	if (!Definition || Definition->DriverClassName != FName(Ch4SteamSessions::NetDriverPath)
		|| !DriverClass || !DriverClass->GetDefaultObject<UNetDriver>()->IsAvailable())
	{
		return RejectSteamRequest(TEXT("SteamSocketsNetDriver is unavailable or incorrectly configured. Restart the game after enabling Steam plugins."));
	}
	return true;
}

bool UCh4_multiGameGameInstance::CheckSteamMenuRequest()
{
	if (IsSteamSessionBusy()) return RejectSteamRequest(TEXT("Another session operation is still running."));
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		return RejectSteamRequest(TEXT("Leave the current room before hosting, searching or joining another room."));
	}
	return EnsureSteamReady();
}

bool UCh4_multiGameGameInstance::HostSteamGame()
{
	if (!CheckSteamMenuRequest()) return false;
	// Reuse the existing Gameplay GameMode's Lobby soft reference; do not duplicate its asset path.
	const FString GameModePath = UGameMapsSettings::GetGlobalDefaultGameMode();
	const UClass* GameModeClass = GameModePath.IsEmpty() ? nullptr : LoadClass<ACh4_multiGameGameMode>(nullptr, *GameModePath);
	const ACh4_multiGameGameMode* Defaults = GameModeClass ? GameModeClass->GetDefaultObject<ACh4_multiGameGameMode>() : nullptr;
	if (!Defaults) return RejectSteamRequest(TEXT("The configured Gameplay GameMode must provide the existing LobbyMap reference."));
	SteamLobbyPackage = Defaults->LobbyMap.ToSoftObjectPath().GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(SteamLobbyPackage) || !FPackageName::DoesPackageExist(SteamLobbyPackage))
	{
		return RejectSteamRequest(TEXT("The Gameplay GameMode LobbyMap is missing or was not cooked."));
	}
	SteamRooms.Reset();
	SteamSearch.Reset();
	if (HasActiveSteamSession())
	{
		bSteamRehostAfterDestroy = true;
		return BeginSteamDestroy();
	}
	return BeginSteamCreate();
}

bool UCh4_multiGameGameInstance::BeginSteamCreate()
{
	SetSteamOperation(ECh4SteamSessionOperation::Creating, LOCTEXT("Creating", "Creating Steam room..."));
	SteamCreateHandle = SteamSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleSteamCreateComplete));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Creating session: four-player presence lobby"));
	// UE 5.8 Steam implements the int32 overload using the process's one local Steam user.
	const bool bAccepted = SteamSessionInterface->CreateSession(0, NAME_GameSession, Ch4SteamSessions::BuildSettings());
	if (!bAccepted && SteamOperation == ECh4SteamSessionOperation::Creating)
	{
		FailSteamOperation(LOCTEXT("CreateStartFailed", "Steam could not start room creation."));
	}
	return bAccepted;
}

void UCh4_multiGameGameInstance::HandleSteamCreateComplete(FName SessionName, bool bSucceeded)
{
	if (bSteamShuttingDown || SessionName != NAME_GameSession || SteamOperation != ECh4SteamSessionOperation::Creating) return;
	ClearSteamOperationDelegates();
	if (bSteamLeaveRequested) { BeginSteamDestroy(); return; }
	if (!bSucceeded) { FailSteamOperation(LOCTEXT("CreateFailed", "Steam room creation failed.")); return; }
	if (!GetWorld() || !GetFirstLocalPlayerController() || SteamLobbyPackage.IsEmpty())
	{
		FailSteamOperation(LOCTEXT("HostWorldMissing", "The local world or Lobby map is unavailable."));
		return;
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Create succeeded; opening configured Lobby as Listen Server"));
	bSteamTravelIsHost = true;
	SetSteamOperation(ECh4SteamSessionOperation::Travelling, LOCTEXT("OpeningLobby", "Opening Steam room..."));
	UGameplayStatics::OpenLevel(this, FName(SteamLobbyPackage), true, TEXT("listen"));
}

bool UCh4_multiGameGameInstance::FindSteamGames()
{
	if (!CheckSteamMenuRequest()) return false;
	if (HasActiveSteamSession()) return RejectSteamRequest(TEXT("Leave the current Steam session before searching."));
	SteamRooms.Reset();
	SteamSearch = Ch4SteamSessions::MakeSearch();
	SetSteamOperation(ECh4SteamSessionOperation::Finding, LOCTEXT("Searching", "Searching Steam rooms..."));
	SteamFindHandle = SteamSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleSteamFindComplete));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Searching sessions with CH4 game and protocol filters"));
	const bool bAccepted = SteamSessionInterface->FindSessions(0, SteamSearch.ToSharedRef());
	if (!bAccepted && SteamOperation == ECh4SteamSessionOperation::Finding)
	{
		CompleteSteamOperation(false, LOCTEXT("FindStartFailed", "Steam could not start the room search."));
	}
	return bAccepted;
}

void UCh4_multiGameGameInstance::HandleSteamFindComplete(bool bSucceeded)
{
	if (bSteamShuttingDown || SteamOperation != ECh4SteamSessionOperation::Finding) return;
	ClearSteamOperationDelegates();
	if (bSteamLeaveRequested) { BeginSteamDestroy(); return; }
	SteamRooms.Reset();
	if (!bSucceeded || !SteamSearch.IsValid())
	{
		CompleteSteamOperation(false, LOCTEXT("FindFailed", "Steam room search failed. Please try again."));
		return;
	}
	const int32 RawSessionCount = SteamSearch->SearchResults.Num();
	int32 PlayingSessionCount = 0;
	for (const FOnlineSessionSearchResult& Result : SteamSearch->SearchResults)
	{
		FString MatchState;
		if (Result.IsValid()
			&& Result.Session.SessionSettings.Get(Ch4SteamSessions::MatchStateKey, MatchState)
			&& MatchState == Ch4SteamSessions::PlayingMatchState)
		{
			++PlayingSessionCount;
		}
	}
	// Keep the existing UI's list index and SearchResultIndex identical after filtering.
	// This also prevents old index-based Blueprint bindings from selecting a different room.
	SteamSearch->SearchResults.RemoveAll([](const FOnlineSessionSearchResult& Result)
	{
		return !Result.IsValid() || !Ch4SteamSessions::IsCompatible(Result.Session.SessionSettings);
	});
	for (int32 Index = 0; Index < SteamSearch->SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = SteamSearch->SearchResults[Index];
		UCh4RoomEntryData* Entry = NewObject<UCh4RoomEntryData>(this);
		Entry->ServerName = Result.Session.OwningUserName.IsEmpty() ? TEXT("Steam Host") : Result.Session.OwningUserName;
		Entry->MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		int32 AdvertisedPlayers = 0;
		if (Result.Session.SessionSettings.Get(Ch4SteamSessions::PlayerCountKey, AdvertisedPlayers))
		{
			Entry->CurrentPlayers = FMath::Clamp(AdvertisedPlayers, 1, Entry->MaxPlayers);
		}
		else
		{
			Entry->CurrentPlayers = FMath::Clamp(Entry->MaxPlayers - Result.Session.NumOpenPublicConnections, 0, Entry->MaxPlayers);
		}
		Entry->PingInMs = Result.PingInMs;
		Entry->SearchResultIndex = Index;
		SteamRooms.Add(Entry);
	}
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SteamSession] Found %d raw sessions | Compatible Lobby sessions: %d | Filtered Playing sessions: %d"),
		RawSessionCount, SteamRooms.Num(), PlayingSessionCount);
	CompleteSteamOperation(true, FText::GetEmpty());
}

bool UCh4_multiGameGameInstance::JoinSteamGame(UCh4RoomEntryData* Room)
{
	if (!CheckSteamMenuRequest()) return false;
	if (!IsValid(Room) || !SteamRooms.Contains(Room) || !SteamSearch.IsValid()
		|| !SteamSearch->SearchResults.IsValidIndex(Room->SearchResultIndex))
	{
		return RejectSteamRequest(TEXT("Select a room from the latest completed search."));
	}
	return BeginSteamJoin(SteamSearch->SearchResults[Room->SearchResultIndex]);
}

bool UCh4_multiGameGameInstance::BeginSteamJoin(const FOnlineSessionSearchResult& Result)
{
	if (HasActiveSteamSession()) return RejectSteamRequest(TEXT("Leave the current Steam session before joining."));
	if (!Result.IsValid())
	{
		return RejectSteamRequest(TEXT("This Steam room is no longer available."));
	}
	if (!Ch4SteamSessions::IsLobbySession(Result.Session.SessionSettings))
	{
		return RejectSteamRequest(TEXT("Game already started. Select a Lobby room from a new search."));
	}
	if (!Ch4SteamSessions::IsCompatible(Result.Session.SessionSettings))
	{
		return RejectSteamRequest(TEXT("This Steam room belongs to a different game or incompatible build/protocol."));
	}
	int32 AdvertisedPlayers = 0;
	const bool bHasAdvertised = Result.Session.SessionSettings.Get(Ch4SteamSessions::PlayerCountKey, AdvertisedPlayers);
	const int32 EffectiveOpenSlots = bHasAdvertised
		? FMath::Max(Result.Session.SessionSettings.NumPublicConnections - AdvertisedPlayers, 0)
		: Result.Session.NumOpenPublicConnections;
	if (EffectiveOpenSlots <= 0) return RejectSteamRequest(TEXT("This Steam room is full."));
	SetSteamOperation(ECh4SteamSessionOperation::Joining, LOCTEXT("Joining", "Joining Steam room..."));
	SteamJoinHandle = SteamSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleSteamJoinComplete));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Joining session"));
	const bool bAccepted = SteamSessionInterface->JoinSession(0, NAME_GameSession, Result);
	if (!bAccepted && SteamOperation == ECh4SteamSessionOperation::Joining)
	{
		FailSteamOperation(LOCTEXT("JoinStartFailed", "Steam could not start joining the room."));
	}
	return bAccepted;
}

void UCh4_multiGameGameInstance::HandleSteamJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (bSteamShuttingDown || SessionName != NAME_GameSession || SteamOperation != ECh4SteamSessionOperation::Joining) return;
	ClearSteamOperationDelegates();
	if (bSteamLeaveRequested) { BeginSteamDestroy(); return; }
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		FailSteamOperation(FText::FromString(FString::Printf(TEXT("Steam join failed: %s."), LexToString(Result))));
		return;
	}
	FString ConnectString;
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC || !SteamSessionInterface.IsValid()
		|| !SteamSessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
	{
		FailSteamOperation(LOCTEXT("ResolveFailed", "Steam could not resolve the host connection or local controller."));
		return;
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Join succeeded; connect string resolved by OSS; starting ClientTravel"));
	bSteamTravelIsHost = false;
	SetSteamOperation(ECh4SteamSessionOperation::Travelling, LOCTEXT("Connecting", "Connecting to Steam host..."));
	PC->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UCh4_multiGameGameInstance::HandleSteamInviteAccepted(bool bSucceeded, int32 LocalUserNum,
	FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
	if (!bSucceeded || LocalUserNum != 0 || !UserId.IsValid() || !CheckSteamMenuRequest()) return;
	// Invite acceptance joins only from the standalone menu. Never silently abandon an active room.
	BeginSteamJoin(Result);
}

void UCh4_multiGameGameInstance::FailSteamOperation(const FText& Message)
{
	UE_LOG(LogCh4_multiGame, Warning, TEXT("[SteamSession] %s"), *Message.ToString());
	ClearSteamOperationDelegates();
	SteamPendingFailure = Message;
	bSteamRehostAfterDestroy = false;
	if (HasActiveSteamSession()) BeginSteamDestroy();
	else CompleteSteamOperation(false, Message);
}

bool UCh4_multiGameGameInstance::DestroySteamSession()
{
	if (bSteamShuttingDown) return false;
	if (bSteamLeaveRequested) return RejectSteamRequest(TEXT("Leaving the room is already in progress."));
	bSteamLeaveRequested = true;
	bSteamRehostAfterDestroy = false;
	// Create/Join cannot be cancelled safely. Their completion cleans up the resulting session.
	if (SteamOperation == ECh4SteamSessionOperation::Creating || SteamOperation == ECh4SteamSessionOperation::Joining
		|| SteamOperation == ECh4SteamSessionOperation::Finding || SteamOperation == ECh4SteamSessionOperation::Destroying)
	{
		SteamSessionStatus = LOCTEXT("LeavePending", "Waiting for the current Steam operation before leaving...");
		OnSteamSessionChanged.Broadcast();
		return true;
	}
	return BeginSteamDestroy();
}

bool UCh4_multiGameGameInstance::BeginSteamDestroy()
{
	ClearSteamUpdateDelegate();
	ClearSteamOperationDelegates();
	SetSteamOperation(ECh4SteamSessionOperation::Destroying, LOCTEXT("Destroying", "Closing Steam session..."));
	if (!HasActiveSteamSession())
	{
		HandleSteamDestroyComplete(NAME_GameSession, true);
		return true;
	}
	SteamDestroyHandle = SteamSessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleSteamDestroyComplete));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Destroying session"));
	const bool bAccepted = SteamSessionInterface->DestroySession(NAME_GameSession);
	if (!bAccepted && SteamOperation == ECh4SteamSessionOperation::Destroying)
	{
		HandleSteamDestroyComplete(NAME_GameSession, false);
	}
	return bAccepted;
}

void UCh4_multiGameGameInstance::HandleSteamDestroyComplete(FName SessionName, bool bSucceeded)
{
	if (bSteamShuttingDown || SessionName != NAME_GameSession || SteamOperation != ECh4SteamSessionOperation::Destroying) return;
	ClearSteamOperationDelegates();
	if (!bSucceeded)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[SteamSession] Destroy failed; no replacement session or menu travel was started"));
		CompleteSteamOperation(false, LOCTEXT("DestroyFailed", "Steam session cleanup failed. Retry leaving the room."));
		return;
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Session cleanup completed"));
	SteamSearch.Reset();
	SteamRooms.Reset();
	if (bSteamLeaveRequested) { TravelToSteamMainMenu(); return; }
	if (bSteamRehostAfterDestroy)
	{
		bSteamRehostAfterDestroy = false;
		if (EnsureSteamReady()) BeginSteamCreate();
		else CompleteSteamOperation(false, SteamSessionStatus);
		return;
	}
	const FText Failure = SteamPendingFailure;
	CompleteSteamOperation(Failure.IsEmpty(), Failure.IsEmpty() ? LOCTEXT("Destroyed", "Steam session closed.") : Failure);
}

void UCh4_multiGameGameInstance::TravelToSteamMainMenu()
{
	const FString MenuPackage = FSoftObjectPath(UGameMapsSettings::GetGameDefaultMap()).GetLongPackageName();
	if (!GetWorld() || !FPackageName::IsValidLongPackageName(MenuPackage) || !FPackageName::DoesPackageExist(MenuPackage))
	{
		CompleteSteamOperation(false, LOCTEXT("MenuMissing", "Session closed, but the configured main menu is unavailable."));
		return;
	}
	if (GetWorld()->GetNetMode() == NM_Standalone && GetWorld()->GetPackage()->GetName() == MenuPackage)
	{
		const FText Failure = SteamPendingFailure;
		CompleteSteamOperation(Failure.IsEmpty(), Failure.IsEmpty() ? LOCTEXT("Left", "Room closed.") : Failure);
		return;
	}
	SetSteamOperation(ECh4SteamSessionOperation::Travelling, LOCTEXT("Returning", "Returning to main menu..."));
	UGameplayStatics::OpenLevel(this, FName(MenuPackage), true);
}

void UCh4_multiGameGameInstance::HandleSteamPostLoadMap(UWorld* LoadedWorld)
{
	if (bSteamShuttingDown || !LoadedWorld || LoadedWorld->GetGameInstance() != this) return;
	if (SteamOperation == ECh4SteamSessionOperation::Travelling)
	{
		if (bSteamLeaveRequested)
		{
			const FText Failure = SteamPendingFailure;
			CompleteSteamOperation(Failure.IsEmpty(), Failure.IsEmpty() ? LOCTEXT("Left", "Room closed.") : Failure);
			return;
		}
		const UNetDriver* Driver = LoadedWorld->GetNetDriver();
		const ENetMode ExpectedMode = bSteamTravelIsHost ? NM_ListenServer : NM_Client;
		if (!Driver || Driver->GetClass()->GetPathName() != Ch4SteamSessions::NetDriverPath || LoadedWorld->GetNetMode() != ExpectedMode)
		{
			bSteamLeaveRequested = true;
			FailSteamOperation(LOCTEXT("DriverMismatch", "Steam connection did not establish the expected SteamSockets network driver."));
			return;
		}
		UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamSession] Connected: SteamSocketsNetDriver, %s"), bSteamTravelIsHost ? TEXT("Listen Server") : TEXT("Client"));
		CompleteSteamOperation(true, LOCTEXT("Connected", "Connected to Steam room."));
		return;
	}
	// Ordinary Lobby -> Gameplay -> Lobby travel does not touch the session.
	// Also clean up if an external Blueprint directly opens the configured main menu.
	if (SteamOperation == ECh4SteamSessionOperation::Idle && HasActiveSteamSession()
		&& LoadedWorld->GetNetMode() == NM_Standalone
		&& LoadedWorld->GetPackage()->GetName() == FSoftObjectPath(UGameMapsSettings::GetGameDefaultMap()).GetLongPackageName())
	{
		DestroySteamSession();
	}
}

void UCh4_multiGameGameInstance::HandleSteamConnectionFailure(UWorld* FailedWorld, const FString& Message)
{
	if (bSteamShuttingDown || bDirectIPDebugEnabled || (FailedWorld && FailedWorld->GetGameInstance() != this)
		|| (!HasActiveSteamSession() && !IsSteamSessionBusy())) return;
	SteamPendingFailure = FText::FromString(Message);
	UE_LOG(LogCh4_multiGame, Warning, TEXT("[SteamSession] Connection failed; cleaning up the session: %s"), *Message);
	if (!bSteamLeaveRequested) DestroySteamSession();
}

#undef LOCTEXT_NAMESPACE
