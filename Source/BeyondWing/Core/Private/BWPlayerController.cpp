#include "BWPlayerController.h"
#include "BeyondWing/Core/Public/BWAutoPlayer.h"
#include "GridManager.h"
#include "DeckComponent.h"
#include "BWCharacter.h"
#include "BWHandWidget.h"
#include "BeyondWing/UI/Public/BWBattleHUDWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Blueprint/WidgetTree.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 카드·적 행동 이펙트: Niagara가 지정돼 있으면 그것을, 아니면 예전 파티클을 쓴다
	void BWSpawnFX(UWorld* World, const TSoftObjectPtr<UNiagaraSystem>& Niagara, const TSoftObjectPtr<UParticleSystem>& Legacy, const FVector& Location)
	{
		if (UBWAutoPlayer::IsActive()) return;
		if (!World) return;
		if (UNiagaraSystem* System = Niagara.LoadSynchronous())
		{
			ABWPlayerController::PlayShortNiagara(World, System, Location);
		}
		else if (UParticleSystem* Particle = Legacy.LoadSynchronous())
		{
			UGameplayStatics::SpawnEmitterAtLocation(World, Particle, Location);
		}
	}
}
#include "Sound/SoundBase.h"
#include "BWDuelRules.h"
#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/UI/Public/BWGuardChoiceWidget.h"
#include "BeyondWing/UI/Public/BWTurnBannerWidget.h"
#include "BeyondWing/UI/Public/BWMiracleWidget.h"
#include "BeyondWing/UI/Public/BWBattleResultWidget.h"
#include "BeyondWing/Combat/Public/BWAugmentRules.h"
#include "BeyondWing/Combat/Public/BWLifestyleRules.h"

ABWPlayerController::ABWPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAutoManageActiveCameraTarget = false; // 말을 조종해도 전투 카메라를 그대로 쓴다
	bIsSkillTargetingMode = false;
	bIsDuelTargetingMode = false;
	CurrentSkillRange = 3;        
	SelectedCardIndex = -1; 
	LastHoveredTile = nullptr;
	HandWidget = nullptr;
	PlacementPreviewActor = nullptr;
}

void ABWPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	ActivateTouchInterface(nullptr);
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	AActor* FoundCamera = UGameplayStatics::GetActorOfClass(GetWorld(), ACameraActor::StaticClass());
	BattleCamera = FoundCamera;
	if (FoundCamera)
	{
		// [추가] 손패에 가리던 맨 아랫줄이 보이도록 시점을 뒤로 민다 (전투 레벨에서만)
		const bool bBattle = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()) != nullptr;
		const FVector Forward = FoundCamera->GetActorForwardVector();
		const FVector Flat = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
		CameraHome = FoundCamera->GetActorLocation() - (bBattle ? Forward * CameraPullBack + Flat * CameraBackShift : FVector::ZeroVector);
		FoundCamera->SetActorLocation(CameraHome);
		SetViewTargetWithBlend(FoundCamera);
	}

	// [수정] 게임 모드가 기본으로 만든 빈 폰(구체 메시)이 판 가운데에 보이지 않게 숨긴다
	if (APawn* StartPawn = GetPawn(); StartPawn && !Cast<ABWCharacter>(StartPawn))
	{
		StartPawn->SetActorHiddenInGame(true);
		StartPawn->SetActorEnableCollision(false);
	}


	AActor* FoundGrid = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass());
	if (FoundGrid) TargetGridManager = Cast<AGridManager>(FoundGrid);
}

void ABWPlayerController::SetupInputComponent() { Super::SetupInputComponent(); }

void ABWPlayerController::EnterMovementMode()
{
	if (bActionInProgress || bEnemyPhase) return;
	if (PossessedBWCharacter && PossessedBWCharacter->RemainingMove > 0)
	{
		CancelSelection();
		bIsMovementMode = true;
		if (TargetGridManager) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->RemainingMove);
		Prompt(FString::Printf(TEXT("파란 칸으로 이동 (남은 %d칸). 적을 누르면 맞닿는 칸까지 다가갑니다"), PossessedBWCharacter->RemainingMove), 0.0f);
	}
	else if (PossessedBWCharacter)
	{
		Prompt(TEXT("이번 턴 이동 칸을 다 썼습니다"));
	}
}

void ABWPlayerController::EnterBasicAttackMode()
{
	if (bActionInProgress || bEnemyPhase) return;
	if (!CanUseBasicStrike())
	{
		if (PossessedBWCharacter && PossessedBWCharacter->IsStunned()) PlayCardUnusableFeedback(TEXT("기절 상태라 격돌을 걸 수 없습니다."));
		else if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent && !PossessedBWCharacter->DeckComponent->CanOpenDuel()) PlayCardUnusableFeedback(TEXT("이번 턴에 이미 격돌을 열었습니다."));
		else PlayCardUnusableFeedback(FString::Printf(TEXT("기본 공격 사거리(%d칸) 안에 적이 없습니다. 먼저 적에게 다가가세요"), GetReachForHandIndex(INDEX_NONE)));
		return;
	}

	// [수정] 가까운 적을 자동으로 고르지 않는다: 사거리 안의 적을 강조하고 플레이어가 누른다 (우클릭 취소)
	EnterDuelTargeting(INDEX_NONE);
	Prompt(TEXT("기본 공격을 할 적을 누르세요 (금색 칸 · 우클릭 취소)"), 0.0f);
}

void ABWPlayerController::EnterDuelTargeting(int32 HandIndex)
{
	CancelSelection();
	bIsDuelTargetingMode = true;
	DuelCardIndex = HandIndex;
	SelectedCardIndex = HandIndex;
	if (HandWidget) HandWidget->SelectedCardIndex = HandIndex;
	RefreshDuelTargetHighlight();
}

void ABWPlayerController::RefreshDuelTargetHighlight()
{
	if (!TargetGridManager) return;
	TargetGridManager->ClearAllRanges();
	// [수정] 이 공격의 사거리 칸을 깔고, 그 안의 적을 대상으로 강조
	const int32 Reach = GetReachForHandIndex(DuelCardIndex);
	if (PossessedBWCharacter && Reach >= 1)
	{
		const FIntPoint Me = PossessedBWCharacter->OwnGridCoordinate;
		for (int32 dx = -Reach; dx <= Reach; ++dx)
		{
			for (int32 dy = -Reach; dy <= Reach; ++dy)
			{
				const int32 Dist = FMath::Abs(dx) + FMath::Abs(dy);
				if (Dist == 0 || Dist > Reach) continue;
				AGridTile* Tile = TargetGridManager->GetTileAtCoordinate(Me + FIntPoint(dx, dy));
				if (Tile && !TargetGridManager->GetCharacterAtCoordinate(Tile->GridCoordinate) && Tile->CurrentState != ETileState::Wall) Tile->SetTileState(ETileState::AttackRange);
			}
		}
	}
	for (ABWCharacter* Enemy : GetEnemiesInReach(Reach))
	{
		if (AGridTile* Tile = TargetGridManager->GetTileAtCoordinate(Enemy->OwnGridCoordinate)) Tile->SetTileState(ETileState::AttackTarget);
	}
}

TArray<ABWCharacter*> ABWPlayerController::GetEnemiesInReach(int32 Reach) const
{
	TArray<ABWCharacter*> Result;
	if (!PossessedBWCharacter || !TargetGridManager) return Result;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		ABWCharacter* Other = Cast<ABWCharacter>(Actor);
		if (IsEnemyInReach(Other, Reach)) Result.Add(Other);
	}
	// 가까운 적부터
	const FIntPoint Me = PossessedBWCharacter->OwnGridCoordinate;
	Result.Sort([Me](const ABWCharacter& A, const ABWCharacter& B)
	{
		const FIntPoint DA = A.OwnGridCoordinate - Me, DB = B.OwnGridCoordinate - Me;
		return FMath::Abs(DA.X) + FMath::Abs(DA.Y) < FMath::Abs(DB.X) + FMath::Abs(DB.Y);
	});
	return Result;
}

bool ABWPlayerController::IsEnemyInReach(const ABWCharacter* Enemy, int32 Reach) const
{
	if (!Enemy || !PossessedBWCharacter || !Enemy->bIsEnemy || Enemy->CurrentHP <= 0 || Enemy->IsActorBeingDestroyed()) return false;
	const FIntPoint Delta = Enemy->OwnGridCoordinate - PossessedBWCharacter->OwnGridCoordinate;
	const int32 Dist = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
	return Dist >= 1 && Dist <= FMath::Max(1, Reach);
}

int32 ABWPlayerController::GetReachForHandIndex(int32 HandIndex) const
{
	if (!PossessedBWCharacter) return 1;
	const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
	if (HandIndex != INDEX_NONE && Deck && Deck->Hand.IsValidIndex(HandIndex) && Deck->Hand[HandIndex].CardKind == ECardKind::Attack)
	{
		return PossessedBWCharacter->GetAttackReach(&Deck->Hand[HandIndex]);
	}
	return PossessedBWCharacter->GetAttackReach(nullptr);
}

void ABWPlayerController::UpdateReachPreview()
{
	// 아무 모드도 아닐 때 공격카드를 집거나 올리면 그 카드의 사거리 칸을 보여 준다 (사거리 2 이상만)
	int32 Index = -1;
	if (bShowReachPreview && HandWidget && PossessedBWCharacter && !bActionInProgress && !bEnemyPhase && !bIsMovementMode && !bIsDuelTargetingMode)
	{
		const int32 Focus = HandWidget->GetFocusedCardIndex();
		const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		if (Deck && Deck->Hand.IsValidIndex(Focus) && Deck->Hand[Focus].CardKind == ECardKind::Attack && GetReachForHandIndex(Focus) > 1) Index = Focus;
	}
	if (Index == ReachPreviewIndex) return;
	ReachPreviewIndex = Index;

	if (Index >= 0)
	{
		const int32 SavedIndex = DuelCardIndex;
		DuelCardIndex = Index;
		RefreshDuelTargetHighlight();
		DuelCardIndex = SavedIndex;
		bReachPreviewShown = true;
	}
	else if (bReachPreviewShown)
	{
		bReachPreviewShown = false;
		if (TargetGridManager && !bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->ClearAllRanges();
	}
}

bool ABWPlayerController::GetDuelForecast(const ABWCharacter* Enemy, FBWDuelForecast& Out, FString& OutLabel, FBWDiceRange& OutWinHP, FBWDiceRange& OutLoseHP) const
{
	ABWCharacter* Me = PossessedBWCharacter;
	if (!Me || !Enemy || !Enemy->bIsEnemy || Enemy->CurrentHP <= 0 || Enemy->IsDying() || bActionInProgress || bEnemyPhase || bIsMovementMode) return false;
	const UDeckComponent* Deck = Me->DeckComponent;
	if (!Deck || !Deck->CanOpenDuel() || Me->IsStunned() || Me->Life.bNoDuelThisTurn) return false;

	// 어떤 공격인지: 대상 고르는 중이면 그 공격, 아니면 집거나 올린 공격카드, 그것도 없으면 마우스 아래 적에게 기본 공격
	int32 Index = INDEX_NONE;
	if (bIsDuelTargetingMode) Index = DuelCardIndex;
	else
	{
		const int32 Focus = HandWidget ? HandWidget->GetFocusedCardIndex() : INDEX_NONE;
		if (Deck->Hand.IsValidIndex(Focus) && Deck->Hand[Focus].CardKind == ECardKind::Attack) Index = Focus;
		else
		{
			const AGridTile* Tile = CurrentTileUnderCursor;
			const ABWCharacter* Hovered = Tile && TargetGridManager ? TargetGridManager->GetCharacterAtCoordinate(Tile->GridCoordinate) : nullptr;
			if (Hovered != Enemy) return false;
		}
	}
	const FBWCardData* Card = Deck->Hand.IsValidIndex(Index) && Deck->Hand[Index].CardKind == ECardKind::Attack ? &Deck->Hand[Index] : nullptr;
	if (Card)
	{
		FString Reason;
		if (!CanUseLifestyleCard(*Card, Reason)) return false;
	}
	if (!IsEnemyInReach(Enemy, GetReachForHandIndex(Card ? Index : INDEX_NONE))) return false;

	const FBWDuelSide Mine = MakeFieldAttackSide(Me, Card, Enemy);
	const FBWDuelSide Theirs = MakeCounterSide(Enemy, Mine);
	Out = UBWDuelRules::ForecastDuel(Mine, Theirs);
	OutLabel = Card ? FString::Printf(TEXT("기본 공격 + %s"), *Card->CardName) : FString(TEXT("기본 공격"));

	// 이기면: 적 방어도가 먼저 깎인다 (관통이면 무시)
	const int32 EnemyBlock = Card && Card->bIgnoreTargetBlock ? 0 : Enemy->Block;
	OutWinHP = FBWDiceRange(FMath::Max(0, Out.WinDamage.Min - EnemyBlock), FMath::Max(0, Out.WinDamage.Max - EnemyBlock));
	// 지면: 내 고정 방어 → 방어도 (내가 연 격돌에서는 마나 가드를 쓰지 않는다)
	const int32 Flat = Me->Defense.Type == EDefenseType::Flat ? Me->Defense.FlatReduction : 0;
	OutLoseHP = FBWDiceRange(FMath::Max(0, Out.LoseDamage.Min - Flat - Me->Block), FMath::Max(0, Out.LoseDamage.Max - Flat - Me->Block));
	if (IsOutOfEnemyReach(Enemy)) OutLoseHP = FBWDiceRange(0, 0);
	return true;
}

FBWDuelSide ABWPlayerController::MakeCounterSide(const ABWCharacter* Enemy, const FBWDuelSide& Mine) const
{
	FBWDuelSide Side = MakeFieldEnemySide(Enemy, 0, true);
	if (Enemy && Enemy->bMirrorCounter && !Side.bGuard) Side.SkillRange = Mine.SkillRange;
	return Side;
}

bool ABWPlayerController::HasSet(EBWAugmentSet Set, int32 Count) const
{
	const ABWCharacter* Me = PossessedBWCharacter;
	return Me && Me->AugmentTotals.SetCounts.IsValidIndex((int32)Set) && Me->AugmentTotals.SetCounts[(int32)Set] >= Count;
}

ABWCharacter* ABWPlayerController::NearestEnemy() const
{
	return PossessedBWCharacter ? PossessedBWCharacter->FindNearestOpponent() : nullptr;
}

void ABWPlayerController::SetEventHit(EBWAugmentSet Set, ABWCharacter* Target, int32 Damage, const TCHAR* Label)
{
	if (!Target || Target->CurrentHP <= 0 || Damage <= 0) return;
	UE_LOG(LogTemp, Warning, TEXT("[BWSet] %s %d"), Label, Damage);
	if (UBWAutoPlayer::IsActive() == false)
	{
		BWSpawnFX(GetWorld(), TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/KTP_Effect/Particles/Fly/Expolison_08_01.Expolison_08_01"))), nullptr, Target->GetActorLocation() + FVector(0, 0, 40));
		Target->ShowPopup(Label, FLinearColor(1.0f, 0.85f, 0.4f), 1.2f);
	}
	Target->TakeFixedDamage(Damage, Label);
}

void ABWPlayerController::OnSetHeal(int32 Healed)
{
	if (HasSet(EBWAugmentSet::Soltheris, 3)) SetEventHit(EBWAugmentSet::Soltheris, NearestEnemy(), Healed / 2, TEXT("정화의 빛"));
}

void ABWPlayerController::OnSetSelfDamage(int32 Amount)
{
	if (HasSet(EBWAugmentSet::Radiant, 3)) SetEventHit(EBWAugmentSet::Radiant, NearestEnemy(), Amount * 2, TEXT("피의 칼날"));
}

void ABWPlayerController::OnSetDebuffed(EStatusEffect Type, int32 Duration, int32 Value)
{
	ABWCharacter* Enemy = HasSet(EBWAugmentSet::Lirin, 3) ? NearestEnemy() : nullptr;
	if (!Enemy) return;
	Enemy->ApplyStatus(Type, Duration, Value);
	Enemy->ShowPopup(TEXT("되돌아가는 노래"), FLinearColor(0.5f, 0.9f, 1.0f), 1.2f);
}

void ABWPlayerController::PlayShortNiagara(UWorld* World, UNiagaraSystem* System, const FVector& Location, float EffectScale, float LifeSeconds)
{
	static constexpr float PlaySpeed = 3.0f;
	if (!World || !System) return;
	// KTP 폭발은 원본이 칸의 두 배가 넘어 줄여서 쓴다
	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(EffectScale));
	if (!Comp) return;
	Comp->SetCustomTimeDilation(PlaySpeed);
	TWeakObjectPtr<UNiagaraComponent> Weak(Comp);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, [Weak]()
	{
		if (Weak.IsValid()) Weak->DestroyComponent();
	}, LifeSeconds, false);
}

bool ABWPlayerController::CancelBattleSelection()
{
	if (!bIsDuelTargetingMode && !bIsMovementMode) return false;
	CancelSelection();
	return true;
}

bool ABWPlayerController::IsEnemyHovered(const ABWCharacter* Enemy) const
{
	const AGridTile* Tile = CurrentTileUnderCursor;
	return Tile && TargetGridManager && Enemy && Tile->GridCoordinate == Enemy->OwnGridCoordinate;
}

const ABWCharacter* ABWPlayerController::GetHoveredEnemy() const
{
	const AGridTile* Tile = CurrentTileUnderCursor;
	const ABWCharacter* Char = Tile && TargetGridManager ? TargetGridManager->GetCharacterAtCoordinate(Tile->GridCoordinate) : nullptr;
	return Char && Char->bIsEnemy && Char->CurrentHP > 0 && !Char->IsDying() ? Char : nullptr;
}

TArray<ABWCharacter*> ABWPlayerController::GetAdjacentEnemies()
{
	TArray<ABWCharacter*> Result;
	if (!PossessedBWCharacter || !TargetGridManager) return Result;

	static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (const FIntPoint& Dir : Dirs)
	{
		ABWCharacter* Other = TargetGridManager->GetCharacterAtCoordinate(PossessedBWCharacter->OwnGridCoordinate + Dir);
		if (Other && Other->bIsEnemy && Other->CurrentHP > 0 && !Other->IsActorBeingDestroyed()) Result.Add(Other);
	}
	return Result;
}

bool ABWPlayerController::IsPendingDuelTargetAdjacent() const
{
	if (!PendingDuelTarget || !PossessedBWCharacter || PendingDuelTarget->IsActorBeingDestroyed() || PendingDuelTarget->CurrentHP <= 0) return false;
	const FIntPoint Delta = PendingDuelTarget->OwnGridCoordinate - PossessedBWCharacter->OwnGridCoordinate;
	return FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) == 1;
}

bool ABWPlayerController::CanUseBasicStrike()
{
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return false;
	UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
	// [수정] 기본 공격은 손패와 상관없이 언제나 (공격카드는 이 공격을 강화한다)
	return Deck->CanOpenDuel() && !PossessedBWCharacter->IsStunned() && GetEnemiesInReach(GetReachForHandIndex(INDEX_NONE)).Num() > 0;
}

bool ABWPlayerController::HasUsableAttackCardInHand() const
{
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return false;
	for (const FBWCardData& Card : PossessedBWCharacter->DeckComponent->Hand)
	{
		if (Card.CardKind == ECardKind::Attack) return true;
	}
	return false;
}

void ABWPlayerController::SetBattlefield(EDimensionType Dimension, bool bOverloaded)
{
	Battlefield = UBWDimensionRules::MakeBattlefield(Dimension, bOverloaded);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Battlefield: %s%s"), *UEnum::GetValueAsString(Dimension), Battlefield.bOverloaded ? TEXT(" (overloaded)") : TEXT(""));
	ApplyDimensionAtmosphere();
}

void ABWPlayerController::ApplyDimensionAtmosphere()
{
	if (UBWAutoPlayer::IsActive()) return;
	// 차원별 분위기 (BeyondWing_세계관.md): 파트로바스=마법의 숲(보랏빛 신비), 베렌드=거대한 숲(따뜻한 숲 빛),
	// 겔레온=하늘의 공중 도시(밝은 금빛), 타이가=달빛 아래 정글(푸른 밤). 차원이 없으면 레벨 기본값 그대로
	struct FAtmosphere
	{
		FLinearColor Sun; float SunIntensity;
		FLinearColor Sky; float SkyIntensity;
		FLinearColor Fog; float FogDensity;
		FLinearColor Tint; float Saturation; float Contrast; float Vignette; float Bloom;
		float ExposureBias; // [추가] 자동 노출 보정 (지형 소품·바닥이 들어와 화면이 하얗게 뜨던 것을 잡는다)
	};
	FAtmosphere A;
	switch (Battlefield.Dimension)
	{
	case EDimensionType::Patrovas:
		A = { FLinearColor(0.75f, 0.68f, 1.0f), 4.5f, FLinearColor(0.7f, 0.6f, 1.0f), 1.1f, FLinearColor(0.35f, 0.22f, 0.6f), 0.04f, FLinearColor(0.92f, 0.86f, 1.08f), 1.15f, 1.05f, 0.45f, 1.2f, -1.0f };
		break;
	case EDimensionType::Berend:
		A = { FLinearColor(1.0f, 0.86f, 0.6f), 7.5f, FLinearColor(0.85f, 0.95f, 0.7f), 1.0f, FLinearColor(0.3f, 0.38f, 0.2f), 0.03f, FLinearColor(1.02f, 1.02f, 0.9f), 1.08f, 1.1f, 0.4f, 0.8f, -1.0f };
		break;
	case EDimensionType::Geleon:
		A = { FLinearColor(1.0f, 0.95f, 0.82f), 6.5f, FLinearColor(0.75f, 0.88f, 1.0f), 1.2f, FLinearColor(0.6f, 0.75f, 0.95f), 0.02f, FLinearColor(1.03f, 1.0f, 0.96f), 1.05f, 1.05f, 0.2f, 1.0f, -1.3f };
		break;
	case EDimensionType::Taiga:
		A = { FLinearColor(0.6f, 0.68f, 0.95f), 1.8f, FLinearColor(0.4f, 0.5f, 0.75f), 0.35f, FLinearColor(0.03f, 0.08f, 0.09f), 0.05f, FLinearColor(0.84f, 0.92f, 1.0f), 0.95f, 1.15f, 0.7f, 1.3f, -0.3f };
		break;
	default:
		if (AtmosphereVolume) AtmosphereVolume->bEnabled = false;
		return;
	}
	if (Battlefield.bOverloaded)
	{
		// 과부하: 붉은 기운이 돌고 가장자리가 더 어둡다
		A.Tint *= FLinearColor(1.1f, 0.86f, 0.84f);
		A.Fog = FMath::Lerp(A.Fog, FLinearColor(0.45f, 0.05f, 0.05f), 0.5f);
		A.Vignette += 0.15f;
		A.Saturation += 0.1f;
	}

	UWorld* World = GetWorld();
	if (!World) return;
	if (ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(World, ADirectionalLight::StaticClass())))
	{
		if (ULightComponent* Light = Sun->GetLightComponent())
		{
			Light->SetLightColor(A.Sun);
			Light->SetIntensity(A.SunIntensity);
		}
	}
	if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(World, ASkyLight::StaticClass())))
	{
		if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
		{
			SkyComp->SetLightColor(A.Sky);
			SkyComp->SetIntensity(A.SkyIntensity);
			SkyComp->RecaptureSky();
		}
	}
	if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(World, AExponentialHeightFog::StaticClass())))
	{
		if (UExponentialHeightFogComponent* FogComp = Fog->GetComponent())
		{
			FogComp->SetFogInscatteringColor(A.Fog);
			FogComp->SetFogDensity(A.FogDensity);
		}
	}

	// 화면 색감은 레벨 전체에 걸리는 후처리 볼륨 하나로 (없으면 만든다)
	if (!AtmosphereVolume)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AtmosphereVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	}
	if (AtmosphereVolume)
	{
		AtmosphereVolume->bUnbound = true;
		AtmosphereVolume->bEnabled = true;
		AtmosphereVolume->Priority = 10.0f;
		FPostProcessSettings& S = AtmosphereVolume->Settings;
		S.bOverride_SceneColorTint = true;   S.SceneColorTint = A.Tint;
		S.bOverride_ColorSaturation = true;  S.ColorSaturation = FVector4(A.Saturation, A.Saturation, A.Saturation, 1.0f);
		S.bOverride_ColorContrast = true;    S.ColorContrast = FVector4(A.Contrast, A.Contrast, A.Contrast, 1.0f);
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = A.Vignette;
		S.bOverride_BloomIntensity = true;   S.BloomIntensity = A.Bloom;
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = A.ExposureBias;
	}
}

void ABWPlayerController::InitBattlefieldFromGameInstance()
{
	// 지금 서 있는 차원 줄의 기믹. 연속 체류 4턴 이상이면 과부하 (지도 기획서 7-1)
	EDimensionType Dimension = EDimensionType::None;
	bool bOverloaded = false;
	float BonusDamage = 0.0f;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI)
	{
		// [수정] 2·3막은 줄 순서가 섞이므로 줄 번호가 아니라 그 줄의 차원을 쓴다
		Dimension = GI->GetCurrentDimension();
		bOverloaded = GI->GetCurrentOverloadLevel() >= 1;
		BonusDamage = GI->ConsumeNextBattleDamageBonus(); // 겔레온 방치 보너스: 이번 전투 1회
	}
	SetBattlefield(Dimension, bOverloaded);
	Battlefield.BonusDamage = BonusDamage;
	if (BonusDamage > 0.0f) UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Map bonus: damage +%.0f%% this battle"), BonusDamage * 100.0f);
}

FBWDuelSide ABWPlayerController::MakeFieldAttackSide(ABWCharacter* Me, const FBWCardData* Card, const ABWCharacter* Enemy) const
{
	const int32 Advance = Me->AugTrack.MovedTiles; // 멀리서 쏘기만 하면 오르지 않는다
	return UBWDimensionRules::ApplyToPlayerSide(Battlefield, Me->MakeAttackSide(Card, Enemy), Advance, UBWDimensionRules::IsOnManaLine(Battlefield, Me->OwnGridCoordinate));
}

FBWDuelSide ABWPlayerController::MakeFieldEnemySide(const ABWCharacter* Enemy, int32 Advance, bool bCounter) const
{
	return UBWDimensionRules::ApplyToEnemySide(Battlefield, Enemy->MakeEnemyDuelSide(bCounter), Advance, UBWDimensionRules::IsOnManaLine(Battlefield, Enemy->OwnGridCoordinate));
}

