// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameLobbyPlayerController.h"

#include "Ch4_multiGame.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Lobby/Ch4_multiGameLobbyGameMode.h"
#include "Lobby/Ch4_multiGameLobbyGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "UI/Lobby/Ch4LobbyReadyWidget.h"
#include "UObject/ConstructorHelpers.h"

ACh4_multiGameLobbyPlayerController::ACh4_multiGameLobbyPlayerController()
{
	bUseTemplateInputMappings = false;
	// The possessed animal supplies the team's existing IMC_Player. The lobby
	// controller adds only the Ready layer so template mappings cannot compete
	// with IA_PlayerMove / IA_PlayerLook at the same priority.
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

void ACh4_multiGameLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InitializeLobbyReadyUI();
}

void ACh4_multiGameLobbyPlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	RemoveLobbyReadyUI();
	Super::EndPlay(EndPlayReason);
}

void ACh4_multiGameLobbyPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InitializeLobbyReadyUI();
}

void ACh4_multiGameLobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeLobbyReadyUI();
}

void ACh4_multiGameLobbyPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	InitializeLobbyReadyUI();
}

void ACh4_multiGameLobbyPlayerController::InitializeLobbyReadyUI()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	BindLobbyReadyState();
	if (!LobbyReadyWidgetClass)
	{
		if (!bMissingLobbyReadyWidgetClassLogged)
		{
			bMissingLobbyReadyWidgetClassLogged = true;
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[LobbyReadyUI] LobbyReadyWidgetClass is not configured on %s"),
				*GetClass()->GetName());
		}
		return;
	}

	if (!IsValid(LobbyReadyWidget) || LobbyReadyWidget->GetWorld() != GetWorld())
	{
		if (LobbyReadyWidget)
		{
			LobbyReadyWidget->RemoveFromParent();
		}
		LobbyReadyWidget = CreateWidget<UCh4LobbyReadyWidget>(this, LobbyReadyWidgetClass);
	}
	if (!LobbyReadyWidget)
	{
		return;
	}
	if (!LobbyReadyWidget->IsInViewport())
	{
		LobbyReadyWidget->AddToPlayerScreen(10);
	}

	RefreshLobbyReadyUI();
}

void ACh4_multiGameLobbyPlayerController::BindLobbyReadyState()
{
	ACh4_multiGameLobbyGameState* LobbyGameState = GetWorld()
		? GetWorld()->GetGameState<ACh4_multiGameLobbyGameState>()
		: nullptr;
	if (BoundLobbyGameState.Get() != LobbyGameState)
	{
		if (ACh4_multiGameLobbyGameState* PreviousGameState = BoundLobbyGameState.Get())
		{
			PreviousGameState->OnReadySummaryChanged.RemoveDynamic(
				this, &ACh4_multiGameLobbyPlayerController::HandleReadySummaryChanged);
		}
		BoundLobbyGameState = LobbyGameState;
	}
	if (LobbyGameState)
	{
		LobbyGameState->OnReadySummaryChanged.RemoveDynamic(
			this, &ACh4_multiGameLobbyPlayerController::HandleReadySummaryChanged);
		LobbyGameState->OnReadySummaryChanged.AddDynamic(
			this, &ACh4_multiGameLobbyPlayerController::HandleReadySummaryChanged);
	}

	ACh4_multiGameLobbyPlayerState* LobbyPlayerState =
		GetPlayerState<ACh4_multiGameLobbyPlayerState>();
	if (BoundLobbyPlayerState.Get() != LobbyPlayerState)
	{
		if (ACh4_multiGameLobbyPlayerState* PreviousPlayerState = BoundLobbyPlayerState.Get())
		{
			PreviousPlayerState->OnReadyStateChanged.RemoveDynamic(
				this, &ACh4_multiGameLobbyPlayerController::HandleLocalReadyStateChanged);
		}
		BoundLobbyPlayerState = LobbyPlayerState;
	}
	if (LobbyPlayerState)
	{
		LobbyPlayerState->OnReadyStateChanged.RemoveDynamic(
			this, &ACh4_multiGameLobbyPlayerController::HandleLocalReadyStateChanged);
		LobbyPlayerState->OnReadyStateChanged.AddDynamic(
			this, &ACh4_multiGameLobbyPlayerController::HandleLocalReadyStateChanged);
	}
}

void ACh4_multiGameLobbyPlayerController::RefreshLobbyReadyUI()
{
	if (!LobbyReadyWidget)
	{
		return;
	}

	const ACh4_multiGameLobbyGameState* LobbyGameState = BoundLobbyGameState.Get();
	const ACh4_multiGameLobbyPlayerState* LobbyPlayerState = BoundLobbyPlayerState.Get();
	if (!LobbyGameState || !LobbyPlayerState)
	{
		return;
	}

	LobbyReadyWidget->UpdateReadyStatus(
		LobbyGameState->GetReadyPlayerCount(),
		LobbyGameState->GetCurrentPlayerCount(),
		LobbyPlayerState->IsReady());
}

void ACh4_multiGameLobbyPlayerController::RemoveLobbyReadyUI()
{
	if (ACh4_multiGameLobbyGameState* LobbyGameState = BoundLobbyGameState.Get())
	{
		LobbyGameState->OnReadySummaryChanged.RemoveDynamic(
			this, &ACh4_multiGameLobbyPlayerController::HandleReadySummaryChanged);
	}
	if (ACh4_multiGameLobbyPlayerState* LobbyPlayerState = BoundLobbyPlayerState.Get())
	{
		LobbyPlayerState->OnReadyStateChanged.RemoveDynamic(
			this, &ACh4_multiGameLobbyPlayerController::HandleLocalReadyStateChanged);
	}
	BoundLobbyGameState.Reset();
	BoundLobbyPlayerState.Reset();
	if (LobbyReadyWidget)
	{
		LobbyReadyWidget->RemoveFromParent();
		LobbyReadyWidget = nullptr;
	}
}

void ACh4_multiGameLobbyPlayerController::HandleReadySummaryChanged(
	const int32 ReadyPlayerCount,
	const int32 CurrentPlayerCount)
{
	const ACh4_multiGameLobbyPlayerState* LobbyPlayerState = BoundLobbyPlayerState.Get();
	if (LobbyReadyWidget && LobbyPlayerState)
	{
		LobbyReadyWidget->UpdateReadyStatus(
			ReadyPlayerCount,
			CurrentPlayerCount,
			LobbyPlayerState->IsReady());
	}
}

void ACh4_multiGameLobbyPlayerController::HandleLocalReadyStateChanged(const bool bIsReady)
{
	const ACh4_multiGameLobbyGameState* LobbyGameState = BoundLobbyGameState.Get();
	if (LobbyReadyWidget && LobbyGameState)
	{
		LobbyReadyWidget->UpdateReadyStatus(
			LobbyGameState->GetReadyPlayerCount(),
			LobbyGameState->GetCurrentPlayerCount(),
			bIsReady);
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

	UE_LOG(LogCh4_multiGame, Log, TEXT("[Lobby] Ready toggle requested by local player"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Cyan,
			TEXT("[LOBBY] Ready toggle request sent"));
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
