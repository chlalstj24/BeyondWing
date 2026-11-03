#include "BeyondWing/Core/Public/BWAutoPlayer.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/BWAugmentRules.h"
#include "BeyondWing/Combat/Public/BWDimensionRules.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/Grid/Public/GridManager.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/UI/Public/BWMapWidget.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

namespace BWAuto
{
	static bool bActive = false;

}

bool UBWAutoPlayer::IsActive() { return BWAuto::bActive; }

void UBWAutoPlayer::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FParse::Value(FCommandLine::Get(), TEXT("-BWAuto="), TargetRuns) || TargetRuns <= 0) return;

	FString ClassName = TEXT("PHA");
	FParse::Value(FCommandLine::Get(), TEXT("-BWClass="), ClassName);
	RunClass = ClassName == TEXT("TEC") ? EClassType::TEC : ClassName == TEXT("INT") ? EClassType::INT
		: ClassName == TEXT("FTH") ? EClassType::FTH : ClassName == TEXT("HP") ? EClassType::HP : EClassType::PHA;

	FString Tag = TEXT("test");
	FParse::Value(FCommandLine::Get(), TEXT("-BWTag="), Tag);
	FilePath = FPaths::ProjectSavedDir() / TEXT("BWAuto") / Tag / FString::Printf(TEXT("%s_%u.csv"), *ClassName, FPlatformProcess::GetCurrentProcessId());
	FMath::RandInit((int32)(FDateTime::Now().GetTicks() % MAX_int32) ^ (int32)FPlatformProcess::GetCurrentProcessId());
	BWAuto::bActive = true;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UBWAutoPlayer::Tick));
}

void UBWAutoPlayer::Deinitialize()
{
	if (TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

void UBWAutoPlayer::Append(const FString& Line)
{
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

FString UBWAutoPlayer::DescribeOwned() const
{
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return FString();
	const FBWAugmentTotals Totals = GI->GetAugmentTotals();
	TArray<FString> Parts;
	for (int32 i = 1; i < Totals.SetCounts.Num(); ++i)
	{
		if (Totals.SetCounts[i] > 0) Parts.Add(FString::Printf(TEXT("%s%d"), *UEnum::GetValueAsName((EBWAugmentSet)i).ToString().RightChop(15), Totals.SetCounts[i]));
	}
	return FString::Join(Parts, TEXT("+"));
}

void UBWAutoPlayer::BeginFight(UBWGameInstance* GI)
{
	FightHPStart = GI->PlayerCurrentHP;
	bFightOpen = true;
	++FightsThisRun;
	TurnNumberSeen = 0;
	TurnActions = 0;
}

void UBWAutoPlayer::EndFight(UBWGameInstance* GI, ABWCharacter* Me, bool bWon)
{
	bFightOpen = false;
	Append(FString::Printf(TEXT("F,%d,%d,%d,%s,%s,%d,%d,%d,%d,%d,%s"), RunsDone, GI->CurrentStage, GI->CurrentStep,
		*UEnum::GetValueAsName(GI->CurrentEncounterType).ToString().RightChop(14), *UEnum::GetValueAsName(GI->GetCurrentDimension()).ToString().RightChop(16),
		Me ? Me->CombatTurnNumber : 0, FightHPStart, GI->PlayerCurrentHP, GI->PlayerMaxHP, bWon ? 1 : 0, *DescribeOwned()));
	if (!bWon) EndRun(TEXT("dead"));
}

void UBWAutoPlayer::EndRun(const TCHAR* Result)
{
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	Append(FString::Printf(TEXT("R,%d,%s,%d,%d,%d,%d,%d,%d,%s"), RunsDone, Result, GI->CurrentStage, GI->CurrentStep, FightsThisRun, GI->CurrentGold, GI->RunDeck.Num(), GI->PlayerMaxHP, *DescribeOwned()));
	++RunsDone;
	bInRun = false;
	FightsThisRun = 0;
}

bool UBWAutoPlayer::Tick(float DeltaTime)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !World->HasBegunPlay()) return true;
	AGridManager* Grid = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(World, AGridManager::StaticClass()));
	ABWPlayerController* PC = Cast<ABWPlayerController>(World->GetFirstPlayerController());
	if (!Grid || !PC) return true;

	// 입탑은 메모리 안에서 동기로 진행한다. 프레임은 가비지 수집을 위해서만 양보한다
	const double End = FPlatformTime::Seconds() + 0.1;
	while (FPlatformTime::Seconds() < End && RunsDone < TargetRuns) Step(PC, Grid);
	if (RunsDone >= TargetRuns) FPlatformMisc::RequestExit(false);
	return true;
}