void ABWPlayerController::ResolvePushAfterDuel(ABWCharacter* Pusher, ABWCharacter* Target, bool bPusherIsPlayer)
{
	if (!Pusher || !Target || !TargetGridManager || Target->CurrentHP <= 0 || Target->IsActorBeingDestroyed()) return;
	if (!Target->bIsEnemy && Target->AugmentTotals.Has(EBWAugStat::PushImmune)) return;

	// 미는 쪽에서 맞은 쪽으로 1칸 (멀리서 밀어도 1칸)
	const FIntPoint Delta = Target->OwnGridCoordinate - Pusher->OwnGridCoordinate;
	const FIntPoint Dir = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y) ? FIntPoint(FMath::Sign(Delta.X), 0) : FIntPoint(0, FMath::Sign(Delta.Y));
	bool bHitWall = false;
	if (TargetGridManager->TryPushCharacter(Target, Dir, bHitWall))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Pushed to %s"), *Target->OwnGridCoordinate.ToString());
		// [추가] 가시덤불로 밀어 넣으면 피해
		TargetGridManager->ApplyEnteredTerrain(Target, { Target->OwnGridCoordinate });
		return;
	}
	if (!bHitWall) return;
	if (!Target->bIsEnemy && Target->AugmentTotals.Has(EBWAugStat::WallImmune)) return;

	// 11-2 베렌드: 벽에 닿으면 추가 피해 + 기절 (과부하면 적도 나를 벽에 밀쳐 추가 피해, 기절은 없음)
	bool bExtraDamage = false, bStun = false;
	UBWDimensionRules::GetWallSlamRule(Battlefield, bPusherIsPlayer, bExtraDamage, bStun);
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Wall slam (extra damage %d, stun %d)"), bExtraDamage ? 1 : 0, bStun ? 1 : 0);
	if (bExtraDamage) Target->TakeHit(UBWDimensionRules::BerendWallDamage * (Battlefield.bOverloaded && !bPusherIsPlayer ? 2 : 1), false); // 과부하: 내가 부딪히는 피해 2배
	if (bStun && Target->CurrentHP > 0 && !Target->IsActorBeingDestroyed()) Target->ApplyStatus(EStatusEffect::Stun, UBWDimensionRules::BerendStunTurns, 0);
}

void ABWPlayerController::PlayCardUnusableFeedback(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Reason);
	Prompt(Reason, 3.0f, EBWPromptStyle::Warning);
	if (CardUnusableSound.LoadSynchronous()) UGameplayStatics::PlaySound2D(GetWorld(), CardUnusableSound.Get());
}

bool ABWPlayerController::OpenDuel(ABWCharacter* Enemy, int32 HandIndex)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	UDeckComponent* Deck = PlayerChar ? PlayerChar->DeckComponent : nullptr;
	if (!Deck || !TargetGridManager || !Enemy || !Enemy->bIsEnemy || Enemy->CurrentHP <= 0 || Enemy->IsActorBeingDestroyed()) return false;

	// 기절하면 이번 턴에 합을 걸지 못한다
	if (PlayerChar->IsStunned())
	{
		PlayCardUnusableFeedback(TEXT("기절 상태라 격돌을 걸 수 없습니다."));
		return false;
	}

	// 내가 선공으로 여는 합은 한 턴에 한 번
	if (!Deck->CanOpenDuel())
	{
		PlayCardUnusableFeedback(TEXT("이번 턴에 이미 격돌을 열었습니다."));
		return false;
	}
	if (PlayerChar->Life.bNoDuelThisTurn)
	{
		PlayCardUnusableFeedback(TEXT("이번 턴에는 격돌을 걸 수 없습니다 (질풍노도·명상 등)."));
		return false;
	}

	// [수정] 사거리(상하좌우 칸 수) 안의 적만. 기본 공격은 보통 1칸(맞닿은 적), 사거리가 긴 공격카드로 강화하면 멀리서도 건다
	const int32 Reach = GetReachForHandIndex(HandIndex);
	if (!IsEnemyInReach(Enemy, Reach))
	{
		PlayCardUnusableFeedback(Reach <= 1 ? FString(TEXT("맞닿은 적에게만 격돌을 걸 수 있습니다."))
			: FString::Printf(TEXT("사거리 %d칸 밖의 적입니다."), Reach));
		return false;
	}

	FBWCardData AttackCard;
	if (HandIndex == INDEX_NONE)
	{
		// [수정] 기본 공격은 언제나 쓸 수 있다 (빙의로 마탄이 강제될 때만 빼고)
		if (IsForcedToMagicBullet())
		{
			PlayCardUnusableFeedback(TEXT("빙의: 패에 있는 마탄을 먼저 쏴야 합니다."));
			return false;
		}
		AttackCard = PlayerChar->GetBasicAttackCard();
	}
	else
	{
		if (!Deck->Hand.IsValidIndex(HandIndex) || Deck->Hand[HandIndex].CardKind != ECardKind::Attack) return false;
		const EBWCardUseResult Check = Deck->CanUseCard(HandIndex, PlayerChar->CurrentMP);
		if (Check != EBWCardUseResult::Ok)
		{
			PlayCardUnusableFeedback(FString::Printf(TEXT("카드를 쓸 수 없습니다: %s"), *UEnum::GetValueAsString(Check)));
			return false;
		}
		FString LifestyleReason;
		if (!CanUseLifestyleCard(Deck->Hand[HandIndex], LifestyleReason))
		{
			PlayCardUnusableFeedback(LifestyleReason);
			return false;
		}
		if (HandWidget) HandWidget->PlayDiscardAnimation(HandIndex);
		Deck->UseCard(HandIndex, AttackCard);

		// 삶의 방식: 폭발은 남은 MP를 최대 10까지 더 건다, 마탄은 장전을 쓴다, 자해·나에게 거는 상태
		if (AttackCard.Special == EBWCardSpecial::Explosion)
		{
			PlayerChar->ExplosionExtraMP = UBWLifestyleRules::ExplosionExtraMP(PlayerChar->CurrentMP);
			PlayerChar->CurrentMP -= PlayerChar->ExplosionExtraMP;
		}
		if (AttackCard.Special == EBWCardSpecial::MagicBullet)
		{
			PlayerChar->Life.bLoaded = false;
			PlayerChar->Life.bFiredMagicThisTurn = true;
		}
		if (AttackCard.SelfStatus != EStatusEffect::None) PlayerChar->ApplyStatus(AttackCard.SelfStatus, AttackCard.SelfStatusDuration, AttackCard.SelfStatusValue);
		if (AttackCard.SelfDamage > 0)
		{
			PlayerChar->TakeFixedDamage(AttackCard.SelfDamage, TEXT("자해"));
			if (PlayerChar->CurrentHP <= 0) return true;
			TriggerMiracle(EBWAugmentSet::Radiant, AttackCard.SelfDamage * 3);
		}
	}
	Deck->NotifyDuelOpened();
	PendingDuelTarget = nullptr;
	CancelSelection();

	// [수정] 사용자 결정: 합을 걸면 이번 턴 남은 이동은 모두 쓴다 (접촉만으로는 이동이 끝나지 않음)
	PlayerChar->AugTrack.bMoveLeftAtDuel = PlayerChar->RemainingMove > 0;
	if (!PlayerChar->AugmentTotals.Has(EBWAugStat::DuelKeepMove)) PlayerChar->RemainingMove = 0;

	// 연출 순서: 합 화면(숫자 굴림) → 이긴 쪽이 찌르는 순간 피해 → 제자리로 돌아온 뒤 다시 조작
	bActionInProgress = true;
	PlayPlayerDuel(PlayerChar, Enemy, AttackCard, HandIndex != INDEX_NONE, [this]()
	{
		bActionInProgress = false;
		if (TargetGridManager) TargetGridManager->ClearAllRanges();
	});
	return true;
}

FString ABWPlayerController::DescribePlayerDefense(const ABWCharacter* PlayerChar) const
{
	if (!PlayerChar) return TEXT("방어");
	switch (PlayerChar->Defense.Type)
	{
	case EDefenseType::Evade:       return FString::Printf(TEXT("회피 %d%%"), FMath::RoundToInt(FMath::Min(PlayerChar->Defense.EvadeChance + PlayerChar->Life.EvadeBonus, UBWDuelRules::MaxEvadeChance) * 100.0f));
	case EDefenseType::MagicShield: return PlayerChar->CharacterClass == EClassType::FTH ? FString::Printf(TEXT("신성 가드 (신성력 %d)"), PlayerChar->CurrentMP) : FString::Printf(TEXT("마나 가드 (MP %d)"), PlayerChar->CurrentMP);
	default:                        return FString::Printf(TEXT("방어 %d"), PlayerChar->Defense.FlatReduction);
	}
}

void ABWPlayerController::ShowDuel(const FBWDuelPresentation& Data, TFunction<void()> OnFinished)
{
	// 측정 봇은 격돌 화면을 건너뛴다 (결과는 이미 정해져 있다)
	if (UBWAutoPlayer::IsActive())
	{
		if (OnFinished) OnFinished();
		return;
	}
	if (!DuelWidget)
	{
		TSubclassOf<UBWDuelWidget> WidgetClass = DuelWidgetClass ? DuelWidgetClass : TSubclassOf<UBWDuelWidget>(UBWDuelWidget::StaticClass());
		DuelWidget = CreateWidget<UBWDuelWidget>(this, WidgetClass);
	}
	if (!DuelWidget)
	{
		if (OnFinished) OnFinished();
		return;
	}
	DuelWidget->PlayDuel(Data, MoveTemp(OnFinished));
}

void ABWPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 연출 도중 레벨이 바뀌어도 시간 배율과 카메라가 원래대로 돌아오게
	if (HitStopTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HitStopTicker);
		HitStopTicker.Reset();
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	}
	if (ShakeTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ShakeTicker);
		ShakeTicker.Reset();
		if (BattleCamera) BattleCamera->SetActorLocation(GetCameraBaseLocation());
	}
	Super::EndPlay(EndPlayReason);
}

void ABWPlayerController::PlayImpactFeedback(ABWCharacter* Attacker, ABWCharacter* Target, const FBWDefenseResult& Defended, int32 HPLost, bool bCritical)
{
	if (!Target || UBWAutoPlayer::IsActive()) return;
	const FVector From = Attacker ? Attacker->GetActorLocation() : Target->GetActorLocation() - Target->GetActorForwardVector() * 100.0f;

	// 회피: 옆으로 비키기만 하고 멈칫·흔들림은 없다
	if (Defended.bEvaded)
	{
		Target->PlayDodge(From);
		return;
	}
	if (Defended.DamageAfterDefense <= 0 && HPLost <= 0) return;

	Target->PlayHitReaction(From, bCritical);
	StartHitStop(bCritical ? CritHitStopTime : HitStopTime);
	StartCameraShake(bCritical ? CritShakeStrength : HitShakeStrength, bCritical ? ShakeDuration * 1.6f : ShakeDuration);
	if (bCritical) Target->ShowPopup(TEXT("치명타!"), FLinearColor(1.0f, 0.8f, 0.2f), 1.5f);
}

void ABWPlayerController::StartHitStop(float RealSeconds)
{
	if (RealSeconds <= 0.0f || UBWAutoPlayer::IsActive()) return;
	// 아주 짧게 세상을 거의 멈춘다. 되돌리기는 실제 시간 기준(코어 티커)이라 시간 배율의 영향을 받지 않는다
	if (HitStopTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(HitStopTicker);
	UGameplayStatics::SetGlobalTimeDilation(this, 0.03f);
	TWeakObjectPtr<ABWPlayerController> WeakThis(this);
	HitStopTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (ABWPlayerController* PC = WeakThis.Get())
		{
			UGameplayStatics::SetGlobalTimeDilation(PC, 1.0f);
			PC->HitStopTicker.Reset();
		}
		return false;
	}), RealSeconds);
}

void ABWPlayerController::StartCameraShake(float Amplitude, float Duration)
{
	if (!BattleCamera || Amplitude <= 0.0f || Duration <= 0.0f) return;
	if (!ShakeTicker.IsValid())
	{
		TWeakObjectPtr<ABWPlayerController> WeakThis(this);
		ShakeTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float DeltaTime)
		{
			ABWPlayerController* PC = WeakThis.Get();
			return PC ? PC->TickCameraShake(DeltaTime) : false;
		}));
	}
	// 이미 흔들리는 중이면 더 센 쪽으로 다시 시작
	ShakeStrength = FMath::Max(Amplitude, ShakeTimeLeft > 0.0f ? ShakeStrength : 0.0f);
	ShakeTimeLeft = ShakeTotalTime = Duration;
}

bool ABWPlayerController::TickCameraShake(float DeltaTime)
{
	if (!BattleCamera)
	{
		ShakeTicker.Reset();
		return false;
	}
	ShakeTimeLeft -= DeltaTime;
	if (ShakeTimeLeft <= 0.0f)
	{
		BattleCamera->SetActorLocation(GetCameraBaseLocation());
		ShakeTicker.Reset();
		return false;
	}
	// 남은 시간에 비례해 약해지는 무작위 흔들림 (WASD로 옮긴 위치 기준)
	const float Falloff = ShakeTimeLeft / FMath::Max(ShakeTotalTime, KINDA_SMALL_NUMBER);
	const FVector Offset = FMath::VRand() * ShakeStrength * Falloff * Falloff;
	BattleCamera->SetActorLocation(GetCameraBaseLocation() + Offset);
	return true;
}

FVector ABWPlayerController::GetCameraBaseLocation() const
{
	if (!BattleCamera) return CameraHome;
	const FVector Forward = BattleCamera->GetActorForwardVector();
	const FVector Flat = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Flat);
	// 기적 동안에는 보는 방향 반대로 물러나 큰 연출이 화면에 다 들어오게 한다
	return CameraHome + Flat * CameraPan.X + Right * CameraPan.Y - Forward * (MiraclePullDistance * MiraclePull);
}

void ABWPlayerController::UpdateCameraPan(float DeltaTime)
{
	if (!BattleCamera || !TargetGridManager) return;
	FVector2D Input = FVector2D::ZeroVector;
	if (IsInputKeyDown(EKeys::W)) Input.X += 1.0f;
	if (IsInputKeyDown(EKeys::S)) Input.X -= 1.0f;
	if (IsInputKeyDown(EKeys::D)) Input.Y += 1.0f;
	if (IsInputKeyDown(EKeys::A)) Input.Y -= 1.0f;
	if (Input.IsZero()) return;

	// 히트스톱(시간 배율)과 상관없이 같은 속도로 움직이도록 실제 시간을 쓴다
	const float Dilation = FMath::Max(UGameplayStatics::GetGlobalTimeDilation(this), KINDA_SMALL_NUMBER);
	const FVector2D Before = CameraPan;
	CameraPan += Input.GetSafeNormal() * CameraPanSpeed * (DeltaTime / Dilation);
	CameraPan.X = FMath::Clamp(CameraPan.X, -CameraPanLimit, CameraPanLimit);
	CameraPan.Y = FMath::Clamp(CameraPan.Y, -CameraPanLimit, CameraPanLimit);
	if (CameraPan != Before && !ShakeTicker.IsValid()) BattleCamera->SetActorLocation(GetCameraBaseLocation());
}

void ABWPlayerController::TriggerMiracle(EBWAugmentSet Set, int32 Damage)
{
	if (!HasSet(Set, 4) || MiraclesThisTurn.Contains((int32)Set) || !PossessedBWCharacter) return;
	MiraclesThisTurn.Add((int32)Set);
	UE_LOG(LogTemp, Warning, TEXT("[BWSet] 기적:%d %d"), (int32)Set, Damage);
	if (UBWAutoPlayer::IsActive())
	{
		// 측정 봇은 연출 없이 바로 적용한다
		for (ABWCharacter* Enemy : GetEnemiesInReach(99))
		{
			if (Damage > 0) Enemy->TakeFixedDamage(Damage, TEXT("기적"));
			if (Set == EBWAugmentSet::Lirin && Enemy->CurrentHP > 0) Enemy->ApplyStatus(EStatusEffect::Stun, 1, 0);
		}
		return;
	}
	QueueMiracleShow(Set, Damage);
}

void ABWPlayerController::QueueMiracleShow(EBWAugmentSet Set, int32 Damage)
{
	if (UBWAutoPlayer::IsActive() || !PossessedBWCharacter) return;
	struct FLook { const TCHAR* Title; const TCHAR* Sub; FLinearColor Color; const TCHAR* Effect; };
	FLook Look = { TEXT("기적"), TEXT(""), FLinearColor::White, TEXT("Expolison_05_01") };
	switch (Set)
	{
	case EBWAugmentSet::Astrin:     Look = { TEXT("별의 심판"), TEXT("별이 내린 판결에, 항소는 없으니"), FLinearColor(0.75f, 0.8f, 1.0f), TEXT("Expolison_05_01") }; break;
	case EBWAugmentSet::Sylpira:    Look = { TEXT("폭풍의 장막"), TEXT("바람을 막아선 자, 아직 없으니"), FLinearColor(0.5f, 1.0f, 0.75f), TEXT("Expolison_10_01") }; break;
	case EBWAugmentSet::Soltheris:  Look = { TEXT("태양 강림"), TEXT("해가 뜨면, 그림자는 제자리로"), FLinearColor(1.0f, 0.85f, 0.35f), TEXT("Expolison_03_01") }; break;
	case EBWAugmentSet::Urska:      Look = { TEXT("곰신의 포효"), TEXT("참아 온 세월만큼"), FLinearColor(0.9f, 0.6f, 0.3f), TEXT("Expolison_08_01") }; break;
	case EBWAugmentSet::Lirin:      Look = { TEXT("망각의 안개"), TEXT("이름을 잊은 자, 칼도 잊으리"), FLinearColor(0.75f, 0.5f, 1.0f), TEXT("Expolison_06_01") }; break;
	case EBWAugmentSet::Lunaris:    Look = { TEXT("월식"), TEXT("달이 눈을 감는 밤"), FLinearColor(0.7f, 0.75f, 0.95f), TEXT("Expolison_09_01") }; break;
	case EBWAugmentSet::Berend:     Look = { TEXT("대지 가르기"), TEXT("무너진 땅 위에, 서 있는 자는 하나"), FLinearColor(1.0f, 0.5f, 0.25f), TEXT("Expolison_03_01") }; break;
	case EBWAugmentSet::Taiga:      Look = { TEXT("천 개의 그림자"), TEXT("그림자를 벤 자는 없으니"), FLinearColor(0.45f, 0.95f, 0.6f), TEXT("Expolison_07_01") }; break;
	case EBWAugmentSet::Geleon:     Look = { TEXT("일제 사격"), TEXT("말이 끝나는 곳에, 포성"), FLinearColor(0.5f, 0.8f, 1.0f), TEXT("Expolison_02_01") }; break;
	case EBWAugmentSet::Patrovas:   Look = { TEXT("마력 폭풍"), TEXT("지식의 끝에 남는 것은, 폭풍"), FLinearColor(0.8f, 0.45f, 1.0f), TEXT("Expolison_04_01") }; break;
	case EBWAugmentSet::Megenberta: Look = { TEXT("황금비"), TEXT("세상 모든 것에는, 값이 있으니"), FLinearColor(1.0f, 0.82f, 0.25f), TEXT("Expolison_08_01") }; break;
	case EBWAugmentSet::Radiant:    Look = { TEXT("피의 비"), TEXT("흘린 피는 거짓을 모르니"), FLinearColor(1.0f, 0.2f, 0.25f), TEXT("Expolison_03_01") }; break;
	default: break;
	}

	MiracleQueue.Add([this, Look, Set, Damage]()
	{
		if (!PossessedBWCharacter) return;
		if (!MiracleWidget) MiracleWidget = CreateWidget<UBWMiracleWidget>(this, UBWMiracleWidget::StaticClass());
		if (MiracleWidget) MiracleWidget->PlayMiracle(Look.Title, Look.Sub, Look.Color);
		// 같은 기적은 전투에서 처음 한 번만 길게, 다음부터는 짧게
		const bool bFull = !MiraclesShownFull.Contains((int32)Set);
		MiraclesShownFull.Add((int32)Set);
		const float SceneLength = BuildMiracleScene(Set, Damage, Look.Color, bFull ? 1.0f : 0.45f);
		if (SceneLength > 0.0f)
		{
			LastMiracleLength = SceneLength + 0.3f;
			return;
		}
		LastMiracleLength = 2.3f;
		StartHitStop(0.12f);

		// 내 주변이 터진 뒤, 세트마다 다른 동선으로 적을 친다
		const FString Path = FString::Printf(TEXT("/Game/KTP_Effect/Particles/Fly/%s.%s"), Look.Effect, Look.Effect);
		if (UNiagaraSystem* System = Cast<UNiagaraSystem>(FSoftObjectPath(Path).TryLoad()))
		{
			const FVector Center = PossessedBWCharacter->GetActorLocation();
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector Ring = FRotator(0.0f, i * 45.0f, 0.0f).RotateVector(FVector(170.0f, 0.0f, 40.0f));
				PlayShortNiagara(GetWorld(), System, Center + Ring, 0.4f, 0.28f);
			}
		}
		BuildMiracleShots(Set, Damage);
	});
	PlayNextMiracle();
}

