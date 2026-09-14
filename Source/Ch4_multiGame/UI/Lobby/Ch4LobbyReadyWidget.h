// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ch4LobbyReadyWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

/** C++ presentation base for the always-visible Lobby Ready summary. */
UCLASS(Abstract, Blueprintable)
class CH4_MULTIGAME_API UCh4LobbyReadyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Applies the replicated lobby Ready count and the owning player's Ready state. */
	void UpdateReadyStatus(int32 ReadyCount, int32 CurrentPlayerCount, bool bLocalPlayerReady);

	/** Localization-friendly text formatter shared by presentation tests. */
	static FText FormatReadyStatus(int32 ReadyCount, int32 CurrentPlayerCount);

	/** Returns the designer-configurable color for the local Ready state. */
	FSlateColor GetStatusColor(bool bLocalPlayerReady) const;

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ReadyStatus;

	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ReadyPopAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Ready|Style")
	FSlateColor ReadyColor = FSlateColor(FLinearColor::Green);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lobby|Ready|Style")
	FSlateColor NotReadyColor = FSlateColor(FLinearColor::Red);

private:
	int32 PreviousReadyCount = -1;
	int32 PreviousPlayerCount = -1;
	bool bPreviousLocalPlayerReady = false;
	bool bHasInitialized = false;
};
