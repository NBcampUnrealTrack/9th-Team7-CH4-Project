#include "Player/Ch4_multiGameGameInstance.h"

#include "Ch4_multiGame.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "LoadingScreen/Ch4LoadingScreen.h"
#include "LoadingScreen/Ch4LoadingScreenDataAsset.h"
#include "LoadingScreen/Ch4LoadingScreenSettings.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "MoviePlayer.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Player/Ch4PlayerProgressSaveGame.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "UI/SkinSelector/Ch4HatUnlockConfigDataAsset.h"
#include "UI/SkinSelector/Ch4HatUnlockSettings.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const FString PlayerProgressSlotName(TEXT("Ch4PlayerProgress"));
	constexpr int32 PlayerProgressUserIndex = 0;
}

UCh4_multiGameGameInstance::UCh4_multiGameGameInstance()
{
	CharacterClasses.SetNum(4);
	for (int32 CharacterIndex = 0; CharacterIndex < CharacterClasses.Num(); ++CharacterIndex)
	{
		const ECh4CharacterType CharacterType = Ch4Character::FromIndex(CharacterIndex);
		CharacterClasses[CharacterIndex] = TSoftClassPtr<ACh4_PlayerCharacter>(
			FSoftObjectPath(Ch4Character::GetClassPath(CharacterType)));
	}
}

void UCh4_multiGameGameInstance::Init()
{
	Super::Init();
	CacheHatUnlockConfig();
	LoadPlayerProgress();
	InitializeSteamSessions();

	// Context-aware PreLoadMap runs before MoviePlayer's ordinary PreLoadMap playback hook.
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
	FWorldDelegates::OnSeamlessTravelTransition.AddUObject(this, &ThisClass::HandleSeamlessTravelTransition);
	if (FApp::CanEverRender() && IsMoviePlayerEnabled())
	{
		CacheLoadingScreenAssets();
	}
}

void UCh4_multiGameGameInstance::Shutdown()
{
	ShutdownSteamSessions();
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FWorldDelegates::OnSeamlessTravelStart.RemoveAll(this);
	FWorldDelegates::OnSeamlessTravelTransition.RemoveAll(this);
	if (SeamlessLoadingWidget.IsValid() && GetGameViewportClient())
	{
		GetGameViewportClient()->RemoveViewportWidgetContent(SeamlessLoadingWidget.ToSharedRef());
	}
	SeamlessLoadingWidget.Reset();
	bSeamlessLoadingScreen = false;
	if (bLoadingScreenPrepared && IsMoviePlayerEnabled())
	{
		// Cancellation/shutdown may bypass PostLoadMap. Release only our local screen.
		GetMoviePlayer()->StopMovie();
		if (GetMoviePlayer()->IsMovieCurrentlyPlaying())
		{
			GetMoviePlayer()->WaitForMovieToFinish();
		}
		GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
	}
	bLoadingScreenPrepared = false;
	CachedLoadingScreenImage = nullptr;
	CachedLoadingScreenFallbackImage = nullptr;
	CachedLoadingScreenImages.Reset();
	CachedLoadingScreenData = nullptr;
	CachedHatUnlockConfig = nullptr;
	PlayerProgress = nullptr;
	Super::Shutdown();
}

void UCh4_multiGameGameInstance::CacheHatUnlockConfig()
{
	const UCh4HatUnlockSettings* Settings = GetDefault<UCh4HatUnlockSettings>();
	CachedHatUnlockConfig = Settings ? Settings->HatUnlockConfig.LoadSynchronous() : nullptr;
	if (!IsValid(CachedHatUnlockConfig))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[HatUnlock] HatUnlockConfig is not assigned in Project Settings > Game > Hat Unlocks; using safe native defaults"));
	}
}