void ABWPlayerController::BuildMiracleShots(EBWAugmentSet Set, int32 Damage)
{
	if (!PossessedBWCharacter || !TargetGridManager) return;
	auto Fx = [](const TCHAR* Name) -> UNiagaraSystem*
	{
		const FString Path = FString::Printf(TEXT("/Game/KTP_Effect/Particles/Fly/Others/%s.%s"), Name, Name);
		return Cast<UNiagaraSystem>(FSoftObjectPath(Path).TryLoad());
	};
	auto Boom = [](const TCHAR* Name) -> UNiagaraSystem*
	{
		const FString Path = FString::Printf(TEXT("/Game/KTP_Effect/Particles/Fly/%s.%s"), Name, Name);
		return Cast<UNiagaraSystem>(FSoftObjectPath(Path).TryLoad());
	};
	const double Now = FPlatformTime::Seconds() + 0.35; // 내 주변 폭발 뒤에 시작
	const FVector Me = PossessedBWCharacter->GetActorLocation();
	const TArray<ABWCharacter*> Enemies = GetEnemiesInReach(99);
	const FVector Up(0.0f, 0.0f, 900.0f);
	auto Spot = [](const ABWCharacter* E) { return E->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f); };
	auto RandomTile = [this]()
	{
		return TargetGridManager->GetTileWorldLocation(FIntPoint(FMath::RandRange(0, TargetGridManager->Width - 1), FMath::RandRange(0, TargetGridManager->Height - 1))) + FVector(0.0f, 0.0f, 30.0f);
	};
	auto Add = [this, Now](float Delay, float Time, const FVector& From, const FVector& To, EMiraclePath Path, UNiagaraSystem* Trail, UNiagaraSystem* Impact, float TrailScale, float ImpactScale) -> FMiracleShot&
	{
		FMiracleShot& Shot = MiracleShots.AddDefaulted_GetRef();
		Shot.StartAt = Now + Delay;
		Shot.Duration = Time;
		Shot.From = From;
		Shot.To = To;
		Shot.Path = Path;
		Shot.Trail = Trail;
		Shot.Impact = Impact;
		Shot.TrailScale = TrailScale;
		Shot.ImpactScale = ImpactScale;
		return Shot;
	};
	// 적 하나에 내리는 마지막 한 방: 여기서 피해가 들어간다
	auto Final = [&](FMiracleShot& Shot, ABWCharacter* Enemy)
	{
		Shot.HitTarget = Enemy;
		Shot.Damage = Damage;
		Shot.bStun = Set == EBWAugmentSet::Lirin;
	};

	switch (Set)
	{
	case EBWAugmentSet::Astrin: // 별똥별이 하늘에서 쏟아진다
		{
			UNiagaraSystem* Star = Fx(TEXT("Trail_06_01")); UNiagaraSystem* Spark = Fx(TEXT("energy_05_01")); UNiagaraSystem* Big = Boom(TEXT("Expolison_05_01"));
			for (int32 i = 0; i < 14; ++i) { const FVector To = RandomTile(); Add(i * 0.06f, 0.35f, To + Up + FVector(-250, 120, 0), To, EMiraclePath::Straight, Star, Spark, 0.5f, 0.5f); }
			float Delay = 0.5f;
			for (ABWCharacter* E : Enemies) { Final(Add(Delay, 0.4f, Spot(E) + Up * 1.2f, Spot(E), EMiraclePath::Straight, Star, Big, 1.0f, 0.9f), E); Delay += 0.12f; }
		}
		break;
	case EBWAugmentSet::Sylpira: // 바람이 소용돌이치며 조여든다
		{
			UNiagaraSystem* Wind = Fx(TEXT("Trail_02_01")); UNiagaraSystem* Coil = Fx(TEXT("energy_03_01"));
			for (ABWCharacter* E : Enemies)
			{
				for (int32 i = 0; i < 6; ++i)
				{
					FMiracleShot& Shot = Add(i * 0.07f, 0.75f, Spot(E) + FRotator(0.0f, i * 60.0f, 0.0f).RotateVector(FVector(330.0f, 0.0f, 120.0f)), Spot(E), EMiraclePath::Spiral, Wind, Coil, 0.5f, 0.8f);
					if (i == 5) Final(Shot, E);
				}
			}
		}
		break;
	case EBWAugmentSet::Soltheris: // 태양이 천천히 내려와 불기둥이 선다
		{
			UNiagaraSystem* Sun = Fx(TEXT("Trail_03_01")); UNiagaraSystem* Ring = Fx(TEXT("energy_07_01")); UNiagaraSystem* Big = Boom(TEXT("Expolison_03_01"));
			const FVector Center = TargetGridManager->GetTileWorldLocation(FIntPoint(TargetGridManager->Width / 2, TargetGridManager->Height / 2));
			Add(0.0f, 0.8f, Center + Up * 1.4f, Center + FVector(0, 0, 320), EMiraclePath::Straight, Sun, Ring, 2.2f, 2.0f);
			for (ABWCharacter* E : Enemies)
			{
				for (int32 h = 0; h < 4; ++h) Add(0.8f + h * 0.05f, 0.16f, Center + FVector(0, 0, 320), Spot(E) + FVector(0, 0, h * 90.0f), EMiraclePath::Straight, Sun, Ring, 0.8f, 0.8f);
				Final(Add(1.05f, 0.1f, Spot(E) + FVector(0, 0, 200), Spot(E), EMiraclePath::Straight, Sun, Big, 0.8f, 0.9f), E);
			}
		}
		break;
	case EBWAugmentSet::Urska: // 포효가 불의 고리로 세 번 퍼져 나간다
		{
			UNiagaraSystem* Flame = Fx(TEXT("Trail_08_01")); UNiagaraSystem* Big = Boom(TEXT("Expolison_08_01"));
			for (int32 Wave = 0; Wave < 3; ++Wave)
			{
				for (int32 i = 0; i < 18; ++i)
				{
					const FVector Dir = FRotator(0.0f, i * 20.0f + Wave * 10.0f, 0.0f).RotateVector(FVector(1.0f, 0.0f, 0.0f));
					Add(Wave * 0.22f, 0.6f, Me + Dir * 60.0f + FVector(0, 0, 40), Me + Dir * 1100.0f + FVector(0, 0, 40), EMiraclePath::Straight, Flame, nullptr, 0.7f, 0.0f);
				}
			}
			for (ABWCharacter* E : Enemies) Final(Add(0.3f, 0.3f, Me + FVector(0, 0, 40), Spot(E), EMiraclePath::Straight, Flame, Big, 0.9f, 0.9f), E);
		}
		break;
	case EBWAugmentSet::Lirin: // 안개가 줄마다 번져 판을 덮는다
		{
			UNiagaraSystem* Mist = Fx(TEXT("energy_02_01")); UNiagaraSystem* Mark = Fx(TEXT("energy_05_01"));
			for (int32 X = TargetGridManager->Width - 1; X >= 0; --X)
			{
				for (int32 Y = 0; Y < TargetGridManager->Height; Y += 2)
				{
					const FVector Tile = TargetGridManager->GetTileWorldLocation(FIntPoint(X, Y)) + FVector(0, 0, 40);
					Add((TargetGridManager->Width - 1 - X) * 0.08f, 0.01f, Tile, Tile, EMiraclePath::Straight, nullptr, Mist, 0.0f, 0.9f);
				}
			}
			for (ABWCharacter* E : Enemies) Final(Add(0.75f, 0.01f, Spot(E), Spot(E), EMiraclePath::Straight, nullptr, Mark, 0.0f, 1.3f), E);
		}
		break;
	case EBWAugmentSet::Lunaris: // 초승달 한 줄기가 적을 차례로 꿰뚫는다
		{
			UNiagaraSystem* Moon = Fx(TEXT("Trail_09_02")); UNiagaraSystem* Crescent = Fx(TEXT("energy_06_01"));
			FVector From = Me + FVector(0, 0, 60);
			float Delay = 0.0f;
			for (ABWCharacter* E : Enemies) { Final(Add(Delay, 0.2f, From, Spot(E), EMiraclePath::Straight, Moon, Crescent, 0.9f, 1.5f), E); From = Spot(E); Delay += 0.22f; }
			Add(Delay, 0.25f, From, Me + FVector(0, 0, 60), EMiraclePath::Straight, Moon, Crescent, 0.9f, 1.0f);
		}
		break;
	case EBWAugmentSet::Berend: // 땅이 내 발밑에서 적까지 차례로 갈라진다
		{
			UNiagaraSystem* Crack = Boom(TEXT("Expolison_03_01")); UNiagaraSystem* Big = Boom(TEXT("Expolison_08_01"));
			for (ABWCharacter* E : Enemies)
			{
				for (int32 i = 1; i <= 7; ++i)
				{
					const FVector Step = FMath::Lerp(Me, E->GetActorLocation(), i / 8.0f);
					Add(i * 0.07f, 0.01f, Step, Step, EMiraclePath::Straight, nullptr, Crack, 0.0f, 0.22f + i * 0.02f);
				}
				Final(Add(0.62f, 0.01f, E->GetActorLocation(), E->GetActorLocation(), EMiraclePath::Straight, nullptr, Big, 0.0f, 1.0f), E);
			}
		}
		break;
	case EBWAugmentSet::Taiga: // 사방에서 그림자가 번갈아 베고 지나간다
		{
			UNiagaraSystem* Blade = Fx(TEXT("Trail_10_02")); UNiagaraSystem* Star = Fx(TEXT("energy_09_01"));
			for (ABWCharacter* E : Enemies)
			{
				for (int32 i = 0; i < 8; ++i)
				{
					const FVector Dir = FRotator(0.0f, i * 135.0f, 0.0f).RotateVector(FVector(300.0f, 0.0f, 0.0f));
					FMiracleShot& Shot = Add(i * 0.08f, 0.13f, Spot(E) + Dir, Spot(E) - Dir, EMiraclePath::Straight, Blade, Star, 0.7f, 0.6f);
					if (i == 7) Final(Shot, E);
				}
			}
		}
		break;
	case EBWAugmentSet::Geleon: // 뒤쪽 포대에서 세 번 일제히 쏜다
		{
			UNiagaraSystem* Bolt = Fx(TEXT("Trail_07_01")); UNiagaraSystem* Blast = Boom(TEXT("Expolison_02_01"));
			for (int32 Volley = 0; Volley < 3; ++Volley)
			{
				for (int32 Y = 0; Y < TargetGridManager->Height; ++Y)
				{
					const FVector Muzzle = TargetGridManager->GetTileWorldLocation(FIntPoint(0, Y)) + FVector(-200, 0, 160);
					const FVector Far = TargetGridManager->GetTileWorldLocation(FIntPoint(TargetGridManager->Width - 1, Y)) + FVector(200, 0, 30);
					Add(Volley * 0.28f, 0.34f, Muzzle, Far, EMiraclePath::Straight, Bolt, nullptr, 0.6f, 0.0f);
				}
			}
			for (ABWCharacter* E : Enemies) Final(Add(0.65f, 0.3f, E->GetActorLocation() + FVector(-700, 0, 160), Spot(E), EMiraclePath::Straight, Bolt, Blast, 0.9f, 0.7f), E);
		}
		break;
	case EBWAugmentSet::Patrovas: // 번개처럼 판 곳곳에 마력이 내리친다
		{
			UNiagaraSystem* Bolt = Fx(TEXT("Trail_05_01")); UNiagaraSystem* Spark = Fx(TEXT("energy_01_01"));
			for (int32 i = 0; i < 22; ++i) { const FVector To = RandomTile(); Add(FMath::FRandRange(0.0f, 0.75f), 0.09f, To + Up, To, EMiraclePath::Straight, Bolt, Spark, 0.45f, 0.8f); }
			float Delay = 0.8f;
			for (ABWCharacter* E : Enemies) { Final(Add(Delay, 0.1f, Spot(E) + Up, Spot(E), EMiraclePath::Straight, Bolt, Spark, 1.0f, 1.8f), E); Delay += 0.1f; }
		}
		break;
	case EBWAugmentSet::Megenberta: // 금화가 포물선을 그리며 쏟아진다
		{
			UNiagaraSystem* Coin = Fx(TEXT("Trail_07_01")); UNiagaraSystem* Glint = Fx(TEXT("energy_06_01")); UNiagaraSystem* Big = Boom(TEXT("Expolison_08_01"));
			for (int32 i = 0; i < 26; ++i) Add(i * 0.03f, 0.5f, Me + FVector(0, 0, 80), RandomTile(), EMiraclePath::Arc, Coin, Glint, 0.3f, 0.45f);
			for (ABWCharacter* E : Enemies) Final(Add(0.65f, 0.45f, Me + FVector(0, 0, 80), Spot(E), EMiraclePath::Arc, Coin, Big, 0.7f, 0.7f), E);
		}
		break;
	case EBWAugmentSet::Radiant: // 내 몸에서 피가 솟아 적에게 쏟아진다
		{
			UNiagaraSystem* Blood = Fx(TEXT("Trail_08_01")); UNiagaraSystem* Splash = Boom(TEXT("Expolison_03_01"));
			for (int32 i = 0; i < 12; ++i) Add(i * 0.03f, 0.3f, Me + FVector(0, 0, 60), Me + FRotator(0.0f, i * 30.0f, 0.0f).RotateVector(FVector(200.0f, 0.0f, 520.0f)), EMiraclePath::Straight, Blood, nullptr, 0.4f, 0.0f);
			float Delay = 0.45f;
			for (ABWCharacter* E : Enemies)
			{
				for (int32 i = 0; i < 4; ++i) { FMiracleShot& Shot = Add(Delay + i * 0.07f, 0.4f, Me + FVector(0, 0, 560), Spot(E) + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 0.0f), EMiraclePath::Arc, Blood, Splash, 0.6f, 0.4f); if (i == 3) Final(Shot, E); }
				Delay += 0.08f;
			}
		}
		break;
	default: break;
	}
}

AActor* ABWPlayerController::SpawnMiracleProp(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (!Mesh || !GetWorld()) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Prop = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Location, Scale), Params);
	if (!Prop) return nullptr;
	UStaticMeshComponent* Comp = Prop->GetStaticMeshComponent();
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(false);
	if (Material) for (int32 i = 0; i < Comp->GetNumMaterials(); ++i) Comp->SetMaterial(i, Material);
	return Prop;
}

void ABWPlayerController::UpdateMiracleProps()
{
	const double Now = FPlatformTime::Seconds();
	for (int32 i = MiracleEvents.Num() - 1; i >= 0; --i)
	{
		if (Now < MiracleEvents[i].Key) continue;
		TFunction<void()> Event = MoveTemp(MiracleEvents[i].Value);
		MiracleEvents.RemoveAt(i);
		if (Event) Event();
	}
	for (int32 i = MiracleProps.Num() - 1; i >= 0; --i)
	{
		FMiracleProp& Move = MiracleProps[i];
		AActor* Actor = Move.Actor.Get();
		if (!Actor) { MiracleProps.RemoveAt(i); continue; }
		if (Now < Move.StartAt) continue;
		float Alpha = FMath::Clamp((float)((Now - Move.StartAt) / FMath::Max(0.01f, Move.Duration)), 0.0f, 1.0f);
		const float Eased = Move.Ease == 1 ? 1.0f - FMath::Pow(1.0f - Alpha, 3.0f) : Move.Ease == 2 ? Alpha * Alpha * Alpha : Alpha;
		if (Move.Ease != 3)
		{
			Actor->SetActorLocation(FMath::Lerp(Move.FromLoc, Move.ToLoc, Eased) + FVector(0.0f, 0.0f, FMath::Sin(Alpha * PI) * Move.ArcHeight));
			Actor->SetActorScale3D(FMath::Lerp(Move.FromScale, Move.ToScale, Eased));
		}
		if (Move.SpinYaw != 0.0f) Actor->SetActorRotation(Move.StartRot + FRotator(0.0f, Move.SpinYaw * Eased, 0.0f));
		if (Alpha < 1.0f) continue;
		if (Move.bDestroyAtEnd) Actor->Destroy();
		MiracleProps.RemoveAt(i);
	}

	// 화면이 세트 색으로 물들었다가 돌아온다
	if (MiracleVolume)
	{
		float Weight = 0.0f;
		if (MoodStart > 0.0 && Now < MoodEnd)
		{
			const float In = FMath::Clamp((float)((Now - MoodStart) / 0.35), 0.0f, 1.0f);
			const float Out = FMath::Clamp((float)((MoodEnd - Now) / 0.6), 0.0f, 1.0f);
			Weight = FMath::Min(In, Out);
		}
		MiracleVolume->BlendWeight = Weight;
		MiraclePull = Weight;
		if (BattleCamera && !ShakeTicker.IsValid()) BattleCamera->SetActorLocation(GetCameraBaseLocation());
	}
	// 일격 순간 카메라가 살짝 당겨졌다 돌아온다
	if (BattleCamera)
	{
		if (UCameraComponent* Camera = BattleCamera->FindComponentByClass<UCameraComponent>())
		{
			if (BaseFOV < 0.0f) BaseFOV = Camera->FieldOfView;
			float Punch = 0.0f;
			if (ZoomAt > 0.0)
			{
				const float T = (float)(Now - ZoomAt);
				Punch = T < 0.0f ? FMath::Clamp(1.0f + T / 0.5f, 0.0f, 1.0f) : FMath::Clamp(1.0f - T / 0.7f, 0.0f, 1.0f);
				if (T > 0.7f) ZoomAt = -1.0;
			}
			Camera->SetFieldOfView(BaseFOV * (1.0f - 0.1f * Punch + 0.12f * MiraclePull));
		}
	}
}

