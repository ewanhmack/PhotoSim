// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "LensJSONImporter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace LensImportTestData
{
	// Julien's real 15-entry lens catalog.
	static const TCHAR* FullCatalogJSON = TEXT(R"([
	  {"focal": "14mm", "fstop": "f/1.4", "focusDistance": 0.30, "weight": 1170, "prime": true},
	  {"focal": "14-24mm", "fstop": "f/2.8", "focusDistance": 0.28, "weight": 795, "prime": false},
	  {"focal": "24mm", "fstop": "f/1.4", "focusDistance": 0.24, "weight": 445, "prime": true},
	  {"focal": "24-70mm", "fstop": "f/2.8", "focusDistance": 0.38, "weight": 886, "prime": false},
	  {"focal": "35mm", "fstop": "f/1.4", "focusDistance": 0.25, "weight": 524, "prime": true},
	  {"focal": "50mm", "fstop": "f/1.4", "focusDistance": 0.40, "weight": 905, "prime": true},
	  {"focal": "24-105mm", "fstop": "f/4", "focusDistance": 0.38, "weight": 663, "prime": false},
	  {"focal": "85mm", "fstop": "f/1.4", "focusDistance": 0.85, "weight": 820, "prime": true},
	  {"focal": "70-200mm", "fstop": "f/2.8", "focusDistance": 0.96, "weight": 1480, "prime": false},
	  {"focal": "100mm", "fstop": "f/2.8", "focusDistance": 0.30, "weight": 730, "prime": true},
	  {"focal": "100-400mm", "fstop": "f/5-6.3", "focusDistance": 1.12, "weight": 1135, "prime": false},
	  {"focal": "135mm", "fstop": "f/1.8", "focusDistance": 0.87, "weight": 1220, "prime": true},
	  {"focal": "200mm", "fstop": "f/2.8", "focusDistance": 1.70, "weight": 1480, "prime": true},
	  {"focal": "70-300mm", "fstop": "f/4.5-5.6", "focusDistance": 0.90, "weight": 710, "prime": false},
	  {"focal": "100-400mm", "fstop": "f/4.5-5.6", "focusDistance": 0.98, "weight": 1640, "prime": false}
	])");

	static const TCHAR* MalformedRowJSON = TEXT(R"([
	  {"focal": "not-a-number", "fstop": "f/2.8", "focusDistance": 0.30, "weight": 500, "prime": true},
	  {"focal": "50mm", "fstop": "f/1.4", "focusDistance": 0.40, "weight": 905, "prime": true}
	])");

	static const TCHAR* PrimeMismatchJSON = TEXT(R"([
	  {"focal": "24-70mm", "fstop": "f/2.8", "focusDistance": 0.38, "weight": 886, "prime": true}
	])");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportAllRowsTest, "PhotoSim.LensImport.AllRowsImportWithoutError", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportAllRowsTest::RunTest(const FString& Parameters)
{
	TArray<FLensDefinition> Lenses;
	TArray<FString> Warnings;
	const bool bSuccess = ULensJSONImporter::ImportLensesFromString(LensImportTestData::FullCatalogJSON, Lenses, Warnings);

	TestTrue(TEXT("Import reports success"), bSuccess);
	TestEqual(TEXT("All 15 rows imported"), Lenses.Num(), 15);
	TestEqual(TEXT("No warnings for a clean, consistent catalog"), Warnings.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportParseFocalRangeTest, "PhotoSim.LensImport.ParseFocalRange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportParseFocalRangeTest::RunTest(const FString& Parameters)
{
	float Min = 0.f, Max = 0.f;

	TestTrue(TEXT("Single value parses"), ULensJSONImporter::ParseFocalRange(TEXT("50mm"), Min, Max));
	TestTrue(TEXT("Single value: Min == Max == 50"), FMath::IsNearlyEqual(Min, 50.f) && FMath::IsNearlyEqual(Max, 50.f));

	TestTrue(TEXT("Ranged value parses"), ULensJSONImporter::ParseFocalRange(TEXT("14-24mm"), Min, Max));
	TestTrue(TEXT("Ranged value: Min == 14, Max == 24"), FMath::IsNearlyEqual(Min, 14.f) && FMath::IsNearlyEqual(Max, 24.f));

	TestFalse(TEXT("Malformed value fails"), ULensJSONImporter::ParseFocalRange(TEXT("not-a-number"), Min, Max));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportParseFstopRangeTest, "PhotoSim.LensImport.ParseFstopRange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportParseFstopRangeTest::RunTest(const FString& Parameters)
{
	float WideAtMin = 0.f, WideAtMax = 0.f;

	TestTrue(TEXT("Single value parses"), ULensJSONImporter::ParseFstopRange(TEXT("f/1.4"), WideAtMin, WideAtMax));
	TestTrue(TEXT("Single value: both ends == 1.4"), FMath::IsNearlyEqual(WideAtMin, 1.4f) && FMath::IsNearlyEqual(WideAtMax, 1.4f));

	TestTrue(TEXT("Ranged value parses"), ULensJSONImporter::ParseFstopRange(TEXT("f/4.5-5.6"), WideAtMin, WideAtMax));
	TestTrue(TEXT("Ranged value: WideAtMin == 4.5, WideAtMax == 5.6"), FMath::IsNearlyEqual(WideAtMin, 4.5f) && FMath::IsNearlyEqual(WideAtMax, 5.6f));

	TestFalse(TEXT("Malformed value fails"), ULensJSONImporter::ParseFstopRange(TEXT("nope"), WideAtMin, WideAtMax));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportDistinctNamesTest, "PhotoSim.LensImport.DistinctNamesForDuplicateFocalRanges", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportDistinctNamesTest::RunTest(const FString& Parameters)
{
	TArray<FLensDefinition> Lenses;
	TArray<FString> Warnings;
	ULensJSONImporter::ImportLensesFromString(LensImportTestData::FullCatalogJSON, Lenses, Warnings);

	TArray<FString> HundredToFourHundredNames;
	for (const FLensDefinition& Lens : Lenses)
	{
		if (FMath::IsNearlyEqual(Lens.MinFocalLengthMM, 100.f) && FMath::IsNearlyEqual(Lens.MaxFocalLengthMM, 400.f))
		{
			HundredToFourHundredNames.Add(Lens.Name.ToString());
		}
	}

	TestEqual(TEXT("Two 100-400mm entries found"), HundredToFourHundredNames.Num(), 2);
	if (HundredToFourHundredNames.Num() == 2)
	{
		TestNotEqual(TEXT("The two 100-400mm entries have distinct names"), HundredToFourHundredNames[0], HundredToFourHundredNames[1]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportPrimeLensTest, "PhotoSim.LensImport.PrimeLensHasEqualFocalBoundsAndIsPrime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportPrimeLensTest::RunTest(const FString& Parameters)
{
	TArray<FLensDefinition> Lenses;
	TArray<FString> Warnings;
	ULensJSONImporter::ImportLensesFromString(LensImportTestData::FullCatalogJSON, Lenses, Warnings);

	TestTrue(TEXT("At least one lens imported"), Lenses.Num() > 0);
	if (Lenses.Num() > 0)
	{
		const FLensDefinition& FourteenMM = Lenses[0];
		TestTrue(TEXT("14mm: MinFocalLengthMM == MaxFocalLengthMM"), FMath::IsNearlyEqual(FourteenMM.MinFocalLengthMM, FourteenMM.MaxFocalLengthMM));
		TestTrue(TEXT("14mm: IsPrime() == true"), FourteenMM.IsPrime());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportUnitConversionTest, "PhotoSim.LensImport.UnitConversions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportUnitConversionTest::RunTest(const FString& Parameters)
{
	TArray<FLensDefinition> Lenses;
	TArray<FString> Warnings;
	ULensJSONImporter::ImportLensesFromString(LensImportTestData::FullCatalogJSON, Lenses, Warnings);

	TestEqual(TEXT("15 rows imported"), Lenses.Num(), 15);
	if (Lenses.Num() == 15)
	{
		// IsNearlyEqual's default tolerance (SMALL_NUMBER, 1e-8) is tighter than the float rounding
		// a *100 / /1000 conversion can legitimately introduce (e.g. 0.30*100 lands on 30.000002,
		// not exactly 30.0) - an explicit, still-exacting 0.001 tolerance is the correct fix here,
		// not chasing bit-exact equality through a decimal-to-binary-float conversion.
		constexpr float Tolerance = 0.001f;

		// Row 0: 14mm, focusDistance 0.30m -> 30cm, weight 1170g -> 1.17kg.
		TestTrue(TEXT("14mm MinFocusDistanceCM == 30"), FMath::IsNearlyEqual(Lenses[0].MinFocusDistanceCM, 30.f, Tolerance));
		TestTrue(TEXT("14mm WeightKG == 1.17"), FMath::IsNearlyEqual(Lenses[0].WeightKG, 1.17f, Tolerance));

		// Row 8: 70-200mm, focusDistance 0.96m -> 96cm, weight 1480g -> 1.48kg.
		TestTrue(TEXT("70-200mm MinFocusDistanceCM == 96"), FMath::IsNearlyEqual(Lenses[8].MinFocusDistanceCM, 96.f, Tolerance));
		TestTrue(TEXT("70-200mm WeightKG == 1.48"), FMath::IsNearlyEqual(Lenses[8].WeightKG, 1.48f, Tolerance));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportMalformedRowTest, "PhotoSim.LensImport.MalformedRowSkippedWithoutAbortingImport", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportMalformedRowTest::RunTest(const FString& Parameters)
{
	TArray<FLensDefinition> Lenses;
	TArray<FString> Warnings;
	const bool bSuccess = ULensJSONImporter::ImportLensesFromString(LensImportTestData::MalformedRowJSON, Lenses, Warnings);

	TestTrue(TEXT("Import still reports success overall"), bSuccess);
	TestEqual(TEXT("Only the valid row is imported"), Lenses.Num(), 1);
	TestTrue(TEXT("A warning was recorded for the bad row"), Warnings.Num() >= 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLensImportPrimeMismatchWarningTest, "PhotoSim.LensImport.PrimeFlagMismatchWarns", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FLensImportPrimeMismatchWarningTest::RunTest(const FString& Parameters)
{
	TArray<FLensDefinition> Lenses;
	TArray<FString> Warnings;
	const bool bSuccess = ULensJSONImporter::ImportLensesFromString(LensImportTestData::PrimeMismatchJSON, Lenses, Warnings);

	TestTrue(TEXT("Import reports success"), bSuccess);
	TestEqual(TEXT("Row still imported despite the mismatch"), Lenses.Num(), 1);
	TestTrue(TEXT("A mismatch warning was recorded"), Warnings.Num() >= 1);
	if (Lenses.Num() == 1)
	{
		TestFalse(TEXT("Parsed value correctly treats 24-70mm as a zoom"), Lenses[0].IsPrime());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
