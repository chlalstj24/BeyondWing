#include "Misc/AutomationTest.h"
#include "BeyondWing/Combat/Public/BWTerrainRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWTerrainTest
{
	// 월드가 필요 없는 빠른 순수 함수 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWTerrainLayoutTest, "BeyondWing.Combat.Terrain.Layout", BWTerrainTest::Flags)
bool FBWTerrainLayoutTest::RunTest(const FString& Parameters)
{
	const EDimensionType Dims[] = { EDimensionType::None, EDimensionType::Patrovas, EDimensionType::Berend, EDimensionType::Geleon, EDimensionType::Taiga };
	const TArray<FIntPoint> Enemies = { FIntPoint(5, 2), FIntPoint(6, 4), FIntPoint(4, 3) };

	for (EDimensionType Dim : Dims)
	{
		for (int32 Seed = 1; Seed <= 40; ++Seed)
		{
			FRandomStream Stream(Seed);
			const FBWTerrainLayout Layout = UBWTerrainRules::GenerateLayout(Dim, 8, 8, 4, Enemies, {}, Stream);

			TestTrue(TEXT("obstacle count"), Layout.Walls.Num() >= UBWTerrainRules::MinObstacles && Layout.Walls.Num() <= UBWTerrainRules::MaxObstacles);
			TestTrue(TEXT("open area connected"), UBWTerrainRules::IsOpenAreaConnected(8, 8, Layout.Walls));
			for (const FIntPoint& Wall : Layout.Walls)
			{
				TestFalse(TEXT("no obstacle on an enemy"), Enemies.Contains(Wall));
				TestTrue(TEXT("obstacle in the middle band"), Wall.X >= 2 && Wall.X <= 5);
			}

			const EBWTerrainType Expected = UBWTerrainRules::GetTerrainForDimension(Dim);
			if (Expected == EBWTerrainType::None)
			{
				TestEqual(TEXT("no terrain without a dimension"), Layout.Terrain.Num(), 0);
				continue;
			}
			TestTrue(TEXT("terrain count"), Layout.Terrain.Num() >= 1 && Layout.Terrain.Num() <= UBWTerrainRules::MaxTerrain);
			for (const TPair<FIntPoint, EBWTerrainType>& Cell : Layout.Terrain)
			{
				TestTrue(TEXT("terrain type matches the dimension"), Cell.Value == Expected);
				TestFalse(TEXT("terrain not on a wall"), Layout.Walls.Contains(Cell.Key));
				TestFalse(TEXT("terrain not on an enemy"), Enemies.Contains(Cell.Key));
			}
		}
	}

	// 인카운터가 벽을 정해 두면 장애물은 새로 만들지 않는다
	FRandomStream Stream(3);
	const TArray<FIntPoint> Preset = { FIntPoint(3, 3) };
	TestEqual(TEXT("preset walls kept"), UBWTerrainRules::GenerateLayout(EDimensionType::Berend, 8, 8, 4, {}, Preset, Stream).Walls, Preset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWTerrainConnectivityTest, "BeyondWing.Combat.Terrain.Connectivity", BWTerrainTest::Flags)
bool FBWTerrainConnectivityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("empty board"), UBWTerrainRules::IsOpenAreaConnected(4, 4, {}));
	// 한 열을 통째로 막으면 판이 둘로 갈린다
	TestFalse(TEXT("wall column splits"), UBWTerrainRules::IsOpenAreaConnected(4, 4, { FIntPoint(2, 0), FIntPoint(2, 1), FIntPoint(2, 2), FIntPoint(2, 3) }));
	TestTrue(TEXT("gap keeps it connected"), UBWTerrainRules::IsOpenAreaConnected(4, 4, { FIntPoint(2, 0), FIntPoint(2, 1), FIntPoint(2, 3) }));
	return true;
}

#endif
