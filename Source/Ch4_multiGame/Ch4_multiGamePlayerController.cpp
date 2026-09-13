// Copyright Epic Games, Inc. All Rights Reserved.


#include "Ch4_multiGamePlayerController.h"
#include "Ch4_multiGame.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Widgets/Input/SVirtualJoystick.h"

// [추가}
#include "EnhancedInputComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "UI/GameResult/Ch4GameResultWidget.h"
#include "UI/PauseMenu/Ch4PauseMenuViewModel.h"
#include "UI/PauseMenu/Ch4PauseMenuWidget.h"
#include "UI/HUD/Ch4HUDViewModel.h"
#include "View/MVVMView.h"
#include "MVVMSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "Player/Ch4_multiGamePlayerState.h"

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

ACh4_multiGamePlayerController::ACh4_multiGamePlayerController()
{
	// IA_Pause 기본값 로드 (로비 포함 모든 자식 클래스에 자동 상속)
	static ConstructorHelpers::FObjectFinder<UInputAction> PauseActionFinder(
		TEXT("/Game/Input/Actions/IA_Pause.IA_Pause"));
	if (PauseActionFinder.Succeeded())
	{
		PauseAction = PauseActionFinder.Object;
	}

	// IA_VoiceToggle 기본값 로드
	static ConstructorHelpers::FObjectFinder<UInputAction> VoiceToggleActionFinder(
		TEXT("/Game/Input/Actions/IA_VoiceToggle.IA_VoiceToggle"));
	if (VoiceToggleActionFinder.Succeeded())
	{
		VoiceToggleAction = VoiceToggleActionFinder.Object;
	}

	// IMC_Default 로드 (본 게임 및 전체 레벨에서 P키 일시정지 및 보이스 토글 입력 보장)
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextFinder(
		TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (DefaultContextFinder.Succeeded())
	{
		DefaultMappingContexts.AddUnique(DefaultContextFinder.Object);
	}
}

void ACh4_multiGamePlayerController::JoinHamachi(FString HostIPv4)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const UCh4_multiGameGameInstance* SessionGI = GetGameInstance<UCh4_multiGameGameInstance>();
	if (!SessionGI || !SessionGI->IsDirectIPDebugEnabled()
		|| SessionGI->HasActiveSteamSession() || SessionGI->IsSteamSessionBusy())
	{
		const FString Message = TEXT("[NetworkDebug] Direct IP is disabled while using Steam. For legacy testing restart both games with -Ch4DirectIP -nosteam and DefaultPlatformService=Null.");
		UE_LOG(LogCh4_multiGame, Warning, TEXT("%s"), *Message);
		ShowNetworkCommandMessage(Message, FColor::Red);
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

	// only execute on local player controllers
	if (IsLocalPlayerController())
	{
		// 메인 메뉴(L_MainMenu)에서는 인게임 HUD를 생성하지 않고, 로비 및 실제 인게임 맵에서만 생성
		const FString CurrentMapName = GetWorld() ? GetWorld()->GetMapName() : FString();
		const bool bIsMainMenu = CurrentMapName.Contains(TEXT("MainMenu")) || CurrentMapName.Contains(TEXT("L_MainMenu"));

		if (!bIsMainMenu)
		{
			// ── HUD ViewModel 및 위젯 생성 ──────────────────────────────────
			HUDViewModel = NewObject<UCh4HUDViewModel>(this);
			if (HUDViewModel)
			{
				HUDViewModel->InitializeWithWorld(GetWorld(), this);
			}

			if (!HUDWidgetClass)
			{
				HUDWidgetClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_HUD.WBP_HUD_C"));
			}

			if (HUDWidgetClass)
			{
				HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
				if (HUDWidget)
				{
					// 1. 위젯을 먼저 뷰포트에 추가하여 UMVVMView의 Slate 및 LoadedProperties를 온전히 초기화
					HUDWidget->AddToViewport(0); // ZOrder 0 (PauseMenu: 100 아래)

					// 2. MVVM 공식 서브시스템을 통한 확실한 뷰모델 의존성 주입 (Dependency Injection)
					if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(HUDWidget))
					{
						View->SetViewModel(FName("Ch4HUDViewModel"), HUDViewModel);
						View->SetViewModelByClass(HUDViewModel);
						View->ExecuteViewModelBindings(FName("Ch4HUDViewModel"));
					}

					// 3. Fallback: Setter 함수 호출
					if (UFunction* SetVMFunc = HUDWidget->FindFunction(FName("SetCh4HUDViewModel")))
					{
						struct FSetVMParams
						{
							UCh4HUDViewModel* InViewModel;
						};
						FSetVMParams Params;
						Params.InViewModel = HUDViewModel;
						HUDWidget->ProcessEvent(SetVMFunc, &Params);
					}

					// 4. 초기 마이크 상태(OFF) 위젯에 즉시 적용
					if (UFunction* SetMicFunc = HUDWidget->FindFunction(FName("SetMicActive")))
					{
						struct FSetMicParams
						{
							bool bIsActive;
						};
						FSetMicParams Params;
						Params.bIsActive = HUDViewModel ? HUDViewModel->bIsMicActive : false;
						HUDWidget->ProcessEvent(SetMicFunc, &Params);
					}
				}
			}
		}

		// spawn touch controls on mobile platforms
		if (ShouldUseTouchControls())
		{
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
			if (MobileControlsWidget)
			{
				MobileControlsWidget->AddToPlayerScreen(0);
			}
		}
	}

	SynchronizeCharacterSelectionForCurrentWorld();
	BindGameResultState();
}

