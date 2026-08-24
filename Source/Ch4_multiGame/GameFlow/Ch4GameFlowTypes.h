// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ch4GameFlowTypes.generated.h"

/** High-level state of a game flow session. */
UENUM(BlueprintType)
enum class ECh4GamePhase : uint8
{
	Waiting UMETA(DisplayName="Waiting"),
	Playing UMETA(DisplayName="Playing"),
	Cleared UMETA(DisplayName="Cleared"),
	GameOver UMETA(DisplayName="Game Over")
};
