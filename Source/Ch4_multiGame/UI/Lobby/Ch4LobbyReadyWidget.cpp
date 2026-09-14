// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Lobby/Ch4LobbyReadyWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "Ch4LobbyReadyWidget"

void UCh4LobbyReadyWidget::UpdateReadyStatus(
	const int32 ReadyCount,
	const int32 CurrentPlayerCount,
	const bool bLocalPlayerReady)
{
	if (!Text_ReadyStatus)
	{
		return;
	}

	Text_ReadyStatus->SetText(FormatReadyStatus(ReadyCount, CurrentPlayerCount));
	Text_ReadyStatus->SetColorAndOpacity(GetStatusColor(bLocalPlayerReady));

	const bool bStatusChanged = bHasInitialized &&
		(ReadyCount != PreviousReadyCount || CurrentPlayerCount != PreviousPlayerCount || bLocalPlayerReady != bPreviousLocalPlayerReady);

	PreviousReadyCount = ReadyCount;
	PreviousPlayerCount = CurrentPlayerCount;
	bPreviousLocalPlayerReady = bLocalPlayerReady;
	bHasInitialized = true;

	if (bStatusChanged && ReadyPopAnim)
	{
		PlayAnimation(ReadyPopAnim);
	}
}

FText UCh4LobbyReadyWidget::FormatReadyStatus(
	const int32 ReadyCount,
	const int32 CurrentPlayerCount)
{
	// A local Lobby widget always represents at least its owning player. This also
	// prevents a transient replication order from displaying Ready 0/0.
	const int32 ValidatedCurrentPlayerCount = FMath::Max(CurrentPlayerCount, 1);
	const int32 ValidatedReadyCount = FMath::Clamp(ReadyCount, 0, ValidatedCurrentPlayerCount);
	return FText::Format(
		LOCTEXT("ReadyStatusFormat", "Ready {0}/{1}"),
		FText::AsNumber(ValidatedReadyCount),
		FText::AsNumber(ValidatedCurrentPlayerCount));
}

FSlateColor UCh4LobbyReadyWidget::GetStatusColor(const bool bLocalPlayerReady) const
{
	return bLocalPlayerReady ? ReadyColor : NotReadyColor;
}

#undef LOCTEXT_NAMESPACE
