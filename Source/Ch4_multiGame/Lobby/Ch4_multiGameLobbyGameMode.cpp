// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameLobbyGameMode.h"

#include "Ch4_multiGame.h"
#include "AssetRegistry/AssetData.h"
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
#include "Misc/AssetRegistryInterface.h"
#include "Player/Ch4CharacterTypes.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACh4_multiGameLobbyGameMode::ACh4_multiGameLobbyGameMode()
{
	GameStateClass = ACh4_multiGameLobbyGameState::StaticClass();
	PlayerStateClass = ACh4_multiGameLobbyPlayerState::StaticClass();
	PlayerControllerClass = ACh4_multiGameLobbyPlayerController::StaticClass();
	bUseSeamlessTravel = true;
	GameplayMaps.Add(TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/Map/Level/ForestLevel.ForestLevel"))));

	LobbyCharacterClasses.SetNum(4);
	for (int32 CharacterIndex = 0; CharacterIndex < LobbyCharacterClasses.Num(); ++CharacterIndex)
	{
		LobbyCharacterClasses[CharacterIndex] = StaticLoadClass(
			APawn::StaticClass(),
			nullptr,
			Ch4Character::GetClassPath(Ch4Character::FromIndex(CharacterIndex)));
	}

	static ConstructorHelpers::FClassFinder<APawn> ThirdPersonPawnClass(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (ThirdPersonPawnClass.Succeeded())
	{
		DefaultPawnClass = ThirdPersonPawnClass.Class;
	}
}

void ACh4_multiGameLobbyGameMode::OnPostLogin(AController* NewPlayer)
{
	if (HasAuthority() && IsValid(Cast<APlayerController>(NewPlayer)))
	{
		AssignCharacterSlot(NewPlayer);
	}

	Super::OnPostLogin(NewPlayer);
}

UClass* ACh4_multiGameLobbyGameMode::GetDefaultPawnClassForController_Implementation(
	AController* InController)
{
	const TWeakObjectPtr<AController> ControllerKey(InController);
	if (!ControllersAwaitingInitialCharacterSpawn.Contains(ControllerKey))
	{
		return Super::GetDefaultPawnClassForController_Implementation(InController);
	}

	const auto* CharacterState = InController ? InController->GetPlayerState<ACh4_multiGameLobbyPlayerState>() : nullptr;
	const int32 CharacterSlot = CharacterState && Ch4Character::IsValidType(CharacterState->GetCharacterType())
		? Ch4Character::ToIndex(CharacterState->GetCharacterType()) : FindAssignedCharacterSlot(InController);
	if (LobbyCharacterClasses.IsValidIndex(CharacterSlot))
	{
		if (UClass* AssignedPawnClass = LobbyCharacterClasses[CharacterSlot].Get())
		{
			return AssignedPawnClass;
		}
	}

	if (IsValid(Cast<APlayerController>(InController)))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] No valid initial character slot for %s; using DefaultPawnClass"),
			*GetPlayerLogLabel(InController));
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ACh4_multiGameLobbyGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MaxLobbyPlayers = FMath::Max(MaxLobbyPlayers, 2);
	MinPlayersToStart = ClampMinimumPlayersToStart(MinPlayersToStart, MaxLobbyPlayers);
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
	UpdateLobbyCounts(GetNumPlayers());
}

void ACh4_multiGameLobbyGameMode::InitSeamlessTravelPlayer(AController* NewController)
{
	Super::InitSeamlessTravelPlayer(NewController);
	if (auto* State = NewController ? NewController->GetPlayerState<ACh4_multiGameLobbyPlayerState>() : nullptr)
	{
		// A returning player keeps their selection, including duplicates, but must Ready again.
		State->SetReadyState(false);
		AssignCharacterSlot(NewController, false);
		UpdateLobbyCounts(GetNumPlayers());
		UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamTravel] Lobby player initialized: Controller=%s Character=%s Ready=false"),
			*GetNameSafe(NewController), *UEnum::GetValueAsString(State->GetCharacterType()));
	}
}

