// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

template <typename ElementType>
FORCEINLINE bool SFArrayIsEmpty(const TArray<ElementType>& Array)
{
	return Array.Num() == 0;
}
