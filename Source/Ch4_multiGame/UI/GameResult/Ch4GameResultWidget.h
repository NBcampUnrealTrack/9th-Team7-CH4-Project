#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateColor.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Ch4GameResultWidget.generated.h"

class UButton;
class UTextBlock;
class UWidgetAnimation;

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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnConfirmClicked();

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

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Btn_Confirm;

	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeInAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Result Appearance")
	FSlateColor SuccessTitleColor = FSlateColor(FLinearColor(0.18f, 0.8f, 0.44f, 1.0f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Result Appearance")
	FSlateColor FailureTitleColor = FSlateColor(FLinearColor(0.9f, 0.24f, 0.24f, 1.0f));

private:
	void UpdateCountdown();
	void StopCountdown();

	bool bDisplayedResultSucceeded = false;
	double CountdownEndLocalTimeSeconds = 0.0;
	FTimerHandle CountdownTimer;
};
