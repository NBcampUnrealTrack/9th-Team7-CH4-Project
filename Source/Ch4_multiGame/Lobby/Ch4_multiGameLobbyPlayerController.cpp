// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameLobbyPlayerController.h"

#include "Ch4_multiGame.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Lobby/Ch4_multiGameLobbyGameMode.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "UObject/ConstructorHelpers.h"

ACh4_multiGameLobbyPlayerController::ACh4_multiGameLobbyPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(
		TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (DefaultContext.Succeeded())
	{
		DefaultMappingContexts.AddUnique(DefaultContext.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookContext(
		TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	if (MouseLookContext.Succeeded())
	{
		MobileExcludedMappingContexts.AddUnique(MouseLookContext.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> LobbyContext(
		TEXT("/Game/Input/Lobby/IMC_Lobby.IMC_Lobby"));
	if (LobbyContext.Succeeded())
	{
		DefaultMappingContexts.AddUnique(LobbyContext.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ReadyAction(
		TEXT("/Game/Input/Lobby/IA_LobbyReady.IA_LobbyReady"));
	if (ReadyAction.Succeeded())
	{
		LobbyReadyAction = ReadyAction.Object;
	}
}

void ACh4_multiGameLobbyPlayerController::LobbyReady()
{
	HandleReadyInput();
}

void ACh4_multiGameLobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent) || !IsValid(LobbyReadyAction))
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[Lobby] Ready input is unavailable. Check IA_LobbyReady and IMC_Lobby."));
		return;
	}

	EnhancedInputComponent->BindAction(
		LobbyReadyAction,
		ETriggerEvent::Started,
		this,
		&ACh4_multiGameLobbyPlayerController::HandleReadyInput);
}

void ACh4_multiGameLobbyPlayerController::HandleReadyInput()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (const ACh4_multiGameLobbyPlayerState* LobbyPlayerState =
		GetPlayerState<ACh4_multiGameLobbyPlayerState>();
		IsValid(LobbyPlayerState) && LobbyPlayerState->IsReady())
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Duplicate Ready input ignored locally"));
		return;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Ready requested by local player"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Cyan,
			TEXT("[LOBBY] Ready request sent"));
	}

	ServerSetReady();
}

void ACh4_multiGameLobbyPlayerController::ServerSetReady_Implementation()
{
	ACh4_multiGameLobbyGameMode* LobbyGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ACh4_multiGameLobbyGameMode>()
		: nullptr;
	if (!IsValid(LobbyGameMode))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[Lobby] Ready request ignored because the Lobby GameMode is not active"));
		return;
	}

	LobbyGameMode->HandlePlayerReady(this);
}
