#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "BeyondWing/Combat/Public/BWStatusRules.h"
#include "BeyondWing/Combat/Public/BWAugmentRules.h"
#include "BWCharacter.generated.h"

class UDeckComponent;

// [추가] 합 연출 전에 미리 정해 둔 피해: 공격 수치 → 방어(회피·매직실드) → 방어도 → HP.
// 연출 화면은 이 값을 보여 주고, 찌르는 순간 ApplyPlannedHit가 그대로 반영한다
struct FBWPlannedHit
{
	int32 RawDamage = 0;
	FBWDefenseResult Defense;   // 방어 후 피해, 회피 여부, 매직실드에 쓸 MP
	bool bIgnoreBlock = false;
	int32 BlockAbsorbed = 0;
	int32 HPLoss = 0;
};

UCLASS()
class BEYONDWING_API ABWCharacter : public APawn
{
	GENERATED_BODY()

public:
	ABWCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Grid") FIntPoint OwnGridCoordinate;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BW Combat") UDeckComponent* DeckComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Character Settings") EClassType CharacterClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Character Settings") TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 PHA;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 INT;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 FTH;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 TEC;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 ATK; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") float ASP; 

	UFUNCTION(BlueprintPure, Category = "BW Status")
	int32 GetEffectiveATK() const { return PHA + TEC; }

	UFUNCTION(BlueprintPure, Category = "BW Status")
	int32 GetEffectiveASP() const { return 1 + (PHA / 20); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 MaxHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentHP;


	// [추가] 속도 (속도 1 = 이동 1칸). 플레이어는 직업값, 적은 FBWEnemyData::SpeedDice
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") FBWDiceRange SpeedDice = FBWDiceRange(3, 6);

	// [추가] 이번 턴에 굴린 속도와 남은 이동 칸 (턴 시작에 공개)
	UPROPERTY(BlueprintReadOnly, Category = "BW Status") int32 RolledSpeed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Status") int32 RemainingMove = 0;

	// 속도를 굴려 RolledSpeed·RemainingMove를 갱신하고 굴린 값을 돌려준다
	UFUNCTION(BlueprintCallable, Category = "BW Status") int32 RollSpeed();

	// [추가] 방어 방식(플레이어만, 직업으로 결정)과 방어도(추가 체력, 전투 종료 시 소멸)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") FBWDefenseStats Defense;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 Block = 0;

	// [추가] INT·FTH 전용 MP. 마법 카드와 매직실드에 쓴다 (전투마다 ClassProfile.StartMP에서 시작)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentMP = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 MaxMP = 0;

	// [추가] 직업별 전투 수치 (방어, MP, HP 직업 방어도). 플레이어만 BeginPlay에서 직업으로 채운다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") FBWClassCombatProfile ClassProfile;

	// 이번 전투에서 몇 번째 턴인지 (1부터)
	UPROPERTY(BlueprintReadOnly, Category = "BW Status") int32 CombatTurnNumber = 0;

	// CharacterClass로 속도·방어·MP 최대치를 다시 정한다 (직업을 바꾼 뒤 호출)
	UFUNCTION(BlueprintCallable, Category = "BW Status") void ApplyClassProfile();

	// 플레이어 말: GameInstance의 고른 직업·능력치·현재 HP를 가져오고 그 직업의 시작 덱을 다시 불러온다 (배치 직후 호출)
	UFUNCTION(BlueprintCallable, Category = "BW Status") void ApplyRunState();

	// 전투 시작 자원: MP = 시작 MP, 방어도 += 시작 방어도
	UFUNCTION(BlueprintCallable, Category = "BW Status") void BeginCombatResources();

	// 4-1 턴 시작 자원 (INT·FTH MP 회복)
	UFUNCTION(BlueprintCallable, Category = "BW Status") void BeginTurnResources();

	// 4-4 턴 종료 자원 (HP 직업 방어도)
	UFUNCTION(BlueprintCallable, Category = "BW Status") void EndTurnResources();

	// [추가] 효과카드 '기세': 이번 턴 공격카드 최댓값 + (턴 시작에 0으로)
	UPROPERTY(BlueprintReadOnly, Category = "BW Status") int32 TurnAttackMaxBonus = 0;

	// [추가] HP 회복 (최대 HP까지, 플레이어는 GameInstance HP도 함께). 실제로 회복한 양을 돌려준다
	UFUNCTION(BlueprintCallable, Category = "BW Status") int32 Heal(int32 Amount);

	// --- [추가] 증강 (전투 기획서 10장) ---
	UPROPERTY(BlueprintReadOnly, Category = "BW Status|Augment") FBWAugmentTotals AugmentTotals;
	UPROPERTY(BlueprintReadOnly, Category = "BW Status|Augment") int32 DuelRerollsLeft = 0; // 재굴림: 이번 전투에 남은 횟수

	// 전투 시작 때 가진 증강을 반영한다 (속도, 시작 방어도, 손패·카드·합 한도, MP 회복량, 재굴림)
	UFUNCTION(BlueprintCallable, Category = "BW Status|Augment") void ApplyAugments(const FBWAugmentTotals& Totals);

	// --- [추가] 상태이상 (전투 기획서 9장) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BW Status|Effects") TArray<FBWStatusInstance> Statuses;

	UFUNCTION(BlueprintCallable, Category = "BW Status|Effects") void ApplyStatus(EStatusEffect Type, int32 Duration, int32 Value);
	UFUNCTION(BlueprintPure, Category = "BW Status|Effects") bool HasStatus(EStatusEffect Type) const;

	// 기절: 이번 자기 턴에 이동하지 못하고 합도 걸지 못한다
	UFUNCTION(BlueprintPure, Category = "BW Status|Effects") bool IsStunned() const { return HasStatus(EStatusEffect::Stun); }

	// 자기 턴이 끝날 때: 출혈 피해(방어 없이 방어도 → HP)를 받고 남은 턴을 줄인다. 받은 출혈 피해를 돌려준다
	UFUNCTION(BlueprintCallable, Category = "BW Status|Effects") int32 EndOwnTurnStatuses();

	// 기절한 적이 이번 턴 행동을 건너뛴다 (공개한 의도는 버리고 다음 패턴으로)
	UFUNCTION(BlueprintCallable, Category = "BW AI") void SkipTurnForStun();

	// --- 적군 전용 AI 속성 ---
	// '강화 후 공격' 행동으로 쌓이는 값. 적 스킬 숫자 범위의 최솟값·최댓값에 더해진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Status") int32 CurrentAttackBuff;

	// [추가] 4-3: 적은 전투 시작 때 배치된 순서(0부터)대로 행동한다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BW AI") int32 EnemyOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW AI") TArray<FBWEnemyTurnPattern> CombatPatterns;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW AI") int32 CurrentPatternIndex;

	UPROPERTY(BlueprintReadOnly, Category = "BW AI") TArray<FIntPoint> LockedAttackTiles;
	UPROPERTY(BlueprintReadOnly, Category = "BW AI") TArray<FIntPoint> LockedMovePath;
	
	UPROPERTY(BlueprintReadOnly, Category = "BW AI") bool bIntentLocked = false;

	// --- 부드러운 이동 시스템 ---
	UPROPERTY(BlueprintReadOnly, Category = "BW Movement")
	TArray<FVector> VisualMoveQueue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Movement")
	float SmoothMoveSpeed = 15.0f;

	TArray<bool> VisualMoveFacing;

	// 한 칸(또는 찌르기) 이동 연출을 큐에 넣는다. bTurnToFace면 가는 방향을 바라본다
	void AddMovementStep(FVector TargetWorldLoc, bool bTurnToFace = true);

	// 이동 연출이 남아 있는지 (합 판정은 이동이 끝난 뒤에)
	bool IsMoving() const { return VisualMoveQueue.Num() > 0; }

	void FaceTowards(const FVector& WorldLocation);

	// 머리 위에 잠깐 글자를 띄운다 (피해 숫자, 상태이상 등)
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void ShowPopup(const FString& Text, const FLinearColor& Color, float Scale = 1.0f);

	// [추가] 맞았을 때: 빨갛게 번쩍 + 맞은 반대쪽으로 살짝 밀렸다 돌아옴 (bHeavy = 치명타 등 큰 타격)
	void PlayHitReaction(const FVector& FromLocation, bool bHeavy);
	// [추가] 회피했을 때: 옆으로 재빨리 비켰다 돌아옴
	void PlayDodge(const FVector& FromLocation);
	// [추가] 쓰러지는 중 (그리드에서는 이미 빠졌고, 가라앉으며 작아진 뒤 사라진다)
	bool IsDying() const { return bIsDying; }

	// 맞았을 때 메시 위에 덧씌우는 재질 (빨간 반투명)
	UPROPERTY(EditAnywhere, Category = "BW Visual") TSoftObjectPtr<UMaterialInterface> HitFlashMaterial;
	UPROPERTY(EditAnywhere, Category = "BW Visual") TSoftObjectPtr<class UNiagaraSystem> DeathEffect;
	UPROPERTY(EditAnywhere, Category = "BW Visual") float HitReactTime = 0.22f;
	UPROPERTY(EditAnywhere, Category = "BW Visual") float HitFlashTime = 0.12f;
	UPROPERTY(EditAnywhere, Category = "BW Visual") float DeathTime = 0.7f;
	// 머리 위 상태 표시의 높이 (말 원점 기준)
	UPROPERTY(EditAnywhere, Category = "BW Visual") float StatusWidgetHeight = 190.0f;
	UPROPERTY(Transient) class UWidgetComponent* StatusWidgetComp = nullptr;


	// [수정] 이번 턴 행동의 의도(돌진 경로, 합을 걸 칸)를 지금 위치 기준으로 다시 계산한다
	UFUNCTION(BlueprintCallable, Category = "BW AI")
	void UpdateEnemyIntent();

	// 이번 턴 행동 (현재 패턴의 첫 행동). 패턴이 없으면 nullptr
	const FBWEnemyPattern* GetCurrentAction() const;

	// 행동을 마치면 다음 패턴으로 (순환)
	UFUNCTION(BlueprintCallable, Category = "BW AI")
	void AdvancePattern();

	// 가장 가까운 상대 진영의 살아 있는 말
	ABWCharacter* FindNearestOpponent() const;

	UFUNCTION(BlueprintCallable, Category = "BW AI")
	void VisualizeEnemyIntent();

	// --- 전투 로직 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsEnemy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	FString EnemyDisplayName;

	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void ReceiveDamage(int32 DamageAmount);

	// [추가] 합에서 받는 피해: 플레이어는 방어(5-3 4단계)를 거친 뒤 방어도 → HP 순으로 깎인다 (5단계)
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	FBWDefenseResult TakeHit(int32 Damage, bool bIgnoreBlock);

	// [추가] 피해를 지금 정해 두기만 한다 (방어 굴림 포함, HP·방어도·MP는 그대로)
	// ShieldMP = 매직실드에 걸기로 고른 MP (INT·FTH가 적의 공격을 가드할 때만, 나머지 피해는 0)
	FBWPlannedHit PlanHit(int32 Damage, bool bIgnoreBlock, int32 ShieldMP = 0);
	// [추가] PlanHit로 정한 피해를 반영한다 (MP 소모 → 방어도 → HP, 피해 숫자 연출)
	FBWDefenseResult ApplyPlannedHit(const FBWPlannedHit& Plan);

	// [추가] 합 계산에 쓰는 스탯 묶음
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel")
	FBWSkillStats GetSkillStats() const;

	// [추가] 플레이어가 Card의 숫자 범위 그대로 합을 걸 때의 굴림 조건 (6-2 숫자 범위, 6-3 치명타)
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel")
	FBWDuelSide MakePlayerDuelSide(const FBWCardData& Card) const;

	// --- [추가] 기본 공격 + 카드 강화 ---
	// 이 삶의 방식의 기본 공격 카드 (DT_Classes의 BasicStrike)
	FBWCardData GetBasicAttackCard() const;
	// 공격카드 강화 배율 (DT_Classes의 CardPower)
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel") float GetCardPower() const;
	// 지금 기본 공격 숫자 범위 (스탯·증강·기세·상태이상 반영 전의 순수 기본 공격)
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel") FBWDiceRange GetBasicAttackRange() const;
	// 공격카드가 기본 공격에 더해 주는 몫 (카드 숫자 범위 × 배율)
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel") FBWDiceRange GetCardBoostRange(const FBWCardData& Card) const;
	// 내가 선공으로 여는 합의 굴림 조건: BoostCard가 없으면 기본 공격, 있으면 기본 공격 + 카드 몫 (카드의 상태이상·밀기를 쓴다)
	FBWDuelSide MakeAttackSide(const FBWCardData* BoostCard) const;
	// [추가] 합을 걸 수 있는 거리 (상하좌우 칸 수). 기본 공격의 Range와 강화 카드의 Range 중 큰 값, 최소 1
	int32 GetAttackReach(const FBWCardData* BoostCard) const;
	// [추가] 매직실드 가드에 지금 걸 수 있는 최대 MP (보유 MP와 한 번 상한 중 작은 값). 매직실드가 아니면 0
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel") int32 GetMaxGuardMP() const;

	// [추가] 적이 이번 턴 공개한 스킬의 굴림 조건 (현재 패턴의 첫 행동 숫자 범위)
	UFUNCTION(BlueprintPure, Category = "BW Combat|Duel")
	FBWDuelSide MakeEnemyDuelSide() const;

	// 약화·취약·혼란·집중을 굴림 조건에 반영
	void ApplyStatusesToDuelSide(FBWDuelSide& Side) const;

	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "BW Combat|Calculations")
	int32 CalculateFinalCardDamage(const FBWCardData& CardInfo, bool& OutIsCritical);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	// 맞음·회피 연출 상태. 메시 컴포넌트의 상대 위치만 흔들어서 이동 연출(액터 위치)과 겹치지 않는다
	void UpdateReactions(float DeltaTime);
	void SetMeshOffset(const FVector& WorldOffset, float ScaleZ);
	void SetHitFlash(bool bOn);
	TArray<TPair<TWeakObjectPtr<USceneComponent>, FTransform>> MeshBaseTransforms;
	float ReactAge = -1.0f;
	FVector ReactDir = FVector::ZeroVector;
	bool bReactHeavy = false;
	bool bReactIsDodge = false;
	bool bFlashOn = false;
	bool bIsDying = false;
	float DyingAge = 0.0f;
	// 같은 순간에 뜬 팝업은 위로 쌓아서 겹치지 않게
	float LastPopupTime = -10.0f;
	int32 PopupStack = 0;
	FVector DyingStartScale = FVector::OneVector;
	FVector DyingStartLocation = FVector::ZeroVector;
};