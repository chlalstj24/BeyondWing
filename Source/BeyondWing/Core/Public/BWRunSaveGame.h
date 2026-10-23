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
	static constexpr int32 CurrentVersion = 1;

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
};