float ABWPlayerController::BuildMiracleScene(EBWAugmentSet Set, int32 Damage, const FLinearColor& Color, float TimeScale)
{
	if (Set == EBWAugmentSet::None || Set > EBWAugmentSet::Radiant) return 0.0f;
	if (!PossessedBWCharacter || !TargetGridManager) return 0.0f;
	const float T = TimeScale;
	const double Now = FPlatformTime::Seconds();
	const FVector Me = PossessedBWCharacter->GetActorLocation();
	const TArray<ABWCharacter*> Enemies = GetEnemiesInReach(99);
	auto Fx = [](const TCHAR* Folder, const TCHAR* Name) -> UNiagaraSystem*
	{
		return Cast<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/KTP_Effect/Particles/Fly/%s%s.%s"), Folder, Name, Name)).TryLoad());
	};
	auto Move = [this, Now, T](AActor* Actor, float Delay, float Time, const FVector& FromLoc, const FVector& ToLoc, const FVector& FromScale, const FVector& ToScale, uint8 Ease, bool bDestroy)
	{
		if (!Actor) return;
		FMiracleProp& Prop = MiracleProps.AddDefaulted_GetRef();
		Prop.Actor = Actor;
		Prop.StartAt = Now + Delay * T;
		Prop.Duration = Time * T;
		Prop.FromLoc = FromLoc; Prop.ToLoc = ToLoc; Prop.FromScale = FromScale; Prop.ToScale = ToScale;
		Prop.Ease = Ease; Prop.bDestroyAtEnd = bDestroy;
	};
	auto At = [this, Now, T](float Delay, TFunction<void()> Event) { MiracleEvents.Emplace(Now + Delay * T, MoveTemp(Event)); };
	// 마지막 한 방: 그 순간 피해, 히트스톱, 흔들림
	auto Strike = [this, Damage](ABWCharacter* Enemy, UNiagaraSystem* Impact, float Scale)
	{
		if (!Enemy) return;
		const FVector Spot = Enemy->GetActorLocation();
		if (Impact) PlayShortNiagara(GetWorld(), Impact, Spot + FVector(0, 0, 40), Scale, 0.6f);
		if (Enemy->CurrentHP > 0 && Damage > 0) Enemy->TakeFixedDamage(Damage, TEXT("기적"));
	};
	TArray<TWeakObjectPtr<ABWCharacter>> Targets;
	for (ABWCharacter* E : Enemies) Targets.Add(E);

	// 기적 주인공 모델: 정면이 카메라를 보게 세운다 (트리포 모델의 정면은 +Y)
	FVector ToCamera = BattleCamera ? -BattleCamera->GetActorForwardVector().GetSafeNormal2D() : FVector(-1.0f, 0.0f, 0.0f);
	if (ToCamera.IsNearlyZero()) ToCamera = FVector(-1.0f, 0.0f, 0.0f);
	const FRotator FaceCamera(0.0f, ToCamera.Rotation().Yaw - 90.0f, 0.0f);
	const FVector BoardCenter = FMath::Lerp(TargetGridManager->GetTileWorldLocation(FIntPoint(TargetGridManager->Width / 2, TargetGridManager->Height / 2)), Me, 0.3f);
	FVector EnemyMid = BoardCenter;
	if (Enemies.Num() > 0) { EnemyMid = FVector::ZeroVector; for (ABWCharacter* E : Enemies) EnemyMid += E->GetActorLocation(); EnemyMid /= (float)Enemies.Num(); }
	const FVector BehindEnemies = EnemyMid - ToCamera * 260.0f;
	float HeroSize = 1.0f;
	auto Hero = [this, &HeroSize](const TCHAR* Name, float Height, const FVector& Where, const FRotator& Facing) -> AActor*
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/TripoModels/%s/%s.%s"), Name, Name, Name)).TryLoad());
		if (!Mesh) return nullptr;
		HeroSize = Height / FMath::Max(1.0f, (float)Mesh->GetBoundingBox().GetSize().Z);
		return SpawnMiracleProp(Mesh, nullptr, Where, Facing, FVector(HeroSize * 0.01f));
	};
	auto Spin = [this, Now, T](AActor* Actor, float Delay, float Time, float Degrees)
	{
		if (!Actor) return;
		FMiracleProp& Prop = MiracleProps.AddDefaulted_GetRef();
		Prop.Actor = Actor; Prop.StartAt = Now + Delay * T; Prop.Duration = Time * T;
		Prop.FromLoc = Prop.ToLoc = FVector::ZeroVector; Prop.FromScale = Prop.ToScale = FVector::ZeroVector;
		Prop.SpinYaw = Degrees; Prop.StartRot = Actor->GetActorRotation();
		Prop.Ease = 3; // 회전만
	};
	auto Bolt = [this, Now, T](float Delay, float Time, const FVector& From, const FVector& To, UNiagaraSystem* Trail, float Scale, EMiraclePath Path)
	{
		FMiracleShot& Shot = MiracleShots.AddDefaulted_GetRef();
		Shot.StartAt = Now + Delay * T; Shot.Duration = Time * T; Shot.From = From; Shot.To = To; Shot.Trail = Trail; Shot.TrailScale = Scale; Shot.Path = Path;
	};

	// 지면에 눕는 효과들: 터짐(나이아가라·캐스케이드)과 바닥에 퍼지는 빛 고리
	auto LoadFx = [](const TCHAR* Path) -> UObject* { return FSoftObjectPath(Path).TryLoad(); };
	UObject* GroundBlast = LoadFx(TEXT("/Game/ParagonProps/FX/Particles/Core/P_Core_Destroyed_MainBlast.P_Core_Destroyed_MainBlast"));
	UObject* FlashDome = LoadFx(TEXT("/Game/ParagonProps/FX/Particles/Core/P_CoreDestroyed_InitialBurst.P_CoreDestroyed_InitialBurst"));
	UObject* FireBurst = LoadFx(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion.NS_Free_Spells_Explosion"));
	UObject* GoldSlash = LoadFx(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Slash2.NS_Free_Magic_Slash2"));
	UObject* SpikeBurst = LoadFx(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Attack1.NS_Free_Magic_Attack1"));
	UObject* MagicCircle = LoadFx(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Circle1.NS_Free_Magic_Circle1"));
	auto Burst = [this](UObject* System, const FVector& Loc, float Scale, float Life, float Speed)
	{
		UWorld* World = GetWorld();
		if (!World || !System) return;
		UFXSystemComponent* Comp = nullptr;
		if (UNiagaraSystem* NS = Cast<UNiagaraSystem>(System))
		{
			UNiagaraComponent* N = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Loc, FRotator::ZeroRotator, FVector(Scale));
			if (N) N->SetCustomTimeDilation(Speed);
			Comp = N;
		}
		else if (UParticleSystem* PS = Cast<UParticleSystem>(System))
		{
			UParticleSystemComponent* P = UGameplayStatics::SpawnEmitterAtLocation(World, PS, Loc, FRotator::ZeroRotator, FVector(Scale), true);
			if (P) P->CustomTimeDilation = Speed;
			Comp = P;
		}
		if (!Comp) return;
		TWeakObjectPtr<UFXSystemComponent> Weak(Comp);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [Weak]() { if (Weak.IsValid()) Weak->DestroyComponent(); }, Life, false);
	};
	UMaterialInterface* RingBase = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/System/Material/M_BWRing.M_BWRing")).TryLoad());
	UStaticMesh* Plane = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")).TryLoad());
	// 바닥에 눕혀 퍼지는 빛 고리 (지름 cm)
	auto Ring = [this, RingBase, Plane, &Move](float Delay, const FVector& Center, float FromSize, float ToSize, float Time, const FLinearColor& RingColor, float Fill)
	{
		if (!RingBase || !Plane) return;
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(RingBase, this);
		Mat->SetVectorParameterValue(TEXT("Color"), RingColor);
		Mat->SetScalarParameterValue(TEXT("Fill"), Fill);
		const FVector Spot(Center.X, Center.Y, TargetGridManager ? TargetGridManager->GetActorLocation().Z + 6.0f : Center.Z);
		AActor* Disc = SpawnMiracleProp(Plane, Mat, Spot, FRotator::ZeroRotator, FVector(0.001f));
		Move(Disc, Delay, Time, Spot, Spot, FVector(FromSize / 100.0f, FromSize / 100.0f, 1.0f), FVector(ToSize / 100.0f, ToSize / 100.0f, 1.0f), 1, true);
	};
	auto Feet = [this](const FVector& Loc) { return FVector(Loc.X, Loc.Y, TargetGridManager ? TargetGridManager->GetActorLocation().Z + 5.0f : Loc.Z - 40.0f); };
	auto Hurt = [this](ABWCharacter* Enemy, int32 Amount)
	{
		if (!Enemy || Enemy->CurrentHP <= 0 || Amount <= 0) return;
		Enemy->TakeFixedDamage(Amount, TEXT("기적"));
		StartCameraShake(9.0f, 0.12f);
	};
	// 연타: 피해를 Count번으로 나눈 n번째 몫
	auto Share = [Damage](int32 Count, int32 Index) { const int32 Base = Damage / FMath::Max(1, Count); return Index == Count - 1 ? Damage - Base * (Count - 1) : Base; };
	UMaterialInterface* SpiritBase = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/System/Material/M_BWSpirit.M_BWSpirit")).TryLoad());
	auto Spirit = [this, SpiritBase](AActor* Actor, const FLinearColor& SpiritColor, float Opacity)
	{
		if (!Actor || !SpiritBase) return;
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(SpiritBase, this);
		Mat->SetVectorParameterValue(TEXT("Color"), SpiritColor);
		Mat->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		if (UStaticMeshComponent* Comp = Actor->FindComponentByClass<UStaticMeshComponent>()) for (int32 i = 0; i < Comp->GetNumMaterials(); ++i) Comp->SetMaterial(i, Mat);
	};
	auto Ball = [this, &Move](const FVector& From, const FVector& To, float Delay, float Time, float Size, float Arc, UMaterialInterface* Mat)
	{
		UStaticMesh* Sphere = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")).TryLoad());
		AActor* Shot = SpawnMiracleProp(Sphere, Mat, From, FRotator::ZeroRotator, FVector(0.001f));
		Move(Shot, Delay, Time, From, To, FVector(Size / 100.0f), FVector(Size / 100.0f), 0, true);
		if (MiracleProps.Num() > 0 && Shot) MiracleProps.Last().ArcHeight = Arc;
	};

	// 예고: 화면이 세트 색으로 물든다
	if (!MiracleVolume)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MiracleVolume = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
		if (MiracleVolume) { MiracleVolume->bUnbound = true; MiracleVolume->Priority = 20.0f; }
	}
	if (MiracleVolume)
	{
		FPostProcessSettings& PP = MiracleVolume->Settings;
		PP.bOverride_SceneColorTint = true; PP.SceneColorTint = FLinearColor::LerpUsingHSV(FLinearColor::White, Color, 0.45f);
		PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 1.1f;
		PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 3.0f;
		PP.bOverride_ColorSaturation = true; PP.ColorSaturation = FVector4(0.75f, 0.75f, 0.75f, 1.0f);
	}
	float Impact = 1.5f, Length = 3.2f;
	MiraclePullDistance = 350.0f;

	switch (Set)
	{
	case EBWAugmentSet::Soltheris: // 태양이 천천히 내려앉고, 빛줄기가 내리꽂혀 발밑에서 터진다
		{
			// 적은 보통 화면 위쪽이라, 판 가운데보다 내 쪽으로 당겨 화면 가운데에 오게 한다
			const FVector Center = FMath::Lerp(TargetGridManager->GetTileWorldLocation(FIntPoint(TargetGridManager->Width / 2, TargetGridManager->Height / 2)), Me, 0.3f) - FVector(0, 0, 200);
			UMaterialInstanceDynamic* Glow = nullptr;
			if (UMaterialInterface* Base = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/System/Material/M_BWProp.M_BWProp")).TryLoad()))
			{
				Glow = UMaterialInstanceDynamic::Create(Base, this);
				Glow->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.8f, 0.35f));
				Glow->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(40.0f, 24.0f, 8.0f));
			}
			UStaticMesh* Orb = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")).TryLoad());
			AActor* Sun = SpawnMiracleProp(Orb, Glow, Center + FVector(0, 0, 1500), FRotator::ZeroRotator, FVector(0.1f));
			Move(Sun, 0.1f, 1.3f, Center + FVector(0, 0, 1500), Center + FVector(0, 0, 450), FVector(0.1f), FVector(3.4f), 1, false);
			Move(Sun, 1.45f, 0.12f, Center + FVector(0, 0, 450), Center + FVector(0, 0, 450), FVector(3.4f), FVector(4.2f), 0, false);
			Move(Sun, 1.6f, 0.35f, Center + FVector(0, 0, 450), Center + FVector(0, 0, 450), FVector(4.2f), FVector(0.0f), 2, true);
			UNiagaraSystem* Halo = Fx(TEXT("Others/"), TEXT("energy_07_01"));
			At(0.5f, [this, Halo, Center]() { if (Halo) PlayShortNiagara(GetWorld(), Halo, Center + FVector(0, 0, 450), 3.0f, 0.9f); });
			// 태양 아래 땅이 먼저 달아오른다
			Ring(0.4f, Center, 200.0f, 900.0f, 1.1f, FLinearColor(1.0f, 0.7f, 0.25f), 0.25f);
			UNiagaraSystem* Beam = Fx(TEXT("Others/"), TEXT("Trail_03_01"));
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				const FVector Spot = Feet(Weak->GetActorLocation());
				Bolt(1.5f, 0.12f, Center + FVector(0, 0, 450), Spot, Beam, 1.4f, EMiraclePath::Straight);
				At(1.62f, [Burst, Hurt, Weak, Spot, GroundBlast, Damage]() { Burst(GroundBlast, Spot, 0.4f, 2.2f, 1.0f); Hurt(Weak.Get(), Damage); });
				Ring(1.62f, Spot, 60.0f, 520.0f, 0.6f, FLinearColor(1.0f, 0.75f, 0.3f), 0.0f);
			}
			Impact = 1.62f;
		}
		break;
	case EBWAugmentSet::Urska: // 하늘빛 곰 정령이 적들 너머에서 솟아 포효하고, 충격파가 둥글게 퍼진다
		{
			UStaticMesh* Bear = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Game/TripoModels/Miracle_UrskaBear_A/Miracle_UrskaBear_A.Miracle_UrskaBear_A")).TryLoad());
			if (!Bear) Bear = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Game/TripoModels/Enemy_Ironheart_A/Enemy_Ironheart_A.Enemy_Ironheart_A")).TryLoad());
			// 정면이 인상적인 모델이라, 적들 너머(화면 위쪽)에서 카메라를 보고 솟는다
			const FVector Base = EnemyMid - ToCamera * 260.0f;
			const float Height = Bear ? FMath::Max(1.0f, (float)Bear->GetBoundingBox().GetSize().Z) : 100.0f;
			const float Size = 480.0f / Height;
			MiraclePullDistance = 650.0f;
			AActor* Ghost = SpawnMiracleProp(Bear, nullptr, Base + FVector(0, 0, -700), FaceCamera, FVector(Size));
			Spirit(Ghost, FLinearColor(0.35f, 0.75f, 1.0f), 0.3f);
			Move(Ghost, 0.15f, 0.9f, Base + FVector(0, 0, -700), Base + FVector(0, 0, -60), FVector(Size * 0.7f), FVector(Size), 1, false);
			// 포효: 몸이 한 번 부풀었다가
			Move(Ghost, 1.15f, 0.12f, Base + FVector(0, 0, -60), Base + ToCamera * 60.0f + FVector(0, 0, -60), FVector(Size), FVector(Size * 1.12f), 1, false);
			Move(Ghost, 2.2f, 0.8f, Base + ToCamera * 60.0f + FVector(0, 0, -60), Base + FVector(0, 0, -800), FVector(Size * 1.12f), FVector(Size * 0.8f), 2, true);
			const FLinearColor Sky(0.45f, 0.85f, 1.0f);
			At(1.2f, [Burst, FlashDome, Base]() { Burst(FlashDome, Base, 0.9f, 1.4f, 1.0f); });
			// 둥근 충격파 세 겹이 곰에서 판 전체로 퍼진다
			for (int32 i = 0; i < 3; ++i) Ring(1.2f + i * 0.16f, Base + ToCamera * 60.0f, 150.0f, 2600.0f + i * 300.0f, 0.75f, Sky, i == 0 ? 0.25f : 0.0f);
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				const float Reach = FVector::Dist2D(Base, Weak->GetActorLocation());
				const float When = 1.2f + FMath::Clamp(Reach / 2600.0f, 0.0f, 1.0f) * 0.4f;
				At(When, [Hurt, Weak, Damage]() { Hurt(Weak.Get(), Damage); });
			}
			Impact = 1.2f;
			Length = 3.2f;
		}
		break;
	case EBWAugmentSet::Berend: // 내 발밑에서 적까지 바위가 차례로 솟구친다
		{
			UStaticMesh* Rock = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Game/BattleMap/Berend/SM_Berend_BreakingRock_A.SM_Berend_BreakingRock_A")).TryLoad());
			const float RockHeight = Rock ? FMath::Max(1.0f, (float)Rock->GetBoundingBox().GetSize().Z) : 100.0f;
			UNiagaraSystem* Dust = Fx(TEXT(""), TEXT("Expolison_03_01"));
			UNiagaraSystem* Blast = Fx(TEXT(""), TEXT("Expolison_08_01"));
			TArray<AActor*> Spikes;
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				const FVector Goal = Weak->GetActorLocation();
				for (int32 i = 1; i <= 6; ++i)
				{
					const FVector Spot = FMath::Lerp(Me, Goal, i / 7.0f) + FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), 0.0f);
					const float Size = (90.0f + i * 18.0f) / RockHeight;
					AActor* Spike = SpawnMiracleProp(Rock, nullptr, Spot + FVector(0, 0, -260), FRotator(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(0.f, 360.f), 0.0f), FVector(Size));
					Move(Spike, 0.25f + i * 0.09f, 0.12f, Spot + FVector(0, 0, -260), Spot + FVector(0, 0, -60), FVector(Size), FVector(Size), 1, false);
					At(0.25f + i * 0.09f, [this, Dust, Spot]() { if (Dust) PlayShortNiagara(GetWorld(), Dust, Spot, 0.35f, 0.3f); });
					Spikes.Add(Spike);
				}
				const float Big = 260.0f / RockHeight;
				AActor* Crown = SpawnMiracleProp(Rock, nullptr, Goal + FVector(0, 0, -420), FRotator(0.0f, FMath::FRandRange(0.f, 360.f), 0.0f), FVector(Big));
				Move(Crown, 0.95f, 0.14f, Goal + FVector(0, 0, -420), Goal + FVector(0, 0, -120), FVector(Big), FVector(Big), 1, false);
				Spikes.Add(Crown);
				At(1.0f, [Strike, Weak, Blast]() { Strike(Weak.Get(), Blast, 1.3f); });
			}
			for (AActor* Spike : Spikes)
			{
				if (!Spike) continue;
				const FVector Loc = Spike->GetActorLocation() + FVector(0, 0, 200);
				Move(Spike, 2.1f, 0.7f, Loc, Loc + FVector(0, 0, -420), Spike->GetActorScale3D(), Spike->GetActorScale3D(), 2, true);
			}
			Impact = 1.0f;
			Length = 3.0f;
		}
		break;
	case EBWAugmentSet::Astrin: // 별 운석이 하늘에서 화면 가운데로 떨어져 땅에서 터진다
		{
			const FVector Land = BoardCenter;
			AActor* Star = Hero(TEXT("Miracle_Astrin_A"), 260.0f, Land + FVector(-500.0f, 300.0f, 1600.0f), FaceCamera);
			Move(Star, 0.1f, 0.6f, Land + FVector(-500.0f, 300.0f, 1600.0f), Land + FVector(-150.0f, 90.0f, 700.0f), FVector(HeroSize * 0.4f), FVector(HeroSize), 1, false);
			Spin(Star, 0.1f, 1.3f, 200.0f);
			Move(Star, 0.9f, 0.32f, Land + FVector(-150.0f, 90.0f, 700.0f), Land, FVector(HeroSize), FVector(HeroSize * 1.1f), 2, false);
			Move(Star, 1.25f, 0.25f, Land, Land + FVector(0, 0, -120), FVector(HeroSize * 1.1f), FVector(0.0f), 1, true);
			// 떨어질 자리에 그림자처럼 고리가 먼저 좁혀 든다
			Ring(0.3f, Land, 900.0f, 160.0f, 0.9f, FLinearColor(0.6f, 0.7f, 1.0f), 0.15f);
			At(1.22f, [Burst, GroundBlast, Land, Feet]() { Burst(GroundBlast, Feet(Land), 0.7f, 2.5f, 1.0f); });
			Ring(1.22f, Land, 100.0f, 2400.0f, 0.7f, FLinearColor(0.7f, 0.8f, 1.0f), 0.2f);
			UNiagaraSystem* Shard = Fx(TEXT("Others/"), TEXT("Trail_06_01"));
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				const FVector Spot = Feet(Weak->GetActorLocation());
				Bolt(1.25f, 0.25f, Land + FVector(0, 0, 60), Spot, Shard, 1.0f, EMiraclePath::Arc);
				At(1.5f, [Burst, Hurt, FireBurst, Weak, Spot, Damage]() { Burst(FireBurst, Spot, 1.3f, 1.4f, 1.2f); Hurt(Weak.Get(), Damage); });
				Ring(1.5f, Spot, 40.0f, 360.0f, 0.45f, FLinearColor(0.7f, 0.8f, 1.0f), 0.0f);
			}
			Impact = 1.22f;
		}
		break;
	case EBWAugmentSet::Lunaris: // 적들 뒤에 초승달이 떠올라, 판을 가로지르며 금빛으로 벤다
		{
			const FVector Rise = BehindEnemies + FVector(0, 0, 120);
			const FVector Side = FVector::CrossProduct(FVector::UpVector, ToCamera) * 700.0f;
			AActor* Moon = Hero(TEXT("Miracle_Lunaris_A"), 380.0f, Rise + FVector(0, 0, -500), FaceCamera);
			Move(Moon, 0.1f, 0.8f, Rise + FVector(0, 0, -500), Rise, FVector(HeroSize * 0.6f), FVector(HeroSize), 1, false);
			Move(Moon, 1.1f, 0.3f, Rise, EnemyMid + Side + FVector(0, 0, 80), FVector(HeroSize), FVector(HeroSize * 0.9f), 2, false);
			Spin(Moon, 1.1f, 0.3f, -120.0f);
			Move(Moon, 1.6f, 0.6f, EnemyMid + Side + FVector(0, 0, 80), EnemyMid + Side * 1.6f + FVector(0, 0, 600), FVector(HeroSize * 0.9f), FVector(0.0f), 1, true);
			int32 Order = 0;
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				const FVector Spot = Weak->GetActorLocation() + FVector(0, 0, 40);
				const float When = 1.2f + 0.05f * Order++;
				At(When, [Burst, Hurt, GoldSlash, Weak, Spot, Damage]() { Burst(GoldSlash, Spot, 2.6f, 1.2f, 1.4f); Hurt(Weak.Get(), Damage); });
				Ring(When, Weak->GetActorLocation(), 60.0f, 420.0f, 0.5f, FLinearColor(0.75f, 0.8f, 1.0f), 0.0f);
			}
			Impact = 1.2f;
		}
		break;
	case EBWAugmentSet::Taiga: // 표범이 내 곁에 버티고 서 있고, 그림자 칼날이 사방에서 적들을 가로질러 벤다
		{
			// 표범은 화면 아래(내 곁)에서 카메라를 보고 서 있기만 한다
			const FVector Start = Me + ToCamera * 60.0f + FVector::CrossProduct(FVector::UpVector, ToCamera) * 140.0f;
			AActor* Cat = Hero(TEXT("Miracle_Taiga_A"), 200.0f, Start + FVector(0, 0, -260), FaceCamera);
			const float CatSize = HeroSize;
			Move(Cat, 0.1f, 0.4f, Start + FVector(0, 0, -260), Start, FVector(CatSize * 0.8f), FVector(CatSize), 1, false);
			const FLinearColor Shade(0.15f, 0.95f, 0.5f);
			UNiagaraSystem* Streak = Fx(TEXT("Others/"), TEXT("Trail_03_01"));
			// 그림자 칼날: 판 가장자리 곳곳에서 적들 한가운데를 가로질러 반대편으로 사라진다
			static constexpr int32 Shadows = 7;
			for (int32 i = 0; i < Shadows; ++i)
			{
				const float Angle = 360.0f / Shadows * i + FMath::FRandRange(-15.0f, 15.0f);
				const FVector Dir = FRotator(0.0f, Angle, 0.0f).RotateVector(FVector(1.0f, 0.0f, 0.0f));
				const float When = 0.55f + i * 0.11f;
				Bolt(When, 0.2f, EnemyMid - Dir * 900.0f + FVector(0, 0, 50), EnemyMid + Dir * 900.0f + FVector(0, 0, 50), Streak, 1.4f, EMiraclePath::Straight);
				At(When + 0.1f, [Burst, GoldSlash, EnemyMid, Dir]() { Burst(GoldSlash, EnemyMid + FVector(0, 0, 40) + Dir * 30.0f, 2.2f, 0.8f, 1.6f); });
				Ring(When + 0.1f, EnemyMid + Dir * 40.0f, 60.0f, 520.0f, 0.4f, Shade, 0.0f);
			}
			// 칼날이 지날 때마다 적들이 조금씩 베인다
			static constexpr int32 Cuts = 4;
			for (int32 c = 0; c < Cuts; ++c)
			{
				for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
					At(0.66f + c * 0.2f, [Hurt, Weak, Share, c]() { Hurt(Weak.Get(), Share(Cuts + 1, c)); });
			}
			// 마지막: 표범이 한 번 으르렁(부풀었다 돌아옴)하면 한가운데가 터진다
			const float Slam = 0.55f + Shadows * 0.11f + 0.35f;
			Move(Cat, Slam - 0.15f, 0.12f, Start, Start + ToCamera * 20.0f, FVector(CatSize), FVector(CatSize * 1.15f), 1, false);
			Move(Cat, Slam, 0.25f, Start + ToCamera * 20.0f, Start, FVector(CatSize * 1.15f), FVector(CatSize), 0, false);
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector Dir = FRotator(0.0f, 45.0f + 90.0f * i, 0.0f).RotateVector(FVector(1.0f, 0.0f, 0.0f));
				Bolt(Slam - 0.12f, 0.12f, EnemyMid - Dir * 700.0f + FVector(0, 0, 50), EnemyMid + FVector(0, 0, 50), Streak, 1.8f, EMiraclePath::Straight);
			}
			At(Slam, [Burst, GroundBlast, EnemyMid, Feet]() { Burst(GroundBlast, Feet(EnemyMid), 0.6f, 2.4f, 1.0f); });
			Ring(Slam, EnemyMid, 120.0f, 2200.0f, 0.7f, Shade, 0.25f);
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
				At(Slam, [Hurt, Weak, Share]() { Hurt(Weak.Get(), Share(Cuts + 1, Cuts)); });
			Move(Cat, Slam + 1.0f, 0.4f, Start, Start + FVector(0, 0, -260), FVector(CatSize), FVector(CatSize * 0.7f), 2, true);
			Impact = Slam;
			Length = Slam + 1.4f;
		}
		break;
	case EBWAugmentSet::Geleon: // 판 옆에 포대가 솟아, 포구가 불을 뿜고 포탄이 날아가 땅에서 터진다
		{
			const FVector SideDir = FVector::CrossProduct(FVector::UpVector, ToCamera);
			const FVector Spot = EnemyMid - SideDir * 650.0f + ToCamera * 150.0f;
			const FRotator Aim(0.0f, SideDir.Rotation().Yaw - 90.0f, 0.0f);
			AActor* Cannon = Hero(TEXT("Miracle_Geleon_A"), 260.0f, Spot + FVector(0, 0, -400), Aim);
			Move(Cannon, 0.1f, 0.6f, Spot + FVector(0, 0, -400), Spot, FVector(HeroSize * 0.8f), FVector(HeroSize), 1, false);
			// 포구: 모델이 적 쪽을 보므로 앞쪽 위
			const FVector Muzzle = Spot + SideDir * 140.0f + FVector(0, 0, 170);
			UMaterialInstanceDynamic* Iron = nullptr;
			if (UMaterialInterface* Base = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/System/Material/M_BWProp.M_BWProp")).TryLoad()))
			{
				Iron = UMaterialInstanceDynamic::Create(Base, this);
				Iron->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.06f));
				Iron->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(0.6f, 0.25f, 0.05f));
			}
			static constexpr int32 Volleys = 3;
			for (int32 Volley = 0; Volley < Volleys; ++Volley)
			{
				const float When = 0.85f + Volley * 0.38f;
				At(When, [this, Cannon, Spot, SideDir, Burst, FireBurst, Muzzle]()
				{
					if (Cannon) Cannon->SetActorLocation(Spot - SideDir * 30.0f);
					Burst(FireBurst, Muzzle, 0.9f, 0.8f, 1.6f);
					StartCameraShake(10.0f, 0.15f);
				});
				At(When + 0.12f, [Cannon, Spot]() { if (Cannon) Cannon->SetActorLocation(Spot); });
				for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
				{
					if (!Weak.IsValid()) continue;
					const FVector Land = Feet(Weak->GetActorLocation()) + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 0.0f);
					Ball(Muzzle, Land, When, 0.4f, 34.0f, 260.0f, Iron);
					At(When + 0.4f, [Burst, Hurt, GroundBlast, Weak, Land, Share, Volley]() { Burst(GroundBlast, Land, 0.28f, 2.0f, 1.2f); Hurt(Weak.Get(), Share(Volleys, Volley)); });
					Ring(When + 0.4f, Land, 40.0f, 420.0f, 0.45f, FLinearColor(1.0f, 0.6f, 0.25f), 0.0f);
				}
			}
			Move(Cannon, 2.5f, 0.6f, Spot, Spot + FVector(0, 0, -400), FVector(HeroSize), FVector(HeroSize * 0.8f), 2, true);
			Impact = 0.85f + (Volleys - 1) * 0.38f + 0.4f;
			Length = 3.3f;
		}
		break;
	case EBWAugmentSet::Megenberta: // 거대한 금화가 돌며 떨어져 판 위에 금빛을 뿌린다
		{
			const FVector Land = BoardCenter + FVector(0, 0, 60);
			AActor* Coin = Hero(TEXT("Miracle_Megenberta_A"), 280.0f, Land + FVector(0, 0, 1400), FaceCamera);
			Move(Coin, 0.1f, 1.0f, Land + FVector(0, 0, 1400), Land, FVector(HeroSize * 0.5f), FVector(HeroSize), 2, false);
			Spin(Coin, 0.1f, 1.0f, 720.0f);
			Move(Coin, 1.15f, 0.3f, Land, Land, FVector(HeroSize), FVector(0.0f), 1, true);
			UNiagaraSystem* Gold = Fx(TEXT("Others/"), TEXT("Trail_07_01"));
			UNiagaraSystem* Glint = Fx(TEXT("Others/"), TEXT("energy_06_01"));
			UNiagaraSystem* Big = Fx(TEXT(""), TEXT("Expolison_08_01"));
			At(1.1f, [this, Big, Land]() { if (Big) PlayShortNiagara(GetWorld(), Big, Land, 1.8f, 0.6f); });
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				for (int32 i = 0; i < 4; ++i) Bolt(1.15f + i * 0.05f, 0.35f, Land, Weak->GetActorLocation() + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 30.0f), Gold, 0.5f, EMiraclePath::Arc);
				At(1.55f, [Strike, Weak, Glint]() { Strike(Weak.Get(), Glint, 1.6f); });
			}
			Impact = 1.1f;
		}
		break;
	case EBWAugmentSet::Radiant: // 피의 성배가 단숨에 솟아, 넘친 피가 여러 번 적에게 쏟아진다
		{
			const FVector Spot = BoardCenter;
			AActor* Grail = Hero(TEXT("Miracle_Radiant_A"), 300.0f, Spot + FVector(0, 0, -400), FaceCamera);
			Move(Grail, 0.05f, 0.3f, Spot + FVector(0, 0, -400), Spot, FVector(HeroSize * 0.7f), FVector(HeroSize), 1, false);
			Ring(0.3f, Spot, 120.0f, 700.0f, 0.5f, FLinearColor(1.0f, 0.15f, 0.2f), 0.3f);
			UNiagaraSystem* Blood = Fx(TEXT("Others/"), TEXT("Trail_08_01"));
			static constexpr int32 Waves = 5;
			for (int32 w = 0; w < Waves; ++w)
			{
				const float When = 0.5f + w * 0.32f;
				// 성배가 뿜을 때마다 한 번 들썩인다
				Move(Grail, When, 0.08f, Spot, Spot + FVector(0, 0, 40), FVector(HeroSize), FVector(HeroSize * 1.1f), 1, false);
				Move(Grail, When + 0.1f, 0.15f, Spot + FVector(0, 0, 40), Spot, FVector(HeroSize * 1.1f), FVector(HeroSize), 0, false);
				for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
				{
					if (!Weak.IsValid()) continue;
					const FVector Land = Feet(Weak->GetActorLocation());
					for (int32 i = 0; i < 3; ++i) Bolt(When + i * 0.04f, 0.5f, Spot + FVector(0, 0, 330), Land + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 30.0f), Blood, 0.9f, EMiraclePath::Arc);
					At(When + 0.5f, [Burst, Hurt, FireBurst, Weak, Land, Share, w]() { Burst(FireBurst, Land, 0.9f, 1.0f, 1.3f); Hurt(Weak.Get(), Share(Waves, w)); });
					Ring(When + 0.5f, Land, 40.0f, 300.0f, 0.4f, FLinearColor(1.0f, 0.1f, 0.15f), 0.0f);
				}
			}
			Move(Grail, 0.5f + Waves * 0.32f + 0.5f, 0.5f, Spot, Spot + FVector(0, 0, -400), FVector(HeroSize), FVector(HeroSize * 0.7f), 2, true);
			Impact = 0.5f + (Waves - 1) * 0.32f + 0.5f;
			Length = Impact + 1.1f;
		}
		break;
	case EBWAugmentSet::Sylpira: // 바람 새가 적들 너머에 떠올라 날개를 펴면, 바람 칼날이 판 곳곳에 내리꽂힌다
		{
			const FVector Hover = BehindEnemies + FVector(0, 0, 260);
			AActor* Bird = Hero(TEXT("Miracle_Sylpira_A"), 380.0f, Hover + FVector(0, 0, 700), FaceCamera);
			Move(Bird, 0.1f, 0.7f, Hover + FVector(0, 0, 700), Hover, FVector(HeroSize * 0.6f), FVector(HeroSize), 1, false);
			// 날갯짓 두 번
			for (int32 f = 0; f < 2; ++f)
			{
				Move(Bird, 0.85f + f * 0.3f, 0.1f, Hover, Hover + FVector(0, 0, 30), FVector(HeroSize), FVector(HeroSize * 1.12f), 1, false);
				Move(Bird, 0.97f + f * 0.3f, 0.16f, Hover + FVector(0, 0, 30), Hover, FVector(HeroSize * 1.12f), FVector(HeroSize), 0, false);
			}
			Move(Bird, 2.3f, 0.6f, Hover, Hover + FVector(0, 0, 900), FVector(HeroSize), FVector(HeroSize * 0.5f), 2, true);
			UNiagaraSystem* Wind = Fx(TEXT("Others/"), TEXT("energy_03_01"));
			const FLinearColor Mint(0.5f, 1.0f, 0.75f);
			// 적 자리 + 판 곳곳, 합쳐 열두 군데에 차례로 꽂힌다
			TArray<FVector> Spots;
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets) if (Weak.IsValid()) Spots.Add(Feet(Weak->GetActorLocation()));
			const int32 EnemySpots = Spots.Num();
			while (Spots.Num() < FMath::Max(12, EnemySpots + 6))
			{
				const FIntPoint Tile(FMath::RandRange(0, TargetGridManager->Width - 1), FMath::RandRange(0, TargetGridManager->Height - 1));
				Spots.Add(Feet(TargetGridManager->GetTileWorldLocation(Tile)));
			}
			for (int32 i = Spots.Num() - 1; i > 0; --i) Spots.Swap(i, FMath::RandRange(0, i));
			for (int32 i = 0; i < Spots.Num(); ++i)
			{
				const float When = 0.95f + i * 0.07f;
				const FVector Spot = Spots[i];
				At(When, [this, Wind, Spot]() { if (Wind) PlayShortNiagara(GetWorld(), Wind, Spot + FVector(0, 0, 40), 2.2f, 0.7f); });
				Ring(When, Spot, 40.0f, 320.0f, 0.4f, Mint, 0.0f);
			}
			// 적은 가장 늦게 꽂히는 자리 기준으로 한 번에 맞는다
			const float Last = 0.95f + (Spots.Num() - 1) * 0.07f;
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				if (!Weak.IsValid()) continue;
				const FVector Mine = Feet(Weak->GetActorLocation());
				const int32 Index = Spots.IndexOfByPredicate([&Mine](const FVector& S) { return S.Equals(Mine, 1.0f); });
				const float When = Index == INDEX_NONE ? Last : 0.95f + Index * 0.07f;
				At(When, [Hurt, Weak, Damage]() { Hurt(Weak.Get(), Damage); });
			}
			Impact = Last;
			Length = Last + 1.3f;
		}
		break;
	case EBWAugmentSet::Patrovas: // 마도서가 떠올라 마법진이 깔리고, 하늘에서 마력 벼락이 연달아 내리친다
		{
			const FVector Spot = BoardCenter + FVector(0, 0, 260);
			AActor* Book = Hero(TEXT("Miracle_Patrovas_A"), 220.0f, Spot + FVector(0, 0, -300), FaceCamera);
			Move(Book, 0.1f, 0.7f, Spot + FVector(0, 0, -300), Spot, FVector(HeroSize * 0.5f), FVector(HeroSize), 1, false);
			Spin(Book, 0.1f, 0.7f, 360.0f);
			const FLinearColor Violet(0.75f, 0.4f, 1.0f);
			At(0.5f, [Burst, MagicCircle, Feet, Spot]() { Burst(MagicCircle, Feet(Spot), 1.2f, 2.4f, 1.0f); });
			Ring(0.45f, Spot, 100.0f, 1300.0f, 0.8f, Violet, 0.3f);
			UNiagaraSystem* Lightning = Fx(TEXT("Others/"), TEXT("Trail_05_01"));
			static constexpr int32 Strikes = 3;
			float Delay = 0.95f;
			for (int32 s = 0; s < Strikes; ++s)
			{
				for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
				{
					if (!Weak.IsValid()) continue;
					const FVector Land = Feet(Weak->GetActorLocation()) + FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), 0.0f);
					Bolt(Delay, 0.09f, Land + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), 1400.0f), Land, Lightning, 2.0f, EMiraclePath::Straight);
					At(Delay + 0.09f, [Burst, Hurt, SpikeBurst, Weak, Land, Share, s]() { Burst(SpikeBurst, Land, 1.6f, 1.0f, 1.4f); Hurt(Weak.Get(), Share(Strikes, s)); });
					Ring(Delay + 0.09f, Land, 40.0f, 380.0f, 0.4f, Violet, 0.0f);
					Delay += 0.06f;
				}
				Delay += 0.22f;
			}
			Move(Book, Delay + 0.6f, 0.5f, Spot, Spot + FVector(0, 0, 400), FVector(HeroSize), FVector(0.0f), 1, true);
			Impact = 0.95f;
			Length = Delay + 1.2f;
		}
		break;
	case EBWAugmentSet::Lirin: // 적들 뒤로 거대한 가면이 떠오르고, 안개가 판을 덮는다
		{
			// 너무 높으면 화면 밖으로 나가 보이지 않아, 적들 바로 뒤 낮은 곳에 세운다
			const FVector Spot = EnemyMid - ToCamera * 200.0f + FVector(0, 0, -40);
			MiraclePullDistance = 550.0f;
			AActor* Mask = Hero(TEXT("Miracle_Lirin_A"), 360.0f, Spot + FVector(0, 0, -400), FaceCamera);
			Move(Mask, 0.1f, 1.0f, Spot + FVector(0, 0, -400), Spot, FVector(HeroSize * 0.7f), FVector(HeroSize), 1, false);
			UNiagaraSystem* Mist = Fx(TEXT("Others/"), TEXT("energy_02_01"));
			for (int32 X = TargetGridManager->Width - 1; X >= 0; --X)
			{
				for (int32 Y = 0; Y < TargetGridManager->Height; Y += 2)
				{
					const FVector Tile = TargetGridManager->GetTileWorldLocation(FIntPoint(X, Y)) + FVector(0, 0, 40);
					At(0.6f + (TargetGridManager->Width - 1 - X) * 0.07f, [this, Mist, Tile]() { if (Mist) PlayShortNiagara(GetWorld(), Mist, Tile, 0.9f, 0.6f); });
				}
			}
			Ring(0.6f, EnemyMid, 200.0f, 2400.0f, 1.0f, FLinearColor(0.7f, 0.5f, 1.0f), 0.35f);
			UNiagaraSystem* Mark = Fx(TEXT("Others/"), TEXT("energy_05_01"));
			for (const TWeakObjectPtr<ABWCharacter>& Weak : Targets)
			{
				At(1.25f, [Strike, Weak, Mark]() { Strike(Weak.Get(), Mark, 1.4f); });
				At(1.26f, [Weak]() { if (Weak.IsValid() && Weak->CurrentHP > 0) Weak->ApplyStatus(EStatusEffect::Stun, 1, 0); });
			}
			Move(Mask, 2.2f, 0.7f, Spot, Spot + FVector(0, 0, 300), FVector(HeroSize), FVector(0.0f), 2, true);
			Impact = 1.25f;
		}
		break;
	default: break;
	}

	// 일격: 멈춤, 흔들림, 카메라 당김
	MoodStart = Now;
	MoodEnd = Now + (Length - 0.3f) * T;
	ZoomAt = Now + Impact * T;
	At(Impact, [this]() { StartHitStop(0.14f); StartCameraShake(22.0f, 0.55f); });
	if (Set == EBWAugmentSet::Lirin) {}
	return Length * T;
}

