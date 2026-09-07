#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GrabbableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UGrabbableInterface : public UInterface
{
	GENERATED_BODY()
};

class CH4_MULTIGAME_API IGrabbableInterface
{
	GENERATED_BODY()

public:
	virtual UPrimitiveComponent* GetGrabbableComponent() { return nullptr; }
};
