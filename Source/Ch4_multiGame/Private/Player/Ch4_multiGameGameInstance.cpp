#include "Player/Ch4_multiGameGameInstance.h"

#include "Ch4_multiGame.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "LoadingScreen/Ch4LoadingScreen.h"
#include "LoadingScreen/Ch4LoadingScreenDataAsset.h"
#include "LoadingScreen/Ch4LoadingScreenSettings.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "MoviePlayer.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "UObject/UObjectGlobals.h"

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

	// Context-aware PreLoadMap runs before MoviePlayer's ordinary PreLoadMap playback hook.
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	if (FApp::CanEverRender() && IsMoviePlayerEnabled())
	{
		CacheLoadingScreenAssets();
	}
}

void UCh4_multiGameGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
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
	Super::Shutdown();
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

void UCh4_multiGameGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!bLoadingScreenPrepared || (LoadedWorld && LoadedWorld->GetGameInstance() != this))
	{
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
	LocalHeadwearID = HeadwearID;
	bHasPendingHeadwearRequest = true;
	bHasStoredHeadwear = true;
	return true;
}

bool UCh4_multiGameGameInstance::CacheAuthoritativeHeadwear(const FName HeadwearID)
{
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

	OutHeadwearID = LocalHeadwearID;
	return true;
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