void ABWPlayerController::UpdateMiracleShots()
{
	const double Now = FPlatformTime::Seconds();
	for (int32 i = MiracleShots.Num() - 1; i >= 0; --i)
	{
		FMiracleShot& Shot = MiracleShots[i];
		if (Now < Shot.StartAt) continue;
		const float Alpha = FMath::Clamp((float)((Now - Shot.StartAt) / FMath::Max(0.01f, Shot.Duration)), 0.0f, 1.0f);
		if (!Shot.bStarted)
		{
			Shot.bStarted = true;
			if (Shot.Trail) Shot.Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Shot.Trail, Shot.From, FRotator::ZeroRotator, FVector(Shot.TrailScale), false);
		}
		FVector Pos = FMath::Lerp(Shot.From, Shot.To, Alpha);
		if (Shot.Path == EMiraclePath::Arc) Pos.Z += FMath::Sin(Alpha * PI) * 420.0f;
		else if (Shot.Path == EMiraclePath::Spiral) Pos = Shot.To + FRotator(0.0f, Alpha * 540.0f, 0.0f).RotateVector((Shot.From - Shot.To) * (1.0f - Alpha));
		if (UNiagaraComponent* Comp = Shot.Comp.Get())
		{
			const FVector Move = Pos - Comp->GetComponentLocation();
			if (!Move.IsNearlyZero()) Comp->SetWorldRotation(Move.Rotation());
			Comp->SetWorldLocation(Pos);
		}
		if (Alpha < 1.0f) continue;

		if (UNiagaraComponent* Comp = Shot.Comp.Get()) Comp->DestroyComponent();
		if (Shot.Impact) PlayShortNiagara(GetWorld(), Shot.Impact, Shot.To, Shot.ImpactScale, 0.45f);
		if (ABWCharacter* Target = Shot.HitTarget.Get())
		{
			if (Target->CurrentHP > 0 && Shot.Damage > 0) Target->TakeFixedDamage(Shot.Damage, TEXT("기적"));
			if (Shot.bStun && Target->CurrentHP > 0) Target->ApplyStatus(EStatusEffect::Stun, 1, 0);
			if (Target->CurrentHP > 0 && Shot.Damage > 0) StartHitStop(0.04f);
		}
		MiracleShots.RemoveAt(i);
	}
}

void ABWPlayerController::PlayNextMiracle()
{
	if (bMiraclePlaying || MiracleQueue.Num() == 0) return;
	bMiraclePlaying = true;
	TFunction<void()> Show = MoveTemp(MiracleQueue[0]);
	MiracleQueue.RemoveAt(0);
	// 연출 동안 HUD를 걷어 모델이 가리지 않게 한다
	SetMiracleHUDHidden(true);
	Show();
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, [this]()
	{
		bMiraclePlaying = false;
		if (MiracleQueue.Num() == 0) SetMiracleHUDHidden(false);
		PlayNextMiracle();
	}, LastMiracleLength, false);
}

void ABWPlayerController::SetMiracleHUDHidden(bool bHideHUD)
{
	UUserWidget* Widgets[2] = { BattleHUDWidget, HandWidget };
	for (int32 i = 0; i < 2; ++i)
	{
		if (!Widgets[i]) continue;
		if (bHideHUD && !bMiracleHUDHidden) MiracleHUDVisibility[i] = Widgets[i]->GetVisibility();
		Widgets[i]->SetVisibility(bHideHUD ? ESlateVisibility::Hidden : MiracleHUDVisibility[i]);
	}
	bMiracleHUDHidden = bHideHUD;
}

void ABWPlayerController::PreviewAllMiracles()
{
	for (int32 i = 1; i <= (int32)EBWAugmentSet::Radiant; ++i) QueueMiracleShow((EBWAugmentSet)i);
}

static FAutoConsoleCommandWithWorld GBWMiraclesCommand(TEXT("bw.Miracles"), TEXT("세트 기적 연출 12종을 순서대로 재생한다"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ABWPlayerController* PC = World ? Cast<ABWPlayerController>(World->GetFirstPlayerController()) : nullptr) PC->PreviewAllMiracles();
	}));

void ABWPlayerController::ShowTurnBanner(const FString& Title, const FString& Sub, const FLinearColor& Accent)
{
	if (UBWAutoPlayer::IsActive()) return;
	if (!TurnBannerWidget) TurnBannerWidget = CreateWidget<UBWTurnBannerWidget>(this, UBWTurnBannerWidget::StaticClass());
	if (TurnBannerWidget) TurnBannerWidget->ShowBanner(Title, Sub, Accent);
}

void ABWPlayerController::ShowBattleResult(bool bVictory)
{
	// 들고 있던 카드·대상 고르기는 접고 입력을 잠근다
	if (HandWidget && HandWidget->IsHoldingCard()) HandWidget->CancelHeldCard();
	bIsDuelTargetingMode = false;
	bActionInProgress = true;
	if (UBWAutoPlayer::IsActive()) return; // 측정 봇이 전투 끝을 직접 처리한다

	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	const ABWCharacter* Me = PossessedBWCharacter;
	TArray<TPair<FString, FString>> Lines;
	if (Me)
	{
		Lines.Emplace(TEXT("전투 턴"), FString::Printf(TEXT("%d턴"), Me->CombatTurnNumber));
		Lines.Emplace(TEXT("남은 HP"), FString::Printf(TEXT("%d / %d"), FMath::Max(0, Me->CurrentHP), Me->MaxHP));
	}

	FString Sub;
	if (bVictory)
	{
		if (GI)
		{
			switch (GI->CurrentEncounterType)
			{
			case EMapNodeType::Elite: Sub = TEXT("엘리트 처치"); break;
			case EMapNodeType::Boss: Sub = GI->IsRunCleared() ? TEXT("보스 처치 · 탑 정복") : FString::Printf(TEXT("보스 처치 · %d층 돌파"), GI->CurrentStage); break;
			default: Sub = TEXT("B.W 정산 완료"); break;
			}
			// 보상: 골드·회복은 이미 받았고, 카드는 지도에서 고른다
			for (const FRewardItem& Reward : GI->PendingRewards)
			{
				switch (Reward.Type)
				{
				case ERewardType::Gold: Lines.Emplace(TEXT("정산금"), FString::Printf(TEXT("+%d"), Reward.Amount)); break;
				case ERewardType::Heal: Lines.Emplace(TEXT("회복"), FString::Printf(TEXT("HP +%d"), Reward.Amount)); break;
				case ERewardType::CardSelection: Lines.Emplace(TEXT("카드 보상"), FString::Printf(TEXT("%d장 중 1장"), Reward.CardChoices.Num())); break;
				default: Lines.Emplace(TEXT("보상"), Reward.RewardName); break;
				}
			}
		}
	}
	else
	{
		Sub = TEXT("당신은 쓰러졌습니다");
		Lines.Emplace(TEXT("B.W 회수팀이 곧 도착합니다. 시신은 수습해 드리죠."), FString());
	}

	if (!BattleResultWidget) BattleResultWidget = CreateWidget<UBWBattleResultWidget>(this, UBWBattleResultWidget::StaticClass());
	if (!BattleResultWidget)
	{
		UGameplayStatics::OpenLevel(GetWorld(), bVictory ? TEXT("/Game/System/Level/MapLevel") : TEXT("TitleLevel"));
		return;
	}
	TWeakObjectPtr<ABWPlayerController> WeakThis(this);
	BattleResultWidget->ShowResult(bVictory, Sub, Lines,
		bVictory ? TEXT("클릭하거나 Space를 눌러 지도로") : TEXT("클릭하거나 Space를 눌러 타이틀로"),
		[WeakThis, bVictory]()
		{
			if (WeakThis.IsValid()) UGameplayStatics::OpenLevel(WeakThis->GetWorld(), bVictory ? TEXT("/Game/System/Level/MapLevel") : TEXT("TitleLevel"));
		});
}

void ABWPlayerController::PreloadCombatAssets()
{
	if (UBWAutoPlayer::IsActive()) return;
	auto Keep = [this](UObject* Asset) { if (Asset) PreloadedAssets.AddUnique(Asset); };
	auto KeepCard = [&Keep](const FBWCardData& Card)
	{
		Keep(Card.NiagaraEffect.LoadSynchronous());
		Keep(Card.SkillEffect.LoadSynchronous());
		Keep(Card.CardSound.LoadSynchronous());
		Keep(Card.BelowImage.LoadSynchronous()); // 카드 그림: 위젯은 참조만 읽어서 미리 불러 둬야 보인다
		Keep(Card.AboveImage.LoadSynchronous());
	};

	if (PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
	{
		const UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		for (const FBWCardData& Card : Deck->Hand) KeepCard(Card);
		for (const FBWCardData& Card : Deck->Deck) KeepCard(Card);
		for (const FBWCardData& Card : Deck->Discard) KeepCard(Card);
		Keep(PossessedBWCharacter->HitFlashMaterial.LoadSynchronous());
		Keep(PossessedBWCharacter->DeathEffect.LoadSynchronous());
	}
	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), Characters);
	for (AActor* Actor : Characters)
	{
		const ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (!Char || !Char->bIsEnemy) continue;
		for (const FBWEnemyTurnPattern& Pattern : Char->CombatPatterns)
		{
			for (const FBWEnemyPattern& Action : Pattern.Actions)
			{
				Keep(Action.ActionNiagara.LoadSynchronous());
				Keep(Action.ActionEffect.LoadSynchronous());
				Keep(Action.ActionSound.LoadSynchronous());
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[BWCombat] Preloaded %d combat assets"), PreloadedAssets.Num());
	if (HandWidget && PossessedBWCharacter && PossessedBWCharacter->DeckComponent) HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand());
}

void ABWPlayerController::PlayPlayerDuel(ABWCharacter* PlayerChar, ABWCharacter* Enemy, const FBWCardData& AttackCard, bool bCardBoost, TFunction<void()> OnFinished)
{
	if (!PlayerChar || !Enemy || !TargetGridManager || Enemy->CurrentHP <= 0)
	{
		if (OnFinished) OnFinished();
		return;
	}

	// 5-1: 양쪽이 굴려 큰 쪽의 숫자가 피해 (5-3: 치명타 → 배율 → 방어 → 방어도 → HP). 국가 기믹은 내 쪽 굴림 조건에 반영
	// 결과는 지금 정하고(같은 시드 = 같은 결과), 화면은 그 결과를 보여 준다
	// [수정] 내 숫자 범위 = 기본 공격 (+ 공격카드로 강화했으면 카드 몫)
	const FBWDuelSide PlayerSide = MakeFieldAttackSide(PlayerChar, bCardBoost ? &AttackCard : nullptr, Enemy);
	FBWDuelSide EnemySide = MakeCounterSide(Enemy, PlayerSide);
	FBWDuelResult Result = UBWDuelRules::ResolveDuel(PlayerSide, EnemySide, TargetGridManager->GetCombatRandom());
	// 적의 공격이 닿지 않는 거리에서 건 격돌: 져도 피해도 상태이상도 받지 않는다
	const bool bSafeLoss = !Result.bDraw && !Result.bAttackerWon && IsOutOfEnemyReach(Enemy);
	if (bSafeLoss)
	{
		Result.FinalDamage = 0;
		EnemySide.AppliedStatus = EStatusEffect::None;
		EnemySide.bPushesTarget = false;
	}
	if (Result.bAttackerRerolled)
	{
		PlayerChar->DuelRerollsLeft = FMath::Max(0, PlayerChar->DuelRerollsLeft - 1);
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Reroll augment used (%d left this battle)"), PlayerChar->DuelRerollsLeft);
	}

	FBWDuelPresentation Show;
	Show.LeftName = TEXT("나");
	Show.LeftSkill = bCardBoost ? FString::Printf(TEXT("기본 공격 + %s"), *AttackCard.CardName) : AttackCard.CardName;
	Show.LeftRange = PlayerSide.SkillRange;
	Show.LeftRoll = Result.AttackerRoll;
	Show.RightName = Enemy->EnemyDisplayName;
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	Show.RightSkill = Action ? (Action->CounterName.IsEmpty() ? Action->ActionName : Action->CounterName) : TEXT("스킬");
	Show.bGuardHeld = Result.bGuardHeld;
	Show.RightRange = EnemySide.SkillRange;
	Show.RightRoll = Result.DefenderRoll;
	Show.Winner = Result.bDraw ? -1 : (Result.bAttackerWon ? 0 : 1);
	Show.Damage = Result.FinalDamage;
	Show.bCritical = Result.bCritical;
	Show.TieRerolls = Result.TieRerolls;
	Show.bRerolled = Result.bAttackerRerolled;
	if (bSafeLoss) Show.Extra = TEXT("사거리 밖 - 피해 없음");

	// 진 쪽이 받을 최종 피해도 지금 정해서 화면에 보여 준다 (내가 지면 내 방어가 적용된다)
	FBWPlannedHit Hit;
	if (!Result.bDraw)
	{
		Hit = PlanDuelHit(Result.bAttackerWon ? Enemy : PlayerChar, Result.FinalDamage, Result.bAttackerWon);
		// 관통 마탄 등: 이긴 합의 피해가 적 방어도를 무시
		if (Result.bAttackerWon && bCardBoost && AttackCard.bIgnoreTargetBlock) Hit = Enemy->PlanHit(Result.FinalDamage, true);
		FillHitPresentation(Show, Hit, !Result.bAttackerWon);
		if (Result.GuardReduced > 0)
		{
			// 방어 카드에 깎인 만큼을 계산식에 보여 준다
			Show.Damage += Result.GuardReduced;
			Show.DefenseReduced += Result.GuardReduced;
		}
	}

	TWeakObjectPtr<ABWCharacter> WeakPlayer(PlayerChar), WeakEnemy(Enemy);
	ShowDuel(Show, [this, WeakPlayer, WeakEnemy, AttackCard, PlayerSide, EnemySide, Result, Hit, OnFinished]()
	{
		ABWCharacter* P = WeakPlayer.Get();
		ABWCharacter* E = WeakEnemy.Get();
		if (!P || !E || Result.bDraw)
		{
			if (P && E) ApplyPlayerDuelOutcome(P, E, AttackCard, PlayerSide, EnemySide, Result, Hit);
			if (OnFinished) OnFinished();
			return;
		}

		// 이긴 쪽이 진 쪽으로 찌르는 순간 피해가 들어간다
		ABWCharacter* Winner = Result.bAttackerWon ? P : E;
		ABWCharacter* Loser = Result.bAttackerWon ? E : P;
		PlayLungeThen(Winner, Loser,
			[this, WeakPlayer, WeakEnemy, AttackCard, PlayerSide, EnemySide, Result, Hit]()
			{
				if (WeakPlayer.IsValid() && WeakEnemy.IsValid()) ApplyPlayerDuelOutcome(WeakPlayer.Get(), WeakEnemy.Get(), AttackCard, PlayerSide, EnemySide, Result, Hit);
			},
			OnFinished);
	});
}

FBWPlannedHit ABWPlayerController::PlanDuelHit(ABWCharacter* Target, int32 Damage, bool bPlayerDeals, int32 ShieldMP)
{
	if (!Target) return FBWPlannedHit();
	// 11-1 타이가: 내가 이겨 피해를 준 최초 3회는 방어도 무시 (과부하면 적의 첫 공격 1회가 내 방어도 무시)
	bool bIgnoreBlock = false;
	if (Damage > 0)
	{
		bIgnoreBlock = bPlayerDeals ? UBWDimensionRules::ConsumeTaigaCharge(Battlefield) : UBWDimensionRules::ConsumeTaigaEnemyCharge(Battlefield);
	}
	return Target->PlanHit(Damage, bIgnoreBlock, ShieldMP);
}

void ABWPlayerController::FillHitPresentation(FBWDuelPresentation& Show, const FBWPlannedHit& Hit, bool bPlayerIsHit) const
{
	Show.bPlayerIsHit = bPlayerIsHit;
	Show.bEvaded = Hit.Defense.bEvaded;
	Show.FinalDamage = Hit.Defense.DamageAfterDefense;
	Show.DefenseReduced = Hit.RawDamage - Hit.Defense.DamageAfterDefense;
	Show.BlockAbsorbed = Hit.BlockAbsorbed;
	Show.HPLoss = Hit.HPLoss;
	if (Hit.bIgnoreBlock) Show.Extra += TEXT("방어도 무시");

	// 내 방어 수치: 방어 = "-2", 회피 = "회피!"/"회피 실패", 매직실드 = "실드 -5"
	if (bPlayerIsHit && PossessedBWCharacter)
	{
		switch (PossessedBWCharacter->Defense.Type)
		{
		case EDefenseType::Evade:
			{
				const ABWCharacter* Me = PossessedBWCharacter;
				const int32 Chance = FMath::RoundToInt(FMath::Clamp(Me->Defense.EvadeChance + Me->Life.EvadeBonus, 0.0f, UBWDuelRules::MaxEvadeChance) * 100.0f);
				Show.DefenseText = Hit.Defense.bEvaded ? FString::Printf(TEXT("회피! (%d%%)"), Chance) : FString::Printf(TEXT("회피 실패 (%d%%)"), Chance);
			}
			break;
		case EDefenseType::MagicShield:
			{
				const int32 Flat = PossessedBWCharacter->Defense.FlatReduction;
				if (Hit.Defense.MPSpent > 0) Show.DefenseText = FString::Printf(TEXT("실드 -%d"), Hit.Defense.ShieldBlocked + Flat);
				else Show.DefenseText = Flat > 0 ? FString::Printf(TEXT("-%d"), Flat) : FString(TEXT("가드 없음"));
			}
			break;
		default:                        Show.DefenseText = FString::Printf(TEXT("-%d"), PossessedBWCharacter->Defense.FlatReduction); break;
		}
	}
}

void ABWPlayerController::ApplyPlayerDuelOutcome(ABWCharacter* PlayerChar, ABWCharacter* Enemy, const FBWCardData& AttackCard, const FBWDuelSide& PlayerSide, const FBWDuelSide& EnemySide, const FBWDuelResult& Result, const FBWPlannedHit& Hit)
{
	if (!PlayerChar || !Enemy) return;
	ABWCharacter* Loser = nullptr;
	if (!Result.bDraw)
	{
		Loser = Result.bAttackerWon ? Enemy : PlayerChar;

		// 합 화면에서 보여 준 그대로 반영 (방어·타이가 방어도 무시는 PlanDuelHit에서 이미 정함)
		const FBWDefenseResult Defended = Loser->ApplyPlannedHit(Hit);
		PlayImpactFeedback(Result.bAttackerWon ? PlayerChar : Enemy, Loser, Defended, Hit.HPLoss, Result.bCritical);
		if (Result.bAttackerWon && IsOutOfEnemyReach(Enemy)) Enemy->RushSpeed = 2; // 멀리서 맞은 적은 다음 턴 달려든다

		// 증강 '가시 갑옷': 적이 이긴 합에서도 받은 피해의 일부를 되돌려준다
		if (!Result.bAttackerWon) ReflectThorns(Enemy, Defended.DamageAfterDefense);

		// 9장: 이긴 쪽 스킬의 상태이상을 진 쪽에 건다 (살아 있을 때만)
		const FBWDuelSide& WinnerSide = Result.bAttackerWon ? PlayerSide : EnemySide;
		if (WinnerSide.AppliedStatus != EStatusEffect::None && Loser->CurrentHP > 0 && !Loser->IsActorBeingDestroyed())
		{
			Loser->ApplyStatus(WinnerSide.AppliedStatus, WinnerSide.StatusDuration, WinnerSide.StatusValue);
		}

		// 밀기 효과가 붙은 스킬이 이기면 진 쪽을 1칸 민다 (베렌드 벽 충돌은 ResolvePushAfterDuel)
		if (WinnerSide.bPushesTarget)
		{
			ResolvePushAfterDuel(Result.bAttackerWon ? PlayerChar : Enemy, Loser, Result.bAttackerWon);
		}
	}

	// 연출: 진 쪽 칸에서 카드 이펙트와 소리
	if (Loser)
	{
		const FVector FxLoc = Loser->GetActorLocation() + FVector(0, 0, 40);
		BWSpawnFX(GetWorld(), AttackCard.NiagaraEffect, AttackCard.SkillEffect, FxLoc);
		if (AttackCard.CardSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), AttackCard.CardSound.Get(), FxLoc);
	}

	// 막고 친다: 방어 카드로 막아 낸 적이 반격한다
	if (Result.bGuardHeld && EnemySide.GuardRiposte > 0) PlayerChar->TakeHit(EnemySide.GuardRiposte, false);

	const FString Summary = Result.bDraw
		? FString::Printf(TEXT("[격돌] %s vs %s: 3연속 동점, 피해 없음"), *AttackCard.CardName, *Enemy->EnemyDisplayName)
		: FString::Printf(TEXT("[격돌] %s %d vs %s %d → %s 승리, 피해 %d%s"), *AttackCard.CardName, Result.AttackerRoll, *Enemy->EnemyDisplayName, Result.DefenderRoll,
			Result.bAttackerWon ? TEXT("내가") : TEXT("적이"), Result.FinalDamage, Result.bCritical ? TEXT(" (치명타)") : TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Summary);
	Prompt(Summary, 3.5f, EBWPromptStyle::Result);

	ApplyLifestyleAfterDuel(PlayerChar, Enemy, AttackCard, Result, Hit);
	OnDuelResolved.Broadcast(Result, Enemy, AttackCard);
}

bool ABWPlayerController::IsForcedToMagicBullet() const
{
	const ABWCharacter* P = PossessedBWCharacter;
	if (!P || !P->Life.bPossessed || !P->DeckComponent) return false;
	return P->DeckComponent->Hand.ContainsByPredicate([](const FBWCardData& Card) { return Card.CardKind == ECardKind::Attack && Card.Special == EBWCardSpecial::MagicBullet; });
}

bool ABWPlayerController::IsOutOfEnemyReach(const ABWCharacter* Enemy) const
{
	const ABWCharacter* P = PossessedBWCharacter;
	if (!P || !Enemy) return false;
	const FIntPoint Gap = P->OwnGridCoordinate - Enemy->OwnGridCoordinate;
	return FMath::Abs(Gap.X) + FMath::Abs(Gap.Y) > FMath::Max(1, Enemy->ActionRange);
}

bool ABWPlayerController::CanUseLifestyleCard(const FBWCardData& Card, FString& OutReason) const
{
	const ABWCharacter* P = PossessedBWCharacter;
	if (!P) return true;
	const FBWLifestyleState& L = P->Life;
	if (Card.CardKind == ECardKind::Attack)
	{
		if (Card.Special == EBWCardSpecial::MagicBullet && !L.bLoaded && !L.bPossessed)
		{
			OutReason = TEXT("마탄은 장전이 있어야 쏠 수 있습니다. 사격 카드로 격돌에서 이기면 장전됩니다.");
			return false;
		}
		if (Card.Special != EBWCardSpecial::MagicBullet && IsForcedToMagicBullet())
		{
			OutReason = TEXT("빙의: 패에 있는 마탄을 먼저 쏴야 합니다.");
			return false;
		}
		return true;
	}
	switch (Card.Special)
	{
	case EBWCardSpecial::Rage:
		if (L.bRageUsed) { OutReason = TEXT("격노는 전투당 한 번입니다."); return false; }
		if (L.WillUses < UBWLifestyleRules::RageWillUses) { OutReason = FString::Printf(TEXT("격노: 이번 전투에 철의 의지를 %d번 써야 합니다 (%d/%d)."), UBWLifestyleRules::RageWillUses, L.WillUses, UBWLifestyleRules::RageWillUses); return false; }
		break;
	case EBWCardSpecial::Meditate:
		if (L.bMovedThisTurn || (P->DeckComponent && P->DeckComponent->DuelsOpenedThisTurn > 0)) { OutReason = TEXT("명상은 이동이나 격돌을 하기 전에만 쓸 수 있습니다."); return false; }
		break;
	case EBWCardSpecial::Descend:
		if (!L.bTenebris && L.Dark < UBWLifestyleRules::TenebrisDarkNeeded) { OutReason = FString::Printf(TEXT("강림: 어둠이 %d 이상이어야 합니다 (지금 %d)."), UBWLifestyleRules::TenebrisDarkNeeded, L.Dark); return false; }
		break;
	case EBWCardSpecial::Gale:
		if (L.GaleCooldown > 0) { OutReason = FString::Printf(TEXT("질풍노도는 %d턴 뒤에 다시 쓸 수 있습니다."), L.GaleCooldown); return false; }
		break;
	default: break;
	}
	return true;
}

