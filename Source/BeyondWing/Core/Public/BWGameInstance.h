#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BeyondWing/Combat/Public/BWClassData.h"
#include "BWGameInstance.generated.h"

// 4개 국가(차원) 정의
UENUM(BlueprintType)
enum class EDimensionType : uint8
{
	None,
	Patrovas, Berend, Geleon, Taiga
};

// 노드 타입 정의
UENUM(BlueprintType)
enum class EMapNodeType : uint8
{
	Battle, Elite, Mystery, Rest, Shop, Chest, Boss
};

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

USTRUCT(BlueprintType)
struct FMapNode
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EMapNodeType NodeType = EMapNodeType::Battle;
	UPROPERTY(BlueprintReadOnly) EDimensionType Dimension = EDimensionType::None;
	UPROPERTY(BlueprintReadOnly) bool bIsVisited = false;
	int32 StepIdx = 0;
	int32 DimIdx = 0;
};

USTRUCT(BlueprintType)
struct FMapStep
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FMapNode> Nodes;
};

UENUM(BlueprintType)
enum class ERewardType : uint8
{
	Gold, CardSelection, Relic, CardRemoval, Heal, Equipment
};

USTRUCT(BlueprintType)
struct FRewardItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ERewardType Type = ERewardType::Gold;
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) FString RewardName = "";
	UPROPERTY(BlueprintReadOnly) FString Description = "";
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

	UPROPERTY(BlueprintReadOnly, Category = "BW Map") TArray<FMapStep> WorldMapData;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CurrentStep = -1;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CurrentDimIdx = -1;
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat") EMapNodeType CurrentEncounterType = EMapNodeType::Battle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") int32 VisibilityRange = 2;
	UPROPERTY(BlueprintReadOnly, Category = "BW Reward") TArray<FRewardItem> PendingRewards;

	UFUNCTION(BlueprintCallable, Category = "BW Map") void GetNodeVisibilityState(int32 Step, EMapNodeType Type, bool& bOutIsVisible, bool& bOutIsFogged);
	UFUNCTION(BlueprintCallable, Category = "BW Map") void GenerateWorldMap(int32 TotalSteps = 15);
	UFUNCTION(BlueprintCallable, Category = "BW Map") bool CanMoveToNode(int32 TargetStep, int32 TargetDimIdx);
	UFUNCTION(BlueprintCallable, Category = "BW Map") void SelectMapNode(int32 Step, int32 DimIdx);
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void GenerateCombatRewards(EMapNodeType EncounterType);
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void ClaimReward(int32 Index);

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
	float MasterVolume = 1.0f;
};