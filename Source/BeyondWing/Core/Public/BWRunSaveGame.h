#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BWRunSaveGame.generated.h"

// '이어하기'용 진행 중인 판 저장. 판을 시작할 때와 전투에서 이길 때마다 덮어쓰고, 패배하면 지운다.
// 지도 위치(지도에서 고른 칸)와 보상까지 들어 있어서 이어하면 마지막으로 저장한 지도 화면으로 돌아간다.
UCLASS()
class BEYONDWING_API UBWRunSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 2: 지도 기획서 구현(16칸 지도, 차원문, 막, 판 덱) — 옛 저장은 지도 형식이 달라 불러오지 않는다
	static constexpr int32 CurrentVersion = 2;

	UPROPERTY() int32 Version = CurrentVersion;
	UPROPERTY() FDateTime SavedAt;

	// 삶의 방식과 능력치
	UPROPERTY() EClassType SelectedClass = EClassType::PHA;
	UPROPERTY() int32 PlayerMaxHP = 100;
	UPROPERTY() int32 PlayerCurrentHP = 100;
	UPROPERTY() int32 PlayerPHA = 4;
	UPROPERTY() int32 PlayerTEC = 4;
	UPROPERTY() int32 PlayerINT = 4;
	UPROPERTY() int32 PlayerFTH = 4;
	UPROPERTY() int32 PlayerDeckCount = 10;

	// 진행
	UPROPERTY() int32 CurrentStage = 1;
	UPROPERTY() int32 CurrentGold = 0;
	UPROPERTY() TArray<FName> OwnedAugments;
	UPROPERTY() TArray<FRewardItem> PendingRewards;
	UPROPERTY() EMapNodeType CurrentEncounterType = EMapNodeType::Battle;

	// 지도
	UPROPERTY() TArray<FMapStep> WorldMapData;
	UPROPERTY() int32 CurrentStep = -1;
	UPROPERTY() int32 CurrentDimIdx = -1;
	UPROPERTY() TMap<EDimensionType, int32> ConsecutiveCounts;
	UPROPERTY() TMap<EDimensionType, int32> NeglectedCounts;
	UPROPERTY() TArray<EDimensionType> RowDimensions;
	UPROPERTY() bool bRiftJumpReady = false;
	UPROPERTY() TMap<EDimensionType, int32> BonusClaimCounts;
	UPROPERTY() FBWMapBonusState MapBonus;
	UPROPERTY() int32 PendingCardRemovals = 0;
	UPROPERTY() bool bActCleared = false;
	UPROPERTY() int32 AugmentPickedAct = 0;

	// 판 덱 (보상으로 얻은 카드·강화·삭제가 반영된 덱)
	UPROPERTY() TArray<FBWRunCard> RunDeck;
	UPROPERTY() TArray<FName> MapUsedCards; // 이번 막에 지도에서 쓴 효과카드
};
