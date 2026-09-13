// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Ch4_multiGameGameMode.h"
#include "Ch4_multiGamePlayerController.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFramework/Pawn.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"

// Explicit game-process fixture only. Uses existing Ready RPCs and the production
// Clear/return route with test-only Cargo initialization; no map assets are edited.
class FCh4SeamlessRoundTripCommand final : public IAutomationLatentCommand
{
public:
	FCh4SeamlessRoundTripCommand(FAutomationTestBase* InTest, int32 InPlayers)
		: Test(InTest), Players(InPlayers), Started(FPlatformTime::Seconds()) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 210.0)
		{
			Test->AddError(FString::Printf(TEXT("Seamless round trip timed out at stage %d"), Stage));
			return true;
		}
		if (!GEngine) return false;
		UWorld* World = nullptr;
		ACh4_multiGamePlayerController* PC = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || Context.WorldType != EWorldType::Game || !Candidate->HasBegunPlay()
				|| Candidate->IsInSeamlessTravel()) continue;
			auto* Local = Cast<ACh4_multiGamePlayerController>(Candidate->GetFirstPlayerController());
			if (Local && Local->IsLocalController() && Local->GetPawn()) { World = Candidate; PC = Local; break; }
		}
		if (!World || !PC) return false;
		const bool bLobby = World->GetPackage()->GetName() == TEXT("/Game/Lobby/L_Lobby");
		const bool bForest = World->GetPackage()->GetName() == TEXT("/Game/Map/Level/ForestLevel");
		auto* State = PC->GetPlayerState<ACh4_multiGamePlayerState>();
		if (!State) return false;

		if (Stage == 0 && bLobby)
		{
			auto* LobbyState = World->GetGameState<ACh4_multiGameLobbyGameState>();
			if (!LobbyState || LobbyState->GetCurrentPlayerCount() != Players) return false;
			if (!bRequestedSelection)
			{
				if (!Ch4Character::IsValidType(State->GetCharacterType())) return false;
				InitialDriver = World->GetNetDriver();
				if (!Test->TestNotNull(TEXT("Initial network driver exists"), InitialDriver.Get())) return true;
				if (World->GetNetMode() == NM_Client) InitialConnections.Add(InitialDriver->ServerConnection);
				else for (UNetConnection* Connection : InitialDriver->ClientConnections) InitialConnections.Add(Connection);
				// Deliberate duplicates prove that returning does not reassign join-order skins.
				PC->RequestCharacterType(ECh4CharacterType::Dog);
				bRequestedSelection = true;
				return false;
			}
			if (!AllPlayersReadyForCheck(World, true)) return false;
			ReadyWithPartialCheck(World, PC, Now);
		}
		else if (Stage <= 1 && bForest && !PC->IsA<ACh4_multiGameLobbyPlayerController>())
		{
			if (Stage == 0)
			{
				CheckTransfer(World, State);
				Test->TestTrue(TEXT("Initial travel followed this player's Ready"), bReadySent);
				Test->TestTrue(TEXT("Partial Ready stayed in Lobby"), Players == 1 || bObservedPartialReady);
				Stage = 1;
				StageTime = Now;
			}
			if (Now - StageTime < 3.0 || !AllPlayersReadyForCheck(World, false)) return false;
			if (PC->HasAuthority() && !bStartedTestMatch)
			{
				auto* Rule = World->GetAuthGameMode<ACh4_multiGameGameMode>();
				auto* Flow = World->GetGameState<ACh4_multiGameGameState>();
				if (!Rule || !Flow) return false;
				Test->TestEqual(TEXT("New gameplay begins in Waiting"), Flow->GetCurrentGamePhase(), ECh4GamePhase::Waiting);
				Rule->SetResultDisplayDurationForTesting(2.0f);
				if (!Test->TestTrue(TEXT("Test fixture initializes one Cargo"), Rule->RequestCargoInitialization(1))) return true;
				if (!Test->TestTrue(TEXT("Test fixture enters Playing through existing rules"), Rule->RequestGameStart())) return true;
				bStartedTestMatch = true;
			}
			auto* Flow = World->GetGameState<ACh4_multiGameGameState>();
			if (Flow && Flow->GetCurrentGamePhase() == ECh4GamePhase::Playing)
			{
				Stage = 2;
				StageTime = Now;
				Test->AddInfo(TEXT("SEAMLESS_FOREST_PASS: original driver/connections, selection, possession and Playing verified."));
			}
		}
		else if (Stage == 2 && bForest)
		{
			if (PC->HasAuthority() && !bCleared && Now - StageTime > 5.0)
			{
				auto* Rule = World->GetAuthGameMode<ACh4_multiGameGameMode>();
				if (!Rule) return false;
				bCleared = true;
				if (!Test->TestTrue(TEXT("Production Clear schedules Lobby return"), Rule->NotifyGoalReached(World->SpawnActor<AActor>()))) return true;
			}
		}
		else if ((Stage == 2 || Stage == 3) && bLobby && PC->IsA<ACh4_multiGameLobbyPlayerController>())
		{
			if (!AllPlayersReadyForCheck(World, true)) return false;
			if (Stage == 2)
			{
				CheckTransfer(World, State);
				for (APlayerState* Player : World->GetGameState()->PlayerArray)
				{
					const auto* LobbyPlayer = Cast<ACh4_multiGameLobbyPlayerState>(Player);
					if (LobbyPlayer && !LobbyPlayer->IsInactive()) Test->TestFalse(TEXT("Every returning player has Ready=false"), LobbyPlayer->IsReady());
				}
				Stage = 3;
				StageTime = Now;
				bReadySent = false;
				bObservedPartialReady = false;
				PartialReadyTime = 0.0;
				Test->AddInfo(TEXT("SEAMLESS_RETURN_PASS: original driver/connections, duplicate selections and Ready reset verified."));
			}
			// Observe the reset lobby before beginning the next match.
			if (Now - StageTime > 8.0) ReadyWithPartialCheck(World, PC, Now);
		}
		else if (Stage == 3 && bForest && !PC->IsA<ACh4_multiGameLobbyPlayerController>())
		{
			CheckTransfer(World, State);
			Test->TestTrue(TEXT("Next match requires a new Ready RPC"), bReadySent);
			Test->TestTrue(TEXT("Second partial Ready also stayed in Lobby"), Players == 1 || bObservedPartialReady);
			Test->AddInfo(FString::Printf(TEXT("SEAMLESS_ROUND_TRIP_PASS Players=%d Driver=%s"), Players, *GetNameSafe(InitialDriver.Get())));
			return true;
		}
		return false;
	}

