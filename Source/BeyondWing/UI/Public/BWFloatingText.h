#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "BWFloatingText.generated.h"

class UWidgetComponent;
class UTextBlock;

// [수정] 팝업 글자 위젯. TextRender는 한글 글리프가 없어 네모로 나와서 UMG 글꼴(한글 지원)로 그린다
UCLASS()
class BEYONDWING_API UBWFloatingTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Popup;

	void Setup(const FString& Text, const FLinearColor& Color, float Scale);

protected:
	virtual void NativeOnInitialized() override;
};

// 피해 숫자처럼 잠깐 떠올랐다 사라지는 글자. 화면 공간 위젯이라 늘 카메라를 향한다
UCLASS()
class BEYONDWING_API ABWFloatingText : public AActor
{
	GENERATED_BODY()

public:
	ABWFloatingText();

	// World 위치 Location에 Text를 Color로 띄운다. Scale은 글자 크기 배율 (치명타 등은 크게)
	static ABWFloatingText* Spawn(UWorld* World, const FVector& Location, const FString& Text, const FLinearColor& Color, float Scale = 1.0f);

	UPROPERTY(VisibleAnywhere, Category = "Floating Text") UWidgetComponent* WidgetComp;
	UPROPERTY(EditAnywhere, Category = "Floating Text") float Lifetime = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Floating Text") float RiseSpeed = 70.0f;
	UPROPERTY(EditAnywhere, Category = "Floating Text") float PopTime = 0.15f;   // 처음 크게 튀어나왔다 줄어드는 시간

protected:
	virtual void Tick(float DeltaTime) override;

private:
	float Age = 0.0f;
	float BaseScale = 1.0f;
};
