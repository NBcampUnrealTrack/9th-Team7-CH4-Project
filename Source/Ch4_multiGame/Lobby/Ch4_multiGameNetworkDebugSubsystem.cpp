// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameNetworkDebugSubsystem.h"

#include "Ch4_multiGame.h"
#include "Engine/Engine.h"

void UCh4_multiGameNetworkDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this,
			&UCh4_multiGameNetworkDebugSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
			this,
			&UCh4_multiGameNetworkDebugSubsystem::HandleTravelFailure);
	}
}

void UCh4_multiGameNetworkDebugSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	Super::Deinitialize();
}

void UCh4_multiGameNetworkDebugSubsystem::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	const ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	const FString FailureName = ENetworkFailure::ToString(FailureType);
	FString Guidance = TEXT("Check the exact IP:port, Hamachi adapter, and Host UDP 7777 firewall rule.");
	if (ErrorString.Contains(TEXT("RemoteAddr: 127.")))
	{
		Guidance = TEXT("Loopback address detected. Use the other PC Host's Hamachi 25.x.x.x address.");
	}
	else if (ErrorString.Contains(TEXT("RemoteAddr: 192.168.")))
	{
		Guidance = TEXT(
			"Private LAN address detected (192.168.x.x).\n"
			"For Hamachi use: JoinHamachi <HOST 25.x.x.x>");
	}
	else if (ErrorString.Contains(TEXT("RemoteAddr: 25.")))
	{
		Guidance = TEXT(
			"Hamachi address was used but the Host did not answer.\n"
			"Verify Host Listen Server, Hamachi peer status, and UDP 7777 firewall.");
	}

	const FString Details = FString::Printf(
		TEXT("%s\n%s"),
		*FailureName,
		*Guidance);

	UE_LOG(LogCh4_multiGame, Error,
		TEXT("[NetworkDebug] NETWORK FAILURE | Type: %s | Error: %s | World: %s | Driver: %s"),
		*FailureName,
		ErrorString.IsEmpty() ? TEXT("No error details") : *ErrorString,
		*GetNameSafe(World),
		*GetNameSafe(NetDriver));
	ShowFailureMessage(TEXT("NETWORK CONNECTION FAILED"), Details);
}

void UCh4_multiGameNetworkDebugSubsystem::HandleTravelFailure(
	UWorld* World,
	const ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	const FString FailureName = ETravelFailure::ToString(FailureType);
	const FString Details = FString::Printf(
		TEXT("%s: %s\nCheck the map name. Lobby map: /Game/Lobby/L_Lobby"),
		*FailureName,
		ErrorString.IsEmpty() ? TEXT("No error details") : *ErrorString);

	UE_LOG(LogCh4_multiGame, Error,
		TEXT("[NetworkDebug] TRAVEL FAILURE | Type: %s | Error: %s | World: %s"),
		*FailureName,
		ErrorString.IsEmpty() ? TEXT("No error details") : *ErrorString,
		*GetNameSafe(World));
	ShowFailureMessage(TEXT("MAP TRAVEL FAILED"), Details);
}

void UCh4_multiGameNetworkDebugSubsystem::ShowFailureMessage(
	const FString& Title,
	const FString& Details) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Red, FString::Printf(
			TEXT("[%s]\n%s\nSee Output Log: [NetworkDebug]"),
			*Title,
			*Details));
	}
}
