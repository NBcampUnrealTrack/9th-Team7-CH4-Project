// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Lobby/Ch4LobbyReadyWidget.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "Ch4LobbyReadyWidget"

void UCh4LobbyReadyWidget::UpdateReadyStatus(
	const int32 ReadyCount,
	const int32 MaxPlayerCount,
	const bool bLocalPlayerReady)
{
	if (!Text_ReadyStatus)
	{
		return;
	}

	Text_ReadyStatus->SetText(FormatReadyStatus(ReadyCount, MaxPlayerCount));
	Text_ReadyStatus->SetColorAndOpacity(GetStatusColor(bLocalPlayerReady));
}

FText UCh4LobbyReadyWidget::FormatReadyStatus(
	const int32 ReadyCount,
	const int32 MaxPlayerCount)
{
	const int32 ValidatedMaxPlayerCount = FMath::Max(MaxPlayerCount, 1);
	const int32 ValidatedReadyCount = FMath::Clamp(ReadyCount, 0, ValidatedMaxPlayerCount);
	return FText::Format(
		LOCTEXT("ReadyStatusFormat", "Ready {0}/{1}"),
		FText::AsNumber(ValidatedReadyCount),
		FText::AsNumber(ValidatedMaxPlayerCount));
}

FSlateColor UCh4LobbyReadyWidget::GetStatusColor(const bool bLocalPlayerReady) const
{
	return bLocalPlayerReady ? ReadyColor : NotReadyColor;
}

#undef LOCTEXT_NAMESPACE