void UCh4_multiGameGameInstance::LoadPlayerProgress()
{
	PlayerProgress = nullptr;
	const bool bSaveExists = UGameplayStatics::DoesSaveGameExist(
		PlayerProgressSlotName, PlayerProgressUserIndex);
	if (bSaveExists)
	{
		PlayerProgress = Cast<UCh4PlayerProgressSaveGame>(
			UGameplayStatics::LoadGameFromSlot(PlayerProgressSlotName, PlayerProgressUserIndex));
		if (!IsValid(PlayerProgress))
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[HatUnlock] Existing progress slot could not be loaded as Ch4PlayerProgressSaveGame; starting with locked defaults"));
		}
	}

	if (!IsValid(PlayerProgress))
	{
		PlayerProgress = Cast<UCh4PlayerProgressSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UCh4PlayerProgressSaveGame::StaticClass()));
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatProgress] Load Slot=%s UserIndex=%d Exists=%s Loaded=%s SaveClass=%s"),
		*PlayerProgressSlotName,
		PlayerProgressUserIndex,
		bSaveExists ? TEXT("true") : TEXT("false"),
		bSaveExists && IsValid(PlayerProgress) ? TEXT("true") : TEXT("false"),
		*UCh4PlayerProgressSaveGame::StaticClass()->GetPathName());
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatProgress] Runtime Profile BestScore=%d BestCargo=%d"),
		GetBestSingleGameScore(), GetBestSingleGameDeliveredCargo());
}

bool UCh4_multiGameGameInstance::SavePlayerProgress() const
{
	return IsValid(PlayerProgress)
		&& UGameplayStatics::SaveGameToSlot(PlayerProgress, PlayerProgressSlotName, PlayerProgressUserIndex);
}

void UCh4_multiGameGameInstance::CacheLoadingScreenAssets()
{
	const UCh4LoadingScreenSettings* Settings = GetDefault<UCh4LoadingScreenSettings>();
	CachedLoadingScreenData = Settings->LoadingScreenData.LoadSynchronous();
	CachedLoadingScreenImages.Reset();
	if (IsValid(CachedLoadingScreenData))
	{
		for (const TSoftObjectPtr<UTexture2D>& ImageReference : CachedLoadingScreenData->BackgroundImages)
		{
			if (UTexture2D* Image = ImageReference.LoadSynchronous(); IsValid(Image))
			{
				CachedLoadingScreenImages.AddUnique(Image);
			}
			else if (!ImageReference.IsNull())
			{
				UE_LOG(LogCh4_multiGame, Warning, TEXT("[LoadingScreen] Skipping unavailable image: %s"),
					*ImageReference.ToString());
			}
		}
	}
	CachedLoadingScreenFallbackImage = IsValid(CachedLoadingScreenData)
		? CachedLoadingScreenData->BackgroundImage.LoadSynchronous()
		: nullptr;
	if ((!Settings->LoadingScreenData.IsNull() && !IsValid(CachedLoadingScreenData))
		|| (IsValid(CachedLoadingScreenData) && !CachedLoadingScreenData->BackgroundImage.IsNull()
			&& !IsValid(CachedLoadingScreenFallbackImage)))
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[LoadingScreen] Could not load data or legacy image; available array images or black background will be used"));
	}
}

void UCh4_multiGameGameInstance::HandlePreLoadMap(const FWorldContext& LoadContext, const FString& MapName)
{
	if (LoadContext.OwningGameInstance != this || bLoadingScreenPrepared)
	{
		return;
	}
	if (LoadContext.WorldType == EWorldType::PIE || !FApp::CanEverRender() || !IsMoviePlayerEnabled())
	{
		UE_LOG(LogCh4_multiGame, Verbose, TEXT("[LoadingScreen] MoviePlayer unavailable in this run; skipping %s"), *MapName);
		return;
	}

	// Resolve soft references on the game thread before loading the next world. Init prewarms them.
	CacheLoadingScreenAssets();
	const UWorld* SourceWorld = LoadContext.World();
	const FString SourceMap = SourceWorld ? SourceWorld->GetPackage()->GetName() : FString();
	if (!Ch4LoadingScreen::ShouldShowLoadingScreen(CachedLoadingScreenData, SourceMap, MapName, bLoadingScreenPrepared)
		|| !FPackageName::DoesPackageExist(Ch4LoadingScreen::GetMapPackageName(MapName)))
	{
		UE_LOG(LogCh4_multiGame, Verbose, TEXT("[LoadingScreen] Travel not eligible: %s -> %s"), *SourceMap, *MapName);
		return;
	}

	// Choose once. The widget keeps this texture for the entire loading screen.
	CachedLoadingScreenImage = Ch4LoadingScreen::SelectRandomImage(
		CachedLoadingScreenImages, CachedLoadingScreenFallbackImage);
	GetMoviePlayer()->SetupLoadingScreen(
		Ch4LoadingScreen::BuildAttributes(CachedLoadingScreenData, CachedLoadingScreenImage));
	bLoadingScreenPrepared = true;
	UE_LOG(LogCh4_multiGame, Log, TEXT("[LoadingScreen] Prepared local map loading: %s | Image: %s"),
		*MapName, *GetNameSafe(CachedLoadingScreenImage));
}

