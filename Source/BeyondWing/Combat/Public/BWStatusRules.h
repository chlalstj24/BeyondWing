#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWStatusRules.generated.h"

// 상태이상 규칙 (전투 기획서 9장). 상태가 없고, 말이 가진 상태이상 배열을 넘겨받아 다룬다
UCLASS()
class BEYONDWING_API UBWStatusRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr float VulnerableMultiplier = 1.25f; // 취약: 받는 피해 +25%
	static constexpr float WeakMaxMultiplier = 0.8f;     // 약화: 스킬 최댓값 -20%
	static constexpr int32 DefaultDuration = 2;

	// 같은 상태가 다시 걸리면 지속 턴은 새 값으로 갱신되고, 수치는 겹치지 않는다(큰 쪽 유지)
	UFUNCTION(BlueprintCallable, Category = "BW Status")
	static void ApplyStatus(UPARAM(ref) TArray<FBWStatusInstance>& Statuses, EStatusEffect Type, int32 Duration, int32 Value);

	UFUNCTION(BlueprintPure, Category = "BW Status")
	static bool HasStatus(const TArray<FBWStatusInstance>& Statuses, EStatusEffect Type);

	UFUNCTION(BlueprintPure, Category = "BW Status")
	static int32 GetStatusValue(const TArray<FBWStatusInstance>& Statuses, EStatusEffect Type);

	// 걸린 말의 자기 턴이 끝날 때: 출혈 피해량을 돌려주고, 모든 상태의 남은 턴을 1 줄여 0이 되면 지운다
	UFUNCTION(BlueprintCallable, Category = "BW Status")
	static int32 TickEndOfOwnTurn(UPARAM(ref) TArray<FBWStatusInstance>& Statuses);

	// 둔화: 속도 최댓값 -1 (최솟값 아래로는 내려가지 않음)
	UFUNCTION(BlueprintPure, Category = "BW Status")
	static FBWDiceRange ApplySlow(const FBWDiceRange& SpeedDice);

	// 약화: 스킬 숫자 범위 최댓값 × 0.8 (내림), 최솟값은 최댓값 이하
	UFUNCTION(BlueprintPure, Category = "BW Status")
	static FBWDiceRange ApplyWeak(const FBWDiceRange& SkillRange);

	// HUD용 짧은 이름: "기절 1, 출혈(3) 2"
	UFUNCTION(BlueprintPure, Category = "BW Status")
	static FString DescribeStatuses(const TArray<FBWStatusInstance>& Statuses);
};