void ABWPlayerController::ApplyLifestyleAfterDuel(ABWCharacter* P, ABWCharacter* Enemy, const FBWCardData& Card, const FBWDuelResult& Result, const FBWPlannedHit& Hit)
{
	if (!P || P->CurrentHP <= 0) return;
	FBWLifestyleState& L = P->Life;
	const bool bWon = !Result.bDraw && Result.bAttackerWon;
	const bool bLost = !Result.bDraw && !Result.bAttackerWon;
	const int32 Dealt = bWon ? Hit.Defense.DamageAfterDefense : 0;

	if (bWon)
	{
		if (Card.WinHeal > 0) P->Heal(Card.WinHeal);
		if (Card.WinBlock > 0) { P->Block += Card.WinBlock; P->ShowPopup(FString::Printf(TEXT("방어도 +%d"), Card.WinBlock), FLinearColor(0.45f, 0.75f, 1.0f)); }
		if (Card.WinMP > 0 && P->ClassProfile.bUsesMP) P->CurrentMP = FMath::Min(P->MaxMP, P->CurrentMP + Card.WinMP);
		if (Card.WinDraw > 0 && P->DeckComponent) P->DeckComponent->DrawCards(Card.WinDraw);
		if (Card.WinDark > 0) L.Dark += Card.WinDark;
		if (Card.WinLifesteal > 0.0f && Dealt > 0) P->Heal(FMath::FloorToInt(Dealt * Card.WinLifesteal));
		if (Card.Special == EBWCardSpecial::Shot && !L.bLoaded) { L.bLoaded = true; P->ShowPopup(TEXT("장전"), FLinearColor(0.6f, 1.0f, 0.6f)); }
		// 철혈: 이기면 방어도 +1 (폭풍 질주면 두 배)
		if (P->CharacterClass == EClassType::HP)
		{
			const int32 Iron = (UBWLifestyleRules::IronBloodBlock + P->AugmentTotals.GetInt(EBWAugStat::IronBloodUp)) * (L.bStormReady ? 2 : 1);
			P->Block += Iron;
			P->ShowPopup(FString::Printf(TEXT("철혈 +%d"), Iron), FLinearColor(0.85f, 0.85f, 0.8f));
		}
		// 테네브리스: 준 피해의 30% 회복 (어둠은 쌓이지 않음)
		if (L.bTenebris && Dealt > 0) P->Heal(FMath::FloorToInt(Dealt * UBWLifestyleRules::TenebrisLifesteal));
	}
	if (bLost && Card.LoseSelfDamage > 0) P->TakeFixedDamage(Card.LoseSelfDamage, TEXT("자해"));
	if (Card.Condition == EBWCardCondition::BlockMax) P->Block /= 2;

	// 마탄: 쏜 횟수 +1. 일곱 번째는 같은 피해가 사수에게 돌아오고(영혼의 탄환이 있으면 대신), 그 밖에는 치명타가 아니면 들림
	if (Card.Special == EBWCardSpecial::MagicBullet)
	{
		++L.MagicCount;
		if (L.MagicCount >= UBWLifestyleRules::SeventhShot)
		{
			const int32 Recoil = bWon ? Result.FinalDamage : 0;
			UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
			if (Recoil > 0 && GI && GI->SoulBullets > 0)
			{
				--GI->SoulBullets;
				P->ShowPopup(TEXT("영혼의 탄환이 대신 맞았다"), FLinearColor(0.7f, 0.8f, 1.0f), 1.2f);
			}
			else if (Recoil > 0)
			{
				P->TakeFixedDamage(Recoil, TEXT("일곱 번째 마탄"));
			}
		}
		// 빙의 중에 쏜 탄은 빙의를 잇지 않는다 (이어지면 마탄이 평생 공짜가 된다)
		else if (!L.bPossessed && !(bWon && Result.bCritical))
		{
			L.bPossessedNext = true;
			P->ShowPopup(TEXT("빙의"), FLinearColor(0.85f, 0.2f, 0.25f));
		}
		L.bPossessed = false; // 이번 턴의 빙의는 마탄을 쏘면서 풀린다
	}
	// 철의 의지: 상처를 모두 쓰고 HP 25% 회복, 출혈 제거
	if (Card.Special == EBWCardSpecial::WillOfIron)
	{
		L.Wounds = 0;
		++L.WillUses;
		P->Statuses.RemoveAll([](const FBWStatusInstance& S) { return S.Type == EStatusEffect::Bleed; });
		P->Heal(FMath::FloorToInt(P->MaxHP * UBWLifestyleRules::WillHealFraction));
	}
	// [추가] 증강: 첫 격돌 표시, 이긴 뒤 처리, 격돌 뒤 이동
	const bool bBoosted = Card.CardName != P->GetBasicAttackCard().CardName;
	if (bWon) P->ApplyAugmentsAfterWin(Enemy, Card, bBoosted, Dealt);
	if (bWon)
	{
		// 세트 사건: 베렌드 충격파, 루나리스 달빛, 타이가 분신, 아스트린 운명
		if (HasSet(EBWAugmentSet::Berend, 3))
		{
			for (ABWCharacter* Other : GetEnemiesInReach(99))
			{
				const FIntPoint D = Other->OwnGridCoordinate - Enemy->OwnGridCoordinate;
				if (Other != Enemy && FMath::Abs(D.X) + FMath::Abs(D.Y) <= 1) SetEventHit(EBWAugmentSet::Berend, Other, 4, TEXT("충격파"));
			}
		}
		if (Result.bCritical && HasSet(EBWAugmentSet::Lunaris, 3)) SetEventHit(EBWAugmentSet::Lunaris, Enemy, FMath::Max(2, Dealt / 5), TEXT("달빛"));
		if (HasSet(EBWAugmentSet::Taiga, 3) && P->AugTrack.MovedTiles >= 3) SetEventHit(EBWAugmentSet::Taiga, Enemy, FMath::Max(2, Dealt * 3 / 10), TEXT("그림자 분신"));
		if (HasSet(EBWAugmentSet::Astrin, 3) && Result.TieRerolls > 0) SetEventHit(EBWAugmentSet::Astrin, Enemy, 5, TEXT("별똥별"));

		// 규칙을 바꾸는 증강
		const FBWAugmentTotals& T = P->AugmentTotals;
		if (T.Has(EBWAugStat::ChainHit))
		{
			ABWCharacter* Next = nullptr;
			for (ABWCharacter* Other : GetEnemiesInReach(99)) if (Other != Enemy) { Next = Other; break; }
			if (Next) SetEventHit(EBWAugmentSet::None, Next, FMath::Max(1, FMath::FloorToInt(Dealt * T.Get(EBWAugStat::ChainHit))), TEXT("연쇄"));
		}
		if (T.Has(EBWAugStat::ReturnCard) && bBoosted && P->DeckComponent && P->DeckComponent->ReturnDiscardedCard(Card.CardName))
		{
			P->ShowPopup(TEXT("환영 무기"), FLinearColor(0.75f, 0.7f, 1.0f));
		}
		if (T.Has(EBWAugStat::WinExtraDuel) && !P->AugTrack.bExtraDuelUsed && P->DeckComponent)
		{
			P->AugTrack.bExtraDuelUsed = true;
			P->DeckComponent->DuelsOpenedThisTurn = 0;
			P->DeckComponent->AttackCardsUsedThisTurn = 0;
			P->ShowPopup(TEXT("연속 공격"), FLinearColor(1.0f, 0.85f, 0.4f));
		}
		if (T.Has(EBWAugStat::FistCharge) && !bBoosted && ++P->AugTrack.FistStacks >= 3)
		{
			P->AugTrack.FistStacks = 0;
			if (P->DeckComponent) P->DeckComponent->DrawCards(T.GetInt(EBWAugStat::FistCharge));
			if (P->ClassProfile.bUsesMP) P->CurrentMP = FMath::Min(P->MaxMP, P->CurrentMP + 3);
			P->ShowPopup(TEXT("신비한 주먹"), FLinearColor(0.6f, 0.9f, 1.0f));
		}

		// 기적
		TriggerMiracle(EBWAugmentSet::Astrin, FMath::Max(Result.AttackerRoll, 1));
		TriggerMiracle(EBWAugmentSet::Berend, FMath::Max(Dealt, 1));
		if (Result.bCritical) TriggerMiracle(EBWAugmentSet::Lunaris, FMath::Max(Dealt / 2, 1));
	}
	P->AugTrack.bFirstDuelDone = true;
	P->AugTrack.bTurnFirstDuelDone = true;
	P->AugTrack.bFirstAttackCritDone = true;
	P->AugTrack.bMoveLeftAtDuel = false;
	if (Result.bCritical) P->AugTrack.bFirstCritDone = true;
	if (bBoosted) P->AugTrack.bFirstCardDone = true;
	P->RemainingMove += P->AugmentTotals.GetInt(EBWAugStat::AfterDuelMove);
	if (Card.MPCost > 0) L.ChantReady = 0; // 영창은 다음 마법 한 번에 쓴다
	L.bStormReady = false;                 // 폭풍 질주는 첫 합 한 번
	P->ExplosionExtraMP = -1;
	if (HandWidget && P->DeckComponent) HandWidget->RefreshHand(P->DeckComponent->GetCurrentHand());
}

void ABWPlayerController::Prompt(const FString& Message, float Duration, EBWPromptStyle Style)
{
	if (BattleHUDWidget) BattleHUDWidget->ShowPrompt(Message, Duration, Style);
}

void ABWPlayerController::HandleCardBurned(const FBWCardData& BurnedCard)
{
	Prompt(FString::Printf(TEXT("손패가 가득 차서(최대 %d장) '%s'을(를) 버렸습니다"),
		PossessedBWCharacter && PossessedBWCharacter->DeckComponent ? PossessedBWCharacter->DeckComponent->MaxHandSize : 10, *BurnedCard.CardName),
		3.5f, EBWPromptStyle::Warning);
}

void ABWPlayerController::WaitForMovementThen(ABWCharacter* Mover, TFunction<void()> Done)
{
	if (UBWAutoPlayer::IsActive()) { if (Done) Done(); return; }
	// 이동 연출(VisualMoveQueue)이 끝날 때까지 기다렸다가 Done을 부른다 (최대 3초)
	TWeakObjectPtr<ABWCharacter> WeakMover(Mover);
	TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakMover, Elapsed, Handle, Done]()
	{
		*Elapsed += 0.05f;
		if (WeakMover.IsValid() && WeakMover->IsMoving() && *Elapsed < 3.0f) return;

		// [수정] ClearTimer가 지금 실행 중인 이 람다(캡처 포함)를 해제하므로, 필요한 것을 먼저 지역 변수로 옮긴다
		TFunction<void()> DoneCopy = Done;
		ABWPlayerController* Self = this;
		TSharedRef<FTimerHandle> HandleCopy = Handle;
		Self->GetWorldTimerManager().ClearTimer(*HandleCopy);
		if (DoneCopy) DoneCopy();
	}), 0.05f, true);
}

void ABWPlayerController::PlayLungeThen(ABWCharacter* Attacker, ABWCharacter* Target, TFunction<void()> OnImpact, TFunction<void()> OnFinished)
{
	if (!Attacker || !Target || UBWAutoPlayer::IsActive())
	{
		if (OnImpact) OnImpact();
		if (OnFinished) OnFinished();
		return;
	}

	// 상대 쪽으로 반 칸 찌르고(이 순간 판정) 제자리로 돌아온다. 멀리 있는 적(사거리 공격)이면 반 칸만 내딛는다
	const FVector Home = Attacker->GetActorLocation();
	const float Dist = FVector::Dist2D(Home, Target->GetActorLocation());
	const float MaxStep = TargetGridManager ? TargetGridManager->TileSpacing * 0.45f : 90.0f;
	FVector Strike = FMath::Lerp(Home, Target->GetActorLocation(), Dist > KINDA_SMALL_NUMBER ? FMath::Min(0.45f, MaxStep / Dist) : 0.0f);
	Strike.Z = Home.Z;
	Attacker->FaceTowards(Target->GetActorLocation());
	Attacker->AddMovementStep(Strike, false);

	TWeakObjectPtr<ABWCharacter> WeakAttacker(Attacker);
	WaitForMovementThen(Attacker, [this, WeakAttacker, Home, OnImpact, OnFinished]()
	{
		if (OnImpact) OnImpact();
		if (!WeakAttacker.IsValid() || WeakAttacker->IsActorBeingDestroyed())
		{
			if (OnFinished) OnFinished();
			return;
		}
		WeakAttacker->AddMovementStep(Home, false);
		WaitForMovementThen(WeakAttacker.Get(), [OnFinished]() { if (OnFinished) OnFinished(); });
	});
}

void ABWPlayerController::HandleEnemyClicked(ABWCharacter* Enemy)
{
	if (!PossessedBWCharacter || !Enemy || !TargetGridManager) return;
	UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;

	if (IsEnemyInReach(Enemy, GetReachForHandIndex(INDEX_NONE)))
	{
		// [수정] 기본 공격 사거리(보통 맞닿은 적) 안의 적을 누르면 기본 공격 (강화하려면 공격카드를 그 적에게 쓴다)
		if (Deck) OpenDuel(Enemy, INDEX_NONE);
		return;
	}

	// 떨어져 있으면 남은 이동 칸 안에서 그 적에게 맞닿는 칸까지 다가간다 (닿으면 합이 열린다)
	if (PossessedBWCharacter->RemainingMove <= 0)
	{
		Prompt(TEXT("이번 턴 이동 칸을 다 썼습니다. 턴을 넘기세요"), 3.0f, EBWPromptStyle::Warning);
		return;
	}
	const TArray<FIntPoint> Path = TargetGridManager->FindRushPath(PossessedBWCharacter->OwnGridCoordinate, Enemy->OwnGridCoordinate, PossessedBWCharacter->RemainingMove, false);
	if (Path.Num() == 0)
	{
		Prompt(TEXT("그 적에게 다가갈 길이 없습니다"), 3.0f, EBWPromptStyle::Warning);
		return;
	}
	MovePlayerAlongPath(Path);
}

void ABWPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (UBWAutoPlayer::IsActive()) return;
	UpdateMiracleShots();
	UpdateMiracleProps();
	UpdateCameraPan(DeltaTime); // [추가] WASD 시점 이동 (좁은 범위)
	TraceGridUnderCursor();

	// [추가] 전투 결과 화면: Space·Enter·좌클릭으로 넘긴다 (화면 위 클릭은 위젯이 직접 받음). 다른 입력은 막는다
	if (BattleResultWidget && BattleResultWidget->IsShowing())
	{
		if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::LeftMouseButton))
			BattleResultWidget->Continue();
		return;
	}

	// [수정] 숫자 키로 카드를 집은 동안: 좌클릭 = 사용(원래 자리 위면 내려놓기), 우클릭 = 내려놓기. 칸 클릭으로는 넘기지 않는다
	if (HandWidget && HandWidget->IsHoldingCard())
	{
		if (bActionInProgress || bEnemyPhase) HandWidget->CancelHeldCard();
		else if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) HandWidget->ReleaseHeldCard();
		else if (WasInputKeyJustPressed(EKeys::RightMouseButton)) HandWidget->CancelHeldCard();
	}
	else
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) OnLeftClick();
		if (WasInputKeyJustPressed(EKeys::RightMouseButton)) OnRightClick();
	}

	UpdateReachPreview();

	// [추가] Q: 기본 공격 (맞닿은 적이 하나면 바로, 여럿이면 고른다)
	if (WasInputKeyJustPressed(EKeys::Q) && !(HandWidget && HandWidget->IsHoldingCard())) EnterBasicAttackMode();

	// [수정] 숫자 키 1~9, 0(키패드 포함): 손패 N번째 카드를 집어 마우스에 붙인다 (손패 위젯이 없으면 바로 사용)
	static const FKey CardKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	static const FKey PadKeys[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine, EKeys::NumPadZero };
	for (int32 i = 0; i < UE_ARRAY_COUNT(CardKeys); ++i)
	{
		if (!WasInputKeyJustPressed(CardKeys[i]) && !WasInputKeyJustPressed(PadKeys[i])) continue;
		if (bActionInProgress || bEnemyPhase) break;
		if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent || !PossessedBWCharacter->DeckComponent->Hand.IsValidIndex(i))
		{
			Prompt(FString::Printf(TEXT("%d번째 카드가 없습니다"), i + 1), 2.0f, EBWPromptStyle::Warning);
			break;
		}
		if (HandWidget) HandWidget->ToggleHoldCard(i);
		else UseCardShortcut(i);
		break;
	}
}

void ABWPlayerController::UseCardShortcut(int32 HandIndex)
{
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return;
	if (!PossessedBWCharacter->DeckComponent->Hand.IsValidIndex(HandIndex))
	{
		Prompt(FString::Printf(TEXT("%d번째 카드가 없습니다"), HandIndex + 1), 2.0f, EBWPromptStyle::Warning);
		return;
	}
	// 집거나 끌어 놓은 카드는 마우스 아래 적을 대상으로 먼저 본다
	TGuardValue<bool> CursorTarget(bPreferCursorTarget, true);
	SelectCard(HandIndex);
}

void ABWPlayerController::OnRightClick() 
{ 
	if (SelectedCardIndex != -1 || bIsMovementMode || bIsDuelTargetingMode) CancelSelection(); 
}

void ABWPlayerController::CancelSelection()
{
	SelectedCardIndex = -1;
	bIsMovementMode = false;
	bIsDuelTargetingMode = false;
	DuelCardIndex = INDEX_NONE;
	if (HandWidget) HandWidget->SelectedCardIndex = -1;
	if (TargetGridManager) TargetGridManager->ClearAllRanges();
}

AGridTile* ABWPlayerController::PickTileUnderCursor()
{
	if (!TargetGridManager) return nullptr;

	FVector RayOrigin, RayDir;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDir) || FMath::IsNearlyZero(RayDir.Z)) return nullptr;

	// 그리드 바닥 높이(칸 0,0의 높이)와 만나는 점
	const float PlaneZ = TargetGridManager->GetTileWorldLocation(FIntPoint::ZeroValue).Z;
	const float T = (PlaneZ - RayOrigin.Z) / RayDir.Z;
	if (T <= 0.0f) return nullptr;

	FIntPoint Coord;
	return TargetGridManager->WorldToGrid(RayOrigin + RayDir * T, Coord) ? TargetGridManager->GetTileAtCoordinate(Coord) : nullptr;
}

void ABWPlayerController::TraceGridUnderCursor()
{
	if (!TargetGridManager)
	{
		AActor* FoundGrid = UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass());
		if (FoundGrid) TargetGridManager = Cast<AGridManager>(FoundGrid);
		else return;
	}

	if (!TargetGridManager->IsGridReady())
	{
		DestroyPlacementPreview();
		CurrentTileUnderCursor = nullptr;
		return;
	}

	// [수정] 처음 맞은 물체 대신 마우스 레이와 그리드 바닥 평면의 교점으로 칸을 고른다
	// (말 모델·이펙트·글자가 칸을 가려도 바닥을 누를 수 있도록)
	{
		{
			AGridTile* HitTile = PickTileUnderCursor();
			if (HitTile)
			{
				CurrentTileUnderCursor = HitTile;
				
				// [추가] 마우스 아래에 캐릭터가 있는지 확인하여 포커스 업데이트
				ABWCharacter* CharOnTile = TargetGridManager->GetCharacterAtCoordinate(HitTile->GridCoordinate);
				if (CharOnTile != FocusedCharacter)
				{
					FocusedCharacter = CharOnTile;
					OnTargetCharacterHovered.Broadcast(FocusedCharacter);
				}

				if (CurrentTileUnderCursor != LastHoveredTile)
				{
					if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->DrawPlacementGuidelines();
					else TargetGridManager->ClearAllRanges();
					
					// [추가] 적 의도 타일 호버 시 툴팁 표시
					if (BattleHUDWidget)
					{
						bool bFoundIntent = false;

						// [추가] 적 위에 손을 얹으면 이름과 설명 (카드는 양 옆에 따로 뜬다)
						if (const ABWCharacter* Hovered = GetHoveredEnemy())
						{
							BattleHUDWidget->ShowTooltip(Hovered->EnemyDisplayName, Hovered->EnemyDescription);
							bFoundIntent = true;
						}

						// 1. 적 공격 의도 툴팁
						if (!bFoundIntent && CurrentTileUnderCursor->CurrentState == ETileState::EnemyAttackRange)
						{
							TArray<AActor*> FoundActors;
							UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
							for (AActor* Actor : FoundActors)
							{
								ABWCharacter* Enemy = Cast<ABWCharacter>(Actor);
								if (Enemy && Enemy->bIsEnemy && Enemy->CurrentHP > 0 && Enemy->LockedAttackTiles.Contains(CurrentTileUnderCursor->GridCoordinate))
								{
									// [수정] 적이 공개한 이번 턴 스킬: {V}=최댓값, {D}=속도, {R}=최솟값
									if (const FBWEnemyPattern* Action = Enemy->GetCurrentAction())
									{
										const FBWDuelSide Side = Enemy->MakeEnemyDuelSide();
										BattleHUDWidget->ShowTooltip(Action->ActionName, Action->Description, Side.SkillRange.Max, Enemy->RolledSpeed, Side.SkillRange.Min);
										bFoundIntent = true;
										break;
									}
								}
							}
						}
						// [추가] 차원 지형·장애물 설명
						FString TerrainTitle, TerrainDesc;
						if (!bFoundIntent && TargetGridManager->DescribeTile(CurrentTileUnderCursor->GridCoordinate, TerrainTitle, TerrainDesc))
						{
							BattleHUDWidget->ShowTooltip(TerrainTitle, TerrainDesc);
							bFoundIntent = true;
						}
						// 2. 아군 배치 구역 툴팁
						else if (!bFoundIntent && !bIsSkillTargetingMode && CurrentTileUnderCursor->CurrentState == ETileState::AllyZone)
						{
							if (BattleHUDWidget->AllyZone_Tooltip.bEnableTooltip)
							{
								BattleHUDWidget->ShowTooltip(BattleHUDWidget->AllyZone_Tooltip.Title, BattleHUDWidget->AllyZone_Tooltip.Description);
								bFoundIntent = true;
							}
						}
						// 3. 이동 가능 범위 툴팁
						else if (CurrentTileUnderCursor->CurrentState == ETileState::MoveRange)
						{
							if (BattleHUDWidget->MoveRange_Tooltip.bEnableTooltip)
							{
								int32 MovePts = PossessedBWCharacter ? PossessedBWCharacter->RemainingMove : 0;
								BattleHUDWidget->ShowTooltip(BattleHUDWidget->MoveRange_Tooltip.Title, BattleHUDWidget->MoveRange_Tooltip.Description, MovePts);
								bFoundIntent = true;
							}
						}
						
						if (!bFoundIntent) BattleHUDWidget->HideTooltip();
					}

					if (bIsDuelTargetingMode && PossessedBWCharacter)
					{
						// [수정] 합 대상 고르기: 맞닿은 적 칸만 강조
						RefreshDuelTargetHighlight();
					}
					else if (bIsSkillTargetingMode && !bIsMovementMode && PossessedBWCharacter && PossessedBWCharacter->DeckComponent)
					{
						TArray<FBWCardData> HandCards = PossessedBWCharacter->DeckComponent->GetCurrentHand();
						if (SelectedCardIndex >= 0 && SelectedCardIndex < HandCards.Num())
						{
							TargetGridManager->DrawSkillPreview(PossessedBWCharacter->OwnGridCoordinate, CurrentTileUnderCursor->GridCoordinate, HandCards[SelectedCardIndex]);
						}
						// [복구] 스킬 선택 중에도 마우스 아래 타일은 노란색(Hovered)으로 표시
						CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
					}
					else if (bIsMovementMode && PossessedBWCharacter)
					{
						TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->RemainingMove);
						if (CurrentTileUnderCursor->CurrentState == ETileState::MoveRange)
						{
							CurrentTileUnderCursor->SetTileState(ETileState::Hovered);
						}
					}
					
					UpdatePlacementPreview();
					LastHoveredTile = CurrentTileUnderCursor;
				}
				return;
			}
		}
	}
	
	if (LastHoveredTile)
	{
		if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->DrawPlacementGuidelines();
		else if (!bIsMovementMode && !bIsDuelTargetingMode) TargetGridManager->ClearAllRanges();
		else if (bIsMovementMode && PossessedBWCharacter) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, PossessedBWCharacter->RemainingMove);
		
		LastHoveredTile = nullptr;
		DestroyPlacementPreview();
	}
	CurrentTileUnderCursor = nullptr;
}

void ABWPlayerController::UpdatePlacementPreview()
{
	if (!bIsSkillTargetingMode && !bIsMovementMode && !bIsDuelTargetingMode && CurrentTileUnderCursor && TargetGridManager->IsValidAllyPlacementArea(CurrentTileUnderCursor->GridCoordinate))
	{
		TSubclassOf<AActor> PreviewClass = PlacementPreviewMap.Contains(CharacterClassToSpawn) ? PlacementPreviewMap[CharacterClassToSpawn] : nullptr;
		
		if (PreviewClass)
		{
			if (PlacementPreviewActor && PlacementPreviewActor->GetClass() != PreviewClass) DestroyPlacementPreview();

			if (!PlacementPreviewActor)
				PlacementPreviewActor = GetWorld()->SpawnActor<AActor>(PreviewClass, CurrentTileUnderCursor->GetActorLocation(), FRotator::ZeroRotator);
			
			if (PlacementPreviewActor)
			{
				PlacementPreviewActor->SetActorLocation(CurrentTileUnderCursor->GetActorLocation() + FVector(0, 0, 20));
				PlacementPreviewActor->SetActorHiddenInGame(false);
			}
		}
	}
	else DestroyPlacementPreview();
}

void ABWPlayerController::DestroyPlacementPreview()
{
	if (PlacementPreviewActor) { PlacementPreviewActor->Destroy(); PlacementPreviewActor = nullptr; }
}

void ABWPlayerController::SelectCard(int32 Index)
{
	if (bActionInProgress || bEnemyPhase) return;
	if (bIsMovementMode || bIsDuelTargetingMode) CancelSelection();
	if (!PossessedBWCharacter || !PossessedBWCharacter->DeckComponent) return;

	UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
	const EBWCardUseResult Check = Deck->CanUseCard(Index, PossessedBWCharacter->CurrentMP);
	if (Check != EBWCardUseResult::Ok)
	{
		PlayCardUnusableFeedback(FString::Printf(TEXT("카드를 쓸 수 없습니다: %s"), *UEnum::GetValueAsString(Check)));
		return;
	}

	// [수정] 공격카드: 내가 선공인 합에만 쓴다. 접촉으로 열린 합이 있으면 바로 그 적과, 아니면 맞닿은 적을 고른다
	if (Deck->Hand[Index].CardKind == ECardKind::Attack)
	{
		if (PossessedBWCharacter->IsStunned()) { PlayCardUnusableFeedback(TEXT("기절 상태라 격돌을 걸 수 없습니다.")); return; }
		if (!Deck->CanOpenDuel()) { PlayCardUnusableFeedback(TEXT("이번 턴에 이미 격돌을 열었습니다.")); return; }
		if (PossessedBWCharacter->Life.bNoDuelThisTurn) { PlayCardUnusableFeedback(TEXT("이번 턴에는 격돌을 걸 수 없습니다 (질풍노도·명상 등).")); return; }
		FString LifestyleReason;
		if (!CanUseLifestyleCard(Deck->Hand[Index], LifestyleReason)) { PlayCardUnusableFeedback(LifestyleReason); return; }
		const int32 Reach = GetReachForHandIndex(Index);
		// 집은 카드를 사거리 안의 적 위에서 놓았으면 그 적과
		if (bPreferCursorTarget && IsEnemyInReach(FocusedCharacter, Reach))
		{
			OpenDuel(FocusedCharacter, Index);
			return;
		}
		const TArray<ABWCharacter*> InReach = GetEnemiesInReach(Reach);
		if (InReach.Num() == 0)
		{
			PlayCardUnusableFeedback(Reach <= 1 ? FString(TEXT("이 공격카드는 맞닿은 적에게만 쓸 수 있습니다. 먼저 적에게 다가가세요"))
				: FString::Printf(TEXT("사거리 %d칸 안에 적이 없습니다. 조금 더 다가가세요"), Reach));
			return;
		}

		// [수정] 사거리 안의 적이 하나여도 자동으로 걸지 않고 고르게 한다
		EnterDuelTargeting(Index);
		Prompt(FString::Printf(TEXT("격돌을 걸 적을 누르세요 (사거리 %d · 금색 칸 · 우클릭 취소)"), Reach), 0.0f);
		return;
	}

	// [수정] 효과카드: 고르는 즉시 나에게 적용 (회복, 방어도, 이동, MP, 뽑기, 강화, 집중)
	UseEffectCard(Index);
}

