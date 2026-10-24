#pragma once

#include "CoreMinimal.h"
#include "BWMapTypes.generated.h"

// 4개 국가(차원) 정의
UENUM(BlueprintType)
enum class EDimensionType : uint8
{
	None,
	Patrovas, Berend, Geleon, Taiga
};

// 노드 타입 정의 (지도 기획서 4장). 저장 파일·데이터 테이블 값이 바뀌지 않게 새 값은 뒤에 붙인다
UENUM(BlueprintType)
enum class EMapNodeType : uint8
{
	Battle, Elite, Mystery, Rest, Shop, Chest, Boss,
	DimensionAltar, // [추가] 차원 제단: 과부하·보너스 내성 초기화
	Rift            // [추가] 균열: 다음 칸에서 2줄 점프 가능
};

USTRUCT(BlueprintType)
struct FMapNode
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EMapNodeType NodeType = EMapNodeType::Battle;
	UPROPERTY(BlueprintReadOnly) EDimensionType Dimension = EDimensionType::None;
	UPROPERTY(BlueprintReadOnly) bool bIsVisited = false;
	UPROPERTY(BlueprintReadOnly) int32 StepIdx = 0; // 몇 번째 칸 (0부터, 막마다 0~14 + 보스 15)
	UPROPERTY(BlueprintReadOnly) int32 DimIdx = 0;  // 몇 번째 줄 (위에서 0~3, 보스 칸은 0 하나)
};

USTRUCT(BlueprintType)
struct FMapStep
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FMapNode> Nodes;

	// [추가] 이 칸에서 다음 칸으로 갈 때 줄 경계의 차원문이 열렸는지. [r] = r줄과 r+1줄 사이 (3개, 마지막 일반 칸과 보스 칸은 비어 있음)
	UPROPERTY(BlueprintReadOnly) TArray<bool> GateOpenBelow;
};

UENUM(BlueprintType)
enum class ERewardType : uint8
{
	Gold, CardSelection, Relic, CardRemoval, Heal, Equipment,
	Buff // [추가] 겔레온 방치 보너스처럼 다음 전투에 붙는 효과
};

USTRUCT(BlueprintType)
struct FRewardItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ERewardType Type = ERewardType::Gold;
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) FString RewardName = "";
	UPROPERTY(BlueprintReadOnly) FString Description = "";

	// [추가] 카드 보상: 고를 수 있는 카드(카드 표 행 이름)와 그중 앞에서 몇 장이 강화 카드인지
	UPROPERTY(BlueprintReadOnly) TArray<FName> CardChoices;
	UPROPERTY(BlueprintReadOnly) int32 UpgradedChoices = 0;
};

// [추가] 판 동안 이어지는 덱의 카드 한 장 (카드 표 행 이름 + 강화 여부)
USTRUCT(BlueprintType)
struct FBWRunCard
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Deck") FName Row;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Deck") bool bUpgraded = false;

	FBWRunCard() {}
	FBWRunCard(FName InRow, bool bInUpgraded = false) : Row(InRow), bUpgraded(bInUpgraded) {}
};

// [추가] 방치 보너스를 받아 두고 다음 기회에 쓰는 효과 (지도 기획서 6장, 내성 7-2 반영 후 값)
USTRUCT(BlueprintType)
struct FBWMapBonusState
{
	GENERATED_BODY()

	// 파트로바스: 다음 카드 보상의 강화 카드 수 / 함께 나오는 일반 카드 수 (둘 다 0이면 없음)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 UpgradedCardChoices = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 NormalCardChoices = 0;
	// 베렌드: 다음 전투에서 이기면 최대 HP의 이 비율만큼 회복 (0.2 = 20%)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") float VictoryHealFraction = 0.0f;
	// 겔레온: 다음 전투 1회 피해량 + (0.5 = +50%, 겔레온 기믹과 합산)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") float NextBattleDamageBonus = 0.0f;
	// 타이가: 다음 전투 보상의 장비 등급 (0 없음, 1 희귀 이상, 2 전설)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 EquipmentGrade = 0;

	bool HasAny() const { return UpgradedCardChoices + NormalCardChoices > 0 || VictoryHealFraction > 0.0f || NextBattleDamageBonus > 0.0f || EquipmentGrade > 0; }
};

// [추가] 칸에 도착했을 때 일어난 일 (지도 화면이 알림으로 보여 준다). 순서: 차원문 → 방치 보너스 → 과부하 → 노드
USTRUCT(BlueprintType)
struct FBWArrivalReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bChangedRow = false;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") EDimensionType Dimension = EDimensionType::None;

	// 방치 보너스
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bClaimedBonus = false;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bDeepNeglect = false;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 NeglectCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 BonusPercent = 100; // 보너스 내성 반영 (100/75/50)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 GoldGained = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CardRemovals = 0;   // 타이가: 지금 지울 카드 수
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") TArray<FString> BonusLines;

	// 과부하 (0 안정, 1 과부하 I, 2 과부하 II)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 Consecutive = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 OverloadLevel = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bOverloadChanged = false;
};
