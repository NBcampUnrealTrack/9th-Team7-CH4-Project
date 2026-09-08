#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4_multiGamePlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FCh4CharacterTypeChangedSignature,
	ECh4CharacterType, CharacterType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FCh4HeadwearChangedSignature,
	FName, HeadwearID);

/** Server-authoritative, replicated character selection shared by lobby and gameplay. */
UCLASS()
class CH4_MULTIGAME_API ACh4_multiGamePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category="Player|Character|Events")
	FCh4CharacterTypeChangedSignature OnCharacterTypeChanged;

	UPROPERTY(BlueprintAssignable, Category="Player|Character|Events")
	FCh4HeadwearChangedSignature OnHeadwearChanged;

	UFUNCTION(BlueprintPure, Category="Player|Character")
	ECh4CharacterType GetCharacterType() const { return CharacterType; }

	UFUNCTION(BlueprintPure, Category="Player|Character")
	FName GetEquippedHeadwearID() const { return EquippedHeadwearID; }

	/** Server-only mutation points. */
	bool SetCharacterTypeFromServer(ECh4CharacterType NewCharacterType);
	bool SetEquippedHeadwearFromServer(FName NewHeadwearID);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

private:
	UFUNCTION()
	void OnRep_CharacterType();

	UFUNCTION()
	void OnRep_EquippedHeadwearID();

	void ApplyCharacterTypeToPawn() const;
	void CacheCharacterTypeForOwningLocalPlayer() const;

	void ApplyHeadwearToPawn() const;
	void CacheHeadwearForOwningLocalPlayer() const;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterType, BlueprintReadOnly, Category="Player|Character", meta=(AllowPrivateAccess="true"))
	ECh4CharacterType CharacterType = ECh4CharacterType::Invalid;

	UPROPERTY(ReplicatedUsing=OnRep_EquippedHeadwearID, BlueprintReadOnly, Category="Player|Character", meta=(AllowPrivateAccess="true"))
	FName EquippedHeadwearID = NAME_None;
};