void ACh4_multiGameLobbyGameMode::StartPlay()
{
	Super::StartPlay();

	if (HasAuthority())
	{
		if (UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
		{
			// Returning from Gameplay keeps the same Steam session. Publish it again only
			// after this Lobby world has initialized and can accept new players.
			GI->RestoreSteamSessionLobbyAvailability();
		}
	}

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
	if (IsValid(NewPlayer->GetPawn()))
	{
		ControllersAwaitingInitialCharacterSpawn.Remove(
			TWeakObjectPtr<AController>(NewPlayer));
	}

	UpdateLobbyCounts(GetNumPlayers());
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
	const ACh4_multiGameLobbyPlayerState* ExitingLobbyPlayerState =
		Exiting ? Exiting->GetPlayerState<ACh4_multiGameLobbyPlayerState>() : nullptr;
	const int32 ReleasedCharacterSlot = HasAuthority() && bWasPlayerController
		? ReleaseCharacterSlot(Exiting)
		: INDEX_NONE;
	const int32 RemainingPlayerCount = bWasPlayerController
		? FMath::Max(GetNumPlayers() - 1, 0)
		: GetNumPlayers();

	Super::Logout(Exiting);

	if (!HasAuthority() || !bWasPlayerController)
	{
		return;
	}

	UpdateLobbyCounts(RemainingPlayerCount, ExitingLobbyPlayerState);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Player Left: %s"), *PlayerLabel);
	if (ReleasedCharacterSlot != INDEX_NONE)
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] Initial Character Released | Player: %s | Slot: %d | Character: %s"),
			*PlayerLabel,
			ReleasedCharacterSlot + 1,
			*GetNameSafe(LobbyCharacterClasses.IsValidIndex(ReleasedCharacterSlot)
				? LobbyCharacterClasses[ReleasedCharacterSlot].Get()
				: nullptr));
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Players: %d / %d"), RemainingPlayerCount, MaxLobbyPlayers);
	ShowServerDebugStatus(
		FString::Printf(TEXT("PLAYER LEFT\nPlayers: %d / %d"), RemainingPlayerCount, MaxLobbyPlayers),
		FColor::Yellow,
		10.0f);

	// PlayerArray cleanup finishes after Logout. Recompute both replicated counts on
	// the next event-loop turn before evaluating the existing all-Ready travel rule.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			this,
			&ACh4_multiGameLobbyGameMode::HandleDeferredLobbyRosterChange);
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

	const bool bNewReadyState = !LobbyPlayerState->IsReady();
	if (!LobbyPlayerState->SetReadyState(bNewReadyState))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] Ready state change rejected: %s"),
			*GetPlayerLogLabel(RequestingPlayer));
		return;
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Player %s: %s"),
		bNewReadyState ? TEXT("Ready") : TEXT("Not Ready"),
		*GetPlayerLogLabel(RequestingPlayer));
	UpdateLobbyCounts(GetNumPlayers());
	CheckAllPlayersReady();
}

ACh4_multiGameLobbyGameState* ACh4_multiGameLobbyGameMode::GetLobbyGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ACh4_multiGameLobbyGameState>() : nullptr;
}

void ACh4_multiGameLobbyGameMode::UpdateLobbyCounts(
	const int32 NewPlayerCount,
	const ACh4_multiGameLobbyPlayerState* ExcludedPlayerState)
{
	if (ACh4_multiGameLobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		int32 ReadyPlayers = 0;
		int32 TotalPlayers = 0;
		GetReadyPlayerCounts(ReadyPlayers, TotalPlayers, ExcludedPlayerState);
		LobbyGameState->SetLobbyCounts(
			NewPlayerCount,
			FMath::Min(ReadyPlayers, TotalPlayers),
			MaxLobbyPlayers);
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] ACh4_multiGameLobbyGameState is not active. Check the Lobby GameMode assignment."));
	}

	if (HasAuthority())
	{
		if (UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
		{
			GI->UpdateSteamSessionPlayerCount(NewPlayerCount);
		}
	}
}