void UCh4_multiGameGameInstance::HandleSeamlessTravelStart(UWorld* World, const FString& MapName)
{
	if (!World || World->GetGameInstance() != this || !GEngine || !GetGameViewportClient() || bLoadingScreenPrepared) return;
	if (const FWorldContext* Context = GEngine->GetWorldContextFromWorld(World))
	{
		// Seamless travel skips PreLoadMap. Reuse the existing screen, image selection
		// and Lobby-only rule, then let the final PostLoadMap finish its display.
		HandlePreLoadMap(*Context, MapName);
		if (bLoadingScreenPrepared)
		{
			// The world keeps ticking during seamless travel. Show the same Slate widget
			// in the persistent viewport; MoviePlayer's nested engine tick trips UE 5.8's
			// render-frame assertion. Hard travel retains the existing MoviePlayer path.
			const FLoadingScreenAttributes Attributes = Ch4LoadingScreen::BuildAttributes(CachedLoadingScreenData, CachedLoadingScreenImage);
			SeamlessLoadingScreenMinimumTime = Attributes.MinimumLoadingScreenDisplayTime;
			GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
			SeamlessLoadingWidget = Attributes.WidgetLoadingScreen;
			GetGameViewportClient()->AddViewportWidgetContent(SeamlessLoadingWidget.ToSharedRef(), 10000);
			bSeamlessLoadingScreen = true;
			SeamlessLoadingScreenStarted = FPlatformTime::Seconds();
			UE_LOG(LogCh4_multiGame, Log, TEXT("[LoadingScreen] Seamless display started Destination=%s"), *MapName);
		}
	}
}

void UCh4_multiGameGameInstance::HandleSeamlessTravelTransition(UWorld* World)
{
	if (!World || World->GetGameInstance() != this) return;
	// These GameModes carry controllers/states, not pawns. PhysicalAnimation's
	// BeginDestroy can run in the *next* world's GC, after its old Chaos scene died.
	// Release departing pawn constraints while that scene still exists (UE 5.8).
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		TInlineComponentArray<UPhysicalAnimationComponent*> Components(*It);
		for (UPhysicalAnimationComponent* Component : Components)
		{
			if (Component->GetSkeletalMesh())
			{
				UE_LOG(LogCh4_multiGame, Log, TEXT("[SteamTravel] Releasing departing pawn physical animation: %s"),
					*Component->GetPathName());
				Component->SetSkeletalMeshComponent(nullptr);
			}
		}
	}
}

void UCh4_multiGameGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!bLoadingScreenPrepared || (LoadedWorld && LoadedWorld->GetGameInstance() != this))
	{
		return;
	}
	if (bSeamlessLoadingScreen)
	{
		FinishSeamlessLoadingScreen();
		return;
	}
	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		// The load has completed (or failed with nullptr). MoviePlayer enforces MinimumDisplayTime.
		// Do not StopMovie here: UE 5.8 resets its start time, bypassing that minimum.
		GetMoviePlayer()->WaitForMovieToFinish();
	}
	bLoadingScreenPrepared = false;
	UE_LOG(LogCh4_multiGame, Log, TEXT("[LoadingScreen] Local map loading finished: %s"), *GetNameSafe(LoadedWorld));
}

