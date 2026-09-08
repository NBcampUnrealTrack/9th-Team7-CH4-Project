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
		GS->OnPreparationTimerUpdated.RemoveDynamic(this, &UCh4HUDViewModel::OnPreparationTimerUpdated);

		GS->OnCargoCountChanged.AddDynamic(this, &UCh4HUDViewModel::OnCargoCountChanged);
		GS->OnGamePhaseChanged.AddDynamic(this, &UCh4HUDViewModel::OnGamePhaseChanged);
		GS->OnPreparationTimerUpdated.AddDynamic(this, &UCh4HUDViewModel::OnPreparationTimerUpdated);

		OnCargoCountChanged(GS->GetRemainingCargoCount(), GS->GetInitialCargoCount());
		OnGamePhaseChanged(GS->GetCurrentGamePhase());

		if (GS->GetCurrentGamePhase() == ECh4GamePhase::Waiting && GS->GetRemainingPreparationTime() > 0.0f)
		{
			StartTimerTick();
		}
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
		PhaseText = FText::FromString(TEXT("Prepare: Load the Cart!"));
		break;
	case ECh4GamePhase::Playing:
		PhaseText = FText::FromString(TEXT("Game Start!"));
		StopTimerTick();
		break;
	case ECh4GamePhase::Cleared:
		PhaseText = FText::FromString(TEXT("Mission Complete!"));
		StopTimerTick();
		break;
	default:
		StopTimerTick();
		break;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PhaseText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CurrentPhase);
}

void UCh4HUDViewModel::OnPreparationTimerUpdated(float RemainingSeconds, float TotalSeconds)
{
	if (RemainingSeconds > 0.0f && CurrentPhase == ECh4GamePhase::Waiting)
	{
		StartTimerTick();
	}
	else
	{
		StopTimerTick();
	}
}

void UCh4HUDViewModel::StartTimerTick()
{
	bIsTimerActive = true;
	TimerVisibility = ESlateVisibility::Visible;
	CurrentScaleValue = 1.0f;
	TimerScale = FVector2D(1.0f, 1.0f);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsTimerActive);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerVisibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerScale);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerTickHandle,
			this,
			&UCh4HUDViewModel::UpdateTimerTick,
			0.05f,
			true);
	}
	UpdateTimerTick();
}

void UCh4HUDViewModel::StopTimerTick()
{
	bIsTimerActive = false;
	TimerVisibility = ESlateVisibility::Collapsed;
	CurrentScaleValue = 1.0f;
	TimerScale = FVector2D(1.0f, 1.0f);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsTimerActive);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerVisibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerScale);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTickHandle);
	}
}

void UCh4HUDViewModel::UpdateTimerTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ACh4_multiGameGameState* GS = World->GetGameState<ACh4_multiGameGameState>();
	if (!GS || GS->GetCurrentGamePhase() != ECh4GamePhase::Waiting)
	{
		StopTimerTick();
		return;
	}

	const float Remaining = GS->GetRemainingPreparationTime();
	const float Ratio = GS->GetPreparationTimeRatio();

	TimerRatio = Ratio;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerRatio);

	const int32 CurrentSeconds = FMath::CeilToInt(Remaining);
	if (CurrentSeconds != LastDisplaySeconds)
	{
		LastDisplaySeconds = CurrentSeconds;
		TimerText = FText::AsNumber(CurrentSeconds);
		bIsUrgent = (CurrentSeconds <= 10 && CurrentSeconds > 0);
		TimerColor = bIsUrgent ? FSlateColor(FLinearColor(1.0f, 0.25f, 0.15f)) : FSlateColor(FLinearColor(1.0f, 0.95f, 0.85f));

		// 1초 단위 팝(펄스) 효과: 10초 이하 위기 상태면 1.35배, 평소에는 1.22배로 팡 튐
		CurrentScaleValue = bIsUrgent ? 1.35f : 1.22f;

		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerText);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsUrgent);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerColor);
	}

	// 0.05초 단위로 스케일을 1.0으로 부드럽게 감쇠 (약 0.15~0.2초 지속)
	if (CurrentScaleValue > 1.0f || TimerScale.X != 1.0f)
	{
		CurrentScaleValue = FMath::FInterpTo(CurrentScaleValue, 1.0f, 0.05f, 12.0f);
		if (FMath::IsNearlyEqual(CurrentScaleValue, 1.0f, 0.005f))
		{
			CurrentScaleValue = 1.0f;
		}
		TimerScale = FVector2D(CurrentScaleValue, CurrentScaleValue);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TimerScale);
	}

	if (Remaining <= 0.0f)
	{
		StopTimerTick();
	}
}
