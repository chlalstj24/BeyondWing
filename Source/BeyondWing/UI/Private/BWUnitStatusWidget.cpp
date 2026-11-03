#include "BeyondWing/UI/Public/BWUnitStatusWidget.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/BWStatusRules.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBWUnitStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!PB_HP && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWUnitStatusWidget::BuildDefaultLayout()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	auto MakeText = [this](const TCHAR* Name, int32 Size) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		return Text;
	};

	// 격돌 예상 판 (공격을 고르는 동안만 보인다)
	BOX_Forecast = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BOX_Forecast"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.035f, 0.9f));
		Brush.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.8f, 0.35f, 0.8f));
		Brush.OutlineSettings.Width = 1.5f;
		BOX_Forecast->SetBrush(Brush);
	}
	BOX_Forecast->SetPadding(FMargin(10.0f, 5.0f, 10.0f, 6.0f));
	UVerticalBox* ForecastColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ForecastColumn"));
	BOX_Forecast->SetContent(ForecastColumn);
	TXT_ForecastChance = MakeText(TEXT("TXT_ForecastChance"), 20);
	if (UVerticalBoxSlot* S = ForecastColumn->AddChildToVerticalBox(TXT_ForecastChance)) S->SetHorizontalAlignment(HAlign_Center);
	TXT_ForecastDetail = MakeText(TEXT("TXT_ForecastDetail"), 13);
	if (UVerticalBoxSlot* S = ForecastColumn->AddChildToVerticalBox(TXT_ForecastDetail)) S->SetHorizontalAlignment(HAlign_Center);
	BOX_Forecast->SetVisibility(ESlateVisibility::Collapsed);

	TXT_Intent = MakeText(TEXT("TXT_Intent"), 17);
	if (UVerticalBoxSlot* IntentSlot = Column->AddChildToVerticalBox(TXT_Intent))
	{
		IntentSlot->SetHorizontalAlignment(HAlign_Center);
		IntentSlot->SetPadding(FMargin(0, 0, 0, 2));
	}

	// HP 바: 흰 잔상 바 위에 빨간 바를 겹치고, 가운데에 숫자
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(96.0f);
	BarBox->SetHeightOverride(15.0f);
	UOverlay* Bars = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Bars"));
	BarBox->SetContent(Bars);
	PB_HPLag = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_HPLag"));
	PB_HPLag->SetFillColorAndOpacity(FLinearColor(1.0f, 0.95f, 0.85f, 0.9f));
	PB_HP = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_HP"));
	PB_HP->SetFillColorAndOpacity(FLinearColor(0.85f, 0.12f, 0.12f));
	// 위쪽 바의 배경은 투명해야 잔상이 비쳐 보인다
	FProgressBarStyle TopStyle = PB_HP->GetWidgetStyle();
	TopStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
	PB_HP->SetWidgetStyle(TopStyle);
	FProgressBarStyle LagStyle = PB_HPLag->GetWidgetStyle();
	LagStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.06f, 0.85f));
	PB_HPLag->SetWidgetStyle(LagStyle);
	TXT_HP = MakeText(TEXT("TXT_HP"), 10);
	for (UWidget* Child : TArray<UWidget*>{ PB_HPLag, PB_HP, TXT_HP })
	{
		if (UOverlaySlot* BarSlot = Bars->AddChildToOverlay(Child))
		{
			BarSlot->SetHorizontalAlignment(Child == TXT_HP ? HAlign_Center : HAlign_Fill);
			BarSlot->SetVerticalAlignment(Child == TXT_HP ? VAlign_Center : VAlign_Fill);
		}
	}

	// 한 줄: [속도 아이콘+수] [체력바] [방패+방어도]
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	auto AddIcon = [this, Row](const TCHAR* Name, const TCHAR* Path, float Size)
	{
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
		if (Texture) Icon->SetBrushFromTexture(Texture);
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(Size);
		IconBox->SetHeightOverride(Size);
		IconBox->SetContent(Icon);
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconBox)) IconSlot->SetVerticalAlignment(VAlign_Center);
		return Texture != nullptr;
	};
	auto AddValue = [Row](UTextBlock* Text)
	{
		if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text))
		{
			TextSlot->SetVerticalAlignment(VAlign_Center);
			TextSlot->SetPadding(FMargin(2, 0, 4, 0));
		}
	};
	AddIcon(TEXT("IMG_Speed"), TEXT("/Game/HUD/Texture/UI/T_UI_Speed.T_UI_Speed"), 34.0f);
	TXT_Status = MakeText(TEXT("TXT_Status"), 18);
	AddValue(TXT_Status);
	if (UHorizontalBoxSlot* RowBarSlot = Row->AddChildToHorizontalBox(BarBox)) RowBarSlot->SetVerticalAlignment(VAlign_Center);
	AddIcon(TEXT("IMG_Shield"), TEXT("/Game/HUD/Texture/UI/T_UI_Shield.T_UI_Shield"), 20.0f);
	AddValue(MakeText(TEXT("TXT_Block"), 13));
	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row)) RowSlot->SetHorizontalAlignment(HAlign_Center);

	// 격돌 예상 판은 체력바 아래에 (열려도 체력바가 밀리지 않게)
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BOX_Forecast))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 4, 0, 0));
	}
}

