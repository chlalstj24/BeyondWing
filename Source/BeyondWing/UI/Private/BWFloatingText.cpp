#include "BeyondWing/UI/Public/BWFloatingText.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"

void UBWFloatingTextWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!TXT_Popup && WidgetTree && !WidgetTree->RootWidget)
	{
		TXT_Popup = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TXT_Popup"));
		TXT_Popup->SetJustification(ETextJustify::Center);
		TXT_Popup->SetShadowOffset(FVector2D(2.0f, 2.0f));
		TXT_Popup->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
		WidgetTree->RootWidget = TXT_Popup;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWFloatingTextWidget::Setup(const FString& Text, const FLinearColor& Color, float Scale)
{
	if (!TXT_Popup) return;
	FSlateFontInfo Font = TXT_Popup->GetFont();
	Font.Size = FMath::RoundToInt(26.0f * Scale);
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.85f);
	TXT_Popup->SetFont(Font);
	TXT_Popup->SetText(FText::FromString(Text));
	TXT_Popup->SetColorAndOpacity(Color);
}

ABWFloatingText::ABWFloatingText()
{
	PrimaryActorTick.bCanEverTick = true;

	WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget"));
	RootComponent = WidgetComp;
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetDrawAtDesiredSize(true);
	WidgetComp->SetPivot(FVector2D(0.5f, 0.5f));
	WidgetComp->SetWidgetClass(UBWFloatingTextWidget::StaticClass());
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

ABWFloatingText* ABWFloatingText::Spawn(UWorld* World, const FVector& Location, const FString& Text, const FLinearColor& Color, float Scale)
{
	if (!World) return nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABWFloatingText* Actor = World->SpawnActor<ABWFloatingText>(ABWFloatingText::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Actor)
	{
		Actor->BaseScale = Scale;
		Actor->WidgetComp->InitWidget();
		if (UBWFloatingTextWidget* Widget = Cast<UBWFloatingTextWidget>(Actor->WidgetComp->GetUserWidgetObject()))
		{
			Widget->Setup(Text, Color, Scale);
		}
	}
	return Actor;
}

void ABWFloatingText::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;
	AddActorWorldOffset(FVector(0.0f, 0.0f, RiseSpeed * DeltaTime));

	// 처음엔 크게 튀어나왔다 제 크기로, 마지막 30%는 흐려지며 사라진다
	if (UUserWidget* Widget = WidgetComp->GetUserWidgetObject())
	{
		const float Pop = PopTime > 0.0f ? FMath::Clamp(Age / PopTime, 0.0f, 1.0f) : 1.0f;
		const float Scale = FMath::Lerp(1.6f, 1.0f, 1.0f - FMath::Square(1.0f - Pop));
		Widget->SetRenderScale(FVector2D(Scale, Scale));
		const float FadeStart = Lifetime * 0.7f;
		Widget->SetRenderOpacity(Age < FadeStart ? 1.0f : FMath::Clamp(1.0f - (Age - FadeStart) / (Lifetime - FadeStart), 0.0f, 1.0f));
	}
	if (Age >= Lifetime) Destroy();
}
