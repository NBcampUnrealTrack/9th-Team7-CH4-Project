#include "UI/GameResult/Ch4GameResultWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Ch4_multiGamePlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "Ch4GameResultWidget"

namespace
{
	constexpr float CountdownUpdateIntervalSeconds = 0.25f;
}

void UCh4GameResultWidget::ApplyGameResult(
	const FCh4GameResult& Result,
	const float CurrentServerTimeSeconds)
{
	if (!Result.bResultAvailable)
	{
		return;
	}

	bDisplayedResultSucceeded = Result.bSucceeded;
	Text_ResultTitle->SetText(GetResultTitle(Result.bSucceeded));
	Text_ResultTitle->SetColorAndOpacity(Result.bSucceeded ? SuccessTitleColor : FailureTitleColor);
	Text_ClearTime->SetText(FormatClearTime(Result.ClearTimeSeconds));
	Text_CargoCount->SetText(FText::AsNumber(FMath::Max(Result.DeliveredCargoCount, 0)));
	Text_TotalScore->SetText(FormatScore(Result.FinalCargoScore));

	if (FadeInAnim)
	{
		PlayAnimation(FadeInAnim);
	}

	if (Btn_Confirm)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
		}
	}

	StopCountdown();
	const float SafeServerTime = FMath::IsFinite(CurrentServerTimeSeconds)
		? CurrentServerTimeSeconds : 0.0f;
	const float RemainingSeconds = FMath::IsFinite(Result.ResultDisplayEndServerTime)
		? FMath::Max(Result.ResultDisplayEndServerTime - SafeServerTime, 0.0f)
		: 0.0f;
	if (UWorld* World = GetWorld())
	{
		CountdownEndLocalTimeSeconds = World->GetTimeSeconds() + RemainingSeconds;
		UpdateCountdown();
		if (Text_Countdown && RemainingSeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				CountdownTimer,
				this,
				&UCh4GameResultWidget::UpdateCountdown,
				CountdownUpdateIntervalSeconds,
				true);
		}
	}
}

FText UCh4GameResultWidget::GetResultTitle(const bool bSucceeded)
{
	return bSucceeded
		? LOCTEXT("DeliveryCompleteTitle", "DELIVERY COMPLETE")
		: LOCTEXT("DeliveryFailedTitle", "DELIVERY FAILED");
}

FText UCh4GameResultWidget::FormatClearTime(const float ClearTimeSeconds)
{
	const double SafeSeconds = FMath::IsFinite(ClearTimeSeconds)
		? FMath::Max(static_cast<double>(ClearTimeSeconds), 0.0) : 0.0;
	const int64 TotalCentiseconds = FMath::RoundToInt64(SafeSeconds * 100.0);
	const int64 Minutes = TotalCentiseconds / 6000;
	const int64 Seconds = (TotalCentiseconds / 100) % 60;
	const int64 Centiseconds = TotalCentiseconds % 100;
	return FText::FromString(FString::Printf(
		TEXT("%02lld:%02lld.%02lld"), Minutes, Seconds, Centiseconds));
}

FText UCh4GameResultWidget::FormatScore(const int32 Score)
{
	return FText::AsNumber(FMath::Max(Score, 0));
}

int32 UCh4GameResultWidget::CalculateCountdownSeconds(
	const float ResultDisplayEndServerTime,
	const float CurrentServerTimeSeconds)
{
	if (!FMath::IsFinite(ResultDisplayEndServerTime)
		|| !FMath::IsFinite(CurrentServerTimeSeconds))
	{
		return 0;
	}
	return FMath::Max(
		FMath::CeilToInt(ResultDisplayEndServerTime - CurrentServerTimeSeconds),
		0);
}

void UCh4GameResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.AddDynamic(this, &UCh4GameResultWidget::OnConfirmClicked);
	}
}

void UCh4GameResultWidget::NativeDestruct()
{
	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.RemoveDynamic(this, &UCh4GameResultWidget::OnConfirmClicked);
	}

	StopCountdown();
	Super::NativeDestruct();
}

void UCh4GameResultWidget::OnConfirmClicked()
{
	if (ACh4_multiGamePlayerController* PC = Cast<ACh4_multiGamePlayerController>(GetOwningPlayer()))
	{
		PC->ReturnToMainMenu();
	}
}

void UCh4GameResultWidget::UpdateCountdown()
{
	if (!Text_Countdown)
	{
		StopCountdown();
		return;
	}

	const UWorld* World = GetWorld();
	const double RemainingSeconds = World
		? FMath::Max(CountdownEndLocalTimeSeconds - World->GetTimeSeconds(), 0.0)
		: 0.0;
	const int32 DisplaySeconds = FMath::Max(FMath::CeilToInt(RemainingSeconds), 0);
	const FText Format = bDisplayedResultSucceeded
		? LOCTEXT("ReturningToLobbyCountdown", "Returning to lobby in {0}...")
		: LOCTEXT("GameOverCountdown", "Game over in {0}...");
	Text_Countdown->SetText(FText::Format(Format, FText::AsNumber(DisplaySeconds)));
	if (RemainingSeconds <= 0.0)
	{
		StopCountdown();
	}
}

void UCh4GameResultWidget::StopCountdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}
}

#undef LOCTEXT_NAMESPACE
