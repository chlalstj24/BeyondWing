#include "BeyondWing/UI/Public/BWDuelWidget.h"
#include "BeyondWing/UI/Public/BWSkin.h"
#include "Blueprint/WidgetTree.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace BWDuelUI
{
	const FLinearColor Gold(1.0f, 0.82f, 0.25f);
	const FLinearColor Dim(0.55f, 0.55f, 0.6f);
	const FLinearColor White(1.0f, 1.0f, 1.0f);
	const FLinearColor Red(1.0f, 0.35f, 0.3f);
	const FLinearColor Blue(0.45f, 0.75f, 1.0f);
}

void UBWDuelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!TXT_LeftRoll && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWDuelWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// 화면 가운데를 가로지르는 옅은 어둠 + 그 위의 석판 (큰 판이라 모서리 장식 석판)
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Shade"));
	Shade->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	if (UCanvasPanelSlot* ShadeSlot = Root->AddChildToCanvas(Shade))
	{
		ShadeSlot->SetAnchors(FAnchors(0.0f, 0.5f, 1.0f, 0.5f));
		ShadeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ShadeSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 300.0f));
	}
	UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Band"));
	if (!BWSkin::StylePanel(Band)) Band->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f));
	Band->SetPadding(FMargin(46.0f, 28.0f, 46.0f, 30.0f));
	if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
	{
		BandSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		BandSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BandSlot->SetAutoSize(true);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Band->SetContent(Column);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row)) RowSlot->SetHorizontalAlignment(HAlign_Center);

	auto MakeText = [this](const TCHAR* Name, int32 Size, ETextJustify::Type Justify) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetJustification(Justify);
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
		return Text;
	};

	auto MakeSide = [&](const TCHAR* Prefix, UTextBlock*& OutName, UTextBlock*& OutSkill, UTextBlock*& OutRoll)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sBox"), Prefix));
		Box->SetWidthOverride(340.0f);
		// 양쪽 칸: 장식 없이 석판색 + 은빛 테두리
		UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sCell"), Prefix));
		Cell->SetBrush(BWSkin::Plain(FLinearColor(0.1f, 0.12f, 0.15f, 0.92f), FLinearColor(0.68f, 0.7f, 0.74f, 0.6f), 1.5f));
		Cell->SetPadding(FMargin(12.0f, 10.0f));
		Box->SetContent(Cell);
		UVerticalBox* SideColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("%sColumn"), Prefix));
		Cell->SetContent(SideColumn);
		OutName = MakeText(*FString::Printf(TEXT("TXT_%sName"), Prefix), 20, ETextJustify::Center);
		OutSkill = MakeText(*FString::Printf(TEXT("TXT_%sSkill"), Prefix), 15, ETextJustify::Center);
		OutRoll = MakeText(*FString::Printf(TEXT("TXT_%sRoll"), Prefix), 64, ETextJustify::Center);
		SideColumn->AddChildToVerticalBox(OutName);
		SideColumn->AddChildToVerticalBox(OutSkill);
		if (UVerticalBoxSlot* RollSlot = SideColumn->AddChildToVerticalBox(OutRoll)) RollSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		return Box;
	};

	Row->AddChildToHorizontalBox(MakeSide(TEXT("Left"), TXT_LeftName, TXT_LeftSkill, TXT_LeftRoll));
	// 가운데: 처음엔 VS, 마지막에 최종 피해가 같은 자리로 들어온다
	USizeBox* CenterBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CenterBox"));
	CenterBox->SetWidthOverride(220.0f);
	UOverlay* Center = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Center"));
	CenterBox->SetContent(Center);
	TXT_Versus = MakeText(TEXT("TXT_Versus"), 34, ETextJustify::Center);
	TXT_Versus->SetText(FText::FromString(TEXT("VS")));
	TXT_Versus->SetColorAndOpacity(BWDuelUI::Gold);
	TXT_Final = MakeText(TEXT("TXT_Final"), 84, ETextJustify::Center);
	TXT_Final->SetRenderOpacity(0.0f);
	for (UTextBlock* Text : { TXT_Versus, TXT_Final })
	{
		if (UOverlaySlot* CenterSlot = Center->AddChildToOverlay(Text))
		{
			CenterSlot->SetHorizontalAlignment(HAlign_Center);
			CenterSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	if (UHorizontalBoxSlot* CenterSlot = Row->AddChildToHorizontalBox(CenterBox)) CenterSlot->SetVerticalAlignment(VAlign_Center);
	Row->AddChildToHorizontalBox(MakeSide(TEXT("Right"), TXT_RightName, TXT_RightSkill, TXT_RightRoll));

	TXT_Result = MakeText(TEXT("TXT_Result"), 24, ETextJustify::Center);
	TXT_Result->SetWrapTextAt(900.0f); // 판 폭(양쪽 칸 + 가운데) 안에서 줄바꿈
	if (UVerticalBoxSlot* ResultSlot = Column->AddChildToVerticalBox(TXT_Result))
	{
		ResultSlot->SetHorizontalAlignment(HAlign_Center);
		ResultSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
	TXT_FinalNote = MakeText(TEXT("TXT_FinalNote"), 18, ETextJustify::Center);
	TXT_FinalNote->SetWrapTextAt(900.0f);
	TXT_FinalNote->SetColorAndOpacity(BWDuelUI::Dim);
	if (UVerticalBoxSlot* NoteSlot = Column->AddChildToVerticalBox(TXT_FinalNote))
	{
		NoteSlot->SetHorizontalAlignment(HAlign_Center);
		NoteSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}
}

void UBWDuelWidget::PlayDuel(const FBWDuelPresentation& Data, TFunction<void()> OnFinished)
{
	Current = Data;
	FinishedCallback = MoveTemp(OnFinished);

	if (TXT_LeftName) TXT_LeftName->SetText(FText::FromString(Data.LeftName));
	if (TXT_RightName) TXT_RightName->SetText(FText::FromString(Data.RightName));
	if (TXT_LeftSkill) TXT_LeftSkill->SetText(FText::FromString(Data.bLeftRolls
		? FString::Printf(TEXT("%s  [%d~%d]"), *Data.LeftSkill, Data.LeftRange.Min, Data.LeftRange.Max) : Data.LeftSkill));
	if (TXT_RightSkill) TXT_RightSkill->SetText(FText::FromString(FString::Printf(TEXT("%s  [%d~%d]"), *Data.RightSkill, Data.RightRange.Min, Data.RightRange.Max)));
	if (TXT_Result)
	{
		TXT_Result->SetText(FText::FromString(Data.bLeftRolls ? TEXT("격돌!") : TEXT("적의 공격!")));
		TXT_Result->SetColorAndOpacity(BWDuelUI::White);
	}
	if (TXT_FinalNote) TXT_FinalNote->SetText(FText::FromString(TEXT(" "))); // 빈 줄도 높이를 잡아 판 크기가 변하지 않게
	if (TXT_Versus) TXT_Versus->SetRenderOpacity(1.0f);
	if (TXT_Final) TXT_Final->SetRenderOpacity(0.0f);

	if (!IsInViewport()) AddToViewport(500); // 손패 카드보다 위
	EnterPhase(0);
}

FString UBWDuelWidget::RollText(bool bRolls, const FBWDiceRange& Range, int32 FinalRoll, bool bFinal) const
{
	if (!bRolls) return TEXT("방어");
	if (bFinal) return FString::FromInt(FinalRoll);
	// 굴러가는 중: 범위 안의 숫자를 빠르게 바꿔 보여 준다 (연출용이라 전투 난수와는 무관)
	return FString::FromInt(FMath::RandRange(Range.Min, FMath::Max(Range.Min, Range.Max)));
}

void UBWDuelWidget::FlickerRolls()
{
	// 범위 안에서 직전과 다른 숫자를 골라야 느려질 때도 멈춘 것처럼 보이지 않는다 (연출용이라 전투 난수와는 무관)
	auto Pick = [](const FBWDiceRange& Range, int32& Last) -> int32
	{
		const int32 Max = FMath::Max(Range.Min, Range.Max);
		int32 Value = FMath::RandRange(Range.Min, Max);
		if (Max > Range.Min && Value == Last) Value = Value == Max ? Range.Min : Value + 1;
		Last = Value;
		return Value;
	};
	if (TXT_LeftRoll && Current.bLeftRolls) TXT_LeftRoll->SetText(FText::AsNumber(Pick(Current.LeftRange, LastFlickerLeft)));
	if (TXT_RightRoll) TXT_RightRoll->SetText(FText::AsNumber(Pick(Current.RightRange, LastFlickerRight)));
}

void UBWDuelWidget::UpdateLandPop()
{
	// 숫자가 멈추는 순간 크게 찍혔다가 제자리 크기로 돌아온다
	const float Alpha = LandPopTime > 0.0f ? FMath::Clamp(PhaseTime / LandPopTime, 0.0f, 1.0f) : 1.0f;
	const float Ease = 1.0f - FMath::Pow(1.0f - Alpha, 2.0f);
	const float Scale = FMath::Lerp(LandPopScale, 1.0f, Ease);
	for (UTextBlock* Text : { TXT_LeftRoll, TXT_RightRoll })
	{
		if (Text) Text->SetRenderScale(FVector2D(Scale, Scale));
	}
}

FString UBWDuelWidget::MakeFinalNote() const
{
	// 공격 수치 → 방어 → 방어도 순서의 계산식
	FString Note = FString::Printf(TEXT("공격 %d"), Current.Damage);
	if (Current.bEvaded) Note += TEXT("  →  회피!");
	else if (Current.DefenseReduced > 0) Note += FString::Printf(TEXT("  −  %s %d"), Current.bPlayerIsHit ? TEXT("내 방어") : TEXT("방어"), Current.DefenseReduced);
	if (Current.BlockAbsorbed > 0) Note += FString::Printf(TEXT("   (방어도가 %d 막음 → HP -%d)"), Current.BlockAbsorbed, Current.HPLoss);
	return Note;
}

namespace BWDuelInput
{
	static constexpr float ResultShowTime = 1.5f; // 결과 한 장면이 자동으로 넘어가는 시간
	static constexpr float MinInputGap = 0.5f;    // 결과가 나온 뒤 이 시간 안의 입력은 무시 (중복 입력 방지)
}

bool UBWDuelWidget::HandleAdvanceInput()
{
	if (CurrentPhase < 0) return false;
	if (CurrentPhase < 2) EnterPhase(2);
	// 다음 틱에서 넘어가게 한다 (입력 처리 도중에 위젯을 닫지 않는다)
	else if (PhaseTime >= BWDuelInput::MinInputGap) PhaseTime = BWDuelInput::ResultShowTime;
	return true;
}

void UBWDuelWidget::EnterPhase(int32 NewPhase)
{
	CurrentPhase = NewPhase;
	PhaseTime = 0.0f;
	FlickerTime = 0.0f;

	auto SetRollColors = [this](const FLinearColor& Left, const FLinearColor& Right)
	{
		if (TXT_LeftRoll) TXT_LeftRoll->SetColorAndOpacity(Left);
		if (TXT_RightRoll) TXT_RightRoll->SetColorAndOpacity(Right);
	};

	if (CurrentPhase == 0)
	{
		if (TXT_LeftRoll) TXT_LeftRoll->SetText(FText::FromString(Current.bLeftRolls ? TEXT("?") : TEXT("방어")));
		if (TXT_RightRoll) TXT_RightRoll->SetText(FText::FromString(TEXT("?")));
		SetRollColors(Current.bLeftRolls ? BWDuelUI::White : BWDuelUI::Blue, BWDuelUI::White);
	}
	else if (CurrentPhase == 2)
	{
		// 양쪽 수치 공개: 내가 굴린 합이면 두 굴림, 적의 공격이면 내 방어 수치와 적의 공격 수치
		if (TXT_LeftRoll) TXT_LeftRoll->SetText(FText::FromString(Current.bLeftRolls ? FString::FromInt(Current.LeftRoll) : Current.DefenseText));
		if (TXT_RightRoll) TXT_RightRoll->SetText(FText::FromString(FString::FromInt(Current.RightRoll)));
		UpdateLandPop();

		FString Result;
		if (Current.Winner < 0)
		{
			SetRollColors(BWDuelUI::Dim, BWDuelUI::Dim);
			Result = Current.bGuardHeld ? TEXT("막혔다 - 피해 없음") : TEXT("3연속 동점 - 피해 없음");
		}
		else if (!Current.bLeftRolls)
		{
			SetRollColors(BWDuelUI::Blue, BWDuelUI::Red);
			Result = FString::Printf(TEXT("공격 %d"), Current.Damage);
		}
		else
		{
			const bool bPlayerWon = Current.Winner == 0;
			SetRollColors(bPlayerWon ? BWDuelUI::Gold : BWDuelUI::Dim, bPlayerWon ? BWDuelUI::Dim : BWDuelUI::Red);
			Result = bPlayerWon ? FString::Printf(TEXT("승리! 공격 %d"), Current.Damage) : FString::Printf(TEXT("패배... 적의 공격 %d"), Current.Damage);
		}
		if (Current.bCritical) Result += TEXT("  치명타!");
		if (Current.TieRerolls > 0 && Current.Winner >= 0) Result += FString::Printf(TEXT("  (동점 %d번 다시 굴림)"), Current.TieRerolls);
		if (Current.bRerolled) Result += TEXT("  (재굴림)");
		if (!Current.Extra.IsEmpty()) Result += FString::Printf(TEXT("  %s"), *Current.Extra);
		if (TXT_Result)
		{
			TXT_Result->SetText(FText::FromString(Result));
			TXT_Result->SetColorAndOpacity(Current.Winner == 0 ? BWDuelUI::Gold : (Current.Winner == 1 ? BWDuelUI::Red : BWDuelUI::White));
		}
	}
	else if (CurrentPhase == 3)
	{
		// 최종 피해가 가운데로 스윽 들어온다
		if (TXT_Final)
		{
			TXT_Final->SetText(FText::FromString(Current.bEvaded ? TEXT("0") : FString::FromInt(Current.FinalDamage)));
			TXT_Final->SetColorAndOpacity(Current.bPlayerIsHit ? BWDuelUI::Red : BWDuelUI::Gold);
		}
		if (TXT_FinalNote) TXT_FinalNote->SetText(FText::FromString(MakeFinalNote()));
		UpdateFinalSlide();
	}
	K2_OnDuelPhase(CurrentPhase, Current);
}

void UBWDuelWidget::UpdateFinalSlide()
{
	// 아래에서 떠오르며 커졌다가 제자리 크기로 (ease-out)
	const float Alpha = FinalSlideTime > 0.0f ? FMath::Clamp(PhaseTime / FinalSlideTime, 0.0f, 1.0f) : 1.0f;
	const float Ease = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
	if (TXT_Versus) TXT_Versus->SetRenderOpacity(1.0f - Ease);
	if (TXT_Final)
	{
		TXT_Final->SetRenderOpacity(Ease);
		FWidgetTransform Transform;
		Transform.Translation = FVector2D(0.0f, FMath::Lerp(60.0f, 0.0f, Ease));
		const float Scale = FMath::Lerp(1.6f, 1.0f, Ease);
		Transform.Scale = FVector2D(Scale, Scale);
		TXT_Final->SetRenderTransform(Transform);
	}
	if (TXT_FinalNote) TXT_FinalNote->SetRenderOpacity(Ease);
}

void UBWDuelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CurrentPhase < 0) return;

	auto Finish = [this]()
	{
		CurrentPhase = -1;
		RemoveFromParent();
		TFunction<void()> Callback = MoveTemp(FinishedCallback);
		FinishedCallback = nullptr;
		if (Callback) Callback();
	};

	PhaseTime += InDeltaTime;
	switch (CurrentPhase)
	{
	case 0:
		if (PhaseTime >= IntroTime) EnterPhase(1);
		break;
	case 1:
		{
			// 진행도의 제곱으로 간격을 늘려 끝으로 갈수록 확 느려진다
			const float Progress = RollTime > 0.0f ? FMath::Clamp(PhaseTime / RollTime, 0.0f, 1.0f) : 1.0f;
			const float Interval = FMath::Lerp(FlickerIntervalStart, FlickerIntervalEnd, Progress * Progress);
			FlickerTime += InDeltaTime;
			if (FlickerTime >= Interval)
			{
				FlickerTime = 0.0f;
				FlickerRolls();
			}
		}
		if (PhaseTime >= RollTime) EnterPhase(2);
		break;
	case 2:
		UpdateLandPop();
		if (PhaseTime >= BWDuelInput::ResultShowTime)
		{
			// 동점이면 피해가 없으니 바로 닫는다
			if (Current.Winner < 0) Finish();
			else EnterPhase(3);
		}
		break;
	case 3:
		UpdateFinalSlide();
		if (PhaseTime >= BWDuelInput::ResultShowTime) Finish();
		break;
	}
}

