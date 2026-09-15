// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Ch4PlayerProgressSaveGame.h"

#include "GameFlow/Ch4GameFlowTypes.h"

bool UCh4PlayerProgressSaveGame::ApplyGameResult(const FCh4GameResult& Result)
{
	if (!Result.bResultAvailable)
	{
		return false;
	}

	const int32 PreviousBestScore = BestSingleGameScore;
	const int32 PreviousBestCargo = BestSingleGameDeliveredCargo;
	BestSingleGameScore = FMath::Max(BestSingleGameScore, FMath::Max(Result.FinalCargoScore, 0));
	BestSingleGameDeliveredCargo = FMath::Max(
		BestSingleGameDeliveredCargo,
		FMath::Max(Result.DeliveredCargoCount, 0));

	return BestSingleGameScore != PreviousBestScore
		|| BestSingleGameDeliveredCargo != PreviousBestCargo;
}