private:
	bool AllPlayersReadyForCheck(UWorld* World, bool bLobby) const
	{
		AGameStateBase* GameState = World->GetGameState();
		if (!GameState) return false;
		int32 Count = 0;
		for (APlayerState* Player : GameState->PlayerArray)
		{
			const auto* Selection = Cast<ACh4_multiGamePlayerState>(Player);
			if (!Selection || Player->IsInactive()) continue;
			if (bLobby != Selection->IsA<ACh4_multiGameLobbyPlayerState>()
				|| Selection->GetCharacterType() != ECh4CharacterType::Dog) return false;
			++Count;
		}
		return Count == Players;
	}
	void ReadyWithPartialCheck(UWorld* World, ACh4_multiGamePlayerController* PC, double Now)
	{
		int32 Ready = 0;
		for (APlayerState* Player : World->GetGameState()->PlayerArray)
		{
			const auto* LobbyPlayer = Cast<ACh4_multiGameLobbyPlayerState>(Player);
			if (LobbyPlayer && !LobbyPlayer->IsInactive() && LobbyPlayer->IsReady()) ++Ready;
		}
		if (Ready == 1 && Players > 1)
		{
			if (PartialReadyTime == 0.0) PartialReadyTime = Now;
			if (Now - PartialReadyTime >= 4.0) bObservedPartialReady = true;
		}
		if (!bReadySent && (PC->HasAuthority() || (PartialReadyTime > 0.0 && Now - PartialReadyTime >= 10.0)))
		{
			CastChecked<ACh4_multiGameLobbyPlayerController>(PC)->LobbyReady();
			bReadySent = true;
		}
	}
	void CheckTransfer(UWorld* World, ACh4_multiGamePlayerState* State)
	{
		Test->TestTrue(TEXT("The same NetDriver survives internal map travel"), InitialDriver.IsValid() && World->GetNetDriver() == InitialDriver.Get());
		Test->TestEqual(TEXT("Selected character survives PlayerState handoff"), State->GetCharacterType(), ECh4CharacterType::Dog);
		for (const TWeakObjectPtr<UNetConnection>& Connection : InitialConnections)
		{
			Test->TestTrue(TEXT("Original peer connection survives without reconnect"), Connection.IsValid()
				&& (World->GetNetMode() == NM_Client ? World->GetNetDriver()->ServerConnection == Connection.Get()
					: World->GetNetDriver()->ClientConnections.Contains(Connection.Get())));
		}
	}
	FAutomationTestBase* Test;
	int32 Players;
	int32 Stage = 0;
	double Started;
	double StageTime = 0.0;
	double PartialReadyTime = 0.0;
	bool bRequestedSelection = false;
	bool bReadySent = false;
	bool bObservedPartialReady = false;
	bool bStartedTestMatch = false;
	bool bCleared = false;
	TWeakObjectPtr<UNetDriver> InitialDriver;
	TArray<TWeakObjectPtr<UNetConnection>> InitialConnections;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4SeamlessRoundTripTest,
	"Ch4_multiGame.Steam.SeamlessRoundTrip",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4SeamlessRoundTripTest::RunTest(const FString& Parameters)
{
	int32 Players = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Ch4SeamlessPlayers="), Players))
	{
		AddInfo(TEXT("Requires explicit -Ch4SeamlessPlayers=N fixture; no gameplay is changed."));
		return true;
	}
	if (Players < 1 || Players > 4) { AddError(TEXT("Ch4SeamlessPlayers must be 1-4")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FCh4SeamlessRoundTripCommand(this, Players));
	return true;
}

#endif
