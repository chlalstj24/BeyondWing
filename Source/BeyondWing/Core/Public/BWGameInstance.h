#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BeyondWing/Combat/Public/BWClassData.h"
#include "BeyondWing/Core/Public/BWMapTypes.h"
#include "BWGameInstance.generated.h"

// 개별 몬스터의 배치 정보 (ID + 좌표)
USTRUCT(BlueprintType)
struct FEnemyPositionInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName EnemyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Coordinate = FIntPoint::ZeroValue;
};

// 전투 조합(인카운터) 데이터 테이블 행 구조 (출현 목록)
USTRUCT(BlueprintType)
struct FEncounterData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Stage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDimensionType Dimension = EDimensionType::Patrovas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMapNodeType EncounterType = EMapNodeType::Battle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FEnemyPositionInfo> EnemyGroup;

	// [추가] 장애물 벽 타일 좌표 (맵 가장자리는 따로 지정하지 않아도 벽으로 본다)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FIntPoint> WallTiles;
};

USTRUCT(BlueprintType)
struct FEnemySpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName EnemyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Coordinate = FIntPoint::ZeroValue;
};

UCLASS()
class BEYONDWING_API UBWGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UBWGameInstance();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Database") UDataTable* EnemyDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Database") UDataTable* EncounterDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Progress") int32 CurrentStage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Progress") int32 CurrentGold = 0;

	// 플레이어 영구 스탯
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerMaxHP = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerCurrentHP = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerPHA = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerINT = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerFTH = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerTEC = 5;

	// [추가] 직업별 속도 시작값. 삶의 방식 테이블(ClassDataTable)에 행이 있으면 그 행의 Speed가 우선하고, 이 값은 테이블이 없을 때만 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") TMap<EClassType, FBWDiceRange> ClassSpeedDice;

	UFUNCTION(BlueprintPure, Category = "BW Player Stats") FBWDiceRange GetClassSpeedDice(EClassType ClassType) const;

	// [추가] 삶의 방식(직업) 테이블: 선택 화면 문구·모양, 시작 능력치·속도·방어·자원, 시작 덱, 기본 공격·카드 강화 배율. 행 구조는 FBWClassData
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Database") TSoftObjectPtr<UDataTable> ClassDataTable;

	// 이 직업의 행 (테이블이 없거나 행이 없으면 nullptr → 아래 Get 함수들은 C++ 기본값을 쓴다)
	const FBWClassData* FindClassData(EClassType ClassType) const;
	UFUNCTION(BlueprintPure, Category = "BW Player Stats") bool GetClassData(EClassType ClassType, FBWClassData& OutData) const;
	// 테이블 행 순서대로 모든 행 (선택 화면 카드 순서)
	UFUNCTION(BlueprintPure, Category = "BW Player Stats") TArray<FBWClassData> GetAllClassData() const;
	UFUNCTION(BlueprintPure, Category = "BW Player Stats") FBWClassStartStats GetClassStartStats(EClassType ClassType) const;
	UFUNCTION(BlueprintPure, Category = "BW Player Stats") FBWClassCombatProfile GetClassCombatProfile(EClassType ClassType) const;
	UFUNCTION(BlueprintPure, Category = "BW Player Stats") FBWCardData GetClassBasicStrike(EClassType ClassType) const;
	// 공격카드 강화 배율 (DT_Classes의 CardPower)
	UFUNCTION(BlueprintPure, Category = "BW Player Stats") float GetClassCardPower(EClassType ClassType) const;
	// 시작 덱 행 이름. 테이블에 없거나 비어 있으면 false (덱 컴포넌트의 직업별 배열을 쓴다)
	bool GetClassStartingDeck(EClassType ClassType, TArray<FName>& OutCards) const;

	// [추가] 캐릭터 선택에서 고른 직업. 전투에 배치되는 말이 이 직업·능력치·HP로 바뀐다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") EClassType SelectedClass = EClassType::PHA;

	// 새 판 시작: 직업과 시작 능력치를 정하고 HP·골드·증강·진행을 초기화한다
	UFUNCTION(BlueprintCallable, Category = "BW Player Stats") void BeginNewRun(EClassType ClassType);

	// --- [추가] 증강 (전투 기획서 10장): 막 시작 시 3개 중 1개, 리롤 1회 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Augment") UDataTable* AugmentDataTable = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "BW Augment") TArray<FName> OwnedAugments;
	UPROPERTY(BlueprintReadOnly, Category = "BW Augment") TArray<FName> AugmentChoices;
	UPROPERTY(BlueprintReadOnly, Category = "BW Augment") int32 AugmentRerollsLeft = 0;
	UPROPERTY(BlueprintReadOnly, Category = "BW Augment") int32 AugmentPickAct = 0;

	// 이번 막의 증강 후보 3개를 뽑고 리롤 1회를 준다
	UFUNCTION(BlueprintCallable, Category = "BW Augment") const TArray<FName>& BeginAugmentPick(int32 Act, EClassType PlayerClass);

	// 후보 3개를 새로 뽑는다 (선택마다 1회)
	UFUNCTION(BlueprintCallable, Category = "BW Augment") bool RerollAugmentChoices();

	// 후보 중 하나를 가진다
	UFUNCTION(BlueprintCallable, Category = "BW Augment") bool ChooseAugment(int32 ChoiceIndex);

	UFUNCTION(BlueprintPure, Category = "BW Augment") bool GetAugmentData(FName RowName, FBWAugmentData& OutData) const;
	UFUNCTION(BlueprintPure, Category = "BW Augment") TArray<FBWAugmentData> GetOwnedAugmentData() const;

