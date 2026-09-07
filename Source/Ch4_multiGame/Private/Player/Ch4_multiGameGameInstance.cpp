#include "Player/Ch4_multiGameGameInstance.h"

#include "Player/Ch4_PlayerCharacter.h"

UCh4_multiGameGameInstance::UCh4_multiGameGameInstance()
{
	CharacterClasses.SetNum(4);
	for (int32 CharacterIndex = 0; CharacterIndex < CharacterClasses.Num(); ++CharacterIndex)
	{
		const ECh4CharacterType CharacterType = Ch4Character::FromIndex(CharacterIndex);
		CharacterClasses[CharacterIndex] = TSoftClassPtr<ACh4_PlayerCharacter>(
			FSoftObjectPath(Ch4Character::GetClassPath(CharacterType)));
	}
}

bool UCh4_multiGameGameInstance::StoreLocalCharacterRequest(
	const ECh4CharacterType CharacterType)
{
	if (!Ch4Character::IsValidType(CharacterType))
	{
		return false;
	}

	LocalCharacterType = CharacterType;
	bHasPendingCharacterRequest = true;
	return true;
}

bool UCh4_multiGameGameInstance::CacheAuthoritativeCharacterType(
	const ECh4CharacterType CharacterType)
{
	if (!Ch4Character::IsValidType(CharacterType))
	{
		return false;
	}

	if (bHasPendingCharacterRequest && CharacterType != LocalCharacterType)
	{
		return false;
	}

	LocalCharacterType = CharacterType;
	bHasPendingCharacterRequest = false;
	return true;
}

bool UCh4_multiGameGameInstance::TryGetLocalCharacterType(ECh4CharacterType& OutCharacterType) const
{
	if (!Ch4Character::IsValidType(LocalCharacterType))
	{
		return false;
	}

	OutCharacterType = LocalCharacterType;
	return true;
}

TSubclassOf<ACh4_PlayerCharacter> UCh4_multiGameGameInstance::LoadCharacterClass(
	const ECh4CharacterType CharacterType) const
{
	const int32 CharacterIndex = Ch4Character::ToIndex(CharacterType);
	if (!CharacterClasses.IsValidIndex(CharacterIndex))
	{
		return nullptr;
	}

	return CharacterClasses[CharacterIndex].LoadSynchronous();
}
