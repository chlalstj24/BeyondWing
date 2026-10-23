#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWAugmentRules.generated.h"

// 가진 증강을 모두 합친 전투 수치 (전투 기획서 10장)
USTRUCT(BlueprintType)
struct FBWAugmentTotals
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 SpeedMaxBonus = 0;        // 질주
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 SpeedMinBonus = 0;        // 신중한 발걸음
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 DuelRerollsPerBattle = 0; // 재굴림
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 StartBlockBonus = 0;      // 단단한 껍질
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 HandSizeBonus = 0;        // 넉넉한 손
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 EffectLimitBonus = 0;     // 연계
	UPROPERTY(BlueprintReadOnly, Category = "Augment") float AttackMaxMultiplier = 1.0f; // 중량타 (PHA)
	UPROPERTY(BlueprintReadOnly, Category = "Augment") float CritChanceBonus = 0.0f;   // 급소 (TEC)
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 MPPerTurnBonus = 0;       // 마나 샘 (INT)
	UPROPERTY(BlueprintReadOnly, Category = "Augment") float ThornFraction = 0.0f;     // 가시 갑옷 (HP)
	UPROPERTY(BlueprintReadOnly, Category = "Augment") bool bReversedHymn = false;     // 역전된 성가 (FTH) - 효과카드 목록이 정해지면 적용
	UPROPERTY(BlueprintReadOnly, Category = "Augment") bool bSelfBlessing = false;     // 자기 축복 (FTH) - 효과카드 목록이 정해지면 적용
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 DuelLimitBonus = 0;       // 이중 개시: 내가 여는 합 +1
	UPROPERTY(BlueprintReadOnly, Category = "Augment") int32 AttackCardLimitBonus = 0; // 이중 개시: 공격카드 한도 +1
};

// 증강 규칙. 상태가 없고, 무작위는 넘겨받은 스트림에서 뽑는다
UCLASS()
class BEYONDWING_API UBWAugmentRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 ChoicesPerPick = 3;
	static constexpr int32 RerollsPerPick = 1;

	UFUNCTION(BlueprintPure, Category = "BW Augment")
	static FBWAugmentTotals Aggregate(const TArray<FBWAugmentData>& Augments);

	// 이번 막에 나올 수 있는 증강인지: 막 범위 안, 공용이거나 내 직업 전용, 아직 가지지 않음
	UFUNCTION(BlueprintPure, Category = "BW Augment")
	static bool IsEligible(const FBWAugmentData& Augment, FName RowName, int32 Act, EClassType PlayerClass, const TArray<FName>& Owned);

	// 후보 Count개를 겹치지 않게 뽑는다. Exclude(리롤 전 후보)는 가능하면 피하고, 모자라면 다시 넣는다
	UFUNCTION(BlueprintCallable, Category = "BW Augment")
	static TArray<FName> PickCandidates(const TArray<FName>& RowNames, const TArray<FBWAugmentData>& Rows, int32 Act, EClassType PlayerClass,
		const TArray<FName>& Owned, const TArray<FName>& Exclude, int32 Count, UPARAM(ref) FRandomStream& Stream);
};