void ACh4_multiGamePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SynchronizeCharacterSelectionForCurrentWorld();
	BindGameResultState();
}

void ACh4_multiGamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveGameResultUI();
	if (bPauseInputCaptured) HidePauseMenu();
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (PauseMenuMappingContext) Subsystem->RemoveMappingContext(PauseMenuMappingContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ACh4_multiGamePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SynchronizeCharacterSelectionForCurrentWorld();
	BindGameResultState();
}

void ACh4_multiGamePlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	BindGameResultState();
}

void ACh4_multiGamePlayerController::BindGameResultState()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	ACh4_multiGameGameState* CurrentGameState = GetWorld()
		? GetWorld()->GetGameState<ACh4_multiGameGameState>() : nullptr;
	if (ACh4_multiGameGameState* PreviousGameState = BoundResultGameState.Get())
	{
		PreviousGameState->OnGameResultChanged.RemoveDynamic(
			this, &ACh4_multiGamePlayerController::HandleGameResultChanged);
	}
	BoundResultGameState = CurrentGameState;

	if (!CurrentGameState)
	{
		RemoveGameResultUI();
		return;
	}

	CurrentGameState->OnGameResultChanged.AddDynamic(
		this, &ACh4_multiGamePlayerController::HandleGameResultChanged);
	if (CurrentGameState->HasGameResultSnapshot())
	{
		HandleGameResultChanged(CurrentGameState->GetGameResult());
	}
}

void ACh4_multiGamePlayerController::HandleGameResultChanged(const FCh4GameResult NewResult)
{
	if (NewResult.bResultAvailable)
	{
		ShowGameResult(NewResult);
	}
}

void ACh4_multiGamePlayerController::ShowGameResult(const FCh4GameResult& Result)
{
	if (!IsLocalPlayerController() || !GameResultWidgetClass)
	{
		if (!GameResultWidgetClass)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameResult] Result received but GameResultWidgetClass is not configured on %s"),
				*GetClass()->GetName());
		}
		return;
	}

	if (!IsValid(GameResultWidget) || GameResultWidget->GetWorld() != GetWorld())
	{
		GameResultWidget = CreateWidget<UCh4GameResultWidget>(this, GameResultWidgetClass);
	}
	if (!GameResultWidget)
	{
		return;
	}
	if (!GameResultWidget->IsInViewport())
	{
		GameResultWidget->AddToPlayerScreen(50);
	}

	const ACh4_multiGameGameState* ResultGameState = BoundResultGameState.Get();
	const float CurrentServerTimeSeconds = ResultGameState
		? ResultGameState->GetServerWorldTimeSeconds() : 0.0f;
	GameResultWidget->ApplyGameResult(Result, CurrentServerTimeSeconds);
}