void ACh4_multiGameLobbyGameMode::HandleDeferredLobbyRosterChange()
{
	if (!HasAuthority())
	{
		return;
	}

	UpdateLobbyCounts(GetNumPlayers());
	CheckAllPlayersReady();
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

	if (CanStartLobbyTravel(
		ReadyPlayers,
		TotalPlayers,
		MinPlayersToStart,
		bTravelStarted))
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] All Players Ready"));
		StartGameTravel();
	}
}

int32 ACh4_multiGameLobbyGameMode::ClampMinimumPlayersToStart(
	const int32 MinimumPlayers, const int32 LobbyCapacity)
{
	return FMath::Clamp(MinimumPlayers, 1, FMath::Max(LobbyCapacity, 1));
}

bool ACh4_multiGameLobbyGameMode::CanStartLobbyTravel(
	const int32 ReadyPlayers,
	const int32 TotalPlayers,
	const int32 MinimumPlayers,
	const bool bIsTravelInProgress)
{
	return !bIsTravelInProgress
		&& MinimumPlayers > 0
		&& TotalPlayers >= MinimumPlayers
		&& ReadyPlayers >= 0
		&& ReadyPlayers == TotalPlayers;
}

bool ACh4_multiGameLobbyGameMode::TrySelectRandomGameplayMap(
	const TArray<TSoftObjectPtr<UWorld>>& GameplayMapCandidates,
	FString& OutMapPackage)
{
	OutMapPackage.Reset();
	TArray<FString> ValidMapPackages;
	const IAssetRegistryInterface* AssetRegistry = IAssetRegistryInterface::GetPtr();

	for (const TSoftObjectPtr<UWorld>& GameplayMapCandidate : GameplayMapCandidates)
	{
		const FSoftObjectPath MapObjectPath = GameplayMapCandidate.ToSoftObjectPath();
		if (MapObjectPath.IsNull())
		{
			continue;
		}

		const FString MapPackage = MapObjectPath.GetLongPackageName();
		FAssetData MapAsset;
		// IoStore packages deliberately have no local filename extension. Check the
		// mounted package and its registered asset type instead of requiring .umap.
		if (!FPackageName::IsValidLongPackageName(MapPackage)
			|| !FPackageName::DoesPackageExist(MapPackage)
			|| !AssetRegistry
			|| AssetRegistry->TryGetAssetByObjectPath(MapObjectPath, MapAsset)
				!= UE::AssetRegistry::EExists::Exists
			|| MapAsset.AssetClassPath != UWorld::StaticClass()->GetClassPathName())
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[Lobby] Rejected gameplay map candidate: Object=%s Package=%s (missing package or non-World asset)"),
				*MapObjectPath.ToString(), *MapPackage);
			continue;
		}

		ValidMapPackages.AddUnique(MapPackage);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Gameplay map candidates: Configured=%d Valid=%d"),
		GameplayMapCandidates.Num(), ValidMapPackages.Num());

	if (ValidMapPackages.IsEmpty())
	{
		return false;
	}

	OutMapPackage = ValidMapPackages[FMath::RandRange(0, ValidMapPackages.Num() - 1)];
	return true;
}