void UBWAutoPlayer::ApplyOverrides()
{
	// 조정기가 넘긴 수치를 메모리의 표에 덮어쓴다 (에셋은 바꾸지 않는다). 형식: E,행,HP,최소,최대 / C,행,CardPower,시작MP,턴MP,최대HP
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	FString Path;
	TArray<FString> Lines;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-BWOverride="), Path) || !FFileHelper::LoadFileToStringArray(Lines, *Path)) return;
	for (const FString& Line : Lines)
	{
		TArray<FString> P;
		Line.ParseIntoArray(P, TEXT(","));
		if (P.Num() >= 5 && P[0] == TEXT("E") && GI->EnemyDataTable)
		{
			if (FBWEnemyData* Row = GI->EnemyDataTable->FindRow<FBWEnemyData>(*P[1], TEXT("BWAuto"), false))
			{
				Row->MaxHP = FCString::Atoi(*P[2]);
				for (FBWEnemyTurnPattern& Pattern : Row->CombatPatterns)
				{
					for (FBWEnemyPattern& Action : Pattern.Actions) Action.SkillRange = FBWDiceRange(FCString::Atoi(*P[3]), FCString::Atoi(*P[4]));
				}
			}
		}
		else if (P.Num() >= 6 && P[0] == TEXT("C"))
		{
			UDataTable* Classes = GI->ClassDataTable.LoadSynchronous();
			KeepClassTable = Classes; // 가비지 수집으로 내려갔다 다시 읽히면 덮어쓴 값이 사라진다
			if (FBWClassData* Row = Classes ? Classes->FindRow<FBWClassData>(*P[1], TEXT("BWAuto"), false) : nullptr)
			{
				Row->CardPower = FCString::Atof(*P[2]);
				if (Row->Combat.bUsesMP)
				{
					Row->Combat.StartMP = FCString::Atoi(*P[3]);
					Row->Combat.MPPerTurn = FCString::Atoi(*P[4]);
				}
				Row->StartStats.MaxHP = FCString::Atoi(*P[5]);
			}
		}
	}
}

void UBWAutoPlayer::Step(ABWPlayerController* PC, AGridManager* Grid)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!bOverridesDone)
	{
		bOverridesDone = true;
		ApplyOverrides();
	}
	if (!bInRun)
	{
		GI->BeginNewRun(RunClass);
		GI->GenerateWorldMap(15);
		if (!MapWidget) MapWidget = CreateWidget<UBWMapWidget>(PC, UBWMapWidget::StaticClass());
		MapWidget->bBusy = false;
		MapWidget->bAutoBattlePending = false;
		MapWidget->bAutoAugmentPending = false;
		MapWidget->ModalOptions.Reset();
		MapWidget->CloseModal();
		bInRun = true;
		LastStep = -2;
		IdleSteps = 0;
		MapWidget->ProcessPendingFlow();
		return;
	}
	if (MapWidget->bAutoBattlePending)
	{
		MapWidget->bAutoBattlePending = false;
		IdleSteps = 0;
		RunBattle(PC, Grid);
		if (!bInRun) return;
		MapWidget->bBusy = false;
		MapWidget->ProcessPendingFlow();
		return;
	}
	if (++IdleSteps > 2000)
	{
		UE_LOG(LogTemp, Error, TEXT("[BWAuto] Stuck on the map"));
		EndRun(TEXT("stuck:map"));
		return;
	}
	MapTick(MapWidget);
}