void ACh4_multiGamePlayerController::RemoveGameResultUI()
{
	if (ACh4_multiGameGameState* PreviousGameState = BoundResultGameState.Get())
	{
		PreviousGameState->OnGameResultChanged.RemoveDynamic(
			this, &ACh4_multiGamePlayerController::HandleGameResultChanged);
	}
	BoundResultGameState.Reset();
	if (GameResultWidget)
	{
		GameResultWidget->RemoveFromParent();
		GameResultWidget = nullptr;
	}
}

void ACh4_multiGamePlayerController::RequestCharacterType(
	const ECh4CharacterType CharacterType)
{
	if (!IsLocalPlayerController() || !Ch4Character::IsValidType(CharacterType))
	{
		return;
	}

	if (UCh4_multiGameGameInstance* GameInstance =
		GetGameInstance<UCh4_multiGameGameInstance>())
	{
		// This local cache bridges non-seamless travel; the server still validates the RPC.
		GameInstance->StoreLocalCharacterRequest(CharacterType);
	}

	if (HasAuthority())
	{
		ApplyServerCharacterType(CharacterType);
	}
	else
	{
		ServerRequestCharacterType(CharacterType);
	}
}

void ACh4_multiGamePlayerController::ServerRequestCharacterType_Implementation(
	const ECh4CharacterType CharacterType)
{
	ApplyServerCharacterType(CharacterType);
}

void ACh4_multiGamePlayerController::ApplyServerCharacterType(
	const ECh4CharacterType CharacterType)
{
	if (!HasAuthority() || !Ch4Character::IsValidType(CharacterType))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[CharacterSelection] Rejected invalid or non-authoritative request from %s"),
			*GetNameSafe(this));
		return;
	}

	ACh4_multiGamePlayerState* CharacterPlayerState =
		GetPlayerState<ACh4_multiGamePlayerState>();
	if (!CharacterPlayerState
		|| !CharacterPlayerState->SetCharacterTypeFromServer(CharacterType))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[CharacterSelection] Could not store selection for %s"),
			*GetNameSafe(this));
	}
}

void ACh4_multiGamePlayerController::RequestHeadwear(const FName HeadwearID)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UCh4_multiGameGameInstance* GameInstance =
		GetGameInstance<UCh4_multiGameGameInstance>())
	{
		GameInstance->StoreLocalHeadwearRequest(HeadwearID);
	}

	if (HasAuthority())
	{
		ApplyServerHeadwear(HeadwearID);
	}
	else
	{
		ServerRequestHeadwear(HeadwearID);
	}
}

void ACh4_multiGamePlayerController::ServerRequestHeadwear_Implementation(const FName HeadwearID)
{
	ApplyServerHeadwear(HeadwearID);
}

void ACh4_multiGamePlayerController::ApplyServerHeadwear(const FName HeadwearID)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ACh4_multiGamePlayerState* PS = GetPlayerState<ACh4_multiGamePlayerState>())
	{
		PS->SetEquippedHeadwearFromServer(HeadwearID);
	}
}

