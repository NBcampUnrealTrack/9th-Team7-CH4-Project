#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Ch4GameResultWidget.generated.h"

class UTextBlock;

/**
 * C++ presentation base for WBP_GameResult.
 * The owning PlayerController supplies the replicated result and synchronized server time.
 */
UCLASS(Abstract, Blueprintable)
class CH4_MULTIGAME_API UCh4GameResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Applies one replicated Goal snapshot and starts its cosmetic local countdown. */
	void ApplyGameResult(const FCh4GameResult& Result, float CurrentServerTimeSeconds);

	static FText GetResultTitle(bool bSucceeded);
	static FText FormatClearTime(float ClearTimeSeconds);
	static FText FormatScore(int32 Score);
	static int32 CalculateCountdownSeconds(float ResultDisplayEndServerTime, float CurrentServerTimeSeconds);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ResultTitle;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ClearTime;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_CargoCount;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_TotalScore;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Countdown;

private:
	void UpdateCountdown();
	void StopCountdown();

	bool bDisplayedResultSucceeded = false;
	double CountdownEndLocalTimeSeconds = 0.0;
	FTimerHandle CountdownTimer;
};
