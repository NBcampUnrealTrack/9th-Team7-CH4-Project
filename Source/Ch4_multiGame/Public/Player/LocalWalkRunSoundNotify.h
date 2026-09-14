#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "LocalWalkRunSoundNotify.generated.h"

UCLASS()
class CH4_MULTIGAME_API ULocalWalkRunSoundNotify : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category="Sound")
	TObjectPtr<class USoundBase> WalkRunSound;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
