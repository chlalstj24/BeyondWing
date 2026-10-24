#include "Misc/AutomationTest.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWMapTest
{
	// 월드가 필요 없는 빠른 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;

	TArray<EDimensionType> DefaultRows()
	{
		FRandomStream Unused(1);
		return UBWMapRules::MakeRowOrder(1, true, Unused);
	}

	// 차원문이 모두 열린 지도 (이동 규칙 테스트용)
	TArray<FMapStep> MakeOpenMap()
	{
		FRandomStream Stream(7);
		TArray<FMapStep> Map = UBWMapRules::GenerateAct(DefaultRows(), Stream);
		for (FMapStep& Step : Map)
		{
			for (int32 i = 0; i < Step.GateOpenBelow.Num(); ++i) Step.GateOpenBelow[i] = true;
		}
		return Map;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWMapGenerateTest, "BeyondWing.Map.Generate", BWMapTest::Flags)
bool FBWMapGenerateTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = 1; Seed <= 60; ++Seed)
	{
		FRandomStream Stream(Seed);
		const TArray<FMapStep> Map = UBWMapRules::GenerateAct(BWMapTest::DefaultRows(), Stream);
		if (!TestEqual(TEXT("15 steps + boss"), Map.Num(), UBWMapRules::BossStep + 1)) return false;
		TestEqual(TEXT("boss step has one node"), Map[UBWMapRules::BossStep].Nodes.Num(), 1);
		TestTrue(TEXT("boss node"), Map[UBWMapRules::BossStep].Nodes[0].NodeType == EMapNodeType::Boss);

		int32 Rifts = 0;
		for (int32 Step = 0; Step < UBWMapRules::StepsPerAct; ++Step)
		{
			TestEqual(TEXT("4 rows"), Map[Step].Nodes.Num(), 4);
			EMapNodeType Fixed;
			const bool bFixed = UBWMapRules::GetFixedNodeType(Step, Fixed);
			for (const FMapNode& Node : Map[Step].Nodes)
			{
				if (bFixed) TestTrue(TEXT("fixed step same type on every row"), Node.NodeType == Fixed);
				TestTrue(TEXT("no boss outside the boss step"), Node.NodeType != EMapNodeType::Boss);
				TestEqual(TEXT("step index"), Node.StepIdx, Step);
				Rifts += Node.NodeType == EMapNodeType::Rift ? 1 : 0;
			}
		}
		TestTrue(TEXT("1~2 rifts per act"), Rifts >= UBWMapRules::MinRiftsPerAct && Rifts <= UBWMapRules::MaxRiftsPerAct);

		// 같은 경계에서 3칸 연속 닫힘 없음
		for (int32 Boundary = 0; Boundary < 3; ++Boundary)
		{
			int32 ClosedRun = 0;
			for (int32 Step = 0; Step < UBWMapRules::StepsPerAct - 1; ++Step)
			{
				if (!TestEqual(TEXT("3 gates per step"), Map[Step].GateOpenBelow.Num(), 3)) return false;
				ClosedRun = Map[Step].GateOpenBelow[Boundary] ? 0 : ClosedRun + 1;
				TestTrue(TEXT("never 3 closed in a row"), ClosedRun <= UBWMapRules::MaxClosedInARow);
			}
		}
	}

	// 1막 줄 순서 고정, 2막은 네 차원이 모두 한 번씩
	FRandomStream Stream(3);
	const TArray<EDimensionType> Act1 = UBWMapRules::MakeRowOrder(1, true, Stream);
	TestTrue(TEXT("act 1 order fixed"), Act1[0] == EDimensionType::Patrovas && Act1[1] == EDimensionType::Berend && Act1[2] == EDimensionType::Geleon && Act1[3] == EDimensionType::Taiga);
	const TArray<EDimensionType> Act2 = UBWMapRules::MakeRowOrder(2, true, Stream);
	for (EDimensionType Dim : Act1) TestTrue(TEXT("act 2 keeps every dimension"), Act2.Contains(Dim));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWMapMoveTest, "BeyondWing.Map.Move", BWMapTest::Flags)