void UBWAutoPlayer::RunBattle(ABWPlayerController* PC, AGridManager* Grid)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());

	// 레벨을 다시 불러오지 않고 판과 컨트롤러 상태만 비운다
	PC->UnPossess();
	Grid->ResetForAutoBattle();
	PC->PossessedBWCharacter = nullptr;
	PC->HandWidget = nullptr;
	PC->BattleHUDWidget = nullptr;
	PC->bActionInProgress = false;
	PC->bEnemyPhase = false;
	PC->bIsMovementMode = false;
	PC->bIsDuelTargetingMode = false;
	PC->PendingDuelTarget = nullptr;
	PC->EnemyTurnQueue.Empty();

	// 적과 가까운 앞줄, 가운데부터 배치
	BeginFight(GI);
	for (int32 X = Grid->AllyZoneColumns - 1; X >= 0 && !PC->PossessedBWCharacter; --X)
	{
		for (const int32 Y : { 3, 4, 2, 5, 1, 6, 0, 7 })
		{
			if (Grid->GetTerrainAt(FIntPoint(X, Y)) == EBWTerrainType::Thorns) continue;
			if (PC->PlaceCharacterAndStartCombat(FIntPoint(X, Y))) break;
		}
	}
	ABWCharacter* Me = PC->PossessedBWCharacter;
	if (!Me)
	{
		bFightOpen = false;
		EndRun(TEXT("stuck:place"));
		return;
	}

	for (int32 Guard = 0; !Grid->bBattleOver; ++Guard)
	{
		if (Guard > 3000 || Me->CombatTurnNumber > 60)
		{
			FString State = FString::Printf(TEXT("turn %d guard %d me %s HP %d move %d stun %d dim %s:"), Me->CombatTurnNumber, Guard, *Me->OwnGridCoordinate.ToString(), Me->CurrentHP, Me->RemainingMove, Me->IsStunned() ? 1 : 0, *UEnum::GetValueAsString(PC->Battlefield.Dimension));
			for (ABWCharacter* Enemy : PC->GetEnemiesInReach(99)) State += FString::Printf(TEXT(" [%s HP %d/%d stun %d speed %d]"), *Enemy->OwnGridCoordinate.ToString(), Enemy->CurrentHP, Enemy->MaxHP, Enemy->IsStunned() ? 1 : 0, Enemy->RolledSpeed);
			UE_LOG(LogTemp, Error, TEXT("[BWAuto] Stuck: %s"), *State);
			bFightOpen = false;
			EndRun(TEXT("stuck:battle"));
			return;
		}
		if (TurnNumberSeen != Me->CombatTurnNumber)
		{
			TurnNumberSeen = Me->CombatTurnNumber;
			TurnActions = 0;
		}
		PlayTurnStep(PC, Grid, Me);
	}
	EndFight(GI, Me, Me->CurrentHP > 0);
}

int32 UBWAutoPlayer::ChooseGuardMP(const ABWCharacter* Me, const FBWDiceRange& Incoming)
{
	// HP가 넉넉하면 평균 피해의 절반만, 위험하면 최댓값까지 막는다
	const int32 Max = Me->GetMaxGuardMP();
	if (Max <= 0) return 0;
	const float Ratio = (float)Me->CurrentHP / FMath::Max(1, Me->MaxHP);
	const int32 Per = FMath::Max(1, FMath::RoundToInt(Me->Defense.ShieldPerMP));
	const int32 Cover = FMath::Min(Max, FMath::CeilToInt((float)Incoming.Max / Per));
	const int32 Avg = FMath::Min(Max, FMath::CeilToInt((Incoming.Min + Incoming.Max) * 0.5f / Per));
	return Ratio < 0.5f ? Cover : Avg / (Ratio > 0.8f ? 2 : 1);
}

