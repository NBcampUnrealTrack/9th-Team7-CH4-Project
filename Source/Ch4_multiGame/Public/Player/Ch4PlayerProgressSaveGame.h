// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Ch4PlayerProgressSaveGame.generated.h"

struct FCh4GameResult;

/** Persistent local records. Unlock flags are derived from these values and are not saved. */
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4PlayerProgressSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Player Progress")
	int32 BestSingleGameScore = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Player Progress")
	int32 BestSingleGameDeliveredCargo = 0;

	/** Applies only an authoritative replicated result snapshot and returns true when a record improves. */
	bool ApplyGameResult(const FCh4GameResult& Result);
};
