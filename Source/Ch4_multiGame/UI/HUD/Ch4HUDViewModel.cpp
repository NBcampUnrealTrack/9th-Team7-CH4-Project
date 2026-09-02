#include "UI/HUD/Ch4HUDViewModel.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UCh4HUDViewModel::UCh4HUDViewModel()
{
	PhaseText = FText::FromString(TEXT("Press R to Ready"));
	CargoText = FText::FromString(TEXT("Cargo: -- / --"));
}

UWorld* UCh4HUDViewModel::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (CachedPC.IsValid())
	{
		return CachedPC->GetWorld();
	}

	if (UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}

	if (GEngine && GEngine->GetWorldContexts().Num() > 0)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
	}

	return nullptr;
}

void UCh4HUDViewModel::InitializeWithWorld(UWorld* World, APlayerController* OwningController)
{
	if (!World)
	{
		World = GetWorld();
	}
	if (!World) return;

	if (OwningController)
	{
		CachedPC = OwningController;
	}
	else if (!CachedPC.IsValid())
	{
		CachedPC = World->GetFirstPlayerController();
	}

	// 1. 인게임 Gameplay GameState가 있으면 이벤트 델리게이트 바인딩
	if (ACh4_multiGameGameState* GS = World->GetGameState<ACh4_multiGameGameState>())
	{
		GS->OnCargoCountChanged.RemoveDynamic(this, &UCh4HUDViewModel::OnCargoCountChanged);
		GS->OnGamePhaseChanged.RemoveDynamic(this, &UCh4HUDViewModel::OnGamePhaseChanged);

		GS->OnCargoCountChanged.AddDynamic(this, &UCh4HUDViewModel::OnCargoCountChanged);
		GS->OnGamePhaseChanged.AddDynamic(this, &UCh4HUDViewModel::OnGamePhaseChanged);

		OnCargoCountChanged(GS->GetRemainingCargoCount(), GS->GetInitialCargoCount());
		OnGamePhaseChanged(GS->GetCurrentGamePhase());
	}
	else
	{
		// 2. 로비 맵: 로비 초기 텍스트 즉시 세팅 및 Ready 상태 감지
		PhaseText = FText::FromString(TEXT("Press R to Ready"));
		CargoText = FText::FromString(TEXT("Cargo: -- / --"));

		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoText);

		TryBindLobbyPlayerState();
	}

	bIsInitialized = true;
}

void UCh4HUDViewModel::TryBindLobbyPlayerState()
{
	APlayerController* PC = CachedPC.Get();
	if (!PC)
	{
		if (UWorld* World = GetWorld())
		{
			PC = World->GetFirstPlayerController();
			CachedPC = PC;
		}
	}

	if (!PC) return;

	if (ACh4_multiGameLobbyPlayerState* LPS = PC->GetPlayerState<ACh4_multiGameLobbyPlayerState>())
	{
		LPS->OnReadyStateChanged.RemoveDynamic(this, &UCh4HUDViewModel::OnLobbyReadyChanged);
		LPS->OnReadyStateChanged.AddDynamic(this, &UCh4HUDViewModel::OnLobbyReadyChanged);
		OnLobbyReadyChanged(LPS->IsReady());
	}
	else
	{
		// PlayerState가 복제/초기화될 때까지 0.1초마다 재시도
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				LobbyBindTimer,
				this,
				&UCh4HUDViewModel::TryBindLobbyPlayerState,
				0.1f,
				false);
		}
	}
}

void UCh4HUDViewModel::OnLobbyReadyChanged(bool bNewReady)
{
	bIsLobbyReady = bNewReady;

	if (bIsLobbyReady)
	{
		PhaseText = FText::FromString(TEXT("READY! (Press R to Cancel)"));
	}
	else
	{
		PhaseText = FText::FromString(TEXT("Press R to Ready"));
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsLobbyReady);
}

void UCh4HUDViewModel::OnCargoCountChanged(int32 Remaining, int32 Initial)
{
	CargoText = FText::FromString(
		FString::Printf(TEXT("Cargo: %d / %d"), Remaining, Initial));
	CargoRatio = (Initial > 0) ? (float)Remaining / (float)Initial : 1.0f;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CargoRatio);
}

void UCh4HUDViewModel::ToggleMic()
{
	SetMicActive(!bIsMicActive);
}

void UCh4HUDViewModel::SetMicActive(bool bActive)
{
	if (bIsMicActive != bActive)
	{
		bIsMicActive = bActive;
		MicIconIndex = bIsMicActive ? 1 : 0;
		MicOnVisibility = bIsMicActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		MicOffVisibility = bIsMicActive ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsMicActive);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicIconIndex);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicOnVisibility);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MicOffVisibility);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				bIsMicActive ? FColor::Green : FColor::Red,
				bIsMicActive ? TEXT("[VOICE] Mic UNMUTED (ON)") : TEXT("[VOICE] Mic MUTED (OFF)"));
		}
	}
}

void UCh4HUDViewModel::OnGamePhaseChanged(ECh4GamePhase NewPhase)
{
	CurrentPhase = NewPhase;

	switch (NewPhase)
	{
	case ECh4GamePhase::Waiting:
		PhaseText = FText::FromString(TEXT("Waiting for Players..."));
		break;
	case ECh4GamePhase::Playing:
		PhaseText = FText::FromString(TEXT("Game Start!"));
		break;
	case ECh4GamePhase::Cleared:
		PhaseText = FText::FromString(TEXT("Mission Complete!"));
		break;
	default:
		break;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CurrentPhase);
}
