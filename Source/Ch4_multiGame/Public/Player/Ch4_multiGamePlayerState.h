#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4_multiGamePlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FCh4CharacterTypeChangedSignature,
	ECh4CharacterType, CharacterType);

/** Server-authoritative, replicated character selection shared by lobby and gameplay. */
UCLASS()
class CH4_MULTIGAME_API ACh4_multiGamePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	UPROPERTY(BlueprintAssignable, Category="Player|Character|Events")
	FCh4CharacterTypeChangedSignature OnCharacterTypeChanged;

	UFUNCTION(BlueprintPure, Category="Player|Character")
	ECh4CharacterType GetCharacterType() const { return CharacterType; }

	/** Server-only mutation point. Invalid enum values are rejected. */
	bool SetCharacterTypeFromServer(ECh4CharacterType NewCharacterType);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_CharacterType();

	void ApplyCharacterTypeToPawn() const;
	void CacheCharacterTypeForOwningLocalPlayer() const;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterType, BlueprintReadOnly, Category="Player|Character", meta=(AllowPrivateAccess="true"))
	ECh4CharacterType CharacterType = ECh4CharacterType::Invalid;
};
