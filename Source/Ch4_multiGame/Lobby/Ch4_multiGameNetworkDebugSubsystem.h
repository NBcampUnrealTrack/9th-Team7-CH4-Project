// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Ch4_multiGameNetworkDebugSubsystem.generated.h"

/** Shared diagnostics for Steam and explicit development direct-IP connection/travel failures. */
UCLASS()
class UCh4_multiGameNetworkDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleNetworkFailure(
		UWorld* World,
		class UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);
	void HandleTravelFailure(
		UWorld* World,
		ETravelFailure::Type FailureType,
		const FString& ErrorString);
	void LogFailureMessage(const FString& Title, const FString& Details) const;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
