// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerController.h"
#include "UI/Lobby/Ch4LobbyReadyWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4LobbyReadyStatusContract,
	"Ch4_multiGame.Lobby.ReadyStatusWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LobbyReadyStatusContract::RunTest(const FString&)
{
	TestEqual(TEXT("Empty lobby Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(0, 4).ToString(),
		FString(TEXT("Ready 0/4")));
	TestEqual(TEXT("Partial lobby Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(2, 4).ToString(),
		FString(TEXT("Ready 2/4")));
	TestEqual(TEXT("Ready count is clamped to capacity"),
		UCh4LobbyReadyWidget::FormatReadyStatus(8, 4).ToString(),
		FString(TEXT("Ready 4/4")));

	const UCh4LobbyReadyWidget* WidgetDefaults = GetDefault<UCh4LobbyReadyWidget>();
	TestTrue(TEXT("Local Ready uses green by default"),
		WidgetDefaults
			&& WidgetDefaults->GetStatusColor(true).GetSpecifiedColor().Equals(FLinearColor::Green));
	TestTrue(TEXT("Local Not Ready uses red by default"),
		WidgetDefaults
			&& WidgetDefaults->GetStatusColor(false).GetSpecifiedColor().Equals(FLinearColor::Red));

	const FProperty* TextProperty = FindFProperty<FProperty>(
		UCh4LobbyReadyWidget::StaticClass(), TEXT("Text_ReadyStatus"));
	TestTrue(TEXT("Text_ReadyStatus is a required BindWidget"),
		TextProperty && TextProperty->HasMetaData(TEXT("BindWidget")));

	const FClassProperty* WidgetClassProperty = FindFProperty<FClassProperty>(
		ACh4_multiGameLobbyPlayerController::StaticClass(), TEXT("LobbyReadyWidgetClass"));
	TestTrue(TEXT("Lobby Controller exposes only Ch4 Lobby Ready Widget subclasses"),
		WidgetClassProperty
			&& WidgetClassProperty->HasAnyPropertyFlags(CPF_Edit)
			&& WidgetClassProperty->MetaClass == UCh4LobbyReadyWidget::StaticClass());

	if (!GEngine)
	{
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Ready summary fixture world"), World))
	{
		return false;
	}
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACh4_multiGameLobbyGameState* State = World->SpawnActor<ACh4_multiGameLobbyGameState>();
	bool bSuccess = TestNotNull(TEXT("Ready summary GameState"), State);
	if (State)
	{
		bSuccess &= State->SetLobbyCounts(2, 0, 4);
		TestEqual(TEXT("Server publishes zero Ready players"), State->GetReadyPlayerCount(), 0);
		TestEqual(TEXT("Server publishes the configured maximum"), State->GetMaxPlayerCount(), 4);

		bSuccess &= State->SetLobbyCounts(2, 2, 4);
		TestEqual(TEXT("Ready toggles update the authoritative summary"),
			State->GetReadyPlayerCount(), 2);

		bSuccess &= State->SetLobbyCounts(1, 1, 4);
		TestEqual(TEXT("Disconnecting one Ready player decreases the summary"),
			State->GetReadyPlayerCount(), 1);
		TestEqual(TEXT("Disconnect does not change the HUD denominator"),
			State->GetMaxPlayerCount(), 4);
	}

	World->EndPlay(EEndPlayReason::Quit);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bSuccess;
}

#endif