void ACh4_multiGamePlayerController::SynchronizeCharacterSelectionForCurrentWorld()
{
	if (!IsLocalPlayerController()) return;
	UCh4_multiGameGameInstance* GameInstance = GetGameInstance<UCh4_multiGameGameInstance>();
	ACh4_multiGamePlayerState* CharacterPlayerState = GetPlayerState<ACh4_multiGamePlayerState>();
	if (!GameInstance || !CharacterPlayerState) return;

	if (CharacterPlayerState->IsA<ACh4_multiGameLobbyPlayerState>())
	{
		const ECh4CharacterType LobbyCharacterType = CharacterPlayerState->GetCharacterType();
		if (Ch4Character::IsValidType(LobbyCharacterType))
		{
			GameInstance->CacheAuthoritativeCharacterType(LobbyCharacterType);
		}
		GameInstance->CacheAuthoritativeHeadwear(CharacterPlayerState->GetEquippedHeadwearID());
		return;
	}

	if (!bSubmittedPersistedCharacterType)
	{
		if (Ch4Character::IsValidType(CharacterPlayerState->GetCharacterType())
			&& !GameInstance->HasPendingCharacterRequest())
		{
			// Seamless travel has already transferred the server's choice. Still run
			// the independent headwear synchronization below instead of returning.
			GameInstance->CacheAuthoritativeCharacterType(CharacterPlayerState->GetCharacterType());
			bSubmittedPersistedCharacterType = true;
		}
		else
		{
			ECh4CharacterType PersistedCharacterType = ECh4CharacterType::Invalid;
			if (GameInstance->TryGetLocalCharacterType(PersistedCharacterType))
			{
				GameInstance->StoreLocalCharacterRequest(PersistedCharacterType);
				bSubmittedPersistedCharacterType = true;
				if (HasAuthority()) ApplyServerCharacterType(PersistedCharacterType);
				else ServerRequestCharacterType(PersistedCharacterType);
				UE_LOG(LogCh4_multiGame, Log,
					TEXT("[CharacterSelection] Re-registered local selection after travel: %s"),
					*UEnum::GetValueAsString(PersistedCharacterType));
			}
		}
	}

	if (!bSubmittedPersistedHeadwear)
	{
		FName PersistedHeadwearID = NAME_None;
		if (GameInstance->TryGetLocalHeadwear(PersistedHeadwearID))
		{
			GameInstance->StoreLocalHeadwearRequest(PersistedHeadwearID);
			bSubmittedPersistedHeadwear = true;
			if (HasAuthority()) ApplyServerHeadwear(PersistedHeadwearID);
			else ServerRequestHeadwear(PersistedHeadwearID);
			UE_LOG(LogCh4_multiGame, Log,
				TEXT("[HeadwearSelection] Re-registered local headwear after travel: %s"),
				*PersistedHeadwearID.ToString());
		}
	}
}

void ACh4_multiGamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalPlayerController()) return;

	// Retain dev's runtime fallbacks before registering or binding the actions.
	if (!PauseAction)
	{
		PauseAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Pause.IA_Pause"));
	}
	if (!VoiceToggleAction)
	{
		VoiceToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_VoiceToggle.IA_VoiceToggle"));
	}

	if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		UInputMappingContext* DefaultIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
		if (DefaultIMC)
		{
			if (bUseTemplateInputMappings) Subsystem->AddMappingContext(DefaultIMC, 1);
			else Subsystem->RemoveMappingContext(DefaultIMC);
		}
		if (PauseAction)
		{
			if (!PauseMenuMappingContext)
			{
				PauseMenuMappingContext = NewObject<UInputMappingContext>(this);
				PauseMenuMappingContext->MapKey(PauseAction, EKeys::Escape);
				if (!bUseTemplateInputMappings && DefaultIMC)
				{
					// Reuse the exact existing P/voice mappings and their modifiers,
					// without template Move/Jump actions consuming the animal's keys.
					for (const FEnhancedActionKeyMapping& Mapping : DefaultIMC->GetMappings())
					{
						if ((Mapping.Action == PauseAction || Mapping.Action == VoiceToggleAction)
							&& !(Mapping.Action == PauseAction && Mapping.Key == EKeys::Escape))
						{
							PauseMenuMappingContext->MapKey(Mapping.Action, Mapping.Key) = Mapping;
						}
					}
				}
			}
			Subsystem->AddMappingContext(PauseMenuMappingContext, 1);
		}
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			// Do not re-register dev's IMC_Default at the lower priority.
			if (CurrentContext && CurrentContext != DefaultIMC) Subsystem->AddMappingContext(CurrentContext, 0);
		}
		if (!ShouldUseTouchControls())
		{
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				if (CurrentContext && CurrentContext != DefaultIMC) Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}

	// One binding per action: BeginPlay must not bind these again.
	if (auto* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (PauseAction) EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ACh4_multiGamePlayerController::TogglePauseMenu);
		if (VoiceToggleAction) EnhancedInput->BindAction(VoiceToggleAction, ETriggerEvent::Started, this, &ACh4_multiGamePlayerController::ToggleVoice);
	}
	// Retain dev's P-key fallback; per-controller debounce handles overlapping mappings.
	if (InputComponent) InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ACh4_multiGamePlayerController::TogglePauseMenu);
}

bool ACh4_multiGamePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

