#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EmotionType.h"
#include "EmotionDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FEmotionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEmotionType EmotionType = EEmotionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> EmoteMontage = nullptr;
};

UCLASS()
class CH4_MULTIGAME_API UEmotionDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FEmotionData> EmotionDataList;
};