void UBWAutoPlayer::PlayTurnStep(ABWPlayerController* PC, AGridManager* Grid, ABWCharacter* Me)
{
	if (++TurnActions > 30)
	{
		PC->RequestEndTurn();
		return;
	}
	const UDeckComponent* Deck = Me->DeckComponent;
	const bool bCanDuel = Deck->CanOpenDuel() && !Me->IsStunned() && !Me->Life.bNoDuelThisTurn;
	const int32 Reach = GetBestReach(PC, Me);
	const bool bCanDuelThisTurn = bCanDuel && (PC->GetEnemiesInReach(Reach).Num() > 0 || Me->RemainingMove > 0);

	if (TryEffectCard(PC, Me, bCanDuelThisTurn)) return;
	if (bCanDuel && TryDuel(PC, Me)) return;
	if (bCanDuel && TryMove(PC, Grid, Me)) return;
	PC->RequestEndTurn();
}

bool UBWAutoPlayer::TryEffectCard(ABWPlayerController* PC, ABWCharacter* Me, bool bCanDuelThisTurn)
{
	const UDeckComponent* Deck = Me->DeckComponent;
	const float Ratio = (float)Me->CurrentHP / FMath::Max(1, Me->MaxHP);
	for (int32 i = 0; i < Deck->Hand.Num(); ++i)
	{
		const FBWCardData& Card = Deck->Hand[i];
		if (Card.CardKind != ECardKind::Effect || Deck->CanUseCard(i, Me->CurrentMP) != EBWCardUseResult::Ok) continue;
		FString Reason;
		if (!PC->CanUseLifestyleCard(Card, Reason)) continue;
		// 격돌이나 이동을 막는 카드는 이번 턴에 어차피 싸울 수 없을 때만 쓴다
		// 명상은 MP가 모자라고 맞닿은 적이 없을 때 격돌 대신 쓴다
		const bool bMeditateNow = Card.Special == EBWCardSpecial::Meditate && (Me->CurrentMP < 3 || (Me->CurrentMP < 6 && (Deck->Hand.Num() >= 8 || PC->GetEnemiesInReach(1).Num() == 0)));
		if (Card.Special == EBWCardSpecial::Meditate ? !bMeditateNow : (Card.bNoDuelThisTurn || Card.bEndMoveThisTurn) && bCanDuelThisTurn) continue;
		if (Deck->Hand.Num() < 8 && Card.EffectHeal > 0 && Me->MaxHP - Me->CurrentHP < Card.EffectHeal && Card.EffectBlock <= 0 && Card.EffectDraw <= 0) continue; // 손패가 차면 일단 쓴다
		if (Card.SelfDamage > 0 && Me->CharacterClass != EClassType::PHA && Ratio < 0.4f) continue;
		if (Card.SelfDamage >= Me->CurrentHP) continue;
		PC->UseEffectCard(i);
		return true;
	}
	return false;
}

int32 UBWAutoPlayer::GetBestReach(ABWPlayerController* PC, ABWCharacter* Me) const
{
	const UDeckComponent* Deck = Me->DeckComponent;
	int32 Reach = PC->GetReachForHandIndex(INDEX_NONE);
	for (int32 i = 0; i < Deck->Hand.Num(); ++i)
	{
		const FBWCardData& Card = Deck->Hand[i];
		FString Reason;
		if (Card.CardKind == ECardKind::Attack && Deck->CanUseCard(i, Me->CurrentMP) == EBWCardUseResult::Ok
			&& PC->CanUseLifestyleCard(Card, Reason))
		{
			Reach = FMath::Max(Reach, PC->GetReachForHandIndex(i));
		}
	}
	return Reach;
}