void ABWPlayerController::UseEffectCard(int32 Index)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!PlayerChar || !PlayerChar->DeckComponent) return;

	if (PlayerChar->DeckComponent->Hand.IsValidIndex(Index))
	{
		FString Reason;
		if (!CanUseLifestyleCard(PlayerChar->DeckComponent->Hand[Index], Reason)) { PlayCardUnusableFeedback(Reason); return; }
	}
	FBWCardData Card;
	if (HandWidget) HandWidget->PlayDiscardAnimation(Index);
	if (!PlayerChar->DeckComponent->UseCard(Index, Card)) return;
	PlayerChar->NoteEffectCardUsed();
	FBWLifestyleState& L = PlayerChar->Life;

	// 적용 순서는 카드에 적힌 순서대로. 하나라도 실제로 바뀐 것만 안내에 넣는다
	TArray<FString> Parts;
	if (Card.SelfDamage > 0)
	{
		PlayerChar->TakeFixedDamage(Card.SelfDamage, TEXT("자해"));
		Parts.Add(FString::Printf(TEXT("자해 %d"), Card.SelfDamage));
		if (PlayerChar->CurrentHP <= 0) return;
	}
	if (Card.Special == EBWCardSpecial::HolyHeal)
	{
		// 어둠의 성자 치유: 치유할 때마다 어둠 +1, HP 절반 이하면 더 회복하고 어둠 +2
		int32 DarkGain = 0;
		const int32 Amount = UBWLifestyleRules::HolyHealAmount(Card, PlayerChar->FTH, PlayerChar->CurrentHP, PlayerChar->MaxHP, DarkGain);
		const int32 Healed = PlayerChar->Heal(Amount);
		L.Dark += DarkGain;
		Parts.Add(FString::Printf(TEXT("HP +%d · 어둠 +%d (%d)"), Healed, DarkGain, L.Dark));
	}
	else if (Card.EffectHeal > 0)
	{
		const int32 Healed = PlayerChar->Heal(Card.EffectHeal, true);
		Parts.Add(FString::Printf(TEXT("HP +%d"), Healed));
	}
	switch (Card.Special)
	{
	case EBWCardSpecial::Unkillable:
		PlayerChar->Statuses.RemoveAll([](const FBWStatusInstance& S) { return S.Type == EStatusEffect::Bleed; });
		Parts.Add(FString::Printf(TEXT("HP +%d · 출혈 제거"), PlayerChar->Heal(FMath::FloorToInt(PlayerChar->MaxHP * UBWLifestyleRules::UnkillableHealFraction))));
		break;
	case EBWCardSpecial::Rage:
		L.bRageGuard = true;
		L.bRageUsed = true;
		PlayerChar->ShowPopup(TEXT("격노"), FLinearColor(1.0f, 0.2f, 0.15f), 1.3f);
		Parts.Add(TEXT("이번 적 턴 받는 피해 -90%"));
		break;
	case EBWCardSpecial::Meditate:
		PlayerChar->RemainingMove = 0;
		PlayerChar->AddLiberation(1 + Card.SpecialValue);
		Parts.Add(PlayerChar->Life.bLiberated ? FString(TEXT("해방")) : FString::Printf(TEXT("명상 %d/%d"), L.Liberation, UBWLifestyleRules::LiberationNeeded));
		break;
	case EBWCardSpecial::Chant:
		L.ChantPending = FMath::Max(L.ChantPending, Card.SpecialValue);
		Parts.Add(FString::Printf(TEXT("영창: 다음 턴 첫 마법 최댓값 +%d (맞으면 깨짐)"), Card.SpecialValue));
		break;
	case EBWCardSpecial::Load:
		L.bLoaded = true;
		Parts.Add(TEXT("장전"));
		break;
	case EBWCardSpecial::ClearPossess:
		L.bPossessed = L.bPossessedNext = false;
		Parts.Add(TEXT("빙의 제거"));
		break;
	case EBWCardSpecial::Descend:
		PlayerChar->SetTenebris(!L.bTenebris);
		Parts.Add(L.bTenebris ? TEXT("테네브리스: 치유 카드가 공격이 된다") : TEXT("빛으로 돌아옴"));
		break;
	case EBWCardSpecial::Gale:
		L.bNoDuelThisTurn = true;
		L.bGaleGuard = true;
		L.bStormPending = true;
		L.GaleCooldown = UBWLifestyleRules::GaleCooldownTurns + 1;
		PlayerChar->ShowPopup(TEXT("질풍노도"), FLinearColor(0.85f, 0.85f, 0.8f), 1.2f);
		Parts.Add(TEXT("이번 적 턴 받는 피해 절반 · 다음 턴 폭풍 질주"));
		break;
	default: break;
	}
	if (HasSet(EBWAugmentSet::Sylpira, 3)) SetEventHit(EBWAugmentSet::Sylpira, NearestEnemy(), 2, TEXT("바람 칼날"));
	if (Card.EffectDark > 0) { L.Dark += Card.EffectDark; Parts.Add(FString::Printf(TEXT("어둠 +%d"), Card.EffectDark)); }
	if (Card.EffectCounter > 0) { L.Counter += Card.EffectCounter; Parts.Add(FString::Printf(TEXT("반격 %d"), L.Counter)); }
	if (Card.EffectEvadeBonus > 0.0f) { L.EvadeBonus += Card.EffectEvadeBonus; Parts.Add(FString::Printf(TEXT("이번 적 턴 회피 +%d%%"), FMath::RoundToInt(Card.EffectEvadeBonus * 100.0f))); }
	if (Card.EffectDefenseBonus > 0) { L.DefenseBonus += Card.EffectDefenseBonus; Parts.Add(FString::Printf(TEXT("이번 적 턴 방어 +%d"), Card.EffectDefenseBonus)); }
	if (Card.CritChanceBonus > 0.0f) { L.TurnCritBonus += Card.CritChanceBonus; Parts.Add(FString::Printf(TEXT("이번 턴 치명타 +%d%%"), FMath::RoundToInt(Card.CritChanceBonus * 100.0f))); }
	if (Card.bNoDuelThisTurn) L.bNoDuelThisTurn = true;
	if (Card.bEndMoveThisTurn) PlayerChar->RemainingMove = 0;
	if (Card.bCleanse)
	{
		PlayerChar->Statuses.RemoveAll([](const FBWStatusInstance& S) { return S.Type != EStatusEffect::Focus; });
		Parts.Add(TEXT("나쁜 상태 제거"));
	}
	if (Card.EffectBlock > 0)
	{
		PlayerChar->Block += Card.EffectBlock;
		PlayerChar->ShowPopup(FString::Printf(TEXT("방어도 +%d"), Card.EffectBlock), FLinearColor(0.45f, 0.75f, 1.0f));
		Parts.Add(FString::Printf(TEXT("방어도 +%d"), Card.EffectBlock));
	}
	if (Card.EffectMove > 0)
	{
		PlayerChar->RemainingMove += Card.EffectMove;
		PlayerChar->ShowPopup(FString::Printf(TEXT("이동 +%d"), Card.EffectMove), FLinearColor(0.5f, 1.0f, 0.6f));
		Parts.Add(FString::Printf(TEXT("이동 +%d"), Card.EffectMove));
	}
	if (Card.EffectMP > 0)
	{
		const int32 Before = PlayerChar->CurrentMP;
		PlayerChar->CurrentMP = FMath::Min(PlayerChar->MaxMP, PlayerChar->CurrentMP + Card.EffectMP);
		PlayerChar->ShowPopup(FString::Printf(TEXT("MP +%d"), PlayerChar->CurrentMP - Before), FLinearColor(0.55f, 0.6f, 1.0f));
		Parts.Add(FString::Printf(TEXT("MP +%d"), PlayerChar->CurrentMP - Before));
	}
	if (Card.EffectAttackMaxBonus > 0)
	{
		PlayerChar->TurnAttackMaxBonus += Card.EffectAttackMaxBonus;
		PlayerChar->ShowPopup(FString::Printf(TEXT("공격 최댓값 +%d"), Card.EffectAttackMaxBonus), FLinearColor(1.0f, 0.75f, 0.3f));
		Parts.Add(FString::Printf(TEXT("이번 턴 공격카드 최댓값 +%d"), Card.EffectAttackMaxBonus));
	}
	if (Card.SelfStatus != EStatusEffect::None)
	{
		PlayerChar->ApplyStatus(Card.SelfStatus, Card.SelfStatusDuration, 0);
		FBWStatusInstance Shown;
		Shown.Type = Card.SelfStatus;
		Shown.RemainingTurns = Card.SelfStatusDuration;
		Parts.Add(UBWStatusRules::DescribeStatuses({ Shown }));
	}
	if (Card.EffectDraw > 0)
	{
		PlayerChar->DeckComponent->DrawCards(Card.EffectDraw);
		Parts.Add(FString::Printf(TEXT("카드 %d장 뽑기"), Card.EffectDraw));
	}

	// 연출: 내 칸에서 카드 이펙트와 소리
	const FVector FxLoc = PlayerChar->GetActorLocation() + FVector(0, 0, 40);
	BWSpawnFX(GetWorld(), Card.NiagaraEffect, Card.SkillEffect, FxLoc);
	if (Card.CardSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), Card.CardSound.Get(), FxLoc);

	TriggerMiracle(EBWAugmentSet::Sylpira, 6);
	if (HandWidget) HandWidget->RefreshHand(PlayerChar->DeckComponent->GetCurrentHand());
	if (bIsMovementMode && TargetGridManager) TargetGridManager->DrawMovementPreview(PlayerChar->OwnGridCoordinate, PlayerChar->RemainingMove);

	const FString Summary = FString::Printf(TEXT("[효과] %s: %s"), *Card.CardName, *FString::Join(Parts, TEXT(", ")));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Summary);
	Prompt(Summary, 2.5f, EBWPromptStyle::Result);

	// 명상은 턴을 통째로 쓴다
	if (Card.Special == EBWCardSpecial::Meditate) RequestEndTurn();
}

void ABWPlayerController::OnLeftClick()
{
	TraceGridUnderCursor();
	if (TargetGridManager && !TargetGridManager->IsGridReady()) return;

	// 이동·합 연출 중이거나 적 턴에는 입력을 받지 않는다
	if (bActionInProgress || bEnemyPhase) return;

	if (!CurrentTileUnderCursor || !TargetGridManager) return;
	ABWCharacter* ClickedChar = TargetGridManager->GetCharacterAtCoordinate(CurrentTileUnderCursor->GridCoordinate);

	if (bIsMovementMode && PossessedBWCharacter)
	{
		// [수정] 적을 누르면 그 적에게 맞닿는 칸까지 다가가고, 빈 칸이면 그 칸으로 이동
		if (ClickedChar && ClickedChar->bIsEnemy) HandleEnemyClicked(ClickedChar);
		else TryMovePlayerTo(CurrentTileUnderCursor->GridCoordinate);
		return;
	}

	if (bIsDuelTargetingMode && PossessedBWCharacter)
	{
		// [수정] 강조된 맞닿은 적을 누르면 합을 연다
		ABWCharacter* Target = TargetGridManager->GetCharacterAtCoordinate(CurrentTileUnderCursor->GridCoordinate);
		if (Target && Target->bIsEnemy) OpenDuel(Target, DuelCardIndex);
		return;
	}

	if (!bIsSkillTargetingMode)
	{
		PlaceCharacterAndStartCombat(CurrentTileUnderCursor->GridCoordinate);
	}
	else if (SelectedCardIndex == -1 && ClickedChar && ClickedChar->bIsEnemy && PossessedBWCharacter)
	{
		// [추가] 아무것도 고르지 않고 적을 누르면: 맞닿아 있으면 합, 떨어져 있으면 다가간다
		HandleEnemyClicked(ClickedChar);
	}
}

void ABWPlayerController::RequestEndTurn()
{
	// 연출 중이거나 이미 적 턴이면 무시 (턴 종료 버튼 연타 방지)
	if (bActionInProgress || bEnemyPhase || !PossessedBWCharacter) return;
	bEnemyPhase = true;
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Player Turn Ended. Gathering Enemies for sequential execution..."));
	Prompt(TEXT("적 턴"), 0.0f, EBWPromptStyle::Result);
	ShowTurnBanner(TEXT("적 턴"), FString(), FLinearColor(1.0f, 0.4f, 0.35f));

	CancelSelection();
	PendingDuelTarget = nullptr;

	// 4-4: 1 출혈 등 상태이상 정산(남은 턴도 여기서 줄인다), 2 남은 손패는 버린 덱으로, 3 HP 직업은 방어도를 얻는다
	if (PossessedBWCharacter)
	{
		PossessedBWCharacter->EndOwnTurnStatuses();
		if (PossessedBWCharacter->DeckComponent) PossessedBWCharacter->DeckComponent->EndTurn();
		PossessedBWCharacter->EndTurnResources();
		PossessedBWCharacter->ApplyAugmentsEndTurn();

		// 세트 사건: 겔레온 지원 포격 (4개면 적 전체), 우르스카 곰의 일격
		if (HasSet(EBWAugmentSet::Geleon, 4)) TriggerMiracle(EBWAugmentSet::Geleon, 8);
		else if (HasSet(EBWAugmentSet::Geleon, 3))
		{
			const TArray<ABWCharacter*> Targets = GetEnemiesInReach(99);
			if (Targets.Num() > 0) SetEventHit(EBWAugmentSet::Geleon, Targets[FMath::RandRange(0, Targets.Num() - 1)], 6, TEXT("지원 포격"));
		}
		if (PossessedBWCharacter->AugmentTotals.Has(EBWAugStat::BlockBurst) && PossessedBWCharacter->Block > 0)
		{
			SetEventHit(EBWAugmentSet::None, NearestEnemy(), FMath::Max(1, FMath::FloorToInt(PossessedBWCharacter->Block * PossessedBWCharacter->AugmentTotals.Get(EBWAugStat::BlockBurst))), TEXT("철벽 반격"));
		}
		TriggerMiracle(EBWAugmentSet::Urska, PossessedBWCharacter->Block);
		if (UBWGameInstance* MiracleGI = Cast<UBWGameInstance>(GetGameInstance())) TriggerMiracle(EBWAugmentSet::Megenberta, FMath::Max(3, MiracleGI->CurrentGold / 15));
		if (HasSet(EBWAugmentSet::Urska, 3) && PossessedBWCharacter->Block >= 10) SetEventHit(EBWAugmentSet::Urska, NearestEnemy(), PossessedBWCharacter->Block / 3, TEXT("곰의 일격"));
		// 11-1 타이가: 격돌을 열지 않고 맞닿은 적 없이 턴을 끝내면 은신, 다음 공격이 치명타
		if (Battlefield.Dimension == EDimensionType::Taiga && Battlefield.TaigaChargesLeft == 0 && PossessedBWCharacter->DeckComponent->DuelsOpenedThisTurn == 0 && GetAdjacentEnemies().Num() == 0)
		{
			Battlefield.TaigaChargesLeft = 1;
			PossessedBWCharacter->ShowPopup(TEXT("은신"), FLinearColor(0.6f, 0.9f, 0.7f));
		}
		// 11-3 파트로바스 과부하: 마력선 위에서 턴을 끝내면 마력이 터진다
		if (Battlefield.bOverloaded && UBWDimensionRules::IsOnManaLine(Battlefield, PossessedBWCharacter->OwnGridCoordinate))
		{
			PossessedBWCharacter->TakeFixedDamage(UBWDimensionRules::ManaOverloadDamage, TEXT("마력 폭주"));
		}
		if (TargetGridManager) TargetGridManager->ApplyEndOfTurnTerrain(PossessedBWCharacter); // 겔레온 사격 방벽
	}

	EnemyTurnQueue.Empty();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy && (Char->CurrentHP > 0 || Char->bRevivePending))
		{
			EnemyTurnQueue.Add(Char);
		}
	}

	// 4-3: 적은 번호 순서(전투 시작 때 배치된 순서)대로 한 마리씩 행동한다
	EnemyTurnQueue.Sort([](const ABWCharacter& A, const ABWCharacter& B) { return A.EnemyOrder < B.EnemyOrder; });

	// 순차 실행 시작
	ProcessNextEnemyTurn();
}

void ABWPlayerController::ProcessNextEnemyTurn()
{
	// 승패가 정해졌으면 남은 적 행동을 멈춘다
	if (TargetGridManager && TargetGridManager->bBattleOver)
	{
		EnemyTurnQueue.Empty();
		return;
	}

	if (EnemyTurnQueue.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] All enemies acted. Delaying 0.5s before Player Turn..."));
		
		// [수정] 모든 적 행동 종료 후 0.5초 대기 후 플레이어 턴 시작
		if (UBWAutoPlayer::IsActive()) { BeginRound(); return; }
		FTimerHandle TurnResetTimer;
		GetWorldTimerManager().SetTimer(TurnResetTimer, this, &ABWPlayerController::BeginRound, 0.5f, false);

		return;
	}

	ABWCharacter* NextEnemy = EnemyTurnQueue[0];
	EnemyTurnQueue.RemoveAt(0);

	// 미회수: 이번 턴에는 일어나기만 한다
	if (NextEnemy && NextEnemy->bRevivePending)
	{
		FIntPoint Cell = NextEnemy->OwnGridCoordinate;
		if (TargetGridManager->FindFreeCellNear(Cell, Cell)) NextEnemy->Revive(Cell);
		ProcessNextEnemyTurn();
		return;
	}

	if (NextEnemy && NextEnemy->IsValidLowLevel() && NextEnemy->CurrentHP > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BWCombat] Enemy Acting: %s"), *NextEnemy->GetName());

		// [수정] 한 마리가 이동·합 연출을 모두 마친 뒤에 다음 적이 움직인다
		TWeakObjectPtr<ABWCharacter> WeakEnemy(NextEnemy);
		auto FinishThisEnemy = [this, WeakEnemy]()
		{
			// 적의 자기 턴 끝: 출혈 정산과 상태이상 남은 턴 감소
			if (WeakEnemy.IsValid() && WeakEnemy->CurrentHP > 0)
			{
				if (TargetGridManager) TargetGridManager->ApplyEndOfTurnTerrain(WeakEnemy.Get());
				WeakEnemy->EndOwnTurnStatuses();
				WeakEnemy->EndEnemyTurnRules();
			}
			if (UBWAutoPlayer::IsActive()) { ProcessNextEnemyTurn(); return; }
			FTimerHandle NextEnemyTimer;
			GetWorldTimerManager().SetTimer(NextEnemyTimer, this, &ABWPlayerController::ProcessNextEnemyTurn, 0.5f, false);
		};

		// 적의 방어도는 자기 턴이 시작될 때 사라진다 (안 그러면 끝없이 쌓여 전투가 끝나지 않는다)
		NextEnemy->Block = 0;
		NextEnemy->CoverShield = 0;

		// 기절한 적은 이번 턴 행동을 건너뛴다
		if (NextEnemy->IsStunned())
		{
			NextEnemy->SkipTurnForStun();
			NextEnemy->ShowPopup(TEXT("기절 - 행동 불가"), FLinearColor(0.7f, 0.5f, 1.0f));
			FinishThisEnemy();
		}
		else
		{
			ExecuteEnemyActionThen(NextEnemy, FinishThisEnemy);
		}
	}
	else
	{
		// 유효하지 않은 적이면 즉시 다음 단계
		ProcessNextEnemyTurn();
	}
}

void ABWPlayerController::ExecuteEnemyAction(ABWCharacter* Enemy)
{
	ExecuteEnemyActionThen(Enemy, nullptr);
}

void ABWPlayerController::ExecuteEnemyActionThen(ABWCharacter* Enemy, TFunction<void()> OnDone)
{
	auto Finish = [OnDone]() { if (OnDone) OnDone(); };
	if (!Enemy || Enemy->CurrentHP <= 0 || !TargetGridManager) { Finish(); return; }

	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	if (!Action)
	{
		Enemy->AdvancePattern();
		Finish();
		return;
	}
	EBWEnemyActionType ActionType = Action->ActionType;
	Enemy->bChanting = false; // 깨지지 않고 여기까지 왔다
	const EStatusEffect HexStatus = Action->AppliedStatus;
	const int32 HexDuration = Action->StatusDuration, HexValue = Action->StatusValue;

	bool bAttack = false;
	switch (ActionType)
	{
	case EBWEnemyActionType::Block:
		Enemy->Block += Action->Value;
		Enemy->ShowPopup(FString::Printf(TEXT("방어도 +%d"), Action->Value), FLinearColor(0.45f, 0.75f, 1.0f));
		break;
	case EBWEnemyActionType::Summon:
		TargetGridManager->SummonBeside(Enemy);
		Enemy->ShowPopup(TEXT("호위 소환"), FLinearColor(1.0f, 0.6f, 0.2f));
		break;
	case EBWEnemyActionType::Hex:
		bAttack = true; // 사거리까지 움직인 뒤 상태이상만 건다
		break;
	case EBWEnemyActionType::Chant:
		Enemy->bChanting = true;
		Enemy->ShowPopup(TEXT("영창"), FLinearColor(0.7f, 0.5f, 1.0f));
		break;
	case EBWEnemyActionType::Reckoning:
		Enemy->CurrentAttackBuff += Enemy->EatenCards * Action->Value;
		Enemy->ShowPopup(TEXT("정산"), FLinearColor(1.0f, 0.3f, 0.25f), 1.3f);
		bAttack = true;
		break;
	case EBWEnemyActionType::Devour:
	{
		ABWCharacter* Meal = nullptr;
		for (ABWCharacter* Other : GetEnemiesInReach(99)) if (Other->SummonedBy == Enemy) { Meal = Other; break; }
		if (Meal)
		{
			Meal->TakeFixedDamage(Meal->CurrentHP + Meal->Block, TEXT("포식"));
			Enemy->CurrentHP = FMath::Min(Enemy->MaxHP, Enemy->CurrentHP + Action->Value);
			Enemy->ShowPopup(FString::Printf(TEXT("포식 +%d"), Action->Value), FLinearColor(0.3f, 1.0f, 0.4f));
		}
		else bAttack = true;
		break;
	}
	case EBWEnemyActionType::Heal:
		// 회복·강화 행동을 하는 적은 이동하지 않고 그 행동만 한다
		Enemy->CurrentHP = FMath::Min(Enemy->MaxHP, Enemy->CurrentHP + Action->Value);
		Enemy->ShowPopup(FString::Printf(TEXT("회복 +%d"), Action->Value), FLinearColor(0.3f, 1.0f, 0.4f));
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s heals %d (HP %d)"), *Enemy->EnemyDisplayName, Action->Value, Enemy->CurrentHP);
		break;
	case EBWEnemyActionType::BuffAttack:
		// 강화 후 공격: 숫자 범위를 올린 뒤 같은 턴에 돌진한다
		Enemy->CurrentAttackBuff = FMath::Min(Enemy->MaxAttackBuff, Enemy->CurrentAttackBuff + Action->Value);
		Enemy->ShowPopup(FString::Printf(TEXT("강화 +%d"), Action->Value), FLinearColor(1.0f, 0.6f, 0.2f));
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s powers up +%d (buff %d)"), *Enemy->EnemyDisplayName, Action->Value, Enemy->CurrentAttackBuff);
		bAttack = true;
		break;
	default:
		bAttack = true;
		break;
	}

	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!bAttack || !PlayerChar || PlayerChar->CurrentHP <= 0)
	{
		Enemy->AdvancePattern();
		Finish();
		return;
	}

	// 자기 속도만큼 나를 향해 최단 경로로 돌진. 접촉하면 멈추고 적이 선공인 합 (이미 접촉해 있으면 이동 없이)
	const TArray<FIntPoint> Path = TargetGridManager->FindEnemyMovePath(Enemy, PlayerChar->OwnGridCoordinate);
	for (const FIntPoint& Step : Path)
	{
		if (AGridTile* StepTile = TargetGridManager->GetTileAtCoordinate(Step)) Enemy->AddMovementStep(StepTile->GetActorLocation() + FVector(0, 0, 20));
	}
	// 강철 심장: 지나는 장애물을 부수고 세진다
	if (Enemy->bBreaksObstacles)
	{
		for (const FIntPoint& Cell : Path)
		{
			if (!TargetGridManager->IsWallOrOutside(Cell)) continue;
			TargetGridManager->BreakObstacle(Cell);
			Enemy->CurrentAttackBuff = FMath::Min(Enemy->MaxAttackBuff, Enemy->CurrentAttackBuff + Enemy->BreakBuff);
			Enemy->ShowPopup(TEXT("문 부수기"), FLinearColor(1.0f, 0.6f, 0.2f), 1.2f);
		}
	}
	if (Path.Num() > 0) Enemy->OwnGridCoordinate = Path.Last();
	Enemy->LastRushSteps = Path.Num(); // 겔레온 활공: 달려온 칸만큼 세다
	Enemy->RemainingMove = FMath::Max(0, Enemy->RemainingMove - Path.Num());

	// [수정] 걸어오는 연출이 끝난 뒤에 접촉을 판정하고, 찌르는 순간 피해가 들어간다
	TWeakObjectPtr<ABWCharacter> WeakEnemy(Enemy), WeakPlayer(PlayerChar);
	const int32 Steps = Path.Num();
	WaitForMovementThen(Enemy, [this, WeakEnemy, WeakPlayer, Steps, Finish, Path, ActionType, HexStatus, HexDuration, HexValue]()
	{
		// [추가] 지나온 칸의 지형 (베렌드 가시덤불)
		if (TargetGridManager && WeakEnemy.IsValid()) TargetGridManager->ApplyEnteredTerrain(WeakEnemy.Get(), Path);

		ABWCharacter* E = WeakEnemy.Get();
		ABWCharacter* P = WeakPlayer.Get();
		if (!E || !P || E->CurrentHP <= 0 || P->CurrentHP <= 0) { if (E) E->AdvancePattern(); Finish(); return; }

		const FIntPoint Delta = P->OwnGridCoordinate - E->OwnGridCoordinate;
		const int32 Gap = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
		if (Gap < 1 || Gap > FMath::Max(1, E->ActionRange))
		{
			UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s moved %d and could not reach the player"), *E->EnemyDisplayName, Steps);
			E->AdvancePattern();
			Finish();
			return;
		}

		// 주술: 피해 없이 상태이상만
		if (ActionType == EBWEnemyActionType::Hex)
		{
			P->ApplyStatus(HexStatus, HexDuration, HexValue);
			E->AdvancePattern();
			Finish();
			return;
		}

		// 합 화면(적의 숫자 굴림) → 적이 찌르는 순간 피해 → 다음 패턴
		PlayEnemyDuel(E, [WeakEnemy, Finish]() { if (WeakEnemy.IsValid()) WeakEnemy->AdvancePattern(); Finish(); });
	});
}

void ABWPlayerController::ReflectThorns(ABWCharacter* Enemy, int32 DamageTaken)
{
	if (!PossessedBWCharacter || !Enemy || Enemy->CurrentHP <= 0 || Enemy->IsActorBeingDestroyed()) return;
	const int32 Reflected = FMath::FloorToInt(DamageTaken * PossessedBWCharacter->AugmentTotals.ThornFraction);
	if (Reflected <= 0) return;
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Thorn armor reflects %d to %s"), Reflected, *Enemy->EnemyDisplayName);
	Enemy->TakeHit(Reflected, false);
}