void UCh4_multiGameGameInstance::FinishSeamlessLoadingScreen()
{
	if (!bSeamlessLoadingScreen) return;
	// A world timer can include the travel frame's delta. Recheck wall time when it
	// fires so fast packaged loads still honor the data asset's minimum exactly.
	const double Remaining = SeamlessLoadingScreenMinimumTime - (FPlatformTime::Seconds() - SeamlessLoadingScreenStarted);
	if (UWorld* World = GetWorld(); World && Remaining > 0.0)
	{
		FTimerHandle FinishTimer;
		World->GetTimerManager().SetTimer(FinishTimer, this,
			&ThisClass::FinishSeamlessLoadingScreen, FMath::Max(static_cast<float>(Remaining), 0.001f), false);
		return;
	}
	if (SeamlessLoadingWidget.IsValid() && GetGameViewportClient())
	{
		GetGameViewportClient()->RemoveViewportWidgetContent(SeamlessLoadingWidget.ToSharedRef());
	}
	SeamlessLoadingWidget.Reset();
	bSeamlessLoadingScreen = false;
	bLoadingScreenPrepared = false;
	UE_LOG(LogCh4_multiGame, Log, TEXT("[LoadingScreen] Seamless loading finished after %.2f seconds: %s"),
		FPlatformTime::Seconds() - SeamlessLoadingScreenStarted, *GetNameSafe(GetWorld()));
}

bool UCh4_multiGameGameInstance::StoreLocalCharacterRequest(
	const ECh4CharacterType CharacterType)
{
	if (!Ch4Character::IsValidType(CharacterType))
	{
		return false;
	}

	LocalCharacterType = CharacterType;
	bHasPendingCharacterRequest = true;
	return true;
}

bool UCh4_multiGameGameInstance::CacheAuthoritativeCharacterType(
	const ECh4CharacterType CharacterType)
{
	if (!Ch4Character::IsValidType(CharacterType))
	{
		return false;
	}

	if (bHasPendingCharacterRequest && CharacterType != LocalCharacterType)
	{
		return false;
	}

	LocalCharacterType = CharacterType;
	bHasPendingCharacterRequest = false;
	return true;
}

bool UCh4_multiGameGameInstance::TryGetLocalCharacterType(ECh4CharacterType& OutCharacterType) const
{
	if (!Ch4Character::IsValidType(LocalCharacterType))
	{
		return false;
	}

	OutCharacterType = LocalCharacterType;
	return true;
}

bool UCh4_multiGameGameInstance::StoreLocalHeadwearRequest(const FName HeadwearID)
{
	if (!IsHeadwearUnlocked(HeadwearID))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[HatUnlock] Rejected locked local headwear request: %s"),
			*HeadwearID.ToString());
		return false;
	}

	LocalHeadwearID = HeadwearID;
	bHasPendingHeadwearRequest = true;
	bHasStoredHeadwear = true;
	return true;
}

bool UCh4_multiGameGameInstance::CacheAuthoritativeHeadwear(const FName HeadwearID)
{
	if (!IsHeadwearUnlocked(HeadwearID))
	{
		LocalHeadwearID = NAME_None;
		bHasPendingHeadwearRequest = false;
		bHasStoredHeadwear = true;
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[HatUnlock] Server-confirmed headwear is locked by local progress; cached fallback None instead: %s"),
			*HeadwearID.ToString());
		return false;
	}

	if (bHasPendingHeadwearRequest && HeadwearID != LocalHeadwearID)
	{
		return false;
	}

	LocalHeadwearID = HeadwearID;
	bHasPendingHeadwearRequest = false;
	bHasStoredHeadwear = true;
	return true;
}

bool UCh4_multiGameGameInstance::TryGetLocalHeadwear(FName& OutHeadwearID) const
{
	if (!bHasStoredHeadwear)
	{
		return false;
	}

	OutHeadwearID = IsHeadwearUnlocked(LocalHeadwearID) ? LocalHeadwearID : NAME_None;
	return true;
}

