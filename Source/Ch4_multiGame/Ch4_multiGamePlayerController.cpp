// Copyright Epic Games, Inc. All Rights Reserved.


#include "Ch4_multiGamePlayerController.h"
#include "Ch4_multiGame.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
	constexpr int32 HamachiDevelopmentPort = 7777;

	bool ParseHamachiAddress(const FString& Input, FString& OutHostIPv4)
	{
		FString HostPart = Input.TrimStartAndEnd();
		FString PortPart;
		if (HostPart.Split(TEXT(":"), &HostPart, &PortPart, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			if (!PortPart.IsNumeric() || FCString::Atoi(*PortPart) != HamachiDevelopmentPort)
			{
				return false;
			}
		}

		TArray<FString> Octets;
		HostPart.ParseIntoArray(Octets, TEXT("."), false);
		if (Octets.Num() != 4)
		{
			return false;
		}

		TArray<int32, TInlineAllocator<4>> ParsedOctets;
		for (const FString& Octet : Octets)
		{
			if (Octet.IsEmpty() || Octet.Len() > 3)
			{
				return false;
			}

			for (const TCHAR Character : Octet)
			{
				if (!FChar::IsDigit(Character))
				{
					return false;
				}
			}

			const int32 Value = FCString::Atoi(*Octet);
			if (Value < 0 || Value > 255)
			{
				return false;
			}
			ParsedOctets.Add(Value);
		}

		if (ParsedOctets[0] != 25)
		{
			return false;
		}

		OutHostIPv4 = FString::Printf(
			TEXT("%d.%d.%d.%d"),
			ParsedOctets[0],
			ParsedOctets[1],
			ParsedOctets[2],
			ParsedOctets[3]);
		return true;
	}

	void ShowNetworkCommandMessage(const FString& Message, const FColor& Color)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.0f, Color, Message);
		}
	}
}

void ACh4_multiGamePlayerController::JoinHamachi(FString HostIPv4)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (GetNetMode() == NM_ListenServer)
	{
		const FString Message = TEXT(
			"[HAMACHI JOIN BLOCKED]\n"
			"This window is already a Listen Server.\n"
			"Launch the Client with Net Mode: Standalone.");
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[NetworkDebug] JoinHamachi blocked: this instance is already a Listen Server"));
		ShowNetworkCommandMessage(Message, FColor::Red);
		return;
	}

	FString NormalizedHostIPv4;
	if (!ParseHamachiAddress(HostIPv4, NormalizedHostIPv4))
	{
		const FString Message = TEXT(
			"[INVALID HAMACHI ADDRESS]\n"
			"Use the Host's 25.x.x.x address.\n"
			"Command: JoinHamachi 25.x.x.x");
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[NetworkDebug] JoinHamachi rejected a non-Hamachi or malformed address"));
		ShowNetworkCommandMessage(Message, FColor::Red);
		return;
	}

	const FString TravelURL = FString::Printf(
		TEXT("%s:%d"),
		*NormalizedHostIPv4,
		HamachiDevelopmentPort);
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[NetworkDebug] Hamachi direct connection requested on UDP port %d"),
		HamachiDevelopmentPort);
	ShowNetworkCommandMessage(TEXT("[HAMACHI] Connecting to Host on UDP 7777..."), FColor::Cyan);
	ClientTravel(TravelURL, TRAVEL_Absolute);
}

void ACh4_multiGamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogCh4_multiGame, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ACh4_multiGamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ACh4_multiGamePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
