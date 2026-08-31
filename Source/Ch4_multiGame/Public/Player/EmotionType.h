#pragma once

#include "CoreMinimal.h"
#include "EmotionType.generated.h"

UENUM(BlueprintType)
enum class EEmotionType : uint8
{
	None UMETA(DisplayName = "None"),

	Emote1 UMETA(DisplayName = "Emote 1"),
	Emote2 UMETA(DisplayName = "Emote 2"),
	Emote3 UMETA(DisplayName = "Emote 3"),
	Emote4 UMETA(DisplayName = "Emote 4")
};