FString UBWUnitStatusWidget::DescribeIntent(const ABWCharacter* Enemy, FLinearColor& OutColor)
{
	OutColor = FLinearColor(1.0f, 0.45f, 0.4f);
	if (!Enemy) return FString();
	if (Enemy->IsStunned())
	{
		OutColor = FLinearColor(0.75f, 0.75f, 0.8f);
		return TEXT("기절 - 행동 없음");
	}
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	if (!Action) return FString();

	// 내 턴에 치면 무엇으로 받는지 / 적 턴에 닿으면 무엇을 하는지. 마우스를 올리면 양 옆에 카드가 뜬다
	const FBWDuelSide CounterSide = Enemy->MakeEnemyDuelSide(true);
	const FBWDiceRange Range = Enemy->MakeEnemyDuelSide().SkillRange;
	const FString Reach = Enemy->ActionRange > 1 ? FString::Printf(TEXT(" · 사거리 %d"), Enemy->ActionRange) : FString();

	FString Turn;
	switch (Action->ActionType)
	{
	case EBWEnemyActionType::Heal:
		OutColor = FLinearColor(0.4f, 1.0f, 0.5f);
		Turn = FString::Printf(TEXT("회복 %d"), Action->Value);
		break;
	case EBWEnemyActionType::BuffAttack:
		OutColor = FLinearColor(1.0f, 0.65f, 0.25f);
		Turn = FString::Printf(TEXT("강화 +%d → 공격 %d~%d%s"), Action->Value, Range.Min + Action->Value, Range.Max + Action->Value, *Reach);
		break;
	case EBWEnemyActionType::Block:
		OutColor = FLinearColor(0.5f, 0.8f, 1.0f);
		Turn = FString::Printf(TEXT("방어도 +%d"), Action->Value);
		break;
	case EBWEnemyActionType::Hex:
		OutColor = FLinearColor(0.8f, 0.55f, 1.0f);
		Turn = FString::Printf(TEXT("주술%s"), *Reach);
		break;
	case EBWEnemyActionType::Summon:
		OutColor = FLinearColor(1.0f, 0.65f, 0.25f);
		Turn = TEXT("호위 소환");
		break;
	case EBWEnemyActionType::Chant:
		OutColor = FLinearColor(0.8f, 0.55f, 1.0f);
		Turn = TEXT("영창 (맞으면 깨짐)");
		break;
	case EBWEnemyActionType::Devour:
		OutColor = FLinearColor(0.4f, 1.0f, 0.5f);
		Turn = TEXT("호위 포식");
		break;
	case EBWEnemyActionType::Reckoning:
		OutColor = FLinearColor(1.0f, 0.3f, 0.25f);
		Turn = FString::Printf(TEXT("정산 %d~%d"), Range.Min + Enemy->EatenCards * Action->Value, Range.Max + Enemy->EatenCards * Action->Value);
		break;
	default:
		Turn = FString::Printf(TEXT("공격 %d~%d%s"), Range.Min, Range.Max, *Reach);
		break;
	}
	const FString Counter = FString::Printf(TEXT("%s %d~%d"), CounterSide.bGuard ? TEXT("방어") : TEXT("공격"), CounterSide.SkillRange.Min, CounterSide.SkillRange.Max);
	return FString::Printf(TEXT("반격 %s\n행동 %s"), *Counter, *Turn);
}

void UBWUnitStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ABWCharacter* Char = OwnerChar.Get();
	if (!Char || Char->IsDying())
	{
		SetRenderOpacity(0.0f);
		return;
	}
	SetRenderOpacity(1.0f);

	const float Percent = Char->MaxHP > 0 ? FMath::Clamp((float)Char->CurrentHP / (float)Char->MaxHP, 0.0f, 1.0f) : 0.0f;
	if (PB_HP) PB_HP->SetPercent(Percent);

	// 잔상: 회복하면 바로 맞추고, 깎이면 잠깐 기다렸다가 천천히 따라 내려온다
	if (LastHP >= 0 && Char->CurrentHP < LastHP) LagWait = LagDelay;
	LastHP = Char->CurrentHP;
	if (ShownLag < 0.0f || ShownLag < Percent) ShownLag = Percent;
	else if (LagWait > 0.0f) LagWait -= InDeltaTime;
	else ShownLag = FMath::Max(Percent, ShownLag - LagSpeed * InDeltaTime);
	if (PB_HPLag) PB_HPLag->SetPercent(ShownLag);

	if (TXT_HP) TXT_HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Char->CurrentHP, Char->MaxHP)));

	// 적의 행동·상태 글자는 마우스를 올리면 나오는 양 옆 카드에서 본다
	if (TXT_Intent) TXT_Intent->SetVisibility(ESlateVisibility::Collapsed);

	UpdateForecast(Char);

	auto ShowValue = [this](const TCHAR* IconName, UWidget* Value, int32 Amount)
	{
		const bool bShow = Amount > 0;
		const ESlateVisibility Visible = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (UWidget* Icon = GetWidgetFromName(IconName)) Icon->SetVisibility(Visible);
		if (UTextBlock* Text = Cast<UTextBlock>(Value))
		{
			Text->SetText(FText::AsNumber(Amount));
			Text->SetVisibility(Visible);
		}
	};
	ShowValue(TEXT("IMG_Speed"), TXT_Status, Char->bIsEnemy ? Char->RolledSpeed : 0);
	ShowValue(TEXT("IMG_Shield"), GetWidgetFromName(TEXT("TXT_Block")), Char->bIsEnemy ? Char->Block : 0);
}

void UBWUnitStatusWidget::UpdateForecast(const ABWCharacter* Char)
{
	if (!BOX_Forecast) return;
	const ABWPlayerController* PC = GetWorld() ? Cast<ABWPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
	FBWDuelForecast F;
	FString Label;
	FBWDiceRange WinHP, LoseHP;
	if (!Char || !Char->bIsEnemy || !PC || PC->IsEnemyHovered(Char) || !PC->GetDuelForecast(Char, F, Label, WinHP, LoseHP))
	{
		BOX_Forecast->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	BOX_Forecast->SetVisibility(ESlateVisibility::HitTestInvisible);

	const int32 Percent = FMath::RoundToInt(F.WinChance * 100.0f);
	const FLinearColor ChanceColor = F.WinChance >= 0.65f ? FLinearColor(0.45f, 1.0f, 0.5f) : F.WinChance >= 0.4f ? FLinearColor(1.0f, 0.85f, 0.35f) : FLinearColor(1.0f, 0.4f, 0.35f);
	if (TXT_ForecastChance)
	{
		TXT_ForecastChance->SetText(FText::FromString(FString::Printf(TEXT("승률 %d%%"), Percent)));
		TXT_ForecastChance->SetColorAndOpacity(ChanceColor);
	}
	if (TXT_ForecastDetail)
	{
		auto Range = [](const FBWDiceRange& R) { return R.Min == R.Max ? FString::FromInt(R.Max) : FString::Printf(TEXT("%d~%d"), R.Min, R.Max); };
		TArray<FString> Lines;
		if (F.WinChance > 0.0f)
		{
			FString Win = FString::Printf(TEXT("이기면 적 HP -%s"), *Range(WinHP));
			if (Char->Block > 0 && WinHP.Max < F.WinDamage.Max) Win += FString::Printf(TEXT(" (방어도 %d)"), Char->Block);
			if (WinHP.Max >= Char->CurrentHP) Win += TEXT(" · 처치 가능");
			Lines.Add(Win);
		}
		if (F.bGuarded) Lines.Add(TEXT("방어: 지면 아무 일도 없음"));
		else Lines.Add(F.LoseChance > 0.0f ? (LoseHP.Max > 0 ? FString::Printf(TEXT("지면 당신의 HP -%s"), *Range(LoseHP)) : FString(TEXT("져도 피해 없음"))) : FString(TEXT("질 일 없음")));
		TArray<FString> Extra;
		if (F.CritChance > 0.0f && F.WinChance > 0.0f) Extra.Add(FString::Printf(TEXT("치명 %d%% ×%.1f"), FMath::RoundToInt(F.CritChance * 100.0f), F.CritMultiplier));
		if (F.DrawChance >= 0.01f) Extra.Add(FString::Printf(TEXT("무승부 %d%%"), FMath::RoundToInt(F.DrawChance * 100.0f)));
		if (Extra.Num() > 0) Lines.Add(FString::Join(Extra, TEXT(" · ")));
		TXT_ForecastDetail->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
		TXT_ForecastDetail->SetColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.96f));
	}
}