bool UBWAutoPlayer::TryDuel(ABWPlayerController* PC, ABWCharacter* Me)
{
	const UDeckComponent* Deck = Me->DeckComponent;
	ABWCharacter* BestEnemy = nullptr;
	int32 BestIndex = INDEX_NONE;
	float BestScore = -MAX_flt;

	auto Consider = [&](int32 Index)
	{
		const FBWCardData* Card = Index == INDEX_NONE ? nullptr : &Deck->Hand[Index];
		for (ABWCharacter* Enemy : PC->GetEnemiesInReach(PC->GetReachForHandIndex(Index)))
		{
			const FBWDuelSide Mine = PC->MakeFieldAttackSide(Me, Card, Enemy);
			const FBWDuelForecast F = UBWDuelRules::ForecastDuel(Mine, PC->MakeCounterSide(Enemy, Mine));
			const float WinAvg = (F.WinDamage.Min + F.WinDamage.Max) * 0.5f * (1.0f + F.CritChance * (F.CritMultiplier - 1.0f));
			const float LoseAvg = (F.LoseDamage.Min + F.LoseDamage.Max) * 0.5f;
			float Score = F.WinChance * WinAvg - F.LoseChance * LoseAvg * 0.6f;
			if (F.WinDamage.Min >= Enemy->CurrentHP + Enemy->Block) Score += 10.0f * F.WinChance;
			if (Enemy->bChanting) Score += 8.0f * F.WinChance; // 영창은 끊는다
			if (Card) Score -= Card->SelfDamage * (Me->CharacterClass == EClassType::PHA ? 0.2f : 0.6f) + Card->MPCost * 0.3f;
			if (Score > BestScore) { BestScore = Score; BestEnemy = Enemy; BestIndex = Index; }
		}
	};

	Consider(INDEX_NONE);
	for (int32 i = 0; i < Deck->Hand.Num(); ++i)
	{
		const FBWCardData& Card = Deck->Hand[i];
		FString Reason;
		if (Card.CardKind != ECardKind::Attack || Deck->CanUseCard(i, Me->CurrentMP) != EBWCardUseResult::Ok) continue;
		if (!PC->CanUseLifestyleCard(Card, Reason)) continue;
		if (Card.SelfDamage >= Me->CurrentHP) continue;
		Consider(i);
	}
	return BestEnemy && PC->OpenDuel(BestEnemy, BestIndex);
}

bool UBWAutoPlayer::TryMove(ABWPlayerController* PC, AGridManager* Grid, ABWCharacter* Me)
{
	if (Me->RemainingMove <= 0 || Me->IsStunned()) return false;
	const TArray<ABWCharacter*> Enemies = PC->GetEnemiesInReach(99);
	if (Enemies.Num() == 0) return false;

	TMap<FIntPoint, int32> Distance;
	TMap<FIntPoint, FIntPoint> Parent;
	Grid->BuildMoveField(Me->OwnGridCoordinate, Me->RemainingMove, false, Distance, Parent);

	const int32 Reach = GetBestReach(PC, Me);
	auto Gap = [&](const FIntPoint& Cell)
	{
		int32 Best = MAX_int32;
		for (const ABWCharacter* Enemy : Enemies)
		{
			const FIntPoint D = Enemy->OwnGridCoordinate - Cell;
			Best = FMath::Min(Best, FMath::Abs(D.X) + FMath::Abs(D.Y));
		}
		return Best;
	};
	auto Cost = [&](const FIntPoint& Cell)
	{
		// 사거리 끝에 서는 쪽이 낫다. 가시덤불은 피하고 공명석·방벽은 고른다
		float C = FMath::Abs(Gap(Cell) - Reach) * 2.0f + (Gap(Cell) > Reach ? 1.0f : 0.0f);
		switch (Grid->GetTerrainAt(Cell))
		{
		case EBWTerrainType::Thorns: C += 3.0f; break;
		case EBWTerrainType::Resonance:
		case EBWTerrainType::Cover: C -= 1.0f; break;
		default: break;
		}
		if (UBWDimensionRules::IsOnManaLine(PC->Battlefield, Cell)) C -= 1.5f; // 마력선 위에서 싸운다
		return C;
	};

	FIntPoint Best = Me->OwnGridCoordinate;
	float BestCost = Cost(Best) - 0.01f;
	for (const TPair<FIntPoint, int32>& Pair : Distance)
	{
		if (Pair.Key == Me->OwnGridCoordinate || Grid->GetCharacterAtCoordinate(Pair.Key) || Grid->IsWallOrOutside(Pair.Key)) continue;
		const float C = Cost(Pair.Key);
		if (C < BestCost) { BestCost = C; Best = Pair.Key; }
	}
	if (Best != Me->OwnGridCoordinate) return PC->TryMovePlayerTo(Best);
	// 직선 거리로는 더 못 줄인다 (벽 뒤): 실제 길을 따라 돌아간다
	if (Gap(Me->OwnGridCoordinate) <= Reach) return false;
	const TArray<FIntPoint> Around = Grid->FindRushPath(Me->OwnGridCoordinate, Enemies[0]->OwnGridCoordinate, Me->RemainingMove, false);
	return Around.Num() > 0 && PC->TryMovePlayerTo(Around.Last());
}

