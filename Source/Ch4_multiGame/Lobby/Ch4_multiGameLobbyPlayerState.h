// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Ch4_multiGameLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FCh4LobbyReadyStateChangedSignature,
	bool, bIsReady);

/** Replicated, per-player Ready state used only while the lobby GameMode is active. */
UCLASS()
class ACh4_multiGameLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

	friend class ACh4_multiGameLobbyGameMode;

public:
	/** Fired on the server and clients when this player's Ready state becomes true. */
	UPROPERTY(BlueprintAssignable, Category="Lobby|Ready|Events")
	FCh4LobbyReadyStateChangedSignature OnReadyStateChanged;

	UFUNCTION(BlueprintPure, Category="Lobby|Ready")
	bool IsReady() const { return bIsReady; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Server-only one-way transition. Returns false for duplicate Ready requests. */
	bool SetReady();

	UFUNCTION()
	void OnRep_IsReady();

private:
	UPROPERTY(ReplicatedUsing=OnRep_IsReady, BlueprintReadOnly, Category="Lobby|Ready", meta=(AllowPrivateAccess="true"))
	bool bIsReady = false;
};
