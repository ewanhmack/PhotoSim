// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensJSONImporter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

bool ULensJSONImporter::ImportLensesFromFile(const FString& AbsoluteFilePath, TArray<FLensDefinition>& OutLenses, TArray<FString>& OutWarnings)
{
	FString JSONString;
	if (!FFileHelper::LoadFileToString(JSONString, *AbsoluteFilePath))
	{
		OutWarnings.Add(FString::Printf(TEXT("Failed to load lens catalog file: %s"), *AbsoluteFilePath));
		return false;
	}

	return ImportLensesFromString(JSONString, OutLenses, OutWarnings);
}

bool ULensJSONImporter::ImportLensesFromString(const FString& JSONString, TArray<FLensDefinition>& OutLenses, TArray<FString>& OutWarnings)
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONString);
	if (!FJsonSerializer::Deserialize(Reader, JsonArray) || JsonArray.Num() == 0)
	{
		OutWarnings.Add(TEXT("Failed to parse lens catalog JSON: expected a top-level array of lens objects."));
		return false;
	}

	for (int32 RowIndex = 0; RowIndex < JsonArray.Num(); ++RowIndex)
	{
		const TSharedPtr<FJsonObject>* RowObjectPtr = nullptr;
		if (!JsonArray[RowIndex].IsValid() || !JsonArray[RowIndex]->TryGetObject(RowObjectPtr))
		{
			OutWarnings.Add(FString::Printf(TEXT("row %d: not a JSON object, skipped"), RowIndex));
			continue;
		}
		const TSharedPtr<FJsonObject>& RowObject = *RowObjectPtr;

		FString Focal, Fstop;
		double FocusDistanceMeters = 0.0;
		double WeightGrams = 0.0;
		bool bJsonPrime = false;
		if (!RowObject->TryGetStringField(TEXT("focal"), Focal) ||
			!RowObject->TryGetStringField(TEXT("fstop"), Fstop) ||
			!RowObject->TryGetNumberField(TEXT("focusDistance"), FocusDistanceMeters) ||
			!RowObject->TryGetNumberField(TEXT("weight"), WeightGrams) ||
			!RowObject->TryGetBoolField(TEXT("prime"), bJsonPrime))
		{
			OutWarnings.Add(FString::Printf(TEXT("row %d: missing one of focal/fstop/focusDistance/weight/prime, skipped"), RowIndex));
			continue;
		}

		float MinFocal, MaxFocal;
		if (!ParseFocalRange(Focal, MinFocal, MaxFocal))
		{
			OutWarnings.Add(FString::Printf(TEXT("row %d: failed to parse focal '%s', skipped"), RowIndex, *Focal));
			continue;
		}

		float WideAtMin, WideAtMax;
		if (!ParseFstopRange(Fstop, WideAtMin, WideAtMax))
		{
			OutWarnings.Add(FString::Printf(TEXT("row %d: failed to parse fstop '%s', skipped"), RowIndex, *Fstop));
			continue;
		}

		FLensDefinition Lens;
		Lens.MinFocalLengthMM = MinFocal;
		Lens.MaxFocalLengthMM = MaxFocal;
		Lens.WideOpenApertureAtMinFocalLength = WideAtMin;
		Lens.WideOpenApertureAtMaxFocalLength = WideAtMax;
		Lens.MaxAperture = 22.f;
		Lens.MinAperture = WideAtMin;
		Lens.MinFocusDistanceCM = static_cast<float>(FocusDistanceMeters) * 100.f;
		Lens.WeightKG = static_cast<float>(WeightGrams) / 1000.f;
		Lens.ZoomStepMM = Lens.IsPrime() ? 1.f : FMath::Max(1.f, (MaxFocal - MinFocal) / 10.f);
		Lens.Name = FText::FromString(FString::Printf(TEXT("%s %s"), *Focal, *Fstop));
		ComputeGridFootprint(Lens.MaxFocalLengthMM, Lens.WeightKG, Lens.GridWidth, Lens.GridHeight);

		if (Lens.IsPrime() != bJsonPrime)
		{
			OutWarnings.Add(FString::Printf(TEXT("row %d: JSON says prime=%s but focal range implies a %s"),
				RowIndex, bJsonPrime ? TEXT("true") : TEXT("false"), Lens.IsPrime() ? TEXT("prime") : TEXT("zoom")));
		}

		OutLenses.Add(Lens);
	}

	return true;
}

bool ULensJSONImporter::ParseFocalRange(const FString& Focal, float& OutMin, float& OutMax)
{
	FString Trimmed = Focal.TrimStartAndEnd();
	if (!Trimmed.EndsWith(TEXT("mm")))
	{
		return false;
	}
	Trimmed.LeftChopInline(2);

	FString LeftStr, RightStr;
	if (Trimmed.Split(TEXT("-"), &LeftStr, &RightStr))
	{
		if (!LeftStr.IsNumeric() || !RightStr.IsNumeric())
		{
			return false;
		}
		OutMin = FCString::Atof(*LeftStr);
		OutMax = FCString::Atof(*RightStr);
		return true;
	}

	if (!Trimmed.IsNumeric())
	{
		return false;
	}
	OutMin = OutMax = FCString::Atof(*Trimmed);
	return true;
}

bool ULensJSONImporter::ParseFstopRange(const FString& Fstop, float& OutWideAtMin, float& OutWideAtMax)
{
	FString Trimmed = Fstop.TrimStartAndEnd();
	if (!Trimmed.StartsWith(TEXT("f/")))
	{
		return false;
	}
	Trimmed.RightChopInline(2);

	FString LeftStr, RightStr;
	if (Trimmed.Split(TEXT("-"), &LeftStr, &RightStr))
	{
		if (!LeftStr.IsNumeric() || !RightStr.IsNumeric())
		{
			return false;
		}
		OutWideAtMin = FCString::Atof(*LeftStr);
		OutWideAtMax = FCString::Atof(*RightStr);
		return true;
	}

	if (!Trimmed.IsNumeric())
	{
		return false;
	}
	OutWideAtMin = OutWideAtMax = FCString::Atof(*Trimmed);
	return true;
}

void ULensJSONImporter::ComputeGridFootprint(float MaxFocalLengthMM, float WeightKG, int32& OutWidth, int32& OutHeight)
{
	OutWidth = (MaxFocalLengthMM <= 50.f) ? 1 : 2;
	OutHeight = (WeightKG >= 1.2f) ? 2 : 1;
}