bool UCh4_multiGameGameInstance::RecordGameResult(const FCh4GameResult& Result)
{
	if (!Result.bResultAvailable)
	{
		return false;
	}
	if (!IsValid(PlayerProgress))
	{
		LoadPlayerProgress();
	}
	if (!IsValid(PlayerProgress))
	{
		return false;
	}

	const int32 OldBestScore = PlayerProgress->BestSingleGameScore;
	const int32 OldBestCargo = PlayerProgress->BestSingleGameDeliveredCargo;
	const bool bImproved = PlayerProgress->ApplyGameResult(Result);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatProgress] Result received Score=%d DeliveredCargo=%d"),
		FMath::Max(Result.FinalCargoScore, 0),
		FMath::Max(Result.DeliveredCargoCount, 0));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatProgress] BestScore %d -> %d"),
		OldBestScore, PlayerProgress->BestSingleGameScore);
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatProgress] BestCargo %d -> %d"),
		OldBestCargo, PlayerProgress->BestSingleGameDeliveredCargo);

	if (bImproved)
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[HatProgress] Saving profile Slot=%s UserIndex=%d..."),
			*PlayerProgressSlotName, PlayerProgressUserIndex);
		if (SavePlayerProgress())
		{
			UE_LOG(LogCh4_multiGame, Log, TEXT("[HatProgress] Save succeeded"));
		}
		else
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[HatProgress] Save FAILED: personal best remains updated in runtime memory"));
		}
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[HatProgress] Best record unchanged; save skipped"));
	}
	return bImproved;
}

void UCh4_multiGameGameInstance::DumpHatUnlockState() const
{
	const UCh4HatUnlockSettings* Settings = GetDefault<UCh4HatUnlockSettings>();
	const FSoftObjectPath ConfigReference = Settings
		? Settings->HatUnlockConfig.ToSoftObjectPath() : FSoftObjectPath();
	const bool bConfigAssigned = ConfigReference.IsValid();
	const bool bConfiguredAssetLoaded = IsValid(CachedHatUnlockConfig);
	const UCh4HatUnlockConfigDataAsset* RuntimeConfig = GetHatUnlockConfig();

	const bool bSaveExists = UGameplayStatics::DoesSaveGameExist(
		PlayerProgressSlotName, PlayerProgressUserIndex);
	UCh4PlayerProgressSaveGame* DiskProgress = bSaveExists
		? Cast<UCh4PlayerProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(
			PlayerProgressSlotName, PlayerProgressUserIndex))
		: nullptr;

	UE_LOG(LogCh4_multiGame, Log, TEXT("[HatDebug] ===== BEGIN ====="));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] SaveGame Class=%s Slot=%s UserIndex=%d Exists=%s Loaded=%s"),
		*UCh4PlayerProgressSaveGame::StaticClass()->GetPathName(),
		*PlayerProgressSlotName,
		PlayerProgressUserIndex,
		bSaveExists ? TEXT("true") : TEXT("false"),
		IsValid(DiskProgress) ? TEXT("true") : TEXT("false"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] Runtime Profile Valid=%s BestScore=%d BestCargo=%d"),
		IsValid(PlayerProgress) ? TEXT("true") : TEXT("false"),
		GetBestSingleGameScore(),
		GetBestSingleGameDeliveredCargo());
	if (IsValid(DiskProgress))
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[HatDebug] Loaded SaveGame BestScore=%d BestCargo=%d"),
			FMath::Max(DiskProgress->BestSingleGameScore, 0),
			FMath::Max(DiskProgress->BestSingleGameDeliveredCargo, 0));
	}
	else
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[HatDebug] Loaded SaveGame BestScore=N/A BestCargo=N/A"));
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] Config Reference=%s Assigned=%s Loaded=%s RuntimeObject=%s Source=%s"),
		bConfigAssigned ? *ConfigReference.ToString() : TEXT("None"),
		bConfigAssigned ? TEXT("true") : TEXT("false"),
		bConfiguredAssetLoaded ? TEXT("true") : TEXT("false"),
		*GetPathNameSafe(RuntimeConfig),
		bConfiguredAssetLoaded ? TEXT("Configured DataAsset") : TEXT("Native fallback"));
	if (!bConfigAssigned)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[HatDebug] ERROR: Unlock config is not assigned"));
	}
	else if (!bConfiguredAssetLoaded)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[HatDebug] ERROR: Assigned unlock config failed to load: %s"),
			*ConfigReference.ToString());
	}
	if (!RuntimeConfig)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[HatDebug] ERROR: No runtime unlock config is available"));
		UE_LOG(LogCh4_multiGame, Log, TEXT("[HatDebug] ===== END ====="));
		return;
	}

	const int32 BestScore = GetBestSingleGameScore();
	const int32 BestCargo = GetBestSingleGameDeliveredCargo();
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] Requirements KnightScore=%d DrinkHelmetScore=%d SnapbackCargo=%d TrooperCargo=%d"),
		RuntimeConfig->KnightScoreRequirement,
		RuntimeConfig->DrinkHelmetScoreRequirement,
		RuntimeConfig->SnapbackCargoRequirement,
		RuntimeConfig->TrooperCargoRequirement);
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] Knight: BestScore %d >= Requirement %d Result=%s"),
		BestScore,
		FMath::Max(RuntimeConfig->KnightScoreRequirement, 0),
		RuntimeConfig->IsHeadwearUnlocked(Ch4Headwear::IronHelmet, BestScore, BestCargo)
			? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] DrinkHelmet: BestScore %d >= Requirement %d Result=%s"),
		BestScore,
		FMath::Max(RuntimeConfig->DrinkHelmetScoreRequirement, 0),
		RuntimeConfig->IsHeadwearUnlocked(Ch4Headwear::DrinkingHat, BestScore, BestCargo)
			? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] Snapback: BestCargo %d >= Requirement %d Result=%s"),
		BestCargo,
		FMath::Max(RuntimeConfig->SnapbackCargoRequirement, 0),
		RuntimeConfig->IsHeadwearUnlocked(Ch4Headwear::Snapback, BestScore, BestCargo)
			? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[HatDebug] Trooper: BestCargo %d >= Requirement %d Result=%s"),
		BestCargo,
		FMath::Max(RuntimeConfig->TrooperCargoRequirement, 0),
		RuntimeConfig->IsHeadwearUnlocked(Ch4Headwear::TrooperHat, BestScore, BestCargo)
			? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[HatDebug] ===== END ====="));
}

