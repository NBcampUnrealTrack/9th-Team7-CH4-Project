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
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Lobby/Ch4_multiGameLobbyPlayerState.h"
#include "UI/PauseMenu/Ch4PauseMenuViewModel.h"
#include "UI/HUD/Ch4HUDViewModel.h"
#include "View/MVVMView.h"
#include "MVVMSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Player/Ch4_multiGameGameInstance.h"
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
}

void ACh4_multiGamePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SynchronizeCharacterSelectionForCurrentWorld();
}

void ACh4_multiGamePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SynchronizeCharacterSelectionForCurrentWorld();
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
	if (!IsLocalPlayerController())
	{
		return;
	}

	UCh4_multiGameGameInstance* GameInstance =
		GetGameInstance<UCh4_multiGameGameInstance>();
	ACh4_multiGamePlayerState* CharacterPlayerState =
		GetPlayerState<ACh4_multiGamePlayerState>();
	if (!GameInstance || !CharacterPlayerState)
	{
		return;
	}

	if (CharacterPlayerState->IsA<ACh4_multiGameLobbyPlayerState>())
	{
		const ECh4CharacterType LobbyCharacterType =
			CharacterPlayerState->GetCharacterType();
		if (Ch4Character::IsValidType(LobbyCharacterType))
		{
			GameInstance->CacheAuthoritativeCharacterType(LobbyCharacterType);
		}
		const FName LobbyHeadwear = CharacterPlayerState->GetEquippedHeadwearID();
		GameInstance->CacheAuthoritativeHeadwear(LobbyHeadwear);
		return;
	}

	if (!bSubmittedPersistedCharacterType)
	{
		ECh4CharacterType PersistedCharacterType = ECh4CharacterType::Invalid;
		if (GameInstance->TryGetLocalCharacterType(PersistedCharacterType))
		{
			GameInstance->StoreLocalCharacterRequest(PersistedCharacterType);
			bSubmittedPersistedCharacterType = true;
			if (HasAuthority())
			{
				ApplyServerCharacterType(PersistedCharacterType);
			}
			else
			{
				ServerRequestCharacterType(PersistedCharacterType);
			}

			UE_LOG(LogCh4_multiGame, Log,
				TEXT("[CharacterSelection] Re-registered local selection after travel: %s"),
				*UEnum::GetValueAsString(PersistedCharacterType));
		}
	}

	if (!bSubmittedPersistedHeadwear)
	{
		FName PersistedHeadwearID = NAME_None;
		if (GameInstance->TryGetLocalHeadwear(PersistedHeadwearID))
		{
			GameInstance->StoreLocalHeadwearRequest(PersistedHeadwearID);
			bSubmittedPersistedHeadwear = true;
			if (HasAuthority())
			{
				ApplyServerHeadwear(PersistedHeadwearID);
			}
			else
			{
				ServerRequestHeadwear(PersistedHeadwearID);
			}

			UE_LOG(LogCh4_multiGame, Log,
				TEXT("[HeadwearSelection] Re-registered local headwear after travel: %s"),
				*PersistedHeadwearID.ToString());
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
			// 1. IMC_Default 확실하게 런타임 로드 및 우선순위 등록 (CDO 의존 제거)
			if (UInputMappingContext* DefaultIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
			{
				Subsystem->AddMappingContext(DefaultIMC, 1);
			}

			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext)
					{
						Subsystem->AddMappingContext(CurrentContext, 0);
					}
				}
			}
		}

		// 2. PauseAction 및 VoiceToggleAction 런타임 로드 보장
		if (!PauseAction)
		{
			PauseAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Pause.IA_Pause"));
		}
		if (!VoiceToggleAction)
		{
			VoiceToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_VoiceToggle.IA_VoiceToggle"));
		}

		// Enhanced Input 액션 바인딩
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			if (PauseAction)
			{
				EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &ACh4_multiGamePlayerController::TogglePauseMenu);
			}

			if (VoiceToggleAction)
			{
				EnhancedInputComponent->BindAction(VoiceToggleAction, ETriggerEvent::Started, this, &ACh4_multiGamePlayerController::ToggleVoice);
			}
		}

		// 3. Fallback: P키 직접 바인딩 (Enhanced Input 매핑 누락이나 우선순위 충돌 시에도 100% 동작 보장)
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ACh4_multiGamePlayerController::TogglePauseMenu);
		}
	}
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

	// 중복/동일 프레임 연속 호출 방지 (최소 0.2초 쿨다운 디바운스)
	const double CurrentTime = FPlatformTime::Seconds();
	static double LastToggleTime = 0.0;
	if (CurrentTime - LastToggleTime < 0.2)
	{
		return;
	}
	LastToggleTime = CurrentTime;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[PauseMenu] P Key Pressed! TogglePauseMenu() Called"));
	}

	if (IsPauseMenuOpen())
	{
		HidePauseMenu();
	}
	else
	{
		ShowPauseMenu();
	}
}

void ACh4_multiGamePlayerController::ShowPauseMenu()
{
	if (!IsLocalPlayerController()) return;

	if (!PauseMenuWidgetClass)
	{
		PauseMenuWidgetClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C"));
	}

	if (!PauseMenuWidgetClass)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[PauseMenu ERROR] PauseMenuWidgetClass is None!"));
		}
		return;
	}
	
	// 1. ViewModel 인스턴스가 없다면 확실하게 생성
	if (!PauseMenuViewModel)
	{
		PauseMenuViewModel = NewObject<UCh4PauseMenuViewModel>(this);
	}

	// 2. 아직 위젯을 만든 적이 없거나 월드가 바뀌어 유효하지 않다면 새로 생성
	if (!IsValid(PauseMenuWidget) || PauseMenuWidget->GetWorld() != GetWorld())
	{
		PauseMenuWidget = CreateWidget<UUserWidget>(this, PauseMenuWidgetClass);
	}
	
	if (PauseMenuWidget)
	{
		// 3. MVVM 공식 서브시스템을 통한 확실한 뷰모델 의존성 주입
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(PauseMenuWidget))
		{
			View->SetViewModel(FName("Ch4PauseMenuViewModel"), PauseMenuViewModel);
			View->SetViewModelByClass(PauseMenuViewModel);
			View->ExecuteViewModelBindings(FName("Ch4PauseMenuViewModel"));
		}

		if (UFunction* SetVMFunc = PauseMenuWidget->FindFunction(FName("SetCh4PauseMenuViewModel")))
		{
			struct FSetVMParams
			{
				UCh4PauseMenuViewModel* InViewModel;
			};
			FSetVMParams Params;
			Params.InViewModel = PauseMenuViewModel;
			PauseMenuWidget->ProcessEvent(SetVMFunc, &Params);
		}

		if (!PauseMenuWidget->IsInViewport())
		{
			// 화면에 위젯 띄우기 (ZOrder: 100)
			PauseMenuWidget->AddToViewport(100);
		}

		PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
		
		// 마우스 커서 보이게 하기
		bShowMouseCursor = true;
		
		// 입력 모드를 GameAndUI로 변경
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("[PauseMenu] Widget & ViewModel Loaded Successfully!"));
		}
	}
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
	
	// 온라인 세션이 활성화되어 있다면 세션 정리
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
		if (Session.IsValid() && Session->GetNamedSession(NAME_GameSession) != nullptr)
		{
			Session->DestroySession(NAME_GameSession);
		}
	}
	
	// 메인 메뉴 레벨로 안전하게 이동
	ClientTravel(TEXT("/Game/Maps/L_MainMenu"), TRAVEL_Absolute);
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
