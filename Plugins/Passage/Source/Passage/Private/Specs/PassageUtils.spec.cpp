#include "PassageUtils.h"

DEFINE_SPEC(FPassageUtilsSpec, "Passage.PassageUtils",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
void FPassageUtilsSpec::Define()
{
	Describe("GetBackendRoot", [this]()
		{
			It("should return a good URL", [this]()
				{
					// This only works if the below matches the hard coded default
					const auto Result = UPassageUtils::GetBackendRoot();
					TestEqual(
						TEXT("Resulting URL"),
						Result,
						TEXT("https://mm.passage3d.com/api/v1"));
				});
		});

	Describe("ParseUInt32", [this]()
		{
			It("should return expected value", [this]()
				{
					if(uint32 A; UPassageUtils::ParseUInt32("0", A))
					{
						TestEqual("Single zero", A, (int32)0);
					} else
					{
						TestFalse("Unable to parse \"0\"", true);
					}

					if (uint32 B; UPassageUtils::ParseUInt32("12345", B))
					{
						TestEqual("Five digit number", B, (int32)12345);
					}
					else
					{
						TestFalse("Unable to parse \"12345\"", true);
					}

					if (uint32 B; UPassageUtils::ParseUInt32("4294967295", B))
					{
						TestTrue("Max uint32 value", B == 4294967295u);
					}
					else
					{
						TestFalse("Unable to parse \"4294967295\"", true);
					}
				});

			It("should fail to parse invalid input", [this]()
				{
					AddExpectedError(TEXT("empty string"));
					if (uint32 Value; UPassageUtils::ParseUInt32("", Value))
					{
						TestFalse("Did not fail on empty string", true);
					}

					AddExpectedError(TEXT("Unrecognized character '-'"));
					if (uint32 Value; UPassageUtils::ParseUInt32("-10", Value))
					{
						TestFalse("Did not fail on signed integer input", true);
					}

					AddExpectedError(TEXT("Unrecognized character '\\.'"));
					if (uint32 Value; UPassageUtils::ParseUInt32("98.5", Value))
					{
						TestFalse("Did not fail on decimal fraction input", true);
					}

				});
		});
}