void ABWPlayerController::OpenAugmentPick(int32 Act)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	// 직업은 조종 중인 말, 없으면 배치할 말의 기본값
	// 직업은 캐릭터 선택에서 고른 것 (지도에서 부를 때는 조종 중인 말이 없다)
	EClassType PlayerClass = GI->SelectedClass;
	if (PossessedBWCharacter) PlayerClass = PossessedBWCharacter->CharacterClass;

	GI->BeginAugmentPick(Act, PlayerClass);
	if (GI->AugmentChoices.Num() == 0) return;

	TSubclassOf<UBWAugmentSelectWidget> WidgetClass = AugmentSelectWidgetClass ? AugmentSelectWidgetClass : TSubclassOf<UBWAugmentSelectWidget>(UBWAugmentSelectWidget::StaticClass());
	if (UBWAugmentSelectWidget* Widget = CreateWidget<UBWAugmentSelectWidget>(this, WidgetClass))
	{
		Widget->AddToViewport(600);
	}
}

bool ABWPlayerController::ComputeEnemyStrike(ABWCharacter* Enemy, FBWDuelSide& OutEnemySide, FBWDuelResult& OutResult)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!Enemy || !PlayerChar || !TargetGridManager || PlayerChar->CurrentHP <= 0) return false;

	// 내 쪽은 카드를 내지 않는다. 받는 피해 배율(취약, 겔레온 과부하)만 반영
	FBWDuelSide TargetSide;
	PlayerChar->ApplyStatusesToDuelSide(TargetSide);
	TargetSide = UBWDimensionRules::ApplyToPlayerSide(Battlefield, TargetSide);
	TargetSide.IncomingDamageMultiplier *= 1.0f + PlayerChar->AugmentTotals.Get(EBWAugStat::TakenPct);
	OutEnemySide = MakeFieldEnemySide(Enemy, Enemy->LastRushSteps);
	OutResult = UBWDuelRules::ResolveEnemyStrike(OutEnemySide, TargetSide, TargetGridManager->GetCombatRandom());
	return true;
}

bool ABWPlayerController::ResolveEnemyDuel(ABWCharacter* Enemy)
{
	FBWDuelSide EnemySide;
	FBWDuelResult Result;
	if (!ComputeEnemyStrike(Enemy, EnemySide, Result)) return false;
	ApplyEnemyStrikeOutcome(Enemy, EnemySide, Result, PlanDuelHit(PossessedBWCharacter, Result.FinalDamage, false));
	return true;
}

void ABWPlayerController::PlayEnemyDuel(ABWCharacter* Enemy, TFunction<void()> OnFinished)
{
	if (UBWAutoPlayer::IsActive() && Enemy && PossessedBWCharacter)
	{
		PlayEnemyDuelWithGuard(Enemy, UBWAutoPlayer::ChooseGuardMP(PossessedBWCharacter, MakeFieldEnemySide(Enemy, Enemy->LastRushSteps).SkillRange), MoveTemp(OnFinished));
		return;
	}
	// [추가] 매직실드(INT·FTH)이고 걸 MP가 있으면, 적이 굴리기 전에 가드에 걸 MP를 고른다
	if (Enemy && PossessedBWCharacter && PossessedBWCharacter->GetMaxGuardMP() > 0)
	{
		TWeakObjectPtr<ABWCharacter> WeakEnemy(Enemy);
		OpenGuardChoice(Enemy, [this, WeakEnemy, OnFinished](int32 ShieldMP)
		{
			if (!WeakEnemy.IsValid()) { if (OnFinished) OnFinished(); return; }
			PlayEnemyDuelWithGuard(WeakEnemy.Get(), ShieldMP, OnFinished);
		});
		return;
	}
	PlayEnemyDuelWithGuard(Enemy, 0, MoveTemp(OnFinished));
}

void ABWPlayerController::OpenGuardChoice(ABWCharacter* Enemy, TFunction<void(int32)> OnChosen)
{
	TSubclassOf<UBWGuardChoiceWidget> WidgetClass = GuardChoiceWidgetClass ? GuardChoiceWidgetClass : TSubclassOf<UBWGuardChoiceWidget>(UBWGuardChoiceWidget::StaticClass());
	UBWGuardChoiceWidget* Widget = CreateWidget<UBWGuardChoiceWidget>(this, WidgetClass);
	if (!Widget || !PossessedBWCharacter)
	{
		if (OnChosen) OnChosen(0);
		return;
	}

	// 보여 주는 적 공격 범위: 받는 피해 배율(취약, 겔레온)까지 반영
	FBWDuelSide TargetSide;
	PossessedBWCharacter->ApplyStatusesToDuelSide(TargetSide);
	TargetSide = UBWDimensionRules::ApplyToPlayerSide(Battlefield, TargetSide);
	TargetSide.IncomingDamageMultiplier *= 1.0f + PossessedBWCharacter->AugmentTotals.Get(EBWAugStat::TakenPct);
	const FBWDuelSide EnemySide = MakeFieldEnemySide(Enemy, Enemy->LastRushSteps);
	const float Mult = EnemySide.DamageMultiplier * TargetSide.IncomingDamageMultiplier;
	const FBWDiceRange Shown(FMath::FloorToInt(EnemySide.SkillRange.Min * Mult), FMath::FloorToInt(EnemySide.SkillRange.Max * Mult));
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();

	// 어둠의 성자는 마나가 아니라 신성력을 쓴다 (세계관: 둘은 공존하지 않는다). 규칙은 같고 이름만 다르다
	const bool bFaith = PossessedBWCharacter->CharacterClass == EClassType::FTH;
	if (bFaith) Widget->TitleText = TEXT("신성 가드");
	Widget->Setup(PossessedBWCharacter, Enemy->EnemyDisplayName, Action ? Action->ActionName : FString(TEXT("공격")), Shown, MoveTemp(OnChosen));
	Widget->AddToViewport(550);
	Prompt(bFaith ? FString::Printf(TEXT("%s의 공격 - 신성 가드에 걸 신성력을 고르세요"), *Enemy->EnemyDisplayName) : FString::Printf(TEXT("%s의 공격 - 마나 가드에 걸 MP를 고르세요"), *Enemy->EnemyDisplayName), 0.0f, EBWPromptStyle::Highlight);
}

void ABWPlayerController::PlayEnemyDuelWithGuard(ABWCharacter* Enemy, int32 ShieldMP, TFunction<void()> OnFinished)
{
	FBWDuelSide EnemySide;
	FBWDuelResult Result;
	if (!ComputeEnemyStrike(Enemy, EnemySide, Result))
	{
		if (OnFinished) OnFinished();
		return;
	}

	FBWDuelPresentation Show;
	Show.LeftName = TEXT("나");
	Show.LeftSkill = DescribePlayerDefense(PossessedBWCharacter);
	if (PossessedBWCharacter->Defense.Type == EDefenseType::MagicShield) Show.LeftSkill = ShieldMP > 0 ? FString::Printf(TEXT("마나 가드 (MP %d)"), ShieldMP) : FString(TEXT("가드 안 함"));
	Show.bLeftRolls = false;
	Show.RightName = Enemy->EnemyDisplayName;
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	Show.RightSkill = Action ? Action->ActionName : TEXT("공격");
	Show.RightRange = EnemySide.SkillRange;
	Show.RightRoll = Result.AttackerRoll;
	Show.Winner = 1;
	Show.Damage = Result.FinalDamage;
	Show.bCritical = Result.bCritical;
	// 적의 공격 수치 → 내 방어 수치 → 가운데에 최종 피해
	const FBWPlannedHit Hit = PlanDuelHit(PossessedBWCharacter, Result.FinalDamage, false, ShieldMP);
	FillHitPresentation(Show, Hit, true);

	TWeakObjectPtr<ABWCharacter> WeakEnemy(Enemy);
	ShowDuel(Show, [this, WeakEnemy, EnemySide, Result, Hit, OnFinished]()
	{
		ABWCharacter* E = WeakEnemy.Get();
		if (!E || !PossessedBWCharacter)
		{
			if (OnFinished) OnFinished();
			return;
		}
		PlayLungeThen(E, PossessedBWCharacter,
			[this, WeakEnemy, EnemySide, Result, Hit]() { if (WeakEnemy.IsValid()) ApplyEnemyStrikeOutcome(WeakEnemy.Get(), EnemySide, Result, Hit); },
			OnFinished);
	});
}

void ABWPlayerController::ApplyEnemyStrikeOutcome(ABWCharacter* Enemy, const FBWDuelSide& EnemySide, const FBWDuelResult& Result, const FBWPlannedHit& Hit)
{
	ABWCharacter* PlayerChar = PossessedBWCharacter;
	if (!Enemy || !PlayerChar || PlayerChar->CurrentHP <= 0) return;

	// 합 화면에서 보여 준 그대로 반영 (타이가 과부하의 방어도 무시도 PlanDuelHit에서 정함)
	const bool bIgnoreBlock = Hit.bIgnoreBlock;
	const FBWDefenseResult Defended = PlayerChar->ApplyPlannedHit(Hit);
	PlayImpactFeedback(Enemy, PlayerChar, Defended, Hit.HPLoss, Result.bCritical);
	ReflectThorns(Enemy, Defended.DamageAfterDefense);

	// [추가] 3층 기믹: 골드를 훔치고, 카드를 먹는다
	if (Hit.HPLoss > 0 && Enemy->GoldSteal > 0)
	{
		if (UBWGameInstance* GoldGI = Cast<UBWGameInstance>(GetGameInstance()))
		{
			const int32 Stolen = FMath::Min(GoldGI->CurrentGold, Enemy->GoldSteal);
			GoldGI->CurrentGold -= Stolen;
			Enemy->StolenGold += Stolen;
			if (Stolen > 0) PlayerChar->ShowPopup(FString::Printf(TEXT("골드 -%d"), Stolen), FLinearColor(1.0f, 0.85f, 0.4f));
		}
	}
	const FBWEnemyPattern* HitAction = Enemy->GetCurrentAction();
	if (Defended.DamageAfterDefense > 0 && HitAction && HitAction->bEatsCard && PlayerChar->DeckComponent && PlayerChar->DeckComponent->Hand.Num() > 0)
	{
		UDeckComponent* EatenDeck = PlayerChar->DeckComponent;
		const int32 Index = FMath::RandRange(0, EatenDeck->Hand.Num() - 1);
		PlayerChar->ShowPopup(FString::Printf(TEXT("%s를 먹혔다"), *EatenDeck->Hand[Index].CardName), FLinearColor(1.0f, 0.3f, 0.25f), 1.2f);
		EatenDeck->Hand.RemoveAt(Index);
		EatenDeck->OnHandChanged.Broadcast(EatenDeck->Hand);
		Enemy->EatenCards += 1;
		Enemy->CurrentAttackBuff = FMath::Min(Enemy->MaxAttackBuff, Enemy->CurrentAttackBuff + 1);
	}

	// [추가] 증강: 적의 공격을 받아치기
	const FBWAugmentTotals& Aug = PlayerChar->AugmentTotals;
	if (Enemy->CurrentHP > 0 && Aug.Has(EBWAugStat::MirrorHit)) Enemy->TakeFixedDamage(Defended.DamageAfterDefense, TEXT("거울 갑옷"));
	if (Hit.HPLoss == 0 && PlayerChar->CurrentHP > 0)
	{
		if (Enemy->CurrentHP > 0) Enemy->TakeFixedDamage(Aug.GetInt(EBWAugStat::ZeroHitFixed), TEXT("보복"));
		if (Aug.Has(EBWAugStat::ZeroHitDraw) && PlayerChar->DeckComponent) PlayerChar->DeckComponent->DrawCards(Aug.GetInt(EBWAugStat::ZeroHitDraw));
	}
	if (Defended.DamageAfterDefense > 0)
	{
		// 반격: 이번 적 턴 동안 맞을 때마다 그 적에게 고정 피해. 영창은 피해를 받으면 깨진다
		if (PlayerChar->Life.Counter > 0 && Enemy->CurrentHP > 0) Enemy->TakeFixedDamage(PlayerChar->Life.Counter, TEXT("반격"));
		if (PlayerChar->Life.ChantPending > 0)
		{
			PlayerChar->Life.ChantPending = 0;
			PlayerChar->ShowPopup(TEXT("영창이 깨졌다"), FLinearColor(0.6f, 0.6f, 0.9f));
		}
	}

	if (EnemySide.AppliedStatus != EStatusEffect::None && PlayerChar->CurrentHP > 0)
	{
		PlayerChar->ApplyStatus(EnemySide.AppliedStatus, EnemySide.StatusDuration, EnemySide.StatusValue);
	}
	if (EnemySide.bPushesTarget) ResolvePushAfterDuel(Enemy, PlayerChar, false);

	// 연출: 적 행동 이펙트와 소리를 내 칸에서
	if (const FBWEnemyPattern* Action = Enemy->GetCurrentAction())
	{
		const FVector FxLoc = PlayerChar->GetActorLocation() + FVector(0, 0, 40);
		BWSpawnFX(GetWorld(), Action->ActionNiagara, Action->ActionEffect, FxLoc);
		if (Action->ActionSound.LoadSynchronous()) UGameplayStatics::PlaySoundAtLocation(GetWorld(), Action->ActionSound.Get(), FxLoc);
	}

	const FString Summary = FString::Printf(TEXT("[적 격돌] %s 공격 %d → 방어 후 %d%s%s%s"), *Enemy->EnemyDisplayName, Result.FinalDamage, Defended.DamageAfterDefense,
		Result.bCritical ? TEXT(" (치명타)") : TEXT(""), bIgnoreBlock ? TEXT(" (방어도 무시)") : TEXT(""),
		Defended.MPSpent > 0 ? *FString::Printf(TEXT(" · 마나 가드 MP %d로 %d 막음"), Defended.MPSpent, Defended.ShieldBlocked) : TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("[BWCombat] %s"), *Summary);
	Prompt(Summary, 3.0f, EBWPromptStyle::Result);

	OnEnemyStrike.Broadcast(Result, Enemy);
}

void ABWPlayerController::BeginRound()
{
	// 4-1 턴 시작: 손패를 채우고, 내 속도와 모든 적의 속도를 굴려 공개한다
	PendingDuelTarget = nullptr;

	// 11-3 파트로바스: 턴마다 마력선 두 줄이 새로 지나간다
	if (HasSet(EBWAugmentSet::Patrovas, 3)) SetEventHit(EBWAugmentSet::Patrovas, NearestEnemy(), 3, TEXT("마력 폭주"));
	if (Battlefield.Dimension == EDimensionType::Patrovas && TargetGridManager)
	{
		UBWDimensionRules::RollManaRows(Battlefield, TargetGridManager->Width, TargetGridManager->GetCombatRandom());
		TargetGridManager->ShowManaLines(Battlefield.ManaRows);
	}

	if (PossessedBWCharacter)
	{
		if (PossessedBWCharacter->DeckComponent) PossessedBWCharacter->DeckComponent->StartTurn();
		PossessedBWCharacter->BeginTurnResources();
		MiraclesThisTurn.Reset();
		PossessedBWCharacter->RollSpeed();

		// [추가] 증강: 첫 턴 이동, 맞닿은 적이 없으면 방어도
		if (PossessedBWCharacter->CombatTurnNumber == 1) PossessedBWCharacter->RemainingMove += PossessedBWCharacter->AugmentTotals.GetInt(EBWAugStat::FirstTurnMove);
		if (GetAdjacentEnemies().Num() == 0) PossessedBWCharacter->GainBlock(PossessedBWCharacter->AugmentTotals.GetInt(EBWAugStat::NoContactBlock), TEXT("거리두기"));

		// 기절: 이번 턴에 이동하지 못한다 (속도는 공개만 하고 이동 칸은 0)
		if (PossessedBWCharacter->IsStunned()) PossessedBWCharacter->RemainingMove = 0;
		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Player speed roll: %d (%d~%d)%s"), PossessedBWCharacter->RolledSpeed, PossessedBWCharacter->SpeedDice.Min, PossessedBWCharacter->SpeedDice.Max,
			PossessedBWCharacter->IsStunned() ? TEXT(" - stunned, cannot move") : TEXT(""));
		ShowTurnBanner(TEXT("당신의 턴"), FString::Printf(TEXT("%d턴 · 속도 %d%s"), PossessedBWCharacter->CombatTurnNumber, PossessedBWCharacter->RolledSpeed,
			PossessedBWCharacter->IsStunned() ? TEXT(" · 기절") : TEXT("")), FLinearColor(0.45f, 0.85f, 1.0f));

		Prompt(PossessedBWCharacter->IsStunned()
			? TEXT("당신의 턴 - 기절해서 움직일 수 없습니다")
			: FString::Printf(TEXT("당신의 턴 - 속도 %d. 적을 누르면 다가가고, 맞닿은 적을 누르면 기본 공격 (공격카드로 강화)"), PossessedBWCharacter->RolledSpeed), 0.0f, EBWPromptStyle::Highlight);
	}
	bEnemyPhase = false;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy && Char->CurrentHP > 0)
		{
			Char->RollSpeed();
			UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Enemy %s speed roll: %d (%d~%d)"), *Char->EnemyDisplayName, Char->RolledSpeed, Char->SpeedDice.Min, Char->SpeedDice.Max);
			Char->UpdateEnemyIntent();
			Char->VisualizeEnemyIntent();
		}
	}
}

bool ABWPlayerController::PlaceCharacterAndStartCombat(FIntPoint Coord)
{
	if (!CharacterClassToSpawn || !TargetGridManager || PossessedBWCharacter) return false;
	if (!TargetGridManager->TryPlaceCharacter(Coord, CharacterClassToSpawn)) return false;

	DestroyPlacementPreview();
	if (BattleHUDWidget) BattleHUDWidget->HideTooltip(); // 배치 안내 툴팁이 남지 않게

	// [수정] 마지막 액터가 아니라 방금 배치한 칸의 말을 조종한다 (적이 먼저 생성되어 있어도 안전)
	PossessedBWCharacter = TargetGridManager->GetCharacterAtCoordinate(Coord);
	if (!PossessedBWCharacter) return false;

	// 캐릭터 선택에서 고른 직업·능력치·이어지는 HP (덱도 그 직업으로 다시 불러온다)
	PossessedBWCharacter->ApplyRunState();

	bIsSkillTargetingMode = true;
	Possess(PossessedBWCharacter);

	// [수정] Possess는 시점을 말로 옮기므로, 전투 내내 레벨의 전투 카메라를 유지한다
	if (BattleCamera) SetViewTarget(BattleCamera);
	if (PossessedBWCharacter && PossessedBWCharacter->CombatTurnNumber > 0)
	{
		TriggerMiracle(EBWAugmentSet::Patrovas, 9);
		TriggerMiracle(EBWAugmentSet::Lirin, 4);
		if (HasSet(EBWAugmentSet::Soltheris, 4)) { PossessedBWCharacter->Heal(10); TriggerMiracle(EBWAugmentSet::Soltheris, 10); }
	}
	if (HandWidget) { HandWidget->LinkToPlayerDeck(PossessedBWCharacter); HandWidget->RefreshHand(PossessedBWCharacter->DeckComponent->GetCurrentHand()); }
	if (UDeckComponent* PlayerDeck = PossessedBWCharacter->DeckComponent)
	{
		PlayerDeck->OnCardBurned.RemoveDynamic(this, &ABWPlayerController::HandleCardBurned);
		PlayerDeck->OnCardBurned.AddDynamic(this, &ABWPlayerController::HandleCardBurned);
	}
	if (BattleHUDWidget) BattleHUDWidget->InitializeBattleHUD(PossessedBWCharacter);

	// 전투 시작 자원 (INT·FTH MP 6, HP 직업 방어도)
	PossessedBWCharacter->BeginCombatResources();
	PreloadCombatAssets();

	// 가진 증강 반영 (10장)
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
	{
		PossessedBWCharacter->ApplyAugments(UBWAugmentRules::Aggregate(GI->GetOwnedAugmentData()));

		// [추가] 켜진 세트를 알린다 (기적 연출은 아직 글자로만)
		TArray<FString> Sets;
		const TArray<int32>& Counts = PossessedBWCharacter->AugmentTotals.SetCounts;
		for (int32 i = 1; i < Counts.Num(); ++i)
		{
			if (Counts[i] >= 2) Sets.Add(FString::Printf(TEXT("%s (%d)%s"), *UBWAugmentRules::GetSetName((EBWAugmentSet)i), Counts[i], Counts[i] >= 4 ? TEXT(" 기적") : TEXT("")));
		}
		if (Sets.Num() > 0) PossessedBWCharacter->ShowPopup(TEXT("세트: ") + FString::Join(Sets, TEXT(" · ")), FLinearColor(1.0f, 0.85f, 0.4f), 1.2f);
		for (int32 i = 1; i < Counts.Num(); ++i)
		{
			if (Counts[i] >= 4) Prompt(FString::Printf(TEXT("기적 - %s: %s"), *UBWAugmentRules::GetSetName((EBWAugmentSet)i), *UBWAugmentRules::DescribeSetTier((EBWAugmentSet)i, 4, PossessedBWCharacter->CharacterClass)), 5.0f, EBWPromptStyle::Highlight);
		}
	}

	// 이번 전투의 국가 기믹 (현재 차원, 과부하 여부)
	InitBattlefieldFromGameInstance();

	// 첫 턴 시작 (손패, 속도 굴림, 적 의도 표시). 겔레온은 카드 종류를 고른 뒤에 시작한다 (11-4)
	BeginRound();

	OnCombatStarted.Broadcast();
	return true;
}

bool ABWPlayerController::TryMovePlayerTo(FIntPoint Target)
{
	if (!PossessedBWCharacter || !TargetGridManager) return false;

	TArray<FIntPoint> Path = TargetGridManager->FindMovePath(PossessedBWCharacter->OwnGridCoordinate, Target, PossessedBWCharacter->RemainingMove, false);
	if (Path.Num() == 0) return false;

	MovePlayerAlongPath(Path);
	return true;
}

void ABWPlayerController::MovePlayerAlongPath(const TArray<FIntPoint>& Path)
{
	if (!PossessedBWCharacter || !TargetGridManager || Path.Num() == 0) return;

	// 한 칸씩 부드럽게 이동 (BWCharacter::Tick이 큐를 소비)
	for (const FIntPoint& Step : Path)
	{
		if (AGridTile* StepTile = TargetGridManager->GetTileAtCoordinate(Step))
		{
			PossessedBWCharacter->AddMovementStep(StepTile->GetActorLocation() + FVector(0, 0, 20));
		}
	}
	PossessedBWCharacter->OwnGridCoordinate = Path.Last();
	PossessedBWCharacter->RemainingMove = FMath::Max(0, PossessedBWCharacter->RemainingMove - Path.Num());
	PossessedBWCharacter->Life.bMovedThisTurn = true;
	PossessedBWCharacter->AugTrack.MovedTiles += Path.Num();
	CancelSelection();

	// 이동 경로는 적과 맞닿는 칸에서 끝난다. [수정] 사용자 결정: 접촉만으로는 남은 이동이 사라지지 않고,
	// 합을 걸었을 때 이동을 모두 쓴다 (기획서 4-2와 다름). 남은 이동이 있으면 이동 모드를 이어 간다
	ABWCharacter* Opponent = TargetGridManager->FindContactOpponent(Path.Last(), false);

	// 걸어가는 연출이 끝난 뒤에 처리 (그동안은 입력을 막는다)
	bActionInProgress = true;
	TWeakObjectPtr<ABWCharacter> WeakOpponent(Opponent);
	WaitForMovementThen(PossessedBWCharacter, [this, WeakOpponent, Path]()
	{
		bActionInProgress = false;
		if (!PossessedBWCharacter) return;

		// [추가] 지나온 칸의 지형 (베렌드 가시덤불)
		if (TargetGridManager && TargetGridManager->ApplyEnteredTerrain(PossessedBWCharacter, Path) > 0 && PossessedBWCharacter->CurrentHP <= 0) return;
		if (Path.Num() > 0 && TargetGridManager && TargetGridManager->GetTerrainAt(Path.Last()) == EBWTerrainType::Thicket && PossessedBWCharacter->RemainingMove > 0)
		{
			PossessedBWCharacter->ShowPopup(TEXT("깊은 수풀 - 이동 멈춤"), FLinearColor(0.6f, 1.0f, 0.65f));
		}
		// 이동: 지나친 적 베기, 기적 천 개의 그림자
		const int32 Slash = PossessedBWCharacter->AugmentTotals.GetInt(EBWAugStat::MoveSlash);
		if (Slash > 0)
		{
			for (ABWCharacter* Other : GetEnemiesInReach(99))
			{
				const bool bPassed = Path.ContainsByPredicate([Other](const FIntPoint& Cell)
				{
					return FMath::Abs(Cell.X - Other->OwnGridCoordinate.X) + FMath::Abs(Cell.Y - Other->OwnGridCoordinate.Y) <= 1;
				});
				if (bPassed) SetEventHit(EBWAugmentSet::None, Other, Slash, TEXT("스치는 칼날"));
			}
		}
		if (PossessedBWCharacter->AugTrack.MovedTiles >= 3) TriggerMiracle(EBWAugmentSet::Taiga, PossessedBWCharacter->AugTrack.MovedTiles * 2);
		const int32 Left = PossessedBWCharacter->RemainingMove;
		ABWCharacter* Opp = WeakOpponent.Get();

		// 남은 이동이 있으면 이동 모드 유지 (바닥을 계속 누를 수 있다)
		if (Left > 0)
		{
			bIsMovementMode = true;
			if (TargetGridManager) TargetGridManager->DrawMovementPreview(PossessedBWCharacter->OwnGridCoordinate, Left);
		}
		else if (TargetGridManager)
		{
			TargetGridManager->ClearAllRanges();
		}

		if (!Opp)
		{
			Prompt(Left > 0 ? FString::Printf(TEXT("남은 이동 %d칸 - 계속 이동하거나 적을 누르세요"), Left) : TEXT("이동을 다 썼습니다"), Left > 0 ? 0.0f : 3.0f);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[BWCombat] Contact with %s at %s"), *Opp->EnemyDisplayName, *PossessedBWCharacter->OwnGridCoordinate.ToString());
		OnContactWithEnemy.Broadcast(Opp);

		UDeckComponent* Deck = PossessedBWCharacter->DeckComponent;
		if (Deck && Deck->CanOpenDuel() && !PossessedBWCharacter->IsStunned())
		{
			// 적을 누르면(또는 Q) 기본 공격, 공격카드를 쓰면(클릭, 위로 끌기, 숫자 키) 강화한 공격으로 이 적과 합
			PendingDuelTarget = Opp;
			Prompt(HasUsableAttackCardInHand()
				? FString::Printf(TEXT("%s와 맞닿았습니다! 적을 누르면 기본 공격, 공격카드를 쓰면 강화 공격 (남은 이동 %d칸)"), *Opp->EnemyDisplayName, Left)
				: FString::Printf(TEXT("%s와 맞닿았습니다! 적을 누르거나 Q로 기본 공격 (남은 이동 %d칸)"), *Opp->EnemyDisplayName, Left), 0.0f, EBWPromptStyle::Highlight);
		}
		else Prompt(TEXT("적과 맞닿았지만 이번 턴에는 격돌을 걸 수 없습니다"), 3.0f, EBWPromptStyle::Warning);
	});
}

AGridTile* ABWPlayerController::GetTileUnderCursor() { return CurrentTileUnderCursor; }
