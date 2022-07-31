#pragma once

#include "Engine/Engine.h"

namespace vendetta
{
	//custom distance between points calculation
	inline float calculate_distance(FVector pointA, FVector pointB, bool ignoreZ = false)
	{
		if (ignoreZ)
			pointA.Z = pointB.Z;

		return (pointA - pointB).Size();
	}

	//debug log
	inline void debug_log(const FString& msg, FColor color = FColor::Yellow)
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, color, msg);
	}

	inline FString normalize_string_from_float(float value)
	{
		const int NORMALIZE_DIVIDER = 10;
		float input_value = value / NORMALIZE_DIVIDER;
		int value_int = (int)input_value;
		int digit_after_decimal = input_value * 10 - value_int * 10;
		return FString::FromInt(value_int) + "." + FString::FromInt(digit_after_decimal);
	}
}