#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWMapWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 생성할 노드 블루프린트 클래스
	UPROPERTY(EditAnywhere, Category = "BW Map")
	TSubclassOf<class UBWMapNodeWidget> NodeWidgetClass;

	// 노드들이 담길 수평(Horizontal) 컨테이너
	UPROPERTY(meta = (BindWidget))
	class UPanelWidget* MapMainContainer;

	// 지도 전체 새로고침 및 렌더링
	UFUNCTION(BlueprintCallable, Category = "BW Map")
	void RenderMap();

	// 현재 진행 단계로 스크롤 이동
	UFUNCTION(BlueprintCallable, Category = "BW Map")
	void ScrollToCurrentStep();

	UPROPERTY(meta = (BindWidget))
	class UScrollBox* MapScrollBox;

	// --- 디자인 설정 ---
	UPROPERTY(EditAnywhere, Category = "BW Map|Design")
	FVector2D NodeSize = FVector2D(180.0f, 180.0f);

	UPROPERTY(EditAnywhere, Category = "BW Map|Design")
	float ColumnSpacing = 250.0f;

	UPROPERTY(EditAnywhere, Category = "BW Map|Design")
	float RowSpacing = 100.0f;

	// --- 네비게이션 설정 ---
	UPROPERTY(EditAnywhere, Category = "BW Map|Navigation")
	float ScrollSensitivity = 2.0f;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	bool bIsDraggingMap = false;
	FVector2D LastMousePos;
};