int32 ACh4_multiGameLobbyGameMode::FindFirstAvailableCharacterSlot(
	const TArray<bool>& UnavailableSlots)
{
	for (int32 SlotIndex = 0; SlotIndex < UnavailableSlots.Num(); ++SlotIndex)
	{
		if (!UnavailableSlots[SlotIndex])
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

int32 ACh4_multiGameLobbyGameMode::AssignCharacterSlot(AController* Controller, bool bInitializeSelection)
{
	if (!HasAuthority() || !IsValid(Controller))
	{
		return INDEX_NONE;
	}

	if (const int32 ExistingSlot = FindAssignedCharacterSlot(Controller);
		ExistingSlot != INDEX_NONE)
	{
		return ExistingSlot;
	}

	for (auto SlotIt = CharacterSlotsByController.CreateIterator(); SlotIt; ++SlotIt)
	{
		if (!SlotIt.Key().IsValid())
		{
			SlotIt.RemoveCurrent();
		}
	}

	TArray<bool> UnavailableSlots;
	UnavailableSlots.Init(false, LobbyCharacterClasses.Num());
	for (int32 SlotIndex = 0; SlotIndex < LobbyCharacterClasses.Num(); ++SlotIndex)
	{
		UnavailableSlots[SlotIndex] = LobbyCharacterClasses[SlotIndex].Get() == nullptr;
	}
	for (const TPair<TWeakObjectPtr<AController>, int32>& Assignment : CharacterSlotsByController)
	{
		if (UnavailableSlots.IsValidIndex(Assignment.Value))
		{
			UnavailableSlots[Assignment.Value] = true;
		}
	}

	const int32 CharacterSlot = FindFirstAvailableCharacterSlot(UnavailableSlots);
	if (CharacterSlot == INDEX_NONE)
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] Initial Character Assignment Failed | Player: %s | Configured Slots: %d"),
			*GetPlayerLogLabel(Controller),
			LobbyCharacterClasses.Num());
		return INDEX_NONE;
	}

	const ECh4CharacterType CharacterType = Ch4Character::FromIndex(CharacterSlot);
	ACh4_multiGameLobbyPlayerState* LobbyPlayerState =
		Controller->GetPlayerState<ACh4_multiGameLobbyPlayerState>();
	if (!LobbyPlayerState
		|| (bInitializeSelection && !LobbyPlayerState->SetCharacterTypeFromServer(CharacterType)))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] Initial Character Assignment Failed | PlayerState unavailable for %s"),
			*GetPlayerLogLabel(Controller));
		return INDEX_NONE;
	}

	CharacterSlotsByController.Add(TWeakObjectPtr<AController>(Controller), CharacterSlot);
	ControllersAwaitingInitialCharacterSpawn.Add(TWeakObjectPtr<AController>(Controller));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Initial Character Assigned | Player: %s | Slot: %d | Character: %s"),
		*GetPlayerLogLabel(Controller),
		CharacterSlot + 1,
		*GetNameSafe(LobbyCharacterClasses[CharacterSlot].Get()));
	return CharacterSlot;
}

int32 ACh4_multiGameLobbyGameMode::ReleaseCharacterSlot(AController* Controller)
{
	if (!IsValid(Controller))
	{
		return INDEX_NONE;
	}

	int32 ReleasedSlot = INDEX_NONE;
	ControllersAwaitingInitialCharacterSpawn.Remove(TWeakObjectPtr<AController>(Controller));
	CharacterSlotsByController.RemoveAndCopyValue(
		TWeakObjectPtr<AController>(Controller),
		ReleasedSlot);
	return ReleasedSlot;
}

int32 ACh4_multiGameLobbyGameMode::FindAssignedCharacterSlot(AController* Controller) const
{
	if (!IsValid(Controller))
	{
		return INDEX_NONE;
	}

	if (const int32* CharacterSlot = CharacterSlotsByController.Find(
		TWeakObjectPtr<AController>(Controller)))
	{
		return *CharacterSlot;
	}

	return INDEX_NONE;
}