bool FBWMapMoveTest::RunTest(const FString& Parameters)
{
	TArray<FMapStep> Map = BWMapTest::MakeOpenMap();
	TestTrue(TEXT("start: any row"), UBWMapRules::CanMove(Map, -1, -1, 0, 3, false));
	TestFalse(TEXT("start: only step 0"), UBWMapRules::CanMove(Map, -1, -1, 1, 0, false));
	TestTrue(TEXT("straight"), UBWMapRules::CanMove(Map, 2, 1, 3, 1, false));
	TestTrue(TEXT("open gate down"), UBWMapRules::CanMove(Map, 2, 1, 3, 2, false));
	TestTrue(TEXT("open gate up"), UBWMapRules::CanMove(Map, 2, 1, 3, 0, false));
	TestFalse(TEXT("two rows without rift"), UBWMapRules::CanMove(Map, 2, 1, 3, 3, false));
	TestTrue(TEXT("two rows with rift"), UBWMapRules::CanMove(Map, 2, 1, 3, 3, true));
	TestFalse(TEXT("must advance one step"), UBWMapRules::CanMove(Map, 2, 1, 4, 1, false));

	Map[2].GateOpenBelow[1] = false; // 1줄·2줄 사이 닫힘
	TestFalse(TEXT("closed gate"), UBWMapRules::CanMove(Map, 2, 1, 3, 2, false));
	TestFalse(TEXT("closed gate both ways"), UBWMapRules::CanMove(Map, 2, 2, 3, 1, false));
	TestTrue(TEXT("other boundary still open"), UBWMapRules::CanMove(Map, 2, 1, 3, 0, false));

	for (int32 Row = 0; Row < 4; ++Row) TestTrue(TEXT("boss from every row"), UBWMapRules::CanMove(Map, UBWMapRules::RestStep, Row, UBWMapRules::BossStep, 0, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWMapPenaltyTest, "BeyondWing.Map.NeglectOverloadResistance", BWMapTest::Flags)
bool FBWMapPenaltyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("taiga condition"), UBWMapRules::GetNeglectCondition(EDimensionType::Taiga), 2);
	TestEqual(TEXT("berend condition"), UBWMapRules::GetNeglectCondition(EDimensionType::Berend), 3);
	TestEqual(TEXT("stable"), UBWMapRules::GetOverloadLevel(3), 0);
	TestEqual(TEXT("overload I"), UBWMapRules::GetOverloadLevel(4), 1);
	TestEqual(TEXT("overload I (5)"), UBWMapRules::GetOverloadLevel(5), 1);
	TestEqual(TEXT("overload II cap"), UBWMapRules::GetOverloadLevel(9), 2);
	TestEqual(TEXT("resistance 1"), UBWMapRules::GetResistancePercent(1), 100);
	TestEqual(TEXT("resistance 2"), UBWMapRules::GetResistancePercent(2), 75);
	TestEqual(TEXT("resistance floor"), UBWMapRules::GetResistancePercent(5), 50);

	// 7-2 표
	TestEqual(TEXT("patrovas 1st: 3 upgraded"), UBWMapRules::MakeNeglectBonus(EDimensionType::Patrovas, 1, false).UpgradedCardChoices, 3);
	const FBWNeglectBonus Patrovas3 = UBWMapRules::MakeNeglectBonus(EDimensionType::Patrovas, 3, false);
	TestTrue(TEXT("patrovas 3rd: 1 + 1"), Patrovas3.UpgradedCardChoices == 1 && Patrovas3.NormalCardChoices == 1);
	TestEqual(TEXT("berend 2nd 15%"), UBWMapRules::MakeNeglectBonus(EDimensionType::Berend, 2, false).VictoryHealFraction, 0.15f);
	TestEqual(TEXT("geleon 3rd +25%"), UBWMapRules::MakeNeglectBonus(EDimensionType::Geleon, 3, false).NextBattleDamageBonus, 0.25f);
	const FBWNeglectBonus Taiga3 = UBWMapRules::MakeNeglectBonus(EDimensionType::Taiga, 3, true);
	TestTrue(TEXT("taiga 3rd: 1 removal + rare, deep gold"), Taiga3.CardRemovals == 1 && Taiga3.EquipmentGrade == 1 && Taiga3.Gold == UBWMapRules::DeepNeglectGold);

	// 방치 카운터: 도착한 줄 0, 나머지 +1, 깊은 방치(조건 2배)에서 멈춤
	TMap<EDimensionType, int32> Neglect = { { EDimensionType::Patrovas, 0 }, { EDimensionType::Taiga, 3 } };
	UBWMapRules::AdvanceNeglect(Neglect, EDimensionType::Patrovas);
	TestEqual(TEXT("arrived row reset"), Neglect[EDimensionType::Patrovas], 0);
	TestEqual(TEXT("taiga +1"), Neglect[EDimensionType::Taiga], 4);
	UBWMapRules::AdvanceNeglect(Neglect, EDimensionType::Patrovas);
	TestEqual(TEXT("taiga capped at 4"), Neglect[EDimensionType::Taiga], 4);

	FBWCardData Card;
	Card.CardName = TEXT("화염파");
	Card.SkillMin = 3;
	Card.SkillMax = 6;
	const FBWCardData Up = UBWMapRules::MakeUpgradedCard(Card);
	TestTrue(TEXT("upgraded attack range"), Up.SkillMin == 4 && Up.SkillMax == 8 && Up.CardName == TEXT("화염파+"));
	TestEqual(TEXT("upgrade twice does nothing"), UBWMapRules::MakeUpgradedCard(Up).SkillMax, 8);
	return true;
}