// [추가]
void ACh4_multiGamePlayerController::ToggleVoice()
{
	if (!IsLocalPlayerController()) return;

	if (HUDViewModel)
	{
		HUDViewModel->ToggleMic();
	}

	// WBP_HUD 위젯의 SetMicActive 함수를 즉시 실행하여 아이콘 확실하게 전환
	if (HUDWidget)
	{
		if (UFunction* SetMicFunc = HUDWidget->FindFunction(FName("SetMicActive")))
		{
			struct FSetMicParams
			{
				bool bIsActive;
			};
			FSetMicParams Params;
			Params.bIsActive = HUDViewModel ? HUDViewModel->bIsMicActive : false;
			HUDWidget->ProcessEvent(SetMicFunc, &Params);
		}
	}
}

void ACh4_multiGamePlayerController::TogglePauseMenu()
{
	if (!IsLocalPlayerController()) return;
	const double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastPauseToggleTime < 0.2) return;
	LastPauseToggleTime = CurrentTime;

	if (IsPauseMenuOpen())
	{
		// 만약 PauseMenu 안에 Settings 창이 열려있는 상태라면, PauseMenu 전체를 닫지 않고 Settings 창만 먼저 닫음!
		if (UCh4PauseMenuWidget* Ch4Pause = Cast<UCh4PauseMenuWidget>(PauseMenuWidget))
		{
			if (Ch4Pause->IsSettingsOpen())
			{
				Ch4Pause->CloseSettingsWindow();
				return;
			}
		}

		HidePauseMenu();
	}
	else
	{
		ShowPauseMenu();
	}
}

bool ACh4_multiGamePlayerController::IsMoveInputIgnored() const
{
	// ClientRestart resets the engine's ignore counters after possession. A menu
	// opened during travel must keep its own lock, without altering those counters.
	return bPauseInputCaptured || Super::IsMoveInputIgnored();
}

bool ACh4_multiGamePlayerController::IsLookInputIgnored() const
{
	return bPauseInputCaptured || Super::IsLookInputIgnored();
}

void ACh4_multiGamePlayerController::ShowPauseMenu()
{
	if (!IsLocalPlayerController() || IsPauseMenuOpen()) return;
	if (!PauseMenuWidgetClass) PauseMenuWidgetClass = PauseMenuWidgetAsset.LoadSynchronous();
	if (!PauseMenuWidgetClass)
	{
		UE_LOG(LogCh4_multiGame, Error, TEXT("[PauseMenu] Failed to load %s; verify the widget was cooked."),
			*PauseMenuWidgetAsset.ToSoftObjectPath().ToString());
		return;
	}
	if (!IsValid(PauseMenuViewModel)) PauseMenuViewModel = NewObject<UCh4PauseMenuViewModel>(this);
	// Preserve dev's world/validity check when a controller survives travel.
	if (!IsValid(PauseMenuWidget) || PauseMenuWidget->GetWorld() != GetWorld())
	{
		PauseMenuWidget = CreateWidget<UUserWidget>(this, PauseMenuWidgetClass);
	}
	if (!PauseMenuWidget) return;
	PauseMenuWidget->SetIsFocusable(true);
	PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
	PauseMenuWidget->AddToViewport(100);

	// Construct initializes MVVM or local variables.
	// If the widget created its own Ch4PauseMenuViewModel property in Construct, adopt it!
	if (FObjectProperty* Prop = CastField<FObjectProperty>(PauseMenuWidget->GetClass()->FindPropertyByName(TEXT("Ch4PauseMenuViewModel"))))
	{
		if (UObject* WidgetVM = Prop->GetObjectPropertyValue_InContainer(PauseMenuWidget))
		{
			if (UCh4PauseMenuViewModel* CastVM = Cast<UCh4PauseMenuViewModel>(WidgetVM))
			{
				PauseMenuViewModel = CastVM;
			}
		}
		else
		{
			Prop->SetObjectPropertyValue_InContainer(PauseMenuWidget, PauseMenuViewModel.Get());
		}
	}

	// Bind one effective instance after construction, retaining dev's explicit binding refresh and Blueprint setter fallback.
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(PauseMenuWidget);
	bool bViewModelBound = false;
	if (View)
	{
		if (auto* Existing = Cast<UCh4PauseMenuViewModel>(View->GetViewModel(TEXT("Ch4PauseMenuViewModel")).GetObject()))
		{
			PauseMenuViewModel = Existing;
			bViewModelBound = true;
		}
		else bViewModelBound = View->SetViewModel(TEXT("Ch4PauseMenuViewModel"), PauseMenuViewModel.Get());
	}
	PauseMenuViewModel->InitializeWithPlayerController(this);
	if (bViewModelBound) View->ExecuteViewModelBindings(TEXT("Ch4PauseMenuViewModel"));
	else if (UFunction* Setter = PauseMenuWidget->FindFunction(TEXT("SetCh4PauseMenuViewModel")))
	{
		struct FSetVMParams { UCh4PauseMenuViewModel* InViewModel; } Params{PauseMenuViewModel.Get()};
		PauseMenuWidget->ProcessEvent(Setter, &Params);
	}

	if (!bPauseInputCaptured)
	{
		bPauseInputCaptured = true;
		PauseBlockedInputComponent = InputComponent;
		if (InputComponent)
		{
			bInputBlockedBeforePause = InputComponent->bBlockInput;
			InputComponent->bBlockInput = true;
		}
	}
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[PauseMenu] Opened locally: Controller=%s WorldPaused=%s"),
		*GetName(), GetWorld()->IsPaused() ? TEXT("true") : TEXT("false"));
}