void ACh4_multiGameLobbyGameMode::GetReadyPlayerCounts(
	int32& OutReadyPlayers,
	int32& OutTotalPlayers,
	const ACh4_multiGameLobbyPlayerState* ExcludedPlayerState) const
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
		if (!IsValid(LobbyPlayerState)
			|| LobbyPlayerState == ExcludedPlayerState
			|| LobbyPlayerState->IsInactive())
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

	FString MapPackage;
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Travel selection started: Current=%s"),
		GetWorld() ? *GetWorld()->GetPackage()->GetName() : TEXT("None"));
	if (!TrySelectRandomGameplayMap(GameplayMaps, MapPackage))
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] Travel aborted: GameplayMaps contains no valid map packages"));
		ShowServerDebugStatus(TEXT("TRAVEL FAILED\nNo valid gameplay maps"), FColor::Red, 15.0f);
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	bTravelStarted = true;
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Selected Gameplay Map: %s"), *MapPackage);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Traveling to %s"), *MapPackage);
	ShowServerDebugStatus(
		FString::Printf(TEXT("ALL PLAYERS READY\nTraveling to %s"), *MapPackage),
		FColor::Green,
		10.0f);

	// Match travel keeps the existing NetDriver through seamless travel. Only the
	// initial Steam room creation uses ?listen; absolute travel drops lobby options.
	const FString TravelURL = MapPackage;
	PendingTravelDestination = TravelURL;
	if (UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
	{
		TWeakObjectPtr<ACh4_multiGameLobbyGameMode> WeakThis(this);
		GI->SetSteamSessionGameplayAvailability(
			[WeakThis](const bool bSucceeded)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleGameplaySessionAvailabilityUpdated(bSucceeded);
				}
			});
		return;
	}

	// The project normally uses UCh4_multiGameGameInstance. Preserve local/direct-IP
	// travel if a test world supplies a different GameInstance class.
	HandleGameplaySessionAvailabilityUpdated(true);
}

void ACh4_multiGameLobbyGameMode::HandleGameplaySessionAvailabilityUpdated(const bool bSucceeded)
{
	if (!HasAuthority() || !bTravelStarted)
	{
		return;
	}

	if (!bSucceeded)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] Travel aborted: Steam room could not be hidden before Gameplay travel"));
		ResetTravelAfterFailure();
		ShowServerDebugStatus(TEXT("TRAVEL FAILED\nSteam room availability update failed"), FColor::Red, 15.0f);
		if (UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
		{
			// UpdateSession may already have copied the requested flags locally even
			// when the backend reports failure. Restore the joinable Lobby policy.
			GI->RestoreSteamSessionLobbyAvailability();
		}
		return;
	}

	PerformGameTravel();
}

void ACh4_multiGameLobbyGameMode::PerformGameTravel()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !bTravelStarted || !IsValid(World) || PendingTravelDestination.IsEmpty())
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] Travel aborted after availability update: World or destination became invalid"));
		ResetTravelAfterFailure();
		if (UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
		{
			GI->RestoreSteamSessionLobbyAvailability();
		}
		return;
	}

	const FString TravelURL = PendingTravelDestination;
	if (const UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
	{
		GI->LogMatchTravel(World, TravelURL, bUseSeamlessTravel);
	}
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] ServerTravel call: Current=%s Destination=%s Absolute=true"),
		*World->GetPackage()->GetName(), *TravelURL);
	const bool bTravelAccepted = World->ServerTravel(TravelURL, true);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] ServerTravel returned: Accepted=%s Destination=%s"),
		bTravelAccepted ? TEXT("true") : TEXT("false"), *TravelURL);
	if (!bTravelAccepted)
	{
		ResetTravelAfterFailure();
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] ServerTravel failed for %s"),
			*TravelURL);
		ShowServerDebugStatus(TEXT("SERVER TRAVEL FAILED\nCheck Output Log"), FColor::Red, 15.0f);
		if (UCh4_multiGameGameInstance* GI = GetGameInstance<UCh4_multiGameGameInstance>())
		{
			GI->RestoreSteamSessionLobbyAvailability();
		}
	}
}

void ACh4_multiGameLobbyGameMode::ResetTravelAfterFailure()
{
	if (!HasAuthority()) return;
	bTravelStarted = false;
	PendingTravelDestination.Reset();
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
