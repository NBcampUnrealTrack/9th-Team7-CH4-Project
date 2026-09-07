#pragma once

#include "CoreMinimal.h"
#include "Ch4CharacterTypes.generated.h"

/** Compact, network-safe identifier for the four selectable player appearances. */
UENUM(BlueprintType)
enum class ECh4CharacterType : uint8
{
	Cat UMETA(DisplayName="Cat"),
	Dog UMETA(DisplayName="Dog"),
	Gorilla UMETA(DisplayName="Gorilla"),
	Otter UMETA(DisplayName="Otter"),
	Invalid UMETA(Hidden)
};

namespace Ch4Character
{
	CH4_MULTIGAME_API bool IsValidType(ECh4CharacterType CharacterType);
	CH4_MULTIGAME_API int32 ToIndex(ECh4CharacterType CharacterType);
	CH4_MULTIGAME_API ECh4CharacterType FromIndex(int32 CharacterIndex);
	CH4_MULTIGAME_API const TCHAR* GetClassPath(ECh4CharacterType CharacterType);
}