int32 UCh4_multiGameGameInstance::GetBestSingleGameScore() const
{
	return IsValid(PlayerProgress) ? FMath::Max(PlayerProgress->BestSingleGameScore, 0) : 0;
}

int32 UCh4_multiGameGameInstance::GetBestSingleGameDeliveredCargo() const
{
	return IsValid(PlayerProgress)
		? FMath::Max(PlayerProgress->BestSingleGameDeliveredCargo, 0) : 0;
}

UCh4HatUnlockConfigDataAsset* UCh4_multiGameGameInstance::GetHatUnlockConfig() const
{
	return IsValid(CachedHatUnlockConfig)
		? CachedHatUnlockConfig.Get()
		: GetMutableDefault<UCh4HatUnlockConfigDataAsset>();
}

bool UCh4_multiGameGameInstance::IsHeadwearUnlocked(const FName HeadwearID) const
{
	const UCh4HatUnlockConfigDataAsset* Config = GetHatUnlockConfig();
	return Config && Config->IsHeadwearUnlocked(
		HeadwearID,
		GetBestSingleGameScore(),
		GetBestSingleGameDeliveredCargo());
}

FText UCh4_multiGameGameInstance::GetHeadwearRequirementText(const FName HeadwearID) const
{
	const UCh4HatUnlockConfigDataAsset* Config = GetHatUnlockConfig();
	return Config ? Config->GetRequirementText(HeadwearID) : FText::GetEmpty();
}

TSubclassOf<ACh4_PlayerCharacter> UCh4_multiGameGameInstance::LoadCharacterClass(
	const ECh4CharacterType CharacterType) const
{
	const int32 CharacterIndex = Ch4Character::ToIndex(CharacterType);
	if (!CharacterClasses.IsValidIndex(CharacterIndex))
	{
		return nullptr;
	}

	return CharacterClasses[CharacterIndex].LoadSynchronous();
}