private:
	EClassType AugmentPickClass = EClassType::Common;
	FRandomStream AugmentRandom;
	void GetAugmentRows(TArray<FName>& OutNames, TArray<FBWAugmentData>& OutRows) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerDeckCount = 10;

	UFUNCTION(BlueprintCallable, Category = "BW Combat") void PrepareEncounter(EMapNodeType Type, EDimensionType Dim);
	FName FindEnemyIDByDisplayName(FString DisplayName);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat") TArray<FEnemySpawnRequest> PendingEnemyRequests;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat") TArray<FIntPoint> PendingWallTiles; // [추가] 이번 전투의 장애물 벽

	// --- 지도 (지도 기획서). CurrentStage = 지금 막(1~3) ---
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") TArray<FMapStep> WorldMapData;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CurrentStep = -1;   // 막 시작 전 -1, 일반 칸 0~14, 보스 15
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CurrentDimIdx = -1; // 서 있는 줄 (보스 칸에서도 마지막 줄을 유지)
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat") EMapNodeType CurrentEncounterType = EMapNodeType::Battle;

	// 이번 막의 줄 순서: RowDimensions[줄] = 그 줄의 차원. 1막은 고정, 2·3막은 섞는다
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") TArray<EDimensionType> RowDimensions;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") bool bShuffleRowsAfterFirstAct = true;

	// 균열을 밟았으면 다음 칸에서 2줄 점프 가능
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bRiftJumpReady = false;

	// 보너스 내성: 이번 막에 차원별로 방치 보너스를 받은 횟수 (막이 바뀌거나 차원 제단에서 초기화)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") TMap<EDimensionType, int32> BonusClaimCounts;

	// 받아 두고 다음 기회에 쓰는 방치 보너스 (막이 바뀌면 사라짐)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") FBWMapBonusState MapBonus;

	// 마지막으로 도착한 칸에서 일어난 일과, 지금 지워야 할 카드 수(타이가 방치 보너스)
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") FBWArrivalReport LastArrival;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 PendingCardRemovals = 0;

	// 보스를 이겨 다음 막으로 넘어갈 차례인지 / 증강을 고른 마지막 막
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") bool bActCleared = false;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 AugmentPickedAct = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") int32 VisibilityRange = 2;
	UPROPERTY(BlueprintReadOnly, Category = "BW Reward") TArray<FRewardItem> PendingRewards;

	UFUNCTION(BlueprintCallable, Category = "BW Map") void GetNodeVisibilityState(int32 Step, EMapNodeType Type, bool& bOutIsVisible, bool& bOutIsFogged);
	// 그 칸이 가시거리 안이라 노드와 차원문 개폐가 보이는지 (지나온 칸 포함)
	UFUNCTION(BlueprintPure, Category = "BW Map") bool IsStepRevealed(int32 Step) const;
	// 지금 막(CurrentStage)의 지도를 새로 만든다. 막 단위 카운터·보너스도 초기화 (TotalSteps는 옛 호출 호환용, 쓰지 않음)
	UFUNCTION(BlueprintCallable, Category = "BW Map") void GenerateWorldMap(int32 TotalSteps = 15);
	UFUNCTION(BlueprintCallable, Category = "BW Map") bool CanMoveToNode(int32 TargetStep, int32 TargetDimIdx);
	// 칸으로 이동: 차원문 → 방치 보너스 수령 → 과부하 판정까지 처리하고 LastArrival에 남긴다. 노드 진입(전투·휴식 등)은 지도 화면이 한다
	UFUNCTION(BlueprintCallable, Category = "BW Map") bool SelectMapNode(int32 Step, int32 DimIdx);
	UFUNCTION(BlueprintPure, Category = "BW Map") bool IsBossStep(int32 Step) const;
	UFUNCTION(BlueprintPure, Category = "BW Map") EDimensionType GetRowDimension(int32 Row) const;
	// 지금 서 있는 줄의 차원 (전투 기믹). 막 시작 전이면 None
	UFUNCTION(BlueprintPure, Category = "BW Map") EDimensionType GetCurrentDimension() const;
	// 지금 줄의 과부하 단계 (0 안정, 1 과부하 I, 2 과부하 II)
	UFUNCTION(BlueprintPure, Category = "BW Map") int32 GetCurrentOverloadLevel() const;
	UFUNCTION(BlueprintPure, Category = "BW Map") const FMapNode& GetCurrentNode() const;

	// 차원 제단: 과부하와 모든 차원의 보너스 내성 초기화
	UFUNCTION(BlueprintCallable, Category = "BW Map") void UseDimensionAltar();
	// 다음 전투 피해량 보너스를 꺼내 쓴다 (겔레온 방치 보너스, 전투 시작에 1회)
	UFUNCTION(BlueprintCallable, Category = "BW Map") float ConsumeNextBattleDamageBonus();

	// 보스를 이긴 뒤: 다음 막으로 (3막이면 false = 판 클리어)
	UFUNCTION(BlueprintCallable, Category = "BW Map") bool AdvanceToNextAct();
	UFUNCTION(BlueprintPure, Category = "BW Map") bool IsRunCleared() const { return bActCleared && CurrentStage >= 3; }

	// --- 보상 ---
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void GenerateCombatRewards(EMapNodeType EncounterType);
	// 보물: 전투 없이 골드 + 카드 보상
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void GenerateTreasureRewards();
	// 전투에서 이겼을 때 (보상 생성, 보스면 막 클리어, 저장)
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void OnBattleWon();
	// 보상 하나를 받는다 (골드·회복은 바로 적용, 카드 보상은 ChooseRewardCard로)
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void ClaimReward(int32 Index);
	// 카드 보상에서 ChoiceIndex번 카드를 덱에 넣는다 (-1이면 건너뜀). 보상은 목록에서 빠진다
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void ChooseRewardCard(int32 RewardIndex, int32 ChoiceIndex);
	// 고를 카드 후보 (이 삶의 방식 + 공용 카드 중 겹치지 않게)
	TArray<FName> RollCardChoices(int32 Count);
	// 다음 카드 보상 하나를 만든다 (파트로바스 방치 보너스·과부하 II 반영)
	FRewardItem MakeCardReward();

	// --- 판 동안 이어지는 덱 ---
	UPROPERTY(BlueprintReadOnly, Category = "BW Deck") TArray<FBWRunCard> RunDeck;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Deck") TSoftObjectPtr<UDataTable> CardDataTable;
	// 행 이름의 카드 (강화면 강화한 값). 없으면 false
	bool GetCardData(FName Row, bool bUpgraded, FBWCardData& OutCard) const;
	UFUNCTION(BlueprintPure, Category = "BW Deck") TArray<FBWCardData> GetRunDeckCards() const;
	UFUNCTION(BlueprintCallable, Category = "BW Deck") void AddRunCard(FName Row, bool bUpgraded = false);
	UFUNCTION(BlueprintCallable, Category = "BW Deck") bool RemoveRunCard(int32 DeckIndex);
	UFUNCTION(BlueprintCallable, Category = "BW Deck") bool UpgradeRunCard(int32 DeckIndex);

	// 효과카드의 지도 사용 (전투 기획서 6장): 지도에서 의미가 있는 효과(HP 회복)만, 같은 카드는 막당 1회
	UPROPERTY(BlueprintReadOnly, Category = "BW Deck") TArray<FName> MapUsedCards;
	bool CanUseCardOnMap(int32 DeckIndex, FString& OutReason) const;
	UFUNCTION(BlueprintCallable, Category = "BW Deck") bool UseCardOnMap(int32 DeckIndex);

	// --- 휴식 · 상점 ---
	UFUNCTION(BlueprintPure, Category = "BW Map") int32 GetRestHealAmount() const;
	UFUNCTION(BlueprintCallable, Category = "BW Map") void RestHeal();
	UFUNCTION(BlueprintCallable, Category = "BW Map") void HealPlayer(int32 Amount);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") float RestHealFraction = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") int32 ShopCardPrice = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") int32 ShopRemovePrice = 75;

	UPROPERTY(BlueprintReadOnly, Category = "BW Stats") TMap<EDimensionType, int32> ConsecutiveCounts;
	UPROPERTY(BlueprintReadOnly, Category = "BW Stats") TMap<EDimensionType, int32> NeglectedCounts;
	void GetCountryStats(EDimensionType Dim, int32& OutConsecutive, int32& OutNeglected);

	// --- [추가] 이어하기: 진행 중인 판 저장 (Saved/SaveGames/BWRun.sav) ---
	static const FString RunSaveSlot;

	// 지금 판을 저장한다 (판 시작, 전투 승리 때 부른다)
	UFUNCTION(BlueprintCallable, Category = "BW Save") bool SaveRun();
	// 저장한 판을 불러와 지금 상태를 덮어쓴다. 불러오면 지도 레벨을 열면 된다
	UFUNCTION(BlueprintCallable, Category = "BW Save") bool LoadRun();
	UFUNCTION(BlueprintPure, Category = "BW Save") bool HasSavedRun() const;
	// 패배하거나 새 판을 확정하면 지운다
	UFUNCTION(BlueprintCallable, Category = "BW Save") void DeleteSavedRun();
	// 타이틀의 '이어하기' 밑에 보여 줄 한 줄 (예: 물리 · 4층 · HP 80/100). 저장이 없으면 빈 글
	UFUNCTION(BlueprintPure, Category = "BW Save") FText GetSavedRunSummary() const;

	// --- [추가] 환경설정 중 엔진 설정(GameUserSettings)에 없는 값: 전체 음량 0~1 ---
	UFUNCTION(BlueprintPure, Category = "BW Settings") float GetMasterVolume() const { return MasterVolume; }
	UFUNCTION(BlueprintCallable, Category = "BW Settings") void SetMasterVolume(float Volume, bool bSave = true);

	virtual void Init() override;

private:
	void ResetStats();
	void ClaimNeglectBonus(EDimensionType Dim, int32 Neglect, FBWArrivalReport& Report);
	FRandomStream& GetMapRandom();
	float MasterVolume = 1.0f;
	FRandomStream MapRandom;
};