// ---------------------------------------------------------------------------------------------
// 지도
// ---------------------------------------------------------------------------------------------

float UBWAutoPlayer::ScoreCard(const FBWCardData& Card) const
{
	float Score = Card.Rarity == EBWCardRarity::Rare ? 3.0f : Card.Rarity == EBWCardRarity::Uncommon ? 2.0f : 1.0f;
	if (Card.ClassType == RunClass) Score += 0.5f;
	if (Card.CardKind == ECardKind::Attack) Score += 0.3f;
	return Score + FMath::FRand() * 0.2f;
}

int32 UBWAutoPlayer::PickModalOption(UBWMapWidget* Map)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	const auto& Options = Map->ModalOptions;
	const FString Title = Map->GetModalTitle();
	const float Ratio = (float)GI->PlayerCurrentHP / FMath::Max(1, GI->PlayerMaxHP);

	auto FindLabel = [&](const TCHAR* Text)
	{
		for (int32 i = 0; i < Options.Num(); ++i)
		{
			if (Options[i].bEnabled && Options[i].Label.ToString().Contains(Text)) return i;
		}
		return (int32)INDEX_NONE;
	};
	auto BestCard = [&](bool bLowest, bool bPricedOnly)
	{
		int32 Best = INDEX_NONE;
		float BestScore = bLowest ? MAX_flt : -MAX_flt;
		for (int32 i = 0; i < Options.Num(); ++i)
		{
			const auto& Option = Options[i];
			if (!Option.bEnabled || !Option.bShowCard || Option.bSoldOut || (bPricedOnly && Option.Price < 0)) continue;
			const float Score = ScoreCard(Option.Card);
			if (bLowest ? Score < BestScore : Score > BestScore) { BestScore = Score; Best = i; }
		}
		return Best;
	};

	if (Title.StartsWith(TEXT("덱 ")))
	{
		const int32 Card = BestCard(false, false);
		return Ratio < 0.7f && Card != INDEX_NONE ? Card : FindLabel(TEXT("닫기"));
	}
	if (Title == TEXT("휴식"))
	{
		return Ratio < 0.7f ? 0 : FMath::Max(FindLabel(TEXT("카드 강화")), 0);
	}
	if (Map->ModalStyle == UBWMapWidget::EModalStyle::Shop)
	{
		const int32 Card = BestCard(false, true);
		if (Card != INDEX_NONE && ShopBuys < 3 && ScoreCard(Options[Card].Card) >= 2.0f) { ++ShopBuys; return Card; }
		ShopBuys = 0;
		return FindLabel(TEXT("떠나기"));
	}
	if (Title.Contains(TEXT("삭제")) || Title.Contains(TEXT("제거")))
	{
		const int32 Card = BestCard(true, false);
		if (Card != INDEX_NONE) return Card;
	}
	if (const int32 Card = BestCard(false, false); Card != INDEX_NONE) return Card;

	// 이벤트: 공짜는 받고, HP가 넉넉하면 대가도 낸다
	if (const int32 Free = FindLabel(TEXT("줍습니다")); Free != INDEX_NONE) return Free;
	if (Ratio > 0.7f)
	{
		for (const TCHAR* Text : { TEXT("HP 6을 잃고"), TEXT("HP 5를 잃고"), TEXT("골드 30을 내고") })
		{
			if (const int32 Index = FindLabel(Text); Index != INDEX_NONE) return Index;
		}
	}
	for (const TCHAR* Text : { TEXT("지나갑니다"), TEXT("건너뛰기"), TEXT("타이틀로") })
	{
		if (const int32 Index = FindLabel(Text); Index != INDEX_NONE) return Index;
	}
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		if (Options[i].bEnabled) return i;
	}
	return INDEX_NONE;
}