void ACh4_multiGamePlayerController::HidePauseMenu()
{
	if (!IsLocalPlayerController()) return;

	if (PauseMenuWidget && PauseMenuWidget->IsInViewport())
	{
		// 1. 화면에서 위젯 내리기
		PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		PauseMenuWidget->RemoveFromParent();
	}
	if (bPauseInputCaptured)
	{
		bPauseInputCaptured = false;
		if (UInputComponent* BlockedInput = PauseBlockedInputComponent.Get())
		{
			BlockedInput->bBlockInput = bInputBlockedBeforePause;
		}
		PauseBlockedInputComponent.Reset();
		UE_LOG(LogCh4_multiGame, Log, TEXT("[PauseMenu] Closed locally: Controller=%s"), *GetName());
	}
	
	// 2. 마우스 커서 숨기기
	bShowMouseCursor = false;
	
	// 3. 입력 모드를 순수한 게임 조작(GameOnly)으로 원상 복구
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

bool ACh4_multiGamePlayerController::IsPauseMenuOpen() const
{
	return PauseMenuWidget && PauseMenuWidget->IsInViewport();
}

void ACh4_multiGamePlayerController::ReturnToMainMenu()
{
	if (!IsLocalPlayerController()) return;
	
	// The persistent owner waits for Steam cleanup before opening the configured main menu.
	if (UCh4_multiGameGameInstance* SessionGI = GetGameInstance<UCh4_multiGameGameInstance>())
	{
		SessionGI->DestroySteamSession();
	}
}

void ACh4_multiGamePlayerController::QuitGame()
{
	if (!IsLocalPlayerController()) return;
	
	UKismetSystemLibrary::QuitGame(GetWorld(), this, EQuitPreference::Quit, false);
}

void ACh4_multiGamePlayerController::AddPitchInput(float Val)
{
	// Settings에 저장된 마우스 감도와 Y축 반전 불러오기
	float Sensitivity = 1.0f;
	bool bInvertY = false;
	GConfig->GetFloat(TEXT("Ch4_multiGame.Settings"), TEXT("MouseSensitivity"), Sensitivity, GGameIni);
	GConfig->GetBool(TEXT("Ch4_multiGame.Settings"), TEXT("InvertY"), bInvertY, GGameIni);

	float FinalVal = Val * Sensitivity;
	if (bInvertY)
	{
		FinalVal = -FinalVal; // Y축 반전
	}

	Super::AddPitchInput(FinalVal);
}

void ACh4_multiGamePlayerController::AddYawInput(float Val)
{
	// Settings에 저장된 마우스 감도 불러오기
	float Sensitivity = 1.0f;
	GConfig->GetFloat(TEXT("Ch4_multiGame.Settings"), TEXT("MouseSensitivity"), Sensitivity, GGameIni);

	Super::AddYawInput(Val * Sensitivity);
}