// 화면 확인용: bw.Duel [enemy] — 예시 격돌을 한 번 재생한다 (전투 결과와 무관)
static FAutoConsoleCommandWithWorldAndArgs GBWDuelPreviewCommand(TEXT("bw.Duel"), TEXT("화면 확인용 격돌 연출 (인자 enemy: 적의 공격)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		UBWDuelWidget* Widget = PC ? CreateWidget<UBWDuelWidget>(PC, UBWDuelWidget::StaticClass()) : nullptr;
		if (!Widget) return;
		const bool bEnemy = Args.Num() > 0 && Args[0] == TEXT("enemy");
		FBWDuelPresentation Data;
		Data.LeftName = TEXT("매직 캐스터");
		Data.LeftSkill = bEnemy ? TEXT("마나 가드") : TEXT("불꽃 화살");
		Data.LeftRange = { 2, 8 };
		Data.LeftRoll = 7;
		Data.bLeftRolls = !bEnemy;
		Data.RightName = TEXT("석상 파수꾼");
		Data.RightSkill = TEXT("내려찍기");
		Data.RightRange = { 1, 6 };
		Data.RightRoll = 4;
		Data.Winner = bEnemy ? 1 : 0;
		Data.Damage = bEnemy ? 4 : 7;
		Data.bCritical = !bEnemy;
		Data.bPlayerIsHit = bEnemy;
		Data.DefenseText = TEXT("-2");
		Data.DefenseReduced = 2;
		Data.FinalDamage = bEnemy ? 2 : 7;
		Data.BlockAbsorbed = bEnemy ? 1 : 0;
		Data.HPLoss = bEnemy ? 1 : 0;
		Widget->PlayDuel(Data, nullptr);
	}));