void UBWAutoPlayer::PickAugment(UBWMapWidget* Map)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	const FBWAugmentTotals Totals = GI->GetAugmentTotals();
	int32 Best = 0;
	float BestScore = -MAX_flt;
	for (int32 i = 0; i < GI->AugmentChoices.Num(); ++i)
	{
		FBWAugmentData Data;
		if (!GI->GetAugmentData(GI->AugmentChoices[i], Data)) continue;
		// 세트를 노린다: 가진 세트 > 전용 > 나머지. 도박은 조금 꺼린다
		const int32 Have = Data.Set != EBWAugmentSet::None && Totals.SetCounts.IsValidIndex((int32)Data.Set) ? Totals.SetCounts[(int32)Data.Set] : 0;
		float Score = Have * 2.0f + (Data.Set == EBWAugmentSet::None ? 1.5f : 1.0f) + FMath::FRand() * 0.5f;
		if (Data.Kind == EBWAugmentKind::Gamble) Score -= 0.5f;
		if (Score > BestScore) { BestScore = Score; Best = i; }
	}
	GI->ChooseAugment(Best);
	Map->FinishAutoAugment();
}

void UBWAutoPlayer::PickNextNode(UBWMapWidget* Map)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	const int32 Step = GI->CurrentStep + 1;
	if (!GI->WorldMapData.IsValidIndex(Step)) return;
	const float Ratio = (float)GI->PlayerCurrentHP / FMath::Max(1, GI->PlayerMaxHP);

	auto Score = [Ratio](EMapNodeType Type)
	{
		if (Ratio < 0.5f)
		{
			switch (Type)
			{
			case EMapNodeType::Rest: return 5.0f;
			case EMapNodeType::Elite: return 0.0f;
			case EMapNodeType::Battle: return 1.0f;
			default: return 3.0f;
			}
		}
		switch (Type)
		{
		case EMapNodeType::Elite: return Ratio > 0.75f ? 4.0f : 1.0f;
		case EMapNodeType::Battle: return 3.0f;
		case EMapNodeType::Rest: return Ratio < 0.7f ? 4.0f : 1.0f;
		case EMapNodeType::Chest: return 3.5f;
		default: return 2.0f;
		}
	};

	int32 BestRow = INDEX_NONE;
	float BestScore = -MAX_flt;
	const TArray<FMapNode>& Nodes = GI->WorldMapData[Step].Nodes;
	for (int32 Row = 0; Row < Nodes.Num(); ++Row)
	{
		if (!GI->CanMoveToNode(Step, Row)) continue;
		const float S = Score(Nodes[Row].NodeType) + FMath::FRand() * 0.3f + (Row == GI->CurrentDimIdx ? 0.0f : -0.2f);
		if (S > BestScore) { BestScore = S; BestRow = Row; }
	}
	if (BestRow != INDEX_NONE) Map->TryMoveTo(Step, BestRow);
}

void UBWAutoPlayer::MapTick(UBWMapWidget* Map)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (Map->bAutoAugmentPending)
	{
		PickAugment(Map);
		return;
	}
	if (Map->IsModalOpen())
	{
		const bool bRunOver = Map->GetModalTitle() == TEXT("탑 정복");
		const int32 Index = PickModalOption(Map);
		if (Index == INDEX_NONE) return;
		if (bRunOver)
		{
			EndRun(TEXT("clear"));
			return;
		}
		Map->ChooseModalOption(Index);
		return;
	}
	if (Map->bBusy) return;

	// 지도에서 쓸 수 있는 회복 카드는 칸마다 한 번 살펴본다
	if (LastStep != GI->CurrentStep)
	{
		LastStep = GI->CurrentStep;
		bDeckChecked = false;
	}
	if (!bDeckChecked && GI->PlayerCurrentHP < GI->PlayerMaxHP * 0.7f)
	{
		bDeckChecked = true;
		Map->ShowDeck();
		return;
	}
	PickNextNode(Map);
}
