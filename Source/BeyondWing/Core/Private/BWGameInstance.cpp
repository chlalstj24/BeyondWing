#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/UI/Public/BWDuelWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/UI/Public/BWBattleResultWidget.h"
#include "BeyondWing/UI/Public/BWClassSelectWidget.h"
#include "BeyondWing/UI/Public/BWGuardChoiceWidget.h"
#include "BeyondWing/UI/Public/BWMapWidget.h"
#include "BeyondWing/UI/Public/BWPauseWidget.h"
#include "BeyondWing/UI/Public/BWSettingsWidget.h"
#include "BeyondWing/UI/Public/BWTitleMenuWidget.h"
#include "BeyondWing/Combat/Public/BWAugmentRules.h"
#include "BeyondWing/Core/Public/BWAutoPlayer.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "BeyondWing/Core/Public/BWRunSaveGame.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

UBWGameInstance::UBWGameInstance()
{
	ResetStats();

	// 전투 기획서 3장 직업별 속도
	ClassSpeedDice.Add(EClassType::PHA, FBWDiceRange(3, 6));
	ClassSpeedDice.Add(EClassType::TEC, FBWDiceRange(3, 7));
	ClassSpeedDice.Add(EClassType::INT, FBWDiceRange(2, 5));
	ClassSpeedDice.Add(EClassType::FTH, FBWDiceRange(2, 5));
	ClassSpeedDice.Add(EClassType::HP,  FBWDiceRange(2, 5));

	ClassDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/HUD/Data/DT_Classes.DT_Classes")));
	CardDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/HUD/Data/BeyondWing_CardDatabase_Template.BeyondWing_CardDatabase_Template")));
}

const FString UBWGameInstance::RunSaveSlot = TEXT("BWRun");
static const TCHAR* BWSettingsSection = TEXT("/Script/BeyondWing.BWSettings");

void UBWGameInstance::BeginBattleSnapshot()
{
	bInBattle = true;
	BattleStartHP = PlayerCurrentHP;
	BattleStartSoul = SoulBullets;
	BattleStartBonus = MapBonus;
}

namespace BWKeys
{
	class FProcessor : public IInputProcessor
	{
	public:
		explicit FProcessor(UBWGameInstance* InGI) : GI(InGI) {}
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			UBWGameInstance* Instance = GI.Get();
			if (!Instance || Event.IsRepeat()) return false;
			if (Event.GetKey() == EKeys::Escape) return Instance->HandleEscapeKey();
			if (Event.GetKey() == EKeys::SpaceBar || Event.GetKey() == EKeys::Enter) return Instance->HandleConfirmKey();
			return false;
		}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
		{
			UBWGameInstance* Instance = GI.Get();
			return Instance && Event.GetEffectingButton() == EKeys::LeftMouseButton && Instance->HandleDuelInput();
		}
	private:
		TWeakObjectPtr<UBWGameInstance> GI;
	};

	template <typename T> static T* FindOnScreen(UWorld* World)
	{
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, T::StaticClass(), false);
		for (UUserWidget* Widget : Found)
		{
			if (Widget && Widget->IsInViewport()) return Cast<T>(Widget);
		}
		return nullptr;
	}
}

