#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BWMapNodeWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWMapNodeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이 노드가 들고 있는 실데이터
	UPROPERTY(BlueprintReadOnly, Category = "BW Map")
	FMapNode NodeData;

	// --- 블루프린트에서 구현할 외형 설정 이벤트 ---
	
	// 노드 타입(전투, 휴식 등)에 따라 아이콘을 바꿀 때 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "BW Map")
	void K2_SetupTypeVisuals();

	// 상태(선택가능, 방문함, 안개 등)에 따라 색상/투명도를 바꿀 때 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "BW Map")
	void K2_UpdateStateVisuals(bool bIsSelectable, bool bIsVisited, bool bIsHidden);

	// --- 함수부 ---

	// 데이터 주입 및 초기화
	void InitNode(const FMapNode& InData);

	// 클릭 시 (블루프린트 버튼 OnClicked에서 이 함수를 호출하세요)
	UFUNCTION(BlueprintCallable, Category = "BW Map")
	void HandleClick();
};
