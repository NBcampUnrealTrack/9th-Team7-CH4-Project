// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Ch4_multiGameLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FCh4LobbyReadyStateChangedSignature,
	bool, bIsReady);

/** Replicated, per-player Ready state used only while the lobby GameMode is active. */
UCLASS()
class ACh4_multiGameLobbyPlayerState : public ACh4_multiGamePlayerState
{
	GENERATED_BODY()

	friend class ACh4_multiGameLobbyGameMode;

public:
	/** Fired on the server and clients whenever this player's Ready state changes. */
	UPROPERTY(BlueprintAssignable, Category="Lobby|Ready|Events")
	FCh4LobbyReadyStateChangedSignature OnReadyStateChanged;

	UFUNCTION(BlueprintPure, Category="Lobby|Ready")
	bool IsReady() const { return bIsReady; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCh4SeamlessStateTest;
#endif
	/** Server-only state change. Returns false if the requested state is already active. */
	bool SetReadyState(bool bNewReady);

	UFUNCTION()
	void OnRep_IsReady();

private:
	UPROPERTY(ReplicatedUsing=OnRep_IsReady, BlueprintReadOnly, Category="Lobby|Ready", meta=(AllowPrivateAccess="true"))
	bool bIsReady = false;
};
