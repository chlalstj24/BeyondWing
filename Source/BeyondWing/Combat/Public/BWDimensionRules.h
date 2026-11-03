#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "BWDimensionRules.generated.h"

// 이번 전투에 적용되는 국가 기믹 상태 (전투 기획서 11장, 지도 기획서 5·7장)
USTRUCT(BlueprintType)
struct FBWBattlefieldState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") EDimensionType Dimension = EDimensionType::None;

	// 그 차원 과부하(연속 체류 4턴+) 중이면 기믹이 플레이어에게 불리하게 뒤틀린다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") bool bOverloaded = false;

	// 타이가 은신: 남은 '방어 무시 + 치명타' 공격 횟수 (0 또는 1) / 과부하 시 적의 방어 무시 공격 횟수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") int32 TaigaChargesLeft = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") int32 TaigaEnemyChargesLeft = 0;

	// 파트로바스: 이번 턴 마력선이 지나는 가로줄 (칸 X). 턴마다 새로 정해지고 판에 보인다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") TArray<int32> ManaRows;

	// [추가] 지도 방치 보너스(겔레온)로 이번 전투 피해량 + (0.5 = +50%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") float BonusDamage = 0.0f;
};

// 국가 기믹 규칙. 상태가 없고, 무작위는 넘겨받은 스트림에서 뽑는다
UCLASS()
class BEYONDWING_API UBWDimensionRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 TaigaOverloadEnemyHits = 1;        // 과부하: 적의 첫 공격 1회
	static constexpr int32 BerendWallDamage = 3;              // 베렌드 벽 충돌 피해 (임시)
	static constexpr int32 BerendStunTurns = 1;
	static constexpr int32 ManaLineCount = 2;                 // 파트로바스: 턴마다 마력선 2줄
	static constexpr float ManaLineMaxMultiplier = 1.4f;      // 마력선 위 말의 격돌 최댓값 ×1.4
	static constexpr int32 ManaOverloadDamage = 3;            // 과부하: 마력선 위에서 턴을 끝내면 고정 피해
	static constexpr float GeleonPerAdvance = 0.15f;          // 활공: 이번 턴 움직인 칸 1당 피해 +15%
	static constexpr float GeleonMaxAdvanceBonus = 0.6f;      // 활공 상한 +60%
	static constexpr float GeleonOverloadIncomingMultiplier = 1.2f;

	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWBattlefieldState MakeBattlefield(EDimensionType Dimension, bool bOverloaded);

	// 파트로바스 턴 시작: 마력선이 지나는 가로줄을 새로 정한다
	static void RollManaRows(FBWBattlefieldState& State, int32 RowCount, FRandomStream& Stream);
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static bool IsOnManaLine(const FBWBattlefieldState& State, FIntPoint Coord);
	// 겔레온 활공 배율: 움직인 칸 (겔레온이 아니면 1)
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static float GetGeleonAdvanceMultiplier(const FBWBattlefieldState& State, int32 Advance);

	// 최댓값에 배율을 곱해 내림, 최솟값은 최댓값 이하
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWDiceRange ScaleSkillMax(const FBWDiceRange& Range, float Multiplier);

	// 플레이어 쪽 굴림 조건에 기믹 반영: 마력선, 활공(Advance = 움직인 칸, 과부하 시 받는 피해 +20%), 은신 확정 치명타, 베렌드 밀기
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWDuelSide ApplyToPlayerSide(const FBWBattlefieldState& State, const FBWDuelSide& Side, int32 Advance = 0, bool bOnManaLine = false);

	// 적 쪽 굴림 조건에 기믹 반영: 마력선, 활공(돌진한 칸), 베렌드 밀기
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWDuelSide ApplyToEnemySide(const FBWBattlefieldState& State, const FBWDuelSide& Side, int32 Advance, bool bOnManaLine);

	// 타이가: 내가 이겨 피해를 준 공격이면 1회 소모하고 true(방어도 무시)
	UFUNCTION(BlueprintCallable, Category = "BW Dimension")
	static bool ConsumeTaigaCharge(UPARAM(ref) FBWBattlefieldState& State);

	// 타이가 과부하: 적이 이겨 피해를 준 공격이면 1회 소모하고 true(내 방어도 무시)
	UFUNCTION(BlueprintCallable, Category = "BW Dimension")
	static bool ConsumeTaigaEnemyCharge(UPARAM(ref) FBWBattlefieldState& State);

	// 베렌드 벽 충돌: 누구든 벽에 부딪히면 피해. 기절은 내가 밀었을 때만 (플레이어를 기절시키면 벽 앞에서 무한 반복된다)
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static void GetWallSlamRule(const FBWBattlefieldState& State, bool bPusherIsPlayer, bool& bOutExtraDamage, bool& bOutStun);
};
