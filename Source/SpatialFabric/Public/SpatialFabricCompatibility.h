// Copyright (c) 2026 SpatialFabric Contributors. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

/** Small helper used where older UE versions differ from TArray::IsEmpty availability. */
template <typename ElementType>
FORCEINLINE bool SFArrayIsEmpty(const TArray<ElementType>& Array)
{
	return Array.Num() == 0;
}