// 지도 기획서 8장 예시 런: 베렌드 → 파트로바스 → 베렌드 → 겔레온 → 타이가
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWMapExampleRunTest, "BeyondWing.Map.ExampleRun", BWMapTest::Flags)
bool FBWMapExampleRunTest::RunTest(const FString& Parameters)
{
	if (!GEngine) { AddInfo(TEXT("Skipped: engine not initialized (startup smoke run)")); return true; }

	UBWGameInstance* GI = NewObject<UBWGameInstance>(GetTransientPackage());
	GI->CurrentStage = 1;
	GI->GenerateWorldMap();
	for (FMapStep& Step : GI->WorldMapData)
	{
		for (int32 i = 0; i < Step.GateOpenBelow.Num(); ++i) Step.GateOpenBelow[i] = true;
		for (FMapNode& Node : Step.Nodes) if (Node.NodeType == EMapNodeType::Rift) Node.NodeType = EMapNodeType::Battle;
	}

	// 줄: 0 파트로바스, 1 베렌드, 2 겔레온, 3 타이가
	const int32 Path[] = { 1, 1, 0, 0, 1, 2, 2, 3 };
	const int32 ExpectedNeglect[] = { 0, 0, 2, 0, 2, 5, 0, 4 }; // 타이가는 7턴 비웠지만 깊은 방치(4)에서 멈춘다
	const bool ExpectedBonus[] = { false, false, false, false, false, true, false, true };
	for (int32 Turn = 0; Turn < UE_ARRAY_COUNT(Path); ++Turn)
	{
		const int32 GoldBefore = GI->CurrentGold;
		if (!TestTrue(FString::Printf(TEXT("turn %d move"), Turn + 1), GI->SelectMapNode(Turn, Path[Turn]))) return false;
		const FBWArrivalReport& R = GI->LastArrival;
		if (R.bChangedRow || Turn == 0) TestEqual(FString::Printf(TEXT("turn %d neglect on arrival"), Turn + 1), R.NeglectCount, ExpectedNeglect[Turn]);
		TestEqual(FString::Printf(TEXT("turn %d bonus"), Turn + 1), R.bClaimedBonus, ExpectedBonus[Turn]);
		if (Turn == 7) TestEqual(TEXT("deep neglect gold"), GI->CurrentGold - GoldBefore, UBWMapRules::DeepNeglectGold);
	}
	TestEqual(TEXT("geleon bonus used? no battle yet -> still pending"), GI->MapBonus.NextBattleDamageBonus, 0.5f);
	TestEqual(TEXT("taiga removals pending"), GI->PendingCardRemovals, 2);
	TestEqual(TEXT("taiga legendary"), GI->MapBonus.EquipmentGrade, 2);

	// 같은 줄에 계속: 4턴째 과부하 I, 6턴째 II
	GI->SelectMapNode(8, 3);
	GI->SelectMapNode(9, 3);
	TestEqual(TEXT("taiga consecutive 3 stable"), GI->GetCurrentOverloadLevel(), 0);
	GI->SelectMapNode(10, 3);
	TestEqual(TEXT("overload I at 4"), GI->GetCurrentOverloadLevel(), 1);
	TestTrue(TEXT("overload change reported"), GI->LastArrival.bOverloadChanged);
	GI->SelectMapNode(11, 3);
	GI->SelectMapNode(12, 3);
	TestEqual(TEXT("overload II at 6"), GI->GetCurrentOverloadLevel(), 2);
	GI->UseDimensionAltar();
	TestEqual(TEXT("altar resets overload"), GI->GetCurrentOverloadLevel(), 0);

	// 보스 칸: 줄은 그대로, 보너스 수령 없음
	GI->SelectMapNode(13, 2);
	GI->SelectMapNode(14, 2);
	TestTrue(TEXT("to boss"), GI->SelectMapNode(UBWMapRules::BossStep, 0));
	TestTrue(TEXT("boss keeps last row's dimension"), GI->GetCurrentDimension() == EDimensionType::Geleon);
	TestFalse(TEXT("no bonus at boss"), GI->LastArrival.bClaimedBonus);
	return true;
}

#endif
