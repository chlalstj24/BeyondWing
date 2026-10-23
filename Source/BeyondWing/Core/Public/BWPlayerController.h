#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GridTile.h"
#include "BWCharacter.h"
#include "BeyondWing/Combat/Public/BWDimensionRules.h"
#include "BeyondWing/UI/Public/BWDuelWidget.h"
#include "Containers/Ticker.h"
#include "BWPlayerController.generated.h"

class AGridManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStartedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetCharacterHovered, ABWCharacter*, TargetChar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContactWithEnemy, ABWCharacter*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDuelResolved, const FBWDuelResult&, Result, ABWCharacter*, Enemy, const FBWCardData&, AttackCard);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyStrike, const FBWDuelResult&, Result, ABWCharacter*, Enemy);

UCLASS()
class BEYONDWING_API ABWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABWPlayerController();

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void SelectCard(int32 Index);

	// [추가] 효과카드를 바로 쓴다 (고르는 즉시 나에게 적용)
	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void UseEffectCard(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void CancelSelection();

	void SetHandWidget(class UBWHandWidget* InWidget) { HandWidget = InWidget; }
	class UBWHandWidget* GetHandWidget() const { return HandWidget; }
	void SetBattleHUDWidget(class UBWBattleHUDWidget* InWidget) { BattleHUDWidget = InWidget; }

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	int32 SelectedCardIndex;

	// [추가] 타격감 조절값 (실제 시간 초 / 언리얼 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Feel") float HitStopTime = 0.07f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Feel") float CritHitStopTime = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Feel") float HitShakeStrength = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Feel") float CritShakeStrength = 11.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Feel") float ShakeDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsSkillTargetingMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	bool bIsMovementMode;

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void EnterMovementMode();

	// [수정] 기본 공격(카드 없이 언제나 가능)으로 합을 건다. 맞닿은 적이 하나면 바로, 여럿이면 고르는 모드
	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void EnterBasicAttackMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	int32 CurrentSkillRange;

	// [수정] 합을 걸 적을 고르는 중 (공격카드로 강화한 공격 또는 기본 공격)
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	bool bIsDuelTargetingMode;

	// 합에 쓸 손패 번호. INDEX_NONE이면 기본 공격
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	int32 DuelCardIndex = INDEX_NONE;

	// 이동하다 접촉해서 합이 열린 적 (공격카드를 고르면 바로 이 적과 합)
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	ABWCharacter* PendingDuelTarget = nullptr;

	// [수정] 5-1: 맞닿은 적과 내가 선공인 합을 연다. INDEX_NONE이면 기본 공격, 손패 번호면 그 공격카드로 강화한 기본 공격
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	bool OpenDuel(ABWCharacter* Enemy, int32 HandIndex);

	// 상하좌우로 맞닿은 살아 있는 적
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	TArray<ABWCharacter*> GetAdjacentEnemies();

	// 기본 공격 버튼을 켤 수 있는지 (합 횟수가 남았고, 기절하지 않았고, 맞닿은 적이 있음)
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	bool CanUseBasicStrike();

	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnDuelResolved OnDuelResolved;

	// [추가] 이번 전투의 국가 기믹 (전투 시작 때 GameInstance의 현재 차원·연속 체류로 정함)
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat|Dimension")
	FBWBattlefieldState Battlefield;

	// 기믹을 직접 지정한다 (디버그·테스트, 또는 보스전처럼 차원을 따로 정할 때)
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Dimension")
	void SetBattlefield(EDimensionType Dimension, bool bOverloaded);

	// 겔레온: 효과카드를 쓸지 고른다 (쓰면 공격카드는 처음 쓴 1종만, 기본 공격은 언제나). 확정 전까지만 바꿀 수 있다
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Dimension")
	bool ChooseGeleonMode(bool bUseEffects);

	// 겔레온 전투 시작 화면에서 고른 방식을 확정하고 첫 턴을 시작한다
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Dimension")
	bool ConfirmGeleonMode(bool bUseEffects);

	// 손패에 지금 쓸 수 있는 공격카드가 있는지 (겔레온 제한 반영)
	UFUNCTION(BlueprintPure, Category = "BW Combat|Dimension")
	bool HasUsableAttackCardInHand() const;

	// 비워 두면 C++ 기본 위젯(UBWGeleonChoiceWidget)이 코드로 화면을 만든다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat|Dimension")
	TSubclassOf<class UBWGeleonChoiceWidget> GeleonChoiceWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	TSubclassOf<ABWCharacter> CharacterClassToSpawn;

	// 직업별 고스트 매핑 (C++에서 자동으로 관리)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	TMap<TSubclassOf<ABWCharacter>, TSubclassOf<AActor>> PlacementPreviewMap;

	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void RequestEndTurn();

	// [추가] 턴 시작: 손패 갱신, 내 속도와 모든 적의 속도를 굴려 공개하고 적 의도를 갱신
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void BeginRound();

	// [추가] 아군 구역의 Coord에 말을 배치하고 첫 턴을 시작한다 (배치 클릭과 같은 흐름)
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	bool PlaceCharacterAndStartCombat(FIntPoint Coord);

	// [추가] 조종 중인 말을 Target까지 남은 이동 칸 안의 최단 경로로 옮긴다. 갈 수 없으면 false
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	bool TryMovePlayerTo(FIntPoint Target);

	// [추가] 이동하다 적과 접촉했을 때 (3단계 합 판정의 시작점)
	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnContactWithEnemy OnContactWithEnemy;

	// [추가] 4-3: 적 한 마리의 이번 턴 행동 (공격: 속도만큼 돌진해 닿으면 적이 선공인 합 / 회복 / 강화 후 공격)
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Enemy")
	void ExecuteEnemyAction(ABWCharacter* Enemy);

	// [추가] 5-2: 맞닿은 적이 선공인 합. 적이 굴린 숫자가 피해가 되고 나는 방어만 한다
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Enemy")
	bool ResolveEnemyDuel(ABWCharacter* Enemy);

	UPROPERTY(BlueprintAssignable, Category = "BW Combat|Enemy")
	FOnEnemyStrike OnEnemyStrike;

	// [추가] 10장: 막 시작 증강 선택 화면을 연다 (3택 1, 리롤 1회). 지도의 막 시작에서 호출
	UFUNCTION(BlueprintCallable, Category = "BW Augment")
	void OpenAugmentPick(int32 Act);

	// 비워 두면 C++ 기본 위젯(UBWAugmentSelectWidget)이 코드로 화면을 만든다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Augment")
	TSubclassOf<class UBWAugmentSelectWidget> AugmentSelectWidgetClass;

	// [추가] 합 전용 화면. 비워 두면 C++ 기본 위젯(UBWDuelWidget)을 쓴다. WBP로 꾸미면 여기에 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat")
	TSubclassOf<class UBWDuelWidget> DuelWidgetClass;

	// [추가] 마나 가드 화면 (비워 두면 C++ 기본 화면)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW UI")
	TSubclassOf<class UBWGuardChoiceWidget> GuardChoiceWidgetClass;
	// [추가] 사거리 미리 보기 칸 표시 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW UI") bool bShowReachPreview = true;

	// [추가] 손패 N번째 카드 사용 (숫자 키 1~0, 위로 끌어 놓기). 공격카드는 맞닿은 적이 하나면 바로 강화 공격
	UFUNCTION(BlueprintCallable, Category = "BW Input")
	void UseCardShortcut(int32 HandIndex);

	// --- 순차적 적 턴 처리 ---
	UFUNCTION(BlueprintCallable, Category = "BW Combat")
	void ProcessNextEnemyTurn();

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	TArray<ABWCharacter*> EnemyTurnQueue;

	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnCombatStartedSignature OnCombatStarted;

	UPROPERTY(BlueprintAssignable, Category = "BW Combat")
	FOnTargetCharacterHovered OnTargetCharacterHovered;

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	ABWCharacter* FocusedCharacter;

	// [수정] 카드를 쓸 수 없을 때 (한도·MP 부족·대상 없음). 옛 InsufficientAPSound 값은 CoreRedirects로 이어받음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat|Sounds")
	TSoftObjectPtr<USoundBase> CardUnusableSound;

	UPROPERTY(BlueprintReadOnly, Category = "BW Combat")
	ABWCharacter* PossessedBWCharacter;

	UFUNCTION(BlueprintCallable, Category = "BW Input")
	AGridTile* GetTileUnderCursor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	void OnLeftClick();
	void OnRightClick();
	void TraceGridUnderCursor();
	AGridTile* PickTileUnderCursor();
	
	void UpdatePlacementPreview();
	void DestroyPlacementPreview();
	void MovePlayerAlongPath(const TArray<FIntPoint>& Path);
	void EnterDuelTargeting(int32 HandIndex);
	void RefreshDuelTargetHighlight();

	// --- [추가] 연출 순서 ---
	// 이동·합 연출 중 (입력을 막는다) / 적 턴 진행 중
	bool bActionInProgress = false;
	bool bEnemyPhase = false;

	void Prompt(const FString& Message, float Duration = 3.0f, EBWPromptStyle Style = EBWPromptStyle::Info);
	// 손패가 가득 차 뽑은 카드가 버려졌을 때 알림
	UFUNCTION() void HandleCardBurned(const FBWCardData& BurnedCard);
	void WaitForMovementThen(ABWCharacter* Mover, TFunction<void()> Done);
	void PlayLungeThen(ABWCharacter* Attacker, ABWCharacter* Target, TFunction<void()> OnImpact, TFunction<void()> OnFinished);
	// 합 화면 → 이긴 쪽 찌르기 → 결과 반영 (내가 연 합 / 적이 연 합)
	// bCardBoost: AttackCard가 기본 공격을 강화하는 공격카드인가 (false면 AttackCard는 기본 공격 자체)
	void PlayPlayerDuel(ABWCharacter* PlayerChar, ABWCharacter* Enemy, const FBWCardData& AttackCard, bool bCardBoost, TFunction<void()> OnFinished);
	void ApplyPlayerDuelOutcome(ABWCharacter* PlayerChar, ABWCharacter* Enemy, const FBWCardData& AttackCard, const FBWDuelSide& PlayerSide, const FBWDuelSide& EnemySide, const FBWDuelResult& Result, const FBWPlannedHit& Hit);
	bool ComputeEnemyStrike(ABWCharacter* Enemy, FBWDuelSide& OutEnemySide, FBWDuelResult& OutResult);
	void PlayEnemyDuel(ABWCharacter* Enemy, TFunction<void()> OnFinished);
	// [추가] INT·FTH: 적이 굴리기 전에 마나 가드에 걸 MP를 고르는 화면 → 고른 MP로 합 화면
	void OpenGuardChoice(ABWCharacter* Enemy, TFunction<void(int32)> OnChosen);
	void PlayEnemyDuelWithGuard(ABWCharacter* Enemy, int32 ShieldMP, TFunction<void()> OnFinished);
	void ApplyEnemyStrikeOutcome(ABWCharacter* Enemy, const FBWDuelSide& EnemySide, const FBWDuelResult& Result, const FBWPlannedHit& Hit);
	// 맞는 쪽의 피해를 연출 전에 정한다 (방어 굴림, 타이가 방어도 무시 횟수 소모). bPlayerDeals = 내가 때리는 피해인가
	FBWPlannedHit PlanDuelHit(ABWCharacter* Target, int32 Damage, bool bPlayerDeals, int32 ShieldMP = 0);
	void FillHitPresentation(FBWDuelPresentation& Show, const FBWPlannedHit& Hit, bool bPlayerIsHit) const;
	void ShowDuel(const FBWDuelPresentation& Data, TFunction<void()> OnFinished);
	FString DescribePlayerDefense(const ABWCharacter* PlayerChar) const;

	UPROPERTY() class UBWDuelWidget* DuelWidget = nullptr;

	// --- [추가] 타격감: 맞는 순간 멈칫(히트스톱), 카메라 흔들림, 맞은 쪽 반응, 치명타 표시 ---
	void PlayImpactFeedback(ABWCharacter* Attacker, ABWCharacter* Target, const FBWDefenseResult& Defended, int32 HPLost, bool bCritical);
	void StartHitStop(float RealSeconds);
	void StartCameraShake(float Amplitude, float Duration);
	bool TickCameraShake(float DeltaTime);
	FTSTicker::FDelegateHandle HitStopTicker;
	FTSTicker::FDelegateHandle ShakeTicker;
	float ShakeTimeLeft = 0.0f;
	float ShakeTotalTime = 0.0f;
	float ShakeStrength = 0.0f;
	FVector ShakeHome = FVector::ZeroVector;

	// [추가] "내 턴 / 적 턴" 띠
	void ShowTurnBanner(const FString& Title, const FString& Sub, const FLinearColor& Accent);
	UPROPERTY() class UBWTurnBannerWidget* TurnBannerWidget = nullptr;

	// [추가] 전투 시작 때 카드·적 이펙트를 미리 불러 둔다 (처음 쓸 때 멈칫하지 않게)
	void PreloadCombatAssets();

	// [추가] 차원(국가)마다 전장 분위기: 햇빛·하늘빛·안개 색과 화면 색감. 과부하면 붉고 어둡게
	void ApplyDimensionAtmosphere();
	UPROPERTY() class APostProcessVolume* AtmosphereVolume = nullptr;
	UPROPERTY() TArray<UObject*> PreloadedAssets;
	void ExecuteEnemyActionThen(ABWCharacter* Enemy, TFunction<void()> OnDone);
	void HandleEnemyClicked(ABWCharacter* Enemy);
	bool IsPendingDuelTargetAdjacent() const;
	// [추가] 사거리: 내 칸에서 상하좌우 거리(맨해튼) Reach 안의 살아 있는 적
	TArray<ABWCharacter*> GetEnemiesInReach(int32 Reach) const;
	bool IsEnemyInReach(const ABWCharacter* Enemy, int32 Reach) const;
	// 손패 번호(INDEX_NONE = 기본 공격)로 합을 걸 수 있는 거리
	int32 GetReachForHandIndex(int32 HandIndex) const;
	// 공격카드를 집거나 올린 동안 사거리 칸을 보여 준다
	void UpdateReachPreview();
	int32 ReachPreviewIndex = -2;
	bool bPreferCursorTarget = false; // UseCardShortcut 동안: 마우스 아래 적을 대상으로 먼저 본다
	bool bReachPreviewShown = false;
	void ResolvePushAfterDuel(ABWCharacter* Pusher, ABWCharacter* Target, bool bPusherIsPlayer);
	void InitBattlefieldFromGameInstance();
	void OpenGeleonChoice();
	bool bAwaitingGeleonChoice = false;
	void ReflectThorns(ABWCharacter* Enemy, int32 DamageTaken);
	void PlayCardUnusableFeedback(const FString& Reason);

	UPROPERTY() class UBWHandWidget* HandWidget;
	UPROPERTY() class UBWBattleHUDWidget* BattleHUDWidget;
	UPROPERTY() AGridManager* TargetGridManager;
	UPROPERTY() AGridTile* LastHoveredTile;
	UPROPERTY() AGridTile* CurrentTileUnderCursor;
	UPROPERTY() AActor* PlacementPreviewActor;
	UPROPERTY() AActor* BattleCamera = nullptr;
};