#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWAutoPlayer.generated.h"

class ABWCharacter;
class ABWPlayerController;
class AGridManager;
class UBWGameInstance;
class UBWMapWidget;

// 밸런스 측정용 봇. 명령줄 -BWAuto=판수 -BWClass=PHA 로 켜면 진짜 게임을 타이틀부터 입탑 끝까지 플레이하고
// Saved/BWAuto/에 전투·입탑 기록을 남긴다. 규칙은 전부 실제 코드를 쓰고, 연출(격돌 화면, 히트스톱, 차원문, 결과 화면)만 건너뛴다
UCLASS()
class BEYONDWING_API UBWAutoPlayer : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 봇이 돌고 있는지 (게임 코드가 연출을 건너뛸 때 본다)
	static bool IsActive();
	// 화면 없이 고르는 값 (마나 가드 MP)
	static int32 ChooseGuardMP(const ABWCharacter* Me, const FBWDiceRange& Incoming);

private:
	bool Tick(float DeltaTime);
	void Step(ABWPlayerController* PC, AGridManager* Grid);
	void ApplyOverrides();
	void RunBattle(ABWPlayerController* PC, AGridManager* Grid);
	void MapTick(UBWMapWidget* Map);

	// 내 턴 행동 하나. 할 일이 없으면 턴을 끝낸다
	void PlayTurnStep(ABWPlayerController* PC, AGridManager* Grid, ABWCharacter* Me);
	bool TryEffectCard(ABWPlayerController* PC, ABWCharacter* Me, bool bCanDuelThisTurn);
	bool TryDuel(ABWPlayerController* PC, ABWCharacter* Me);
	bool TryMove(ABWPlayerController* PC, AGridManager* Grid, ABWCharacter* Me);
	int32 GetBestReach(ABWPlayerController* PC, ABWCharacter* Me) const;

	// 지도
	int32 PickModalOption(UBWMapWidget* Map);
	void PickAugment(UBWMapWidget* Map);
	void PickNextNode(UBWMapWidget* Map);
	float ScoreCard(const FBWCardData& Card) const;

	// 기록
	void BeginFight(UBWGameInstance* GI);
	void EndFight(UBWGameInstance* GI, ABWCharacter* Me, bool bWon);
	void EndRun(const TCHAR* Result);
	void Append(const FString& Line);
	FString DescribeOwned() const;

	FTSTicker::FDelegateHandle TickHandle;
	FString FilePath;
	EClassType RunClass = EClassType::PHA;
	int32 TargetRuns = 0;
	int32 RunsDone = 0;
	bool bInRun = false;
	bool bFightOpen = false;
	bool bOverridesDone = false;
	bool bDeckChecked = false;
	int32 LastStep = -2;
	int32 FightHPStart = 0;
	int32 FightsThisRun = 0;
	int32 ShopBuys = 0;
	int32 TurnActions = 0;
	int32 TurnNumberSeen = 0;
	int32 IdleSteps = 0;
	UPROPERTY() TObjectPtr<UBWMapWidget> MapWidget = nullptr;
	UPROPERTY() TObjectPtr<UDataTable> KeepClassTable = nullptr;
};
