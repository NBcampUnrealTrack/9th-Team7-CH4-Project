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
	TestEqual(TEXT("Solo Not Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(0, 1).ToString(),
		FString(TEXT("Ready 0/1")));
	TestEqual(TEXT("Solo Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(1, 1).ToString(),
		FString(TEXT("Ready 1/1")));
	TestEqual(TEXT("One of two Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(1, 2).ToString(),
		FString(TEXT("Ready 1/2")));
	TestEqual(TEXT("Two of three Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(2, 3).ToString(),
		FString(TEXT("Ready 2/3")));
	TestEqual(TEXT("Three of four Ready text"),
		UCh4LobbyReadyWidget::FormatReadyStatus(3, 4).ToString(),
		FString(TEXT("Ready 3/4")));
	TestEqual(TEXT("Ready count is clamped to current players"),
		UCh4LobbyReadyWidget::FormatReadyStatus(8, 4).ToString(),
		FString(TEXT("Ready 4/4")));
	TestEqual(TEXT("Transient zero-player state is not displayed as zero over zero"),
		UCh4LobbyReadyWidget::FormatReadyStatus(0, 0).ToString(),
		FString(TEXT("Ready 0/1")));

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

	const FProperty* AnimProperty = FindFProperty<FProperty>(
		UCh4LobbyReadyWidget::StaticClass(), TEXT("ReadyPopAnim"));
	TestTrue(TEXT("ReadyPopAnim is an optional BindWidgetAnim"),
		AnimProperty && AnimProperty->HasMetaData(TEXT("BindWidgetAnimOptional")));

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
		bSuccess &= State->SetLobbyCounts(2, 1, 4);
		TestEqual(TEXT("Initial two-player summary"),
			UCh4LobbyReadyWidget::FormatReadyStatus(
				State->GetReadyPlayerCount(), State->GetCurrentPlayerCount()).ToString(),
			FString(TEXT("Ready 1/2")));

		bSuccess &= State->SetLobbyCounts(3, 1, 4);
		TestEqual(TEXT("Joining player increases the current-player denominator"),
			UCh4LobbyReadyWidget::FormatReadyStatus(
				State->GetReadyPlayerCount(), State->GetCurrentPlayerCount()).ToString(),
			FString(TEXT("Ready 1/3")));

		bSuccess &= State->SetLobbyCounts(3, 2, 4);
		bSuccess &= State->SetLobbyCounts(2, 1, 4);
		TestEqual(TEXT("Ready player leaving decreases both counts"),
			UCh4LobbyReadyWidget::FormatReadyStatus(
				State->GetReadyPlayerCount(), State->GetCurrentPlayerCount()).ToString(),
			FString(TEXT("Ready 1/2")));

		bSuccess &= State->SetLobbyCounts(3, 2, 4);
		bSuccess &= State->SetLobbyCounts(2, 2, 4);
		TestEqual(TEXT("Not Ready player leaving decreases only the denominator"),
			UCh4LobbyReadyWidget::FormatReadyStatus(
				State->GetReadyPlayerCount(), State->GetCurrentPlayerCount()).ToString(),
			FString(TEXT("Ready 2/2")));
		TestEqual(TEXT("Maximum lobby capacity remains unchanged"),
			State->GetMaxPlayerCount(), 4);
	}

	World->EndPlay(EEndPlayReason::Quit);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bSuccess;
}

#endif