void UBWGameInstance::Init()
{
	Super::Init();

	// [추가] Esc·Space는 입력 초점과 상관없이 받는다 (PIE의 Esc 종료보다 먼저)
	if (FSlateApplication::IsInitialized() && !UBWAutoPlayer::IsActive())
	{
		InputProcessor = MakeShared<BWKeys::FProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
	// 저장해 둔 전체 음량을 켜자마자 적용
	float SavedVolume = 1.0f;
	if (GConfig && GConfig->GetFloat(BWSettingsSection, TEXT("MasterVolume"), SavedVolume, GGameUserSettingsIni)) SetMasterVolume(SavedVolume, false);
}

void UBWGameInstance::SetMasterVolume(float Volume, bool bSave)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	FApp::SetVolumeMultiplier(MasterVolume);
	if (bSave && GConfig)
	{
		GConfig->SetFloat(BWSettingsSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

bool UBWGameInstance::SaveRun()
{
	if (UBWAutoPlayer::IsActive()) return false; // 측정 봇이 플레이어의 이어하기를 덮어쓰지 않게
	UBWRunSaveGame* Save = Cast<UBWRunSaveGame>(UGameplayStatics::CreateSaveGameObject(UBWRunSaveGame::StaticClass()));
	if (!Save) return false;
	Save->SavedAt = FDateTime::Now();
	Save->SelectedClass = SelectedClass;
	Save->PlayerMaxHP = PlayerMaxHP;
	Save->PlayerCurrentHP = PlayerCurrentHP;
	Save->PlayerPHA = PlayerPHA;
	Save->PlayerTEC = PlayerTEC;
	Save->PlayerINT = PlayerINT;
	Save->PlayerFTH = PlayerFTH;
	Save->PlayerDeckCount = PlayerDeckCount;
	Save->SoulBullets = SoulBullets;
	Save->CurrentStage = CurrentStage;
	Save->CurrentGold = CurrentGold;
	Save->OwnedAugments = OwnedAugments;
	Save->PendingRewards = PendingRewards;
	Save->CurrentEncounterType = CurrentEncounterType;
	Save->WorldMapData = WorldMapData;
	Save->CurrentStep = CurrentStep;
	Save->CurrentDimIdx = CurrentDimIdx;
	Save->ConsecutiveCounts = ConsecutiveCounts;
	Save->NeglectedCounts = NeglectedCounts;
	Save->RowDimensions = RowDimensions;
	Save->bRiftJumpReady = bRiftJumpReady;
	Save->BonusClaimCounts = BonusClaimCounts;
	Save->MapBonus = MapBonus;
	Save->PendingCardRemovals = PendingCardRemovals;
	Save->bActCleared = bActCleared;
	Save->PendingStatPoints = PendingStatPoints;
	Save->PlayerDefensePoints = PlayerDefensePoints;
	Save->AugmentPickedAct = AugmentPickedAct;
	Save->AugmentBossPickedAct = AugmentBossPickedAct;
	Save->RunDeck = RunDeck;
	Save->MapUsedCards = MapUsedCards;
	if (bInBattle)
	{
		Save->bResumeBattle = true;
		Save->PlayerCurrentHP = BattleStartHP;
		Save->SoulBullets = BattleStartSoul;
		Save->MapBonus = BattleStartBonus;
	}
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, RunSaveSlot, 0);
	UE_LOG(LogTemp, Warning, TEXT("[BWSave] Run saved (%s): step %d, HP %d/%d"), bOk ? TEXT("ok") : TEXT("FAILED"), CurrentStep, PlayerCurrentHP, PlayerMaxHP);
	return bOk;
}

bool UBWGameInstance::LoadRun()
{
	const UBWRunSaveGame* Save = Cast<UBWRunSaveGame>(UGameplayStatics::LoadGameFromSlot(RunSaveSlot, 0));
	if (!Save || Save->Version != UBWRunSaveGame::CurrentVersion || Save->WorldMapData.Num() == 0) return false;
	SelectedClass = Save->SelectedClass;
	bResumeBattle = Save->bResumeBattle;
	bInBattle = false;
	PlayerMaxHP = Save->PlayerMaxHP;
	PlayerCurrentHP = Save->PlayerCurrentHP;
	PlayerPHA = Save->PlayerPHA;
	PlayerTEC = Save->PlayerTEC;
	PlayerINT = Save->PlayerINT;
	PlayerFTH = Save->PlayerFTH;
	PlayerDeckCount = Save->PlayerDeckCount;
	SoulBullets = Save->SoulBullets;
	CurrentStage = Save->CurrentStage;
	CurrentGold = Save->CurrentGold;
	OwnedAugments = Save->OwnedAugments;
	PendingRewards = Save->PendingRewards;
	CurrentEncounterType = Save->CurrentEncounterType;
	WorldMapData = Save->WorldMapData;
	CurrentStep = Save->CurrentStep;
	CurrentDimIdx = Save->CurrentDimIdx;
	ConsecutiveCounts = Save->ConsecutiveCounts;
	NeglectedCounts = Save->NeglectedCounts;
	RowDimensions = Save->RowDimensions;
	bRiftJumpReady = Save->bRiftJumpReady;
	BonusClaimCounts = Save->BonusClaimCounts;
	MapBonus = Save->MapBonus;
	PendingCardRemovals = Save->PendingCardRemovals;
	bActCleared = Save->bActCleared;
	PendingStatPoints = Save->PendingStatPoints;
	PlayerDefensePoints = Save->PlayerDefensePoints;
	AugmentPickedAct = Save->AugmentPickedAct;
	AugmentBossPickedAct = Save->AugmentBossPickedAct;
	RunDeck = Save->RunDeck;
	MapUsedCards = Save->MapUsedCards;
	PlayerDeckCount = RunDeck.Num() > 0 ? RunDeck.Num() : PlayerDeckCount;
	AugmentChoices.Reset();
	LastArrival = FBWArrivalReport();
	UE_LOG(LogTemp, Warning, TEXT("[BWSave] Run loaded: %s, step %d, HP %d/%d"), *UEnum::GetValueAsString(SelectedClass), CurrentStep, PlayerCurrentHP, PlayerMaxHP);
	return true;
}

void UBWGameInstance::Shutdown()
{
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	InputProcessor.Reset();
	Super::Shutdown();
}

bool UBWGameInstance::HandleDuelInput()
{
	UWorld* World = GetWorld();
	UBWDuelWidget* Duel = World ? BWKeys::FindOnScreen<UBWDuelWidget>(World) : nullptr;
	return Duel && Duel->HandleAdvanceInput();
}

bool UBWGameInstance::HandleConfirmKey()
{
	if (HandleDuelInput()) return true;
	UWorld* World = GetWorld();
	UBWBattleResultWidget* Result = World ? BWKeys::FindOnScreen<UBWBattleResultWidget>(World) : nullptr;
	if (!Result || !Result->IsShowing()) return false;
	Result->Continue();
	return true;
}

bool UBWGameInstance::HandleEscapeKey()
{
	UWorld* World = GetWorld();
	if (!World) return false;
	using namespace BWKeys;

	// 위에 뜬 창부터 닫는다
	if (UBWSettingsWidget* Settings = FindOnScreen<UBWSettingsWidget>(World)) { Settings->Close(); return true; }
	if (UBWPauseWidget* Pause = FindOnScreen<UBWPauseWidget>(World)) { Pause->Close(); return true; }
	if (FindOnScreen<UBWGuardChoiceWidget>(World)) return false; // 마나 가드 창은 Esc = 안 함
	if (FindOnScreen<UBWBattleResultWidget>(World) || FindOnScreen<UBWAugmentSelectWidget>(World) || FindOnScreen<UBWClassSelectWidget>(World) || FindOnScreen<UBWTitleMenuWidget>(World)) return true;

	if (UBWMapWidget* Map = FindOnScreen<UBWMapWidget>(World))
	{
		if (Map->IsModalOpen()) { Map->CloseModalByEscape(); return true; }
	}
	if (ABWPlayerController* PC = Cast<ABWPlayerController>(World->GetFirstPlayerController()))
	{
		if (PC->CancelBattleSelection()) return true;
	}
	if (APlayerController* Owner = World->GetFirstPlayerController())
	{
		if (UBWPauseWidget* Pause = CreateWidget<UBWPauseWidget>(Owner, UBWPauseWidget::StaticClass()))
		{
			Pause->AddToViewport(900);
			UGameplayStatics::SetGamePaused(World, true);
		}
	}
	return true;
}

bool UBWGameInstance::HasSavedRun() const
{
	if (!UGameplayStatics::DoesSaveGameExist(RunSaveSlot, 0)) return false;
	// 옛 형식(지도 구조가 다른 저장)은 이어할 수 없으니 없는 것으로 본다
	const UBWRunSaveGame* Save = Cast<UBWRunSaveGame>(UGameplayStatics::LoadGameFromSlot(RunSaveSlot, 0));
	return Save && Save->Version == UBWRunSaveGame::CurrentVersion;
}

void UBWGameInstance::DeleteSavedRun()
{
	if (UBWAutoPlayer::IsActive()) return;
	if (HasSavedRun()) UGameplayStatics::DeleteGameInSlot(RunSaveSlot, 0);
}

FText UBWGameInstance::GetSavedRunSummary() const
{
	if (!HasSavedRun()) return FText::GetEmpty();
	const UBWRunSaveGame* Save = Cast<UBWRunSaveGame>(UGameplayStatics::LoadGameFromSlot(RunSaveSlot, 0));
	if (!Save) return FText::GetEmpty();
	const FBWClassData* Data = FindClassData(Save->SelectedClass);
	const FString Name = Data ? Data->DisplayName.ToString() : UEnum::GetDisplayValueAsText(Save->SelectedClass).ToString();
	const FString Floor = Save->CurrentStep >= UBWMapRules::BossStep ? FString::Printf(TEXT("%d층 보스"), Save->CurrentStage)
		: Save->CurrentStep >= 0 ? FString::Printf(TEXT("%d층 %d칸"), Save->CurrentStage, Save->CurrentStep + 1)
		: FString::Printf(TEXT("%d층 시작"), Save->CurrentStage);
	return FText::FromString(FString::Printf(TEXT("%s · %s · HP %d/%d"), *Name, *Floor, Save->PlayerCurrentHP, Save->PlayerMaxHP));
}

const FBWClassData* UBWGameInstance::FindClassData(EClassType ClassType) const
{
	const UDataTable* Table = ClassDataTable.LoadSynchronous();
	if (!Table) return nullptr;
	for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
	{
		const FBWClassData* Data = reinterpret_cast<const FBWClassData*>(Row.Value);
		if (Data && Data->ClassType == ClassType) return Data;
	}
	return nullptr;
}

bool UBWGameInstance::GetClassData(EClassType ClassType, FBWClassData& OutData) const
{
	const FBWClassData* Data = FindClassData(ClassType);
	if (Data) OutData = *Data;
	return Data != nullptr;
}

TArray<FBWClassData> UBWGameInstance::GetAllClassData() const
{
	TArray<FBWClassData> Rows;
	if (const UDataTable* Table = ClassDataTable.LoadSynchronous())
	{
		Table->ForeachRow<FBWClassData>(TEXT("Class Rows"), [&Rows](const FName&, const FBWClassData& Row) { Rows.Add(Row); });
	}
	return Rows;
}

FBWDiceRange UBWGameInstance::GetClassSpeedDice(EClassType ClassType) const
{
	if (const FBWClassData* Data = FindClassData(ClassType)) return Data->Speed;
	const FBWDiceRange* Found = ClassSpeedDice.Find(ClassType);
	return Found ? *Found : FBWDiceRange(3, 6);
}

FBWClassStartStats UBWGameInstance::GetClassStartStats(EClassType ClassType) const
{
	const FBWClassData* Data = FindClassData(ClassType);
	return Data ? Data->StartStats : UBWDuelRules::MakeClassStartStats(ClassType);
}

FBWClassCombatProfile UBWGameInstance::GetClassCombatProfile(EClassType ClassType) const
{
	const FBWClassData* Data = FindClassData(ClassType);
	return Data ? Data->Combat : UBWDuelRules::MakeClassProfile(ClassType);
}

FBWCardData UBWGameInstance::GetClassBasicStrike(EClassType ClassType) const
{
	const FBWClassData* Data = FindClassData(ClassType);
	if (!Data) return UBWDuelRules::MakeBasicStrike(ClassType);
	FBWCardData Strike = Data->BasicStrike;
	Strike.CardKind = ECardKind::Attack; // 기본 공격은 언제나 공격
	Strike.ClassType = ClassType;
	return Strike;
}

float UBWGameInstance::GetClassCardPower(EClassType ClassType) const
{
	const FBWClassData* Data = FindClassData(ClassType);
	return Data ? Data->CardPower : UBWDuelRules::MakeClassCardPower(ClassType);
}

bool UBWGameInstance::GetClassStartingDeck(EClassType ClassType, TArray<FName>& OutCards) const
{
	const FBWClassData* Data = FindClassData(ClassType);
	if (!Data || Data->StartingDeck.Num() == 0) return false;
	OutCards = Data->StartingDeck;
	return true;
}

void UBWGameInstance::BeginNewRun(EClassType ClassType)
{
	bInBattle = false;
	bResumeBattle = false;
	SelectedClass = ClassType;
	const FBWClassStartStats Stats = GetClassStartStats(ClassType);
	PlayerPHA = Stats.PHA;
	PlayerTEC = Stats.TEC;
	PlayerINT = Stats.INT;
	PlayerFTH = Stats.FTH;
	PlayerMaxHP = Stats.MaxHP;
	PlayerCurrentHP = Stats.MaxHP;

	CurrentStage = 1;
	CurrentGold = 0;
	SoulBullets = ClassType == EClassType::TEC ? 1 : 0;
	OwnedAugments.Reset();
	AugmentChoices.Reset();
	PendingRewards.Reset();
	PendingCardRemovals = 0;
	bActCleared = false;
	AugmentPickedAct = 0;
	AugmentBossPickedAct = 0;
	PendingStatPoints = 0;
	PlayerDefensePoints = 0;
	WorldMapData.Reset();
	CurrentStep = -1;
	CurrentDimIdx = -1;
	ResetStats();

	// 판 덱은 그 삶의 방식의 시작 덱에서 출발한다 (표에 덱이 없으면 비워 두고 덱 컴포넌트의 직업별 배열을 쓴다)
	RunDeck.Reset();
	TArray<FName> StartCards;
	if (GetClassStartingDeck(ClassType, StartCards))
	{
		for (const FName& Row : StartCards) RunDeck.Add(FBWRunCard(Row));
	}
	PlayerDeckCount = RunDeck.Num() > 0 ? RunDeck.Num() : PlayerDeckCount;
	UE_LOG(LogTemp, Warning, TEXT("[BWRun] New run as %s (HP %d, PHA %d TEC %d INT %d FTH %d)"), *UEnum::GetValueAsString(ClassType), PlayerMaxHP, PlayerPHA, PlayerTEC, PlayerINT, PlayerFTH);
}

void UBWGameInstance::GetAugmentRows(TArray<FName>& OutNames, TArray<FBWAugmentData>& OutRows) const
{
	OutNames.Reset();
	OutRows.Reset();
	if (!AugmentDataTable) return;
	AugmentDataTable->ForeachRow<FBWAugmentData>(TEXT("Augment Rows"), [&](const FName& Key, const FBWAugmentData& Row)
	{
		OutNames.Add(Key);
		OutRows.Add(Row);
	});
}

const TArray<FName>& UBWGameInstance::BeginAugmentPick(int32 Act, EClassType PlayerClass)
{
	if (AugmentRandom.GetInitialSeed() == 0) AugmentRandom.Initialize(FMath::Rand());

	AugmentPickAct = Act;
	AugmentPickClass = PlayerClass;
	AugmentRerollsLeft = UBWAugmentRules::RerollsPerPick;

	TArray<FName> Names;
	TArray<FBWAugmentData> Rows;
	GetAugmentRows(Names, Rows);
	AugmentChoices = UBWAugmentRules::PickByKind(Names, Rows, PlayerClass, OwnedAugments, {}, AugmentRandom);

	UE_LOG(LogTemp, Warning, TEXT("[BWAugment] Act %d picks: %s"), Act, *FString::JoinBy(AugmentChoices, TEXT(", "), [](const FName& N) { return N.ToString(); }));
	return AugmentChoices;
}

bool UBWGameInstance::RerollAugmentChoices()
{
	if (AugmentRerollsLeft <= 0 || AugmentChoices.Num() == 0) return false;
	--AugmentRerollsLeft;

	TArray<FName> Names;
	TArray<FBWAugmentData> Rows;
	GetAugmentRows(Names, Rows);
	const TArray<FName> Previous = AugmentChoices;
	AugmentChoices = UBWAugmentRules::PickByKind(Names, Rows, AugmentPickClass, OwnedAugments, Previous, AugmentRandom);

	UE_LOG(LogTemp, Warning, TEXT("[BWAugment] Reroll: %s"), *FString::JoinBy(AugmentChoices, TEXT(", "), [](const FName& N) { return N.ToString(); }));
	return true;
}

bool UBWGameInstance::ChooseAugment(int32 ChoiceIndex)
{
	if (!AugmentChoices.IsValidIndex(ChoiceIndex)) return false;
	OwnedAugments.Add(AugmentChoices[ChoiceIndex]);

	// [추가] 최대 HP를 바꾸는 증강은 고르는 즉시 판 전체에 적용한다
	FBWAugmentData Chosen;
	if (GetAugmentData(AugmentChoices[ChoiceIndex], Chosen))
	{
		for (const FBWAugmentMod& Mod : Chosen.Mods)
		{
			if (Mod.Stat != EBWAugStat::MaxHPFlat && Mod.Stat != EBWAugStat::MaxHPPct) continue;
			const int32 Delta = Mod.Stat == EBWAugStat::MaxHPFlat ? FMath::RoundToInt(Mod.Value) : FMath::RoundToInt(PlayerMaxHP * Mod.Value);
			PlayerMaxHP = FMath::Max(1, PlayerMaxHP + Delta);
			PlayerCurrentHP = FMath::Clamp(PlayerCurrentHP + FMath::Max(0, Delta), 1, PlayerMaxHP);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[BWAugment] Chose %s (owned %d)"), *AugmentChoices[ChoiceIndex].ToString(), OwnedAugments.Num());
	AugmentChoices.Reset();
	AugmentRerollsLeft = 0;
	return true;
}

bool UBWGameInstance::GetAugmentData(FName RowName, FBWAugmentData& OutData) const
{
	if (!AugmentDataTable) return false;
	if (const FBWAugmentData* Row = AugmentDataTable->FindRow<FBWAugmentData>(RowName, TEXT("Augment Lookup"), false))
	{
		OutData = *Row;
		return true;
	}
	return false;
}

TArray<FBWAugmentData> UBWGameInstance::GetOwnedAugmentData() const
{
	TArray<FBWAugmentData> Result;
	for (const FName& Name : OwnedAugments)
	{
		FBWAugmentData Data;
		if (GetAugmentData(Name, Data)) Result.Add(Data);
	}
	return Result;
}

FRandomStream& UBWGameInstance::GetMapRandom()
{
	if (MapRandom.GetInitialSeed() == 0) MapRandom.Initialize(FMath::Rand() | 1);
	return MapRandom;
}

static FRewardItem MakeGoldReward(int32 Amount, const FString& Why)
{
	FRewardItem Gold;
	Gold.Type = ERewardType::Gold;
	Gold.Amount = Amount;
	Gold.RewardName = FString::Printf(TEXT("골드 %d"), Amount);
	Gold.Description = Why;
	return Gold;
}

int32 UBWGameInstance::GetShopCardPrice(const FBWCardData& Card, bool bOnSale) const
{
	const int32 Base = Card.Rarity == EBWCardRarity::Rare ? ShopRareCardPrice : Card.Rarity == EBWCardRarity::Uncommon ? ShopUncommonCardPrice : ShopCardPrice;
	const float Mult = FMath::Max(0.0f, 1.0f + GetAugmentTotals().Get(EBWAugStat::ShopPct));
	const int32 Price = FMath::RoundToInt(Base * Mult);
	return bOnSale ? Price / 2 : Price;
}

FBWAugmentTotals UBWGameInstance::GetAugmentTotals() const
{
	return UBWAugmentRules::Aggregate(GetOwnedAugmentData());
}

TArray<FName> UBWGameInstance::RollCardChoices(int32 Count)
{
	// 내 삶의 방식·공용 카드 중 시작 카드와 저주를 뺀 것. 등급 가중치로 뽑는다 (일반 60 · 고급 30 · 희귀 10)
	TArray<FName> Pool;
	TArray<int32> Weights;
	if (const UDataTable* Table = CardDataTable.LoadSynchronous())
	{
		Table->ForeachRow<FBWCardData>(TEXT("Card Reward Pool"), [&](const FName& Key, const FBWCardData& Card)
		{
			if (Card.ClassType != SelectedClass && Card.ClassType != EClassType::Common) return;
			if (Card.Rarity == EBWCardRarity::Starter) return;
			const int32 W = Card.Rarity == EBWCardRarity::Rare ? RareCardWeight : Card.Rarity == EBWCardRarity::Uncommon ? UncommonCardWeight : CommonCardWeight;
			if (W <= 0) return;
			Pool.Add(Key);
			Weights.Add(W);
		});
	}
	TArray<FName> Picked;
	FRandomStream& Stream = GetMapRandom();
	while (Picked.Num() < Count && Pool.Num() > 0)
	{
		int32 Total = 0;
		for (int32 W : Weights) Total += W;
		int32 Roll = Stream.RandRange(0, Total - 1);
		int32 Index = 0;
		while (Index < Weights.Num() - 1 && Roll >= Weights[Index]) Roll -= Weights[Index++];
		Picked.Add(Pool[Index]);
		Pool.RemoveAt(Index);
		Weights.RemoveAt(Index);
	}
	return Picked;
}

FRewardItem UBWGameInstance::MakeCardReward()
{
	FRewardItem Reward;
	Reward.Type = ERewardType::CardSelection;

	// 파트로바스 방치 보너스: 강화 카드 n장 + 일반 m장 (내성 반영). 없으면 일반 3장
	int32 Upgraded = 0, Total = 3;
	if (MapBonus.UpgradedCardChoices + MapBonus.NormalCardChoices > 0)
	{
		Upgraded = MapBonus.UpgradedCardChoices;
		Total = MapBonus.UpgradedCardChoices + MapBonus.NormalCardChoices;
		MapBonus.UpgradedCardChoices = 0;
		MapBonus.NormalCardChoices = 0;
	}
	// 과부하 II: 카드 보상 선택지 한 장 줄어듦 (3 → 2)
	if (GetCurrentOverloadLevel() >= 2) Total = FMath::Max(1, Total - 1);
	Total += GetAugmentTotals().GetInt(EBWAugStat::CardChoices);
	Upgraded = FMath::Min(Upgraded, Total);

	Reward.CardChoices = RollCardChoices(Total);
	Reward.UpgradedChoices = FMath::Min(Upgraded, Reward.CardChoices.Num());
	Reward.RewardName = Reward.UpgradedChoices > 0 ? FString::Printf(TEXT("카드 %d장 중 1장 (강화 %d장)"), Reward.CardChoices.Num(), Reward.UpgradedChoices)
		: FString::Printf(TEXT("카드 %d장 중 1장"), Reward.CardChoices.Num());
	return Reward;
}

void UBWGameInstance::GenerateCombatRewards(EMapNodeType EncounterType)
{
	PendingRewards.Empty();

	// 1. 골드 (엘리트 2배, 보스 5배) — 바로 받는다
	int32 Gold = GetMapRandom().RandRange(20, 40);
	if (EncounterType == EMapNodeType::Elite) Gold *= 2;
	else if (EncounterType == EMapNodeType::Boss) Gold *= 5;
	PendingRewards.Add(MakeGoldReward(Gold, TEXT("B.W 정산 완료")));

	// 2. 베렌드 방치 보너스: 도착 후 첫 승리에 HP 회복 (1회)
	if (MapBonus.VictoryHealFraction > 0.0f)
	{
		FRewardItem Heal;
		Heal.Type = ERewardType::Heal;
		Heal.Amount = FMath::Max(1, FMath::RoundToInt(PlayerMaxHP * MapBonus.VictoryHealFraction));
		Heal.RewardName = FString::Printf(TEXT("HP %d 회복"), Heal.Amount);
		Heal.Description = FString::Printf(TEXT("베렌드 방치 보너스 (최대 HP %d%%)"), FMath::RoundToInt(MapBonus.VictoryHealFraction * 100.0f));
		PendingRewards.Add(Heal);
		MapBonus.VictoryHealFraction = 0.0f;
	}

	// 3. 타이가 방치 보너스: 다음 전투 보상에 유물 (유물 시스템은 아직 없어 표시만)
	if (MapBonus.EquipmentGrade > 0)
	{
		FRewardItem Equip;
		Equip.Type = ERewardType::Equipment;
		Equip.Amount = MapBonus.EquipmentGrade;
		Equip.RewardName = MapBonus.EquipmentGrade >= 2 ? TEXT("전설 유물") : TEXT("희귀 이상 유물");
		Equip.Description = TEXT("타이가 방치 보너스 · 유물 준비 중이라 아직 효과 없음");
		PendingRewards.Add(Equip);
		MapBonus.EquipmentGrade = 0;
	}

	// 4. 카드 보상 (3장 중 1장, 파트로바스 보너스·과부하 II 반영)
	PendingRewards.Add(MakeCardReward());

	// 골드·회복은 바로 적용해 두고, 목록은 지도 화면이 보여 준 뒤 비운다
	for (int32 i = 0; i < PendingRewards.Num(); ++i)
	{
		if (PendingRewards[i].Type == ERewardType::Gold) CurrentGold += PendingRewards[i].Amount;
		else if (PendingRewards[i].Type == ERewardType::Heal) HealPlayer(PendingRewards[i].Amount);
	}
	UE_LOG(LogTemp, Warning, TEXT("[BWReward] %d rewards for %s (gold now %d)"), PendingRewards.Num(), *UEnum::GetValueAsString(EncounterType), CurrentGold);
}

void UBWGameInstance::GenerateTreasureRewards()
{
	PendingRewards.Empty();
	const int32 Gold = GetMapRandom().RandRange(50, 80);
	PendingRewards.Add(MakeGoldReward(Gold, TEXT("보물 상자")));
	CurrentGold += Gold;
	PendingRewards.Add(MakeCardReward());
}

void UBWGameInstance::OnBattleWon()
{
	bInBattle = false;
	GenerateCombatRewards(CurrentEncounterType);
	if (CurrentEncounterType == EMapNodeType::Boss)
	{
		bActCleared = true;
		if (CurrentStage < UBWMapRules::NumActs) PendingStatPoints += StatPointsPerGrant;
	}

	// [추가] 증강: 전투 승리·엘리트 처치 회복
	const FBWAugmentTotals Totals = GetAugmentTotals();
	HealPlayer(Totals.GetInt(EBWAugStat::WinHeal));
	if (CurrentEncounterType == EMapNodeType::Elite) HealPlayer(Totals.GetInt(EBWAugStat::EliteHeal));
	SaveRun();
}

void UBWGameInstance::ClaimReward(int32 Index)
{
	if (!PendingRewards.IsValidIndex(Index)) return;
	const FRewardItem Reward = PendingRewards[Index];
	// 골드·회복은 보상을 만들 때 이미 적용했다. 카드 보상을 그냥 받으면 건너뛴 것으로 본다
	UE_LOG(LogTemp, Log, TEXT("[BWReward] Claimed: %s"), *Reward.RewardName);
	PendingRewards.RemoveAt(Index);
}

void UBWGameInstance::ChooseRewardCard(int32 RewardIndex, int32 ChoiceIndex)
{
	if (!PendingRewards.IsValidIndex(RewardIndex)) return;
	const FRewardItem Reward = PendingRewards[RewardIndex];
	PendingRewards.RemoveAt(RewardIndex);
	if (Reward.Type != ERewardType::CardSelection || !Reward.CardChoices.IsValidIndex(ChoiceIndex)) return;
	AddRunCard(Reward.CardChoices[ChoiceIndex], ChoiceIndex < Reward.UpgradedChoices);
}

bool UBWGameInstance::GetCardData(FName Row, bool bUpgraded, FBWCardData& OutCard) const
{
	const UDataTable* Table = CardDataTable.LoadSynchronous();
	const FBWCardData* Found = Table ? Table->FindRow<FBWCardData>(Row, TEXT("Run Deck"), false) : nullptr;
	if (!Found) return false;
	OutCard = bUpgraded ? UBWMapRules::MakeUpgradedCard(*Found) : *Found;
	return true;
}

TArray<FBWCardData> UBWGameInstance::GetRunDeckCards() const
{
	TArray<FBWCardData> Cards;
	for (const FBWRunCard& Entry : RunDeck)
	{
		FBWCardData Card;
		if (GetCardData(Entry.Row, Entry.bUpgraded, Card)) Cards.Add(Card);
	}
	return Cards;
}

void UBWGameInstance::AddRunCard(FName Row, bool bUpgraded)
{
	RunDeck.Add(FBWRunCard(Row, bUpgraded));
	PlayerDeckCount = RunDeck.Num();
	UE_LOG(LogTemp, Warning, TEXT("[BWDeck] Added %s%s (deck %d)"), *Row.ToString(), bUpgraded ? TEXT("+") : TEXT(""), RunDeck.Num());
}

bool UBWGameInstance::RemoveRunCard(int32 DeckIndex)
{
	if (!RunDeck.IsValidIndex(DeckIndex) || RunDeck.Num() <= 1) return false;
	UE_LOG(LogTemp, Warning, TEXT("[BWDeck] Removed %s"), *RunDeck[DeckIndex].Row.ToString());
	RunDeck.RemoveAt(DeckIndex);
	PlayerDeckCount = RunDeck.Num();
	return true;
}

bool UBWGameInstance::UpgradeRunCard(int32 DeckIndex)
{
	if (!RunDeck.IsValidIndex(DeckIndex) || RunDeck[DeckIndex].bUpgraded) return false;
	RunDeck[DeckIndex].bUpgraded = true;
	UE_LOG(LogTemp, Warning, TEXT("[BWDeck] Upgraded %s"), *RunDeck[DeckIndex].Row.ToString());
	return true;
}

bool UBWGameInstance::CanUseCardOnMap(int32 DeckIndex, FString& OutReason) const
{
	FBWCardData Card;
	if (!RunDeck.IsValidIndex(DeckIndex) || !GetCardData(RunDeck[DeckIndex].Row, RunDeck[DeckIndex].bUpgraded, Card)) { OutReason = TEXT("카드 없음"); return false; }
	if (Card.CardKind != ECardKind::Effect) { OutReason = TEXT("공격카드는 전투에서만"); return false; }
	if (Card.EffectHeal <= 0) { OutReason = TEXT("전투에서만 의미 있는 효과"); return false; }
	if (MapUsedCards.Contains(RunDeck[DeckIndex].Row)) { OutReason = TEXT("이번 층에 이미 씀"); return false; }
	if (PlayerCurrentHP >= PlayerMaxHP) { OutReason = TEXT("HP가 가득 참"); return false; }
	OutReason.Reset();
	return true;
}

bool UBWGameInstance::UseCardOnMap(int32 DeckIndex)
{
	FString Reason;
	if (!CanUseCardOnMap(DeckIndex, Reason)) return false;
	FBWCardData Card;
	GetCardData(RunDeck[DeckIndex].Row, RunDeck[DeckIndex].bUpgraded, Card);
	HealPlayer(Card.EffectHeal);
	MapUsedCards.Add(RunDeck[DeckIndex].Row); // 같은 카드(같은 행)는 막당 1회
	UE_LOG(LogTemp, Warning, TEXT("[BWMap] Used %s on the map: HP +%d -> %d/%d"), *Card.CardName, Card.EffectHeal, PlayerCurrentHP, PlayerMaxHP);
	return true;
}

int32 UBWGameInstance::GetRestHealAmount() const
{
	const float Mult = SelectedClass == EClassType::HP ? HPClassRestMultiplier : 1.0f; // 철혈의 분쇄자 '털어내기'
	const FBWAugmentTotals Totals = GetAugmentTotals();
	if (Totals.Has(EBWAugStat::NoRestHeal)) return 0;
	return FMath::Max(1, FMath::RoundToInt(PlayerMaxHP * RestHealFraction * Mult * (1.0f + Totals.Get(EBWAugStat::RestPct))));
}

void UBWGameInstance::RestHeal()
{
	HealPlayer(GetRestHealAmount());
	// [추가] 고행자: 휴식할 때마다 최대 HP +
	const int32 Grow = GetAugmentTotals().GetInt(EBWAugStat::RestMaxHP);
	PlayerMaxHP += Grow;
	PlayerCurrentHP += Grow;
}

void UBWGameInstance::HealPlayer(int32 Amount)
{
	if (GetAugmentTotals().Has(EBWAugStat::NoHeal)) return;
	PlayerCurrentHP = FMath::Clamp(PlayerCurrentHP + FMath::Max(0, Amount), 1, PlayerMaxHP);
}

FName UBWGameInstance::FindEnemyIDByDisplayName(FString DisplayName)
{
	if (!EnemyDataTable) return NAME_None;

	static const FString ContextString(TEXT("Display Name Search Context"));
	TArray<FName> RowNames = EnemyDataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		FBWEnemyData* Data = EnemyDataTable->FindRow<FBWEnemyData>(RowName, ContextString);
		if (Data && Data->EnemyDisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			return RowName;
		}
	}

	return NAME_None;
}

void UBWGameInstance::PrepareEncounter(EMapNodeType Type, EDimensionType Dim)
{
	PendingEnemyRequests.Empty();
	PendingWallTiles.Empty();
	CurrentEncounterType = Type; // 보상 생성을 위해 저장

	if (!EncounterDataTable) 
	{
		UE_LOG(LogTemp, Error, TEXT("[BWGameInstance] EncounterDataTable is NULL! Please assign DT_Encounters in BP_BWGameInstance."));
		return;
	}

	TArray<FEncounterData*> EligibleEncounters;
	float TotalWeight = 0.0f;

	// 1. 현재 단계(Stage), 차원(Dimension), 타입(Battle/Elite/Boss)에 맞는 후보 필터링
	// [수정] 맞는 조합이 없으면 막 조건 → 종류 조건 순서로 풀어서 찾는다 (엘리트·보스·2·3막 조합이 아직 표에 없을 때)
	static const FString ContextString(TEXT("Encounter Filter Context"));
	auto Collect = [&](bool bMatchStage, bool bMatchType)
	{
		EncounterDataTable->ForeachRow<FEncounterData>(ContextString, [&](const FName& Key, const FEncounterData& Row) {
			// [수정] 차원이 정확히 일치하거나, None(공통)인 경우 모두 후보에 포함
			bool bDimMatch = (Row.Dimension == Dim || Row.Dimension == EDimensionType::None);
			const bool bStageOk = !bMatchStage || Row.Stage == CurrentStage;
			const bool bTypeOk = bMatchType ? Row.EncounterType == Type : Row.EncounterType == EMapNodeType::Battle;
			if (bStageOk && bDimMatch && bTypeOk)
			{
				EligibleEncounters.Add(const_cast<FEncounterData*>(&Row));
				TotalWeight += Row.Weight;
			}
		});
	};
	Collect(true, true);
	if (EligibleEncounters.Num() == 0) Collect(false, true);
	if (EligibleEncounters.Num() == 0) Collect(true, false);
	if (EligibleEncounters.Num() == 0) Collect(false, false);

	if (EligibleEncounters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWGameInstance] No Eligible Encounters found for Stage %d, Dim %d, Type %d"), CurrentStage, (int32)Dim, (int32)Type);
		return;
	}

	// 2. 가중치 기반 랜덤 선택 (Weighted Random)
	float RandomRoll = FMath::FRandRange(0.0f, TotalWeight);
	float CurrentWeightSum = 0.0f;
	FEncounterData* ChosenEncounter = EligibleEncounters[0];

	for (FEncounterData* Encounter : EligibleEncounters)
	{
		CurrentWeightSum += Encounter->Weight;
		if (RandomRoll <= CurrentWeightSum)
		{
			ChosenEncounter = Encounter;
			break;
		}
	}

	PendingWallTiles = ChosenEncounter->WallTiles;

	// 3. 선택된 조합의 몬스터들을 스폰 목록에 추가
	for (const FEnemyPositionInfo& PosInfo : ChosenEncounter->EnemyGroup)
	{
		FEnemySpawnRequest Req;
		
		// [개선] 만약 입력한 ID가 실제 Row Name이 아니라면, Display Name에서 찾아서 보정
		FName FinalID = PosInfo.EnemyID;
		if (EnemyDataTable && !EnemyDataTable->FindRow<FBWEnemyData>(FinalID, TEXT("")))
		{
			FName SearchResult = FindEnemyIDByDisplayName(FinalID.ToString());
			if (SearchResult != NAME_None) FinalID = SearchResult;
		}

		Req.EnemyID = FinalID;
		Req.Coordinate = PosInfo.Coordinate;
		PendingEnemyRequests.Add(Req);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BWGameInstance] Encounter Prepared: %d enemies will spawn."), PendingEnemyRequests.Num());
}

void UBWGameInstance::ResetStats()
{
	ConsecutiveCounts.Empty();
	NeglectedCounts.Empty();
	BonusClaimCounts.Empty();

	// 4개 국가 초기화 (막 시작 시 모두 0)
	for (int32 i = 1; i <= 4; ++i)
	{
		EDimensionType Dim = (EDimensionType)i;
		ConsecutiveCounts.Add(Dim, 0);
		NeglectedCounts.Add(Dim, 0);
		BonusClaimCounts.Add(Dim, 0);
	}
	MapBonus = FBWMapBonusState();
	bRiftJumpReady = false;
	LastArrival = FBWArrivalReport();
	MapUsedCards.Reset();
}

void UBWGameInstance::GenerateWorldMap(int32 TotalSteps)
{
	// 막 길이는 기획(15칸 + 보스)으로 고정. TotalSteps는 옛 블루프린트 호출 호환용
	CurrentStage = FMath::Clamp(CurrentStage, 1, UBWMapRules::NumActs);
	CurrentStep = -1;
	CurrentDimIdx = -1;
	bActCleared = false;
	ResetStats(); // 방치·연속·내성·받아 둔 보너스는 다음 막으로 이월되지 않는다

	FRandomStream& Stream = GetMapRandom();
	RowDimensions = UBWMapRules::MakeRowOrder(CurrentStage, bShuffleRowsAfterFirstAct, Stream);
	WorldMapData = UBWMapRules::GenerateAct(RowDimensions, Stream);

	UE_LOG(LogTemp, Warning, TEXT("[BWMap] Act %d generated: rows %s"), CurrentStage,
		*FString::JoinBy(RowDimensions, TEXT(", "), [](EDimensionType D) { return UBWMapRules::GetDimensionName(D).ToString(); }));
}

bool UBWGameInstance::IsBossStep(int32 Step) const
{
	return Step == UBWMapRules::BossStep;
}

EDimensionType UBWGameInstance::GetRowDimension(int32 Row) const
{
	if (RowDimensions.IsValidIndex(Row)) return RowDimensions[Row];
	return Row >= 0 && Row < 4 ? (EDimensionType)(Row + 1) : EDimensionType::None; // 옛 지도(줄 순서 없음)
}

EDimensionType UBWGameInstance::GetCurrentDimension() const
{
	return CurrentStep >= 0 ? GetRowDimension(CurrentDimIdx) : EDimensionType::None;
}

int32 UBWGameInstance::GetCurrentOverloadLevel() const
{
	const int32* Consecutive = ConsecutiveCounts.Find(GetCurrentDimension());
	return Consecutive ? UBWMapRules::GetOverloadLevel(*Consecutive) : 0;
}

const FMapNode& UBWGameInstance::GetCurrentNode() const
{
	static const FMapNode Empty;
	if (!WorldMapData.IsValidIndex(CurrentStep)) return Empty;
	const TArray<FMapNode>& Nodes = WorldMapData[CurrentStep].Nodes;
	const int32 Index = IsBossStep(CurrentStep) ? 0 : CurrentDimIdx;
	return Nodes.IsValidIndex(Index) ? Nodes[Index] : Empty;
}

bool UBWGameInstance::CanMoveToNode(int32 TargetStep, int32 TargetDimIdx)
{
	if (bActCleared) return false;
	return UBWMapRules::CanMove(WorldMapData, CurrentStep, CurrentDimIdx, TargetStep, TargetDimIdx, bRiftJumpReady);
}

void UBWGameInstance::ClaimNeglectBonus(EDimensionType Dim, int32 Neglect, FBWArrivalReport& Report)
{
	int32& Claims = BonusClaimCounts.FindOrAdd(Dim);
	++Claims;
	const FBWNeglectBonus Bonus = UBWMapRules::MakeNeglectBonus(Dim, Claims, Neglect >= UBWMapRules::GetNeglectCap(Dim));

	Report.bClaimedBonus = true;
	Report.bDeepNeglect = Bonus.bDeep;
	Report.BonusPercent = Bonus.Percent;
	Report.GoldGained = Bonus.Gold;
	CurrentGold += Bonus.Gold;

	switch (Dim)
	{
	case EDimensionType::Patrovas:
		MapBonus.UpgradedCardChoices = Bonus.UpgradedCardChoices;
		MapBonus.NormalCardChoices = Bonus.NormalCardChoices;
		Report.BonusLines.Add(Bonus.NormalCardChoices > 0
			? FString::Printf(TEXT("다음 카드 보상: 강화 카드 %d장 + 일반 %d장 중 선택"), Bonus.UpgradedCardChoices, Bonus.NormalCardChoices)
			: FString::Printf(TEXT("다음 카드 보상: 강화 카드 %d장 중 선택"), Bonus.UpgradedCardChoices));
		break;
	case EDimensionType::Berend:
		MapBonus.VictoryHealFraction = FMath::Max(MapBonus.VictoryHealFraction, Bonus.VictoryHealFraction);
		Report.BonusLines.Add(FString::Printf(TEXT("다음 전투에서 이기면 최대 HP의 %d%% 회복"), FMath::RoundToInt(Bonus.VictoryHealFraction * 100.0f)));
		break;
	case EDimensionType::Geleon:
		MapBonus.NextBattleDamageBonus = FMath::Max(MapBonus.NextBattleDamageBonus, Bonus.NextBattleDamageBonus);
		Report.BonusLines.Add(FString::Printf(TEXT("다음 전투 1회 피해량 +%d%%"), FMath::RoundToInt(Bonus.NextBattleDamageBonus * 100.0f)));
		break;
	case EDimensionType::Taiga:
		PendingCardRemovals += Bonus.CardRemovals;
		Report.CardRemovals = Bonus.CardRemovals;
		MapBonus.EquipmentGrade = FMath::Max(MapBonus.EquipmentGrade, Bonus.EquipmentGrade);
		Report.BonusLines.Add(FString::Printf(TEXT("지금 카드 %d장 삭제"), Bonus.CardRemovals));
		Report.BonusLines.Add(Bonus.EquipmentGrade >= 2 ? TEXT("다음 전투 보상에 전설 유물 확정") : TEXT("다음 전투 보상에 희귀 이상 유물 확정"));
		break;
	default:
		break;
	}
	if (Bonus.Gold > 0) Report.BonusLines.Add(FString::Printf(TEXT("깊은 방치: 골드 +%d"), Bonus.Gold));
	UE_LOG(LogTemp, Warning, TEXT("[BWMap] Neglect bonus %s (neglect %d, claim #%d, %d%%)"), *UBWMapRules::GetDimensionName(Dim).ToString(), Neglect, Claims, Bonus.Percent);
}

bool UBWGameInstance::SelectMapNode(int32 Step, int32 DimIdx)
{
	if (!CanMoveToNode(Step, DimIdx)) return false;

	const bool bFirst = CurrentStep < 0;
	const bool bBoss = IsBossStep(Step);
	const int32 PrevRow = CurrentDimIdx;
	const EDimensionType PrevDim = GetCurrentDimension();
	const int32 PrevLevel = bFirst ? 0 : GetCurrentOverloadLevel();

	// 1. 차원문 통과 (보스 칸은 줄이 합류할 뿐 줄을 바꾸지 않는다)
	CurrentStep = Step;
	if (!bBoss) CurrentDimIdx = DimIdx;
	if (WorldMapData.IsValidIndex(Step) && WorldMapData[Step].Nodes.IsValidIndex(DimIdx))
	{
		WorldMapData[Step].Nodes[DimIdx].bIsVisited = true;
	}
	bRiftJumpReady = false; // 균열 점프는 바로 다음 칸 한 번뿐

	const EDimensionType Dim = GetCurrentDimension();
	FBWArrivalReport Report;
	Report.Dimension = Dim;
	Report.bChangedRow = !bFirst && !bBoss && PrevRow != CurrentDimIdx;

	// 2. 방치 보너스 수령 (도착 전 카운터로 판정, 보스 칸에서는 수령 불가) → 카운터 갱신
	if (!bBoss)
	{
		const int32 Neglect = NeglectedCounts.FindRef(Dim);
		Report.NeglectCount = Neglect;
		if (Neglect >= UBWMapRules::GetNeglectCondition(Dim)) ClaimNeglectBonus(Dim, Neglect, Report);
		UBWMapRules::AdvanceNeglect(NeglectedCounts, Dim);
	}

	// 3. 과부하 판정: 같은 줄이면 연속 +1, 줄을 바꾸면 1부터 (다른 줄은 0)
	for (TPair<EDimensionType, int32>& Pair : ConsecutiveCounts)
	{
		if (Pair.Key != Dim) Pair.Value = 0;
		else Pair.Value = (bFirst || Report.bChangedRow || PrevDim != Dim) ? 1 : Pair.Value + 1;
	}
	Report.Consecutive = ConsecutiveCounts.FindRef(Dim);
	Report.OverloadLevel = UBWMapRules::GetOverloadLevel(Report.Consecutive);
	Report.bOverloadChanged = Report.OverloadLevel != PrevLevel;

	// 4. 노드 진입 중 지도 규칙에 속하는 것: 균열은 다음 칸 2줄 점프
	const FMapNode& Node = GetCurrentNode();
	if (Node.NodeType == EMapNodeType::Rift) bRiftJumpReady = true;

	LastArrival = Report;
	UE_LOG(LogTemp, Warning, TEXT("[BWMap] Act %d step %d row %d (%s, %s): consecutive %d, overload %d, bonus %s"), CurrentStage, Step, CurrentDimIdx,
		*UBWMapRules::GetDimensionName(Dim).ToString(), *UBWMapRules::GetNodeTypeName(Node.NodeType).ToString(), Report.Consecutive, Report.OverloadLevel, Report.bClaimedBonus ? TEXT("yes") : TEXT("no"));
	return true;
}

void UBWGameInstance::UseDimensionAltar()
{
	for (TPair<EDimensionType, int32>& Pair : ConsecutiveCounts) Pair.Value = 0;
	for (TPair<EDimensionType, int32>& Pair : BonusClaimCounts) Pair.Value = 0;
	UE_LOG(LogTemp, Warning, TEXT("[BWMap] Dimension altar: overload and bonus resistance reset"));
}

float UBWGameInstance::ConsumeNextBattleDamageBonus()
{
	const float Bonus = MapBonus.NextBattleDamageBonus;
	MapBonus.NextBattleDamageBonus = 0.0f;
	return Bonus;
}

int32 UBWGameInstance::GetMainGrowthStat() const
{
	switch (SelectedClass)
	{
	case EClassType::TEC: return 1;
	case EClassType::INT: return 2;
	case EClassType::FTH: return 3;
	case EClassType::HP:  return 4;
	default:              return 0;
	}
}

void UBWGameInstance::SpendStatPoint(int32 Stat)
{
	if (PendingStatPoints <= 0) return;
	--PendingStatPoints;
	switch (Stat)
	{
	case 0: ++PlayerPHA; break;
	case 1: ++PlayerTEC; break;
	case 2: ++PlayerINT; break;
	case 3: ++PlayerFTH; break;
	case 4: PlayerMaxHP += HPPerStatPoint; PlayerCurrentHP += HPPerStatPoint; break;
	default: ++PlayerDefensePoints; break;
	}
}

bool UBWGameInstance::AdvanceToNextAct()
{
	if (CurrentStage >= UBWMapRules::NumActs) return false;
	++CurrentStage;

	// 배분하지 않은 포인트(측정 봇 등)는 주 능력치로
	while (PendingStatPoints > 0) SpendStatPoint(GetMainGrowthStat());
	// 영혼의 탄환은 마지막 막(3막)에 들어서는 순간 영혼을 잃는다
	if (CurrentStage >= UBWMapRules::NumActs) SoulBullets = 0;
	GenerateWorldMap();
	SaveRun();
	return true;
}

bool UBWGameInstance::IsStepRevealed(int32 Step) const
{
	return Step <= CurrentStep + VisibilityRange;
}

void UBWGameInstance::GetNodeVisibilityState(int32 Step, EMapNodeType Type, bool& bOutIsVisible, bool& bOutIsFogged)
{
	// 가시거리(2칸) 안만 노드가 보인다. 고정 칸(1·8·15칸)과 보스는 규칙으로 정해져 있어 멀리서도 종류를 보여 준다
	EMapNodeType FixedType;
	bOutIsVisible = true;
	bOutIsFogged = !IsStepRevealed(Step) && !UBWMapRules::GetFixedNodeType(Step, FixedType);
}

void UBWGameInstance::GetCountryStats(EDimensionType Dim, int32& OutConsecutive, int32& OutNeglected)
{
	OutConsecutive = ConsecutiveCounts.Contains(Dim) ? ConsecutiveCounts[Dim] : 0;
	OutNeglected = NeglectedCounts.Contains(Dim) ? NeglectedCounts[Dim] : 0;
}
