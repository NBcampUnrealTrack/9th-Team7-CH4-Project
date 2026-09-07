#include "Player/Ch4CharacterTypes.h"

namespace
{
	constexpr int32 CharacterTypeCount = 4;
	const TCHAR* CharacterClassPaths[CharacterTypeCount] = {
		TEXT("/Game/Player/Cat/BP_CatCharacter.BP_CatCharacter_C"),
		TEXT("/Game/Player/Dog/BP_DogCharacter.BP_DogCharacter_C"),
		TEXT("/Game/Player/Gorilla/BP_GorillaCharacter.BP_GorillaCharacter_C"),
		TEXT("/Game/Player/Otter/BP_OtterCharacter.BP_OtterCharacter_C")};
}

bool Ch4Character::IsValidType(const ECh4CharacterType CharacterType)
{
	return ToIndex(CharacterType) != INDEX_NONE;
}

int32 Ch4Character::ToIndex(const ECh4CharacterType CharacterType)
{
	const int32 CharacterIndex = static_cast<int32>(CharacterType);
	return CharacterIndex >= 0 && CharacterIndex < CharacterTypeCount
		? CharacterIndex
		: INDEX_NONE;
}

ECh4CharacterType Ch4Character::FromIndex(const int32 CharacterIndex)
{
	return CharacterIndex >= 0 && CharacterIndex < CharacterTypeCount
		? static_cast<ECh4CharacterType>(CharacterIndex)
		: ECh4CharacterType::Invalid;
}

const TCHAR* Ch4Character::GetClassPath(const ECh4CharacterType CharacterType)
{
	const int32 CharacterIndex = ToIndex(CharacterType);
	return CharacterIndex != INDEX_NONE ? CharacterClassPaths[CharacterIndex] : TEXT("");
}
