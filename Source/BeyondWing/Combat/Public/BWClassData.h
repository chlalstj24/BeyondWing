#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "BWClassData.generated.h"

class UTexture2D;

// 삶의 방식(직업) 한 줄. 데이터 테이블 /Game/HUD/Data/DT_Classes 의 행 구조체
// 행 이름은 PHA / TEC / INT / HP / FTH. 선택 화면의 카드는 테이블의 행 순서대로 놓인다 (최대 5개)
USTRUCT(BlueprintType)
struct FBWClassData : public FTableRowBase
{
	GENERATED_BODY()

	// 이 행이 어떤 삶의 방식인지 (게임 규칙이 이 값으로 행을 찾는다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	EClassType ClassType = EClassType::PHA;

	// false면 선택 화면에서 막히고 LockedText가 보인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	bool bSelectable = true;

	// ── 선택 화면 문구 ──
	// 카드 맨 위 큰 글자 (예: PHA)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText Title;

	// 이름 (예: 물리). 소개 줄 앞과 '선택: PHA · 물리'에 쓰인다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText DisplayName;

	// 이름 옆 한 줄 소개 (예: 값이 큰 한 방)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText Tagline;

	// 카드 수치 칸의 방어 줄 (예: 방어 · 받는 피해 -2). 실제 방어 수치는 아래 Combat에서 바꾼다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText DefenseText;

	// 카드 수치 칸의 자원 줄 (예: MP 6 · 턴마다 +3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText ResourceText;

	// 비워 두면 '주 능력치 이름 + 값'(예: PHA 8)을 자동으로 쓴다. 적으면 그 줄을 이 글로 바꾼다 (예: 공격이 최대 HP 비례)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText MainStatText;

	// 카드를 펼쳤을 때 나오는 긴 설명. 줄바꿈은 Shift+Enter
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구", meta = (MultiLine = true))
	FText Description;

	// 고를 수 없을 때 카드에 나오는 글 (예: 준비 중)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 문구")
	FText LockedText;

	// ── 선택 화면 모양 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양")
	FLinearColor Color = FLinearColor::White;

	// 밝은 색 카드(체력 회백색)는 고르면 글씨를 검게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양")
	bool bDarkText = false;

	// 일러스트. 비워 두면 /Game/HUD/Texture/ClassArt/T_ClassArt_<행 이름> 을 찾는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양")
	TSoftObjectPtr<UTexture2D> Illustration;

	// 능력 막대 (0~5칸, 화면에 보이는 느낌값일 뿐 규칙에는 영향 없음). 넷 다 0이면 막대를 숨긴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양", meta = (ClampMin = "0", ClampMax = "5"))
	int32 RatingFirepower = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양", meta = (ClampMin = "0", ClampMax = "5"))
	int32 RatingSurvival = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양", meta = (ClampMin = "0", ClampMax = "5"))
	int32 RatingMobility = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 모양", meta = (ClampMin = "0", ClampMax = "5"))
	int32 RatingGrowth = 0;

	// ── 규칙 수치 ──
	// 새 판 시작 능력치와 최대 HP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 수치")
	FBWClassStartStats StartStats;

	// 턴마다 굴리는 속도 범위
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 수치")
	FBWDiceRange Speed = FBWDiceRange(3, 6);

	// 방어 방식·수치, MP(시작·턴마다·최대), 방어도(전투 시작·턴 종료마다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 수치")
	FBWClassCombatProfile Combat;

	// ── 스킬 ──
	// 시작 덱: 카드 데이터 테이블(BeyondWing_CardDatabase_Template)의 행 이름. 같은 카드를 여러 번 넣으면 여러 장
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 스킬")
	TArray<FName> StartingDeck;

	// 기본 공격: 카드 없이 언제나 쓸 수 있다. 숫자 범위(SkillMin~SkillMax)와 능력치 계수(PHAScale 등)로 세기를 정한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 스킬")
	FBWCardData BasicStrike;

	// 공격카드로 강화할 때 카드 몫에 곱하는 배율. 강화한 공격 = 기본 공격 + 카드 숫자 범위 × 이 값
	// (기본 공격이 센 PHA·TEC는 작게, 기본 공격이 약한 INT·FTH는 크게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 스킬", meta = (ClampMin = "0.0"))
	float CardPower = 1.0f;
};
