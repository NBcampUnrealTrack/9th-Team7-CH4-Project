// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Ch4_multiGameGameMode.h"
#include "Lobby/Ch4_multiGameLobbyGameMode.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4SeamlessStateTest,
	"Ch4_multiGame.Steam.SeamlessPlayerState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4SeamlessStateTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Lobby native default uses seamless travel"), GetDefault<ACh4_multiGameLobbyGameMode>()->bUseSeamlessTravel);
	TestTrue(TEXT("Gameplay native default uses seamless return"), GetDefault<ACh4_multiGameGameMode>()->bUseSeamlessTravel);
	for (const TCHAR* Path : {TEXT("/Game/Lobby/BP_LobbyGameMode.BP_LobbyGameMode_C"),
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C")})
	{
		UClass* ModeClass = LoadClass<AGameModeBase>(nullptr, Path);
		TestTrue(FString::Printf(TEXT("Actual BP default is seamless: %s"), Path),
			ModeClass && ModeClass->GetDefaultObject<AGameModeBase>()->bUseSeamlessTravel);
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("State fixture world"), World)) return false;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Lobby = World->SpawnActor<ACh4_multiGameLobbyPlayerState>();
		auto* Game = World->SpawnActor<ACh4_multiGamePlayerState>();
		auto* Returned = World->SpawnActor<ACh4_multiGameLobbyPlayerState>();
		const ECh4CharacterType Selection = Index < 2 ? ECh4CharacterType::Dog : Ch4Character::FromIndex(Index);
		Lobby->SetCharacterTypeFromServer(Selection);
		const FName Headwear = Index == 0 ? NAME_None : FName(TEXT("TestHeadwear"));
		Lobby->SetEquippedHeadwearFromServer(Headwear);
		Lobby->SetReadyState(true);
		Lobby->Reset(); // Same sequence as APlayerController::SeamlessTravelFrom.
		Lobby->SeamlessTravelTo(Game);
		TestEqual(TEXT("Lobby to Gameplay copies authoritative selection"), Game->GetCharacterType(), Selection);
		TestEqual(TEXT("Lobby to Gameplay copies dev headwear, including unequipped"), Game->GetEquippedHeadwearID(), Headwear);
		Game->Reset();
		Game->SeamlessTravelTo(Returned);
		TestEqual(TEXT("Return keeps selection, including duplicates"), Returned->GetCharacterType(), Selection);
		TestEqual(TEXT("Return keeps dev headwear"), Returned->GetEquippedHeadwearID(), Headwear);
		TestFalse(TEXT("Returned Lobby does not inherit Ready"), Returned->IsReady());
	}
	World->DestroyWorld(false);
	return true;
}

#endif
