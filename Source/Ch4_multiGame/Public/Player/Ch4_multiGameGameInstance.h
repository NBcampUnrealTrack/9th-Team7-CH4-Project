#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4_multiGameGameInstance.generated.h"

class ACh4_PlayerCharacter;

/**
 * Keeps only this process's local player's final selection across non-seamless travel.
 * Replicated PlayerState remains the authoritative in-world source of truth.
 */
UCLASS()
class CH4_MULTIGAME_API UCh4_multiGameGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UCh4_multiGameGameInstance();

	/** Records the owning local player's newest choice before its server RPC is confirmed. */
	bool StoreLocalCharacterRequest(ECh4CharacterType CharacterType);

	/**
	 * Caches a value confirmed by the server. A late older replication cannot overwrite
	 * a newer local request that is still waiting for its matching confirmation.
	 */
	bool CacheAuthoritativeCharacterType(ECh4CharacterType CharacterType);
	bool TryGetLocalCharacterType(ECh4CharacterType& OutCharacterType) const;
	bool HasPendingCharacterRequest() const { return bHasPendingCharacterRequest; }

	UFUNCTION(BlueprintPure, Category="Player|Character")
	TSubclassOf<ACh4_PlayerCharacter> LoadCharacterClass(ECh4CharacterType CharacterType) const;

	int32 GetCharacterClassCount() const { return CharacterClasses.Num(); }

private:
	/** Shared appearance catalog. Array order follows ECh4CharacterType. */
	UPROPERTY(EditDefaultsOnly, Category="Player|Character")
	TArray<TSoftClassPtr<ACh4_PlayerCharacter>> CharacterClasses;

	/** Local transfer cache only; the server validates and stores the world state in PlayerState. */
	UPROPERTY(Transient)
	ECh4CharacterType LocalCharacterType = ECh4CharacterType::Invalid;

	UPROPERTY(Transient)
	bool bHasPendingCharacterRequest = false;
};
