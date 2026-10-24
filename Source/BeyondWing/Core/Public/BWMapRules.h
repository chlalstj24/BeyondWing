#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWMapTypes.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWMapRules.generated.h"

// 방치 보너스 하나를 받을 때의 값 (내성 반영 후). 지도 기획서 6장·7-2
USTRUCT(BlueprintType)
struct FBWNeglectBonus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BW Map") EDimensionType Dimension = EDimensionType::None;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 Percent = 100;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bDeep = false;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 Gold = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 UpgradedCardChoices = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 NormalCardChoices = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") float VictoryHealFraction = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") float NextBattleDamageBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CardRemovals = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 EquipmentGrade = 0;
};

// 지도 규칙 (지도 기획서 2~7장). 상태가 없고, 무작위는 넘겨받은 스트림에서 뽑는다
UCLASS()
class BEYONDWING_API UBWMapRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 NumActs = 3;
	static constexpr int32 NumRows = 4;
	static constexpr int32 StepsPerAct = 15;          // 15칸 + 보스 칸
	static constexpr int32 BossStep = StepsPerAct;     // 보스 칸 번호 (15)
	static constexpr int32 FirstBattleStep = 0;        // 1칸: 전 줄 일반 전투
	static constexpr int32 TreasureStep = 7;           // 8칸: 전 줄 보물
	static constexpr int32 RestStep = 14;              // 15칸: 전 줄 휴식
	static constexpr float GateOpenChance = 0.7f;      // 차원문 70% 열림
	static constexpr int32 MaxClosedInARow = 2;        // 같은 경계에서 3칸 연속 닫힘 금지
	static constexpr int32 MinRiftsPerAct = 1;
	static constexpr int32 MaxRiftsPerAct = 2;
	static constexpr int32 DeepNeglectGold = 50;
	static constexpr int32 OverloadIStart = 4;         // 연속 체류 4~5턴
	static constexpr int32 OverloadIIStart = 6;        // 6턴 이상 (상한)

	// 한 막의 지도: 0~14칸은 줄마다 노드 4개, 15칸은 보스 하나. RowDimensions[r] = r줄의 차원
	static TArray<FMapStep> GenerateAct(const TArray<EDimensionType>& RowDimensions, FRandomStream& Stream);

	// 1막은 파트로바스·베렌드·겔레온·타이가 순서, 2·3막은 섞는다(bShuffle)
	static TArray<EDimensionType> MakeRowOrder(int32 Act, bool bShuffleAfterFirstAct, FRandomStream& Stream);

	// 고정 칸(1·8·15칸)이면 그 노드 종류를 돌려준다
	static bool GetFixedNodeType(int32 Step, EMapNodeType& OutType);

	// 고정 칸이 아닌 칸의 노드 종류를 비중(4장) + 차원 성향(5장)으로 뽑는다
	static EMapNodeType PickNodeType(EDimensionType Dimension, FRandomStream& Stream);

	// 노드 비중 (합 100). 차원 성향이 반영된다
	static void GetNodeWeights(EDimensionType Dimension, TArray<TPair<EMapNodeType, int32>>& OutWeights);

	// FromStep의 r줄과 r+1줄 사이 차원문이 열렸는지
	static bool IsGateOpen(const TArray<FMapStep>& Map, int32 FromStep, int32 RowA, int32 RowB);

	// (FromStep, FromRow) → (FromStep+1, ToRow)로 갈 수 있는지. FromStep -1이면 시작 줄 고르기(아무 줄이나).
	// 직진 / 열린 차원문으로 위·아래 한 줄 / 균열을 밟았으면 두 줄 점프(차원문과 무관). 보스 칸은 어느 줄에서나
	static bool CanMove(const TArray<FMapStep>& Map, int32 FromStep, int32 FromRow, int32 ToStep, int32 ToRow, bool bRiftJumpReady);

	// 방치 조건: 타이가 2, 나머지 3. 깊은 방치는 그 2배이고 카운터는 거기서 더 쌓이지 않는다
	static int32 GetNeglectCondition(EDimensionType Dimension);
	static int32 GetNeglectCap(EDimensionType Dimension) { return GetNeglectCondition(Dimension) * 2; }

	// 연속 체류 → 0 안정 / 1 과부하 I / 2 과부하 II
	UFUNCTION(BlueprintPure, Category = "BW Map")
	static int32 GetOverloadLevel(int32 Consecutive);

	// 같은 차원 보너스를 이번 막에 몇 번째로 받는지(1부터) → 100 / 75 / 50
	UFUNCTION(BlueprintPure, Category = "BW Map")
	static int32 GetResistancePercent(int32 ClaimNumber);

	// 그 차원의 방치 보너스 (ClaimNumber = 이번 막에 몇 번째 수령인지, 1부터)
	static FBWNeglectBonus MakeNeglectBonus(EDimensionType Dimension, int32 ClaimNumber, bool bDeep);

	// 한 칸 전진할 때 카운터 갱신 (6장 1·4번): 도착한 줄은 0, 나머지는 +1(상한). 도착 전 값으로 보너스를 판정한 다음 부른다
	static void AdvanceNeglect(TMap<EDimensionType, int32>& Neglected, EDimensionType Arrived);

	// 강화 카드: 공격카드는 숫자 범위 최소 +1·최대 +2, 효과카드는 수치 +1(회복·방어도 +3), 이름 뒤에 +
	UFUNCTION(BlueprintPure, Category = "BW Map")
	static FBWCardData MakeUpgradedCard(const FBWCardData& Card);

	UFUNCTION(BlueprintPure, Category = "BW Map")
	static FText GetNodeTypeName(EMapNodeType Type);

	UFUNCTION(BlueprintPure, Category = "BW Map")
	static FText GetDimensionName(EDimensionType Dimension);
};
