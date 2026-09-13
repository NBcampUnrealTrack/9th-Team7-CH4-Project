// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"

namespace Ch4LobbyPackagedTests
{
	// Opt-in, real-process integration test. Launch a listen server and N-1 clients
	// with -Ch4LobbyTestPlayers=N -ExecCmds="Automation RunTests Ch4_multiGame.Lobby.PackagedTravel".
	// Clients deliberately leave the host Ready alone for ten seconds before readying.
	class FReadyAndTravel final : public IAutomationLatentCommand
	{
	public:
		FReadyAndTravel(FAutomationTestBase* InTest, int32 InExpectedPlayers)
			: Test(InTest), ExpectedPlayers(InExpectedPlayers), Started(FPlatformTime::Seconds()) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - Started > 180.0)
			{
				Test->AddError(TEXT("Timed out waiting for lobby players, Ready RPCs, or ForestLevel possession."));
				return true;
			}
			if (!GEngine) return false;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (!World || Context.WorldType != EWorldType::Game || !World->HasBegunPlay()) continue;
				APlayerController* LocalController = World->GetFirstPlayerController();
				if (!LocalController || !LocalController->IsLocalController() || !LocalController->GetPawn()) continue;
				if (World->GetPackage()->GetName() == TEXT("/Game/Map/Level/ForestLevel"))
				{
					Test->TestTrue(TEXT("ForestLevel was entered after this player's Ready RPC"), bReadySent);
					if (ExpectedPlayers > 1)
						Test->TestTrue(TEXT("Partial Ready did not travel during the observation interval"), bObservedPartialReady);
					Test->AddInfo(FString::Printf(TEXT("PACKAGED_TRAVEL_PASS Players=%d NetMode=%d Map=%s Pawn=%s"),
						ExpectedPlayers, int32(World->GetNetMode()), *World->GetPackage()->GetName(), *LocalController->GetPawn()->GetName()));
					return true;
				}
				ACh4_multiGameLobbyPlayerController* LobbyController = Cast<ACh4_multiGameLobbyPlayerController>(LocalController);
				ACh4_multiGameLobbyGameState* State = World->GetGameState<ACh4_multiGameLobbyGameState>();
				if (!LobbyController || !State || State->GetCurrentPlayerCount() != ExpectedPlayers) continue;
				int32 ReadyPlayers = 0;
				for (APlayerState* Player : State->PlayerArray)
				{
					const auto* LobbyPlayer = Cast<ACh4_multiGameLobbyPlayerState>(Player);
					if (LobbyPlayer && !LobbyPlayer->IsInactive() && LobbyPlayer->IsReady()) ++ReadyPlayers;
				}
				if (ReadyPlayers == 1 && ExpectedPlayers > 1)
				{
					if (PartialReadyStarted == 0.0) PartialReadyStarted = Now;
					if (Now - PartialReadyStarted >= 5.0 && !bObservedPartialReady)
					{
						bObservedPartialReady = true;
						Test->TestEqual(TEXT("Replicated Ready summary matches the authoritative player states"),
							State->GetReadyPlayerCount(), ReadyPlayers);
						Test->TestEqual(TEXT("Ready HUD denominator matches the current lobby population"),
							State->GetCurrentPlayerCount(), ExpectedPlayers);
						Test->AddInfo(TEXT("PARTIAL_READY_PASS: one player Ready, all players still in Lobby."));
					}
				}
				const bool bMayReady = World->GetNetMode() != NM_Client
					|| (PartialReadyStarted > 0.0 && Now - PartialReadyStarted >= 10.0);
				if (!bReadySent && bMayReady)
				{
					bReadySent = true;
					LobbyController->LobbyReady();
					Test->AddInfo(TEXT("Sent existing LobbyReady command through local controller."));
				}
			}
			return false;
		}
	private:
		FAutomationTestBase* Test;
		int32 ExpectedPlayers;
		double Started;
		double PartialReadyStarted = 0.0;
		bool bReadySent = false;
		bool bObservedPartialReady = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4LobbyPackagedTravelTest,
	"Ch4_multiGame.Lobby.PackagedTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4LobbyPackagedTravelTest::RunTest(const FString& Parameters)
{
	int32 ExpectedPlayers = 0;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Ch4LobbyTestPlayers="), ExpectedPlayers))
	{
		AddInfo(TEXT("Requires an explicit 1-4 process fixture with -Ch4LobbyTestPlayers=N; no travel requested."));
		return true;
	}
	if (ExpectedPlayers < 1 || ExpectedPlayers > 4)
	{
		AddError(TEXT("Ch4LobbyTestPlayers must be between one and four."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(Ch4LobbyPackagedTests::FReadyAndTravel(this, ExpectedPlayers));
	return true;
}

#endif
