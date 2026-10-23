#include "BeyondWing/UI/Public/BWDuelWidget.h"
#include "Blueprint/WidgetTree.h"
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

	// 화면 가운데를 가로지르는 어두운 띠
	UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Band"));
	Band->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f));
	Band->SetPadding(FMargin(40.0f, 18.0f));
	if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
	{
		BandSlot->SetAnchors(FAnchors(0.0f, 0.5f, 1.0f, 0.5f));
		BandSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BandSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 230.0f));
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
		UVerticalBox* SideColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("%sColumn"), Prefix));
		Box->SetContent(SideColumn);
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
	if (UVerticalBoxSlot* ResultSlot = Column->AddChildToVerticalBox(TXT_Result))
	{
		ResultSlot->SetHorizontalAlignment(HAlign_Center);
		ResultSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
	TXT_FinalNote = MakeText(TEXT("TXT_FinalNote"), 18, ETextJustify::Center);
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
		TXT_Result->SetText(FText::FromString(Data.bLeftRolls ? TEXT("합!") : TEXT("적의 공격!")));
		TXT_Result->SetColorAndOpacity(BWDuelUI::White);
	}
	if (TXT_FinalNote) TXT_FinalNote->SetText(FText::GetEmpty());
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
			Result = TEXT("3연속 동점 - 피해 없음");
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
		if (PhaseTime >= HoldTime)
		{
			// 동점이면 피해가 없으니 바로 닫는다
			if (Current.Winner < 0) Finish();
			else EnterPhase(3);
		}
		break;
	case 3:
		UpdateFinalSlide();
		if (PhaseTime >= FinalSlideTime + FinalHoldTime) Finish();
		break;
	}
}
