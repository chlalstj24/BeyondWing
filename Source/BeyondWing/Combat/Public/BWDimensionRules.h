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

	// 타이가: 남은 '방어 무시 + 치명타' 공격 횟수 / 과부하 시 적의 방어 무시 공격 횟수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") int32 TaigaChargesLeft = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") int32 TaigaEnemyChargesLeft = 0;

	// 파트로바스: 이번 턴 내 스킬 최댓값 배율 (×0.7~×1.3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") float PatrovasMaxMultiplier = 1.0f;

	// 겔레온: 전투 시작에 고른다 (사용자 결정, 기획서 11-4 수정)
	// true = 효과카드 사용 + 처음 쓴 공격카드 1종만(기본 공격은 언제나) / false = 효과카드 안 씀, 공격카드는 모두
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") bool bGeleonUseEffects = false;
	// 효과카드 사용 모드에서 처음 쓴 공격카드 이름 (비어 있으면 아직 안 씀)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") FString GeleonAttackCardName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") bool bGeleonKindLocked = false;

	// [추가] 지도 방치 보너스(겔레온)로 이번 전투 피해량 + (0.5 = +50%). 겔레온 기믹 +50%와 합산
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battlefield") float BonusDamage = 0.0f;
};

// 국가 기믹 규칙. 상태가 없고, 무작위는 넘겨받은 스트림에서 뽑는다
UCLASS()
class BEYONDWING_API UBWDimensionRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 TaigaFreeHits = 3;                 // 타이가: 최초 3회
	static constexpr int32 TaigaOverloadEnemyHits = 1;        // 과부하: 적의 첫 공격 1회
	static constexpr int32 BerendWallDamage = 3;              // 베렌드 벽 추가 피해 (기획 수치 없음, 임시)
	static constexpr int32 BerendStunTurns = 1;
	static constexpr float PatrovasMinMultiplier = 0.7f;
	static constexpr float PatrovasMaxMultiplier = 1.3f;
	static constexpr float PatrovasGrowChance = 0.5f;
	static constexpr float PatrovasGrowChanceOverloaded = 0.3f; // 과부하 시 커질 확률 (기획 수치 없음, 임시)
	static constexpr float GeleonDamageMultiplier = 1.5f;
	static constexpr float GeleonOverloadIncomingMultiplier = 1.2f;

	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWBattlefieldState MakeBattlefield(EDimensionType Dimension, bool bOverloaded);

	// 파트로바스 턴 시작: 커질지 좁아질지 정하고 배율을 굴려 State에 저장한다 (다른 차원은 1.0)
	UFUNCTION(BlueprintCallable, Category = "BW Dimension")
	static float RollPatrovasMultiplier(UPARAM(ref) FBWBattlefieldState& State, UPARAM(ref) FRandomStream& Stream);

	// 최댓값에 배율을 곱해 내림, 최솟값은 최댓값 이하
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWDiceRange ScaleSkillMax(const FBWDiceRange& Range, float Multiplier);

	// 플레이어 쪽 굴림 조건에 기믹 반영: 파트로바스 범위, 겔레온 피해 +50%(과부하 시 받는 피해 +20%), 타이가 확정 치명타
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static FBWDuelSide ApplyToPlayerSide(const FBWBattlefieldState& State, const FBWDuelSide& Side);

	// 겔레온: 이번 전투에 이 카드를 쓸 수 있는지 (기본 공격은 언제나 쓸 수 있어 여기서 묻지 않는다)
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static bool CanUseCard(const FBWBattlefieldState& State, const FBWCardData& Card);

	// 겔레온 효과카드 사용 모드: 처음 쓴 공격카드를 이번 전투의 1종으로 정한다
	UFUNCTION(BlueprintCallable, Category = "BW Dimension")
	static void NoteAttackCardUsed(UPARAM(ref) FBWBattlefieldState& State, const FBWCardData& Card);

	// 타이가: 내가 이겨 피해를 준 공격이면 1회 소모하고 true(방어도 무시)
	UFUNCTION(BlueprintCallable, Category = "BW Dimension")
	static bool ConsumeTaigaCharge(UPARAM(ref) FBWBattlefieldState& State);

	// 타이가 과부하: 적이 이겨 피해를 준 공격이면 1회 소모하고 true(내 방어도 무시)
	UFUNCTION(BlueprintCallable, Category = "BW Dimension")
	static bool ConsumeTaigaEnemyCharge(UPARAM(ref) FBWBattlefieldState& State);

	// 베렌드 벽 충돌 보너스가 붙는지: 플레이어가 밀면 베렌드에서, 적이 밀면 베렌드 과부하에서만 (기절은 플레이어가 밀 때만)
	UFUNCTION(BlueprintPure, Category = "BW Dimension")
	static void GetWallSlamRule(const FBWBattlefieldState& State, bool bPusherIsPlayer, bool& bOutExtraDamage, bool& bOutStun);
};
