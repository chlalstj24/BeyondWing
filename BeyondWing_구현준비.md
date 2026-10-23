# BeyondWing 구현 준비 — 전투·지도 기획서 대비 코드 점검

2026-10-22 · 기준 문서: `BeyondWing_전투기획서.md`, `BeyondWing_지도기획서2.md`

> 2026-10-24: 전투 1~10단계는 구현을 마쳤다. 바뀐 규칙과 진행 상황은 `BeyondWing_전투기획서.md`(16장)가 기준이고, 아래 내용은 구현 전 점검 기록이다.

이 문서는 두 기획서를 현재 코드(`Source/BeyondWing`, 약 4,200줄)와 에셋(`/Game/System`, `/Game/HUD`)에 맞춰 본 결과다. 구현은 전투 기획서 16장의 단계 순서를 따르고, 단계마다 지시를 받아 진행한다.

## 1. 가능 여부 판단

| 영역 | 가능 여부 | 비고 |
| --- | --- | --- |
| C++ 규칙 전체 (전투 1~10단계, 지도 규칙) | 가능 | 기존 모듈 안에서 해결된다. Build.cs 의존성을 추가할 필요가 없다 |
| 합 판정 단위 테스트 (3단계 완료 기준) | 가능 | `IMPLEMENT_SIMPLE_AUTOMATION_TEST`와 `FRandomStream` 시드로 결과를 결정적으로 만들 수 있다. Revvy `run_automation_tests`로 실행한다 |
| 데이터 테이블 수정 (카드·적·인카운터) | 가능 | Revvy Python으로 CSV를 읽고 쓴다. 저장 전에 체크포인트가 자동으로 남는다 |
| 위젯 블루프린트 그래프 수정 | **사람 작업 필요** | Revvy에는 BP 그래프 편집 도구가 없다. `WBP_BattleHUD`와 `WBP_Card`의 AP 바인딩은 에디터에서 손으로 고쳐야 한다 |
| 신규 UI (증강 선택, 합 연출, 상태이상 아이콘) | 부분 가능 | C++ 위젯 베이스 클래스와 이벤트는 만들 수 있다. 레이아웃 WBP는 사람이 만든다 |
| 플레이 감각, 밸런스 | 사람 판단 필요 | PIE를 실행해 로그를 확인하는 것까지는 할 수 있다 |

## 2. 빌드·검증 루프

- 에디터에 Live Coding 세션이 켜져 있어서 `build_project`(UBT)가 막힌다(`blocks_ubt: true`).
- `.cpp`만 바꿨을 때는 Revvy `live_coding compile`로 반영한다.
- `UPROPERTY`, `USTRUCT`, `UENUM`을 추가하거나 지웠을 때는 에디터를 닫고 CLI로 빌드한 뒤 다시 연다. 빌드 명령은 아래와 같다.
  `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat BeyondWingEditor Win64 Development -Project="D:\GitStudy\BeyondWing\BeyondWing.uproject" -WaitMutex`
- 1단계는 거의 모두 헤더 변경이라 위 순서를 따라야 한다.

## 3. 기획 대비 현재 코드 차이

### 지도

| 항목 | 기획 | 현재 코드 (`BWGameInstance.cpp`) |
| --- | --- | --- |
| 막 길이 | 15칸 + 보스 칸 (총 16열) | 보스를 포함해 15열 |
| 고정 칸 | 1·8·15칸 = 인덱스 0·7·14, 보스는 15 | 인덱스 0 전투, 8 보물, 13 휴식, 14 보스 |
| 노드 비중 | 전투 43, 이벤트 20, 엘리트 10, 휴식 10, 상점 8, 보물 5, 제단 2, 균열 2, 여기에 차원별 성향 | 상점 5, 휴식 12, 이벤트 22, 엘리트 8(앞 5칸 금지), 나머지 전투 |
| 차원문 개폐 (70/30, 3연속 닫힘 금지) | 있음 | 없음. 인접 줄이면 항상 이동 가능 |
| 균열 2줄 점프, 차원 제단 | 있음 | 없음 (`EMapNodeType`에 값 없음) |
| 3막 진행, 2·3막 줄 섞기 | 있음 | `CurrentStage` 변수만 있고 막 전환이 없음 |
| 방치 보너스 수령 | 도착할 때 수령하고 즉시형 | 보상 생성 시점에 카운터를 보는데, 도착할 때 이미 0으로 초기화되어 베렌드·타이가 보너스가 **발동하지 않는다** (버그) |
| 깊은 방치, 보너스 내성, 과부하 I·II | 있음 | 없음 |
| 가시거리 예외 | 방치 빛만 항상 표시 | 인덱스 8·13·14 노드를 항상 표시 |

### 전투

| 항목 | 기획 | 현재 코드 |
| --- | --- | --- |
| 그리드 | 8×5, 아군 구역 4열 | 8×8 (`BP_GridManager` CDO 기준), 아군 구역 3열(X 0~2) |
| 자원 | AP 없음, INT·FTH만 MP | AP 3→10 증가, 무료 이동·무료 공격권 |
| 이동 | 속도, 접촉하면 정지 | `MovePoints`, 클릭한 칸으로 순간이동 |
| 공격 | 합 1:1, 이긴 쪽 숫자가 피해 | `ExecuteSkill` 범위 피해, ASP만큼 이펙트 반복 |
| 치명타 | 5% + TEC×0.5%, 배수 1.5 + TEC×0.01 | TEC/2 %, 배수 1.5 + TEC/66 |
| 방어 3종, 방어도, 상태이상 7종, 증강, 벽 타일 | 있음 | 없음 |
| 적 | 속도와 스킬 범위 | `Value`, `MoveDistance`, `Range`, `BaseAttackPower` |
| 플레이어 HP | GI가 영구 값을 가짐 | 캐릭터가 GI 값으로 초기화되지 않음 (캐릭터 100 고정, 피해만 GI에 반영) |

## 4. 삭제 대상 필드를 참조하는 에셋

1단계에서 AP 등을 지우면 아래 에셋이 깨진다. 이름 테이블을 바이너리 검색해서 찾았다.

| 필드 | 참조 에셋 |
| --- | --- |
| `APCost`, `MovePointsGained` | `WBP_Card`, `BeyondWing_CardDatabase_Template` (5행) |
| `CurrentAP`, `MaxAP`, `K2_UpdateAP`, `CurrentMovePoints`, `EnterMovementMode` | `WBP_BattleHUD` |
| `InsufficientAPSound` | `BP_BWPlayerController` |
| `BaseAttackPower`, `MoveDistance` | `DT_BWEnemyDB` (GI가 사용 중), `DT_EnemyDB` (미사용으로 보임) |

권장하는 순서는 세 단계다.
1. 새 필드를 추가하고, 옛 필드는 `meta=(DeprecatedProperty)`로 남겨 둔다.
2. BP와 데이터 테이블을 새 필드로 옮긴다.
3. 옛 필드를 삭제한다.

이렇게 하면 단계마다 에디터가 깨지지 않는다.

## 5. 1단계 전에 정할 것

1. **막 길이:** 기획대로 16열(15칸 + 보스)로 바꿀지 → 지도 위젯 레이아웃도 함께 바뀐다.
2. **그리드:** 8×5와 아군 4열로 바꿀지 → `BP_GridManager`와 레벨의 인스턴스 값, `DT_Encounters` 좌표(현재 (4,2), (4,4))에 영향이 있다.
3. **엘리트 앞 5칸 금지:** 기획에 없는 현재 규칙이다. 유지할지 정한다.
4. **AP 제거 방식:** 4장 권장안(2단계 이행)으로 할지, 한 번에 지우고 BP를 바로 고칠지 정한다.
5. **기존 미확정 항목:** 기획서의 열린 질문이다. 2·3막 줄 섞기, 카드 실제 목록, FTH 직업.

## 6. 단계별 작업 범위 (전투 기획서 16장 기준)

| 단계 | 주요 파일 | 사람 작업 |
| --- | --- | --- |
| 1 데이터 타입 | `BWTypes.h`, `BWGameInstance.h`, `DeckComponent.h`, `BWCharacter.h`, 신규 `BWCombatTypes.h` | 에디터 닫고 빌드, BP 바인딩 수정 |
| 2 속도·이동 | `BWPlayerController.cpp`, `GridManager.cpp`, `BWCharacter.cpp` | 이동 연출 확인 |
| 3 합 판정 코어 | 신규 `Combat/BWDuelRules` (상태 없는 함수와 `FRandomStream`), 신규 테스트 | 없음 |
| 4 방어 3종 | `BWDuelRules`, `BWCharacter` | 없음 |
| 5 카드·손패 | `DeckComponent.cpp`, `BWHandWidget.cpp` | 카드 WBP 표시 수정 |
| 6 직업·MP | `BWGameInstance`, `DeckComponent` | MP UI |
| 7 상태이상 | 신규 상태이상 컴포넌트 또는 `BWCharacter` | 아이콘 UI |
| 8 국가 기믹·벽 | `GridTile`, `GridManager`, `BWDuelRules` | 벽 타일 메시 |
| 9 적 AI | `BWCharacter.cpp` (UpdateEnemyIntent, ExecuteNextPattern) | 데이터 테이블 값 |
| 10 증강 | 신규 `FBWAugmentData`, 증강 DT, 선택 위젯 베이스 | 증강 선택 WBP |
| 11 지도 연동 | `BWGameInstance.cpp` (지도 생성, 방치·과부하·내성), `BWMapWidget` | 지도 레이아웃 |

지도 규칙 중 지도 생성, 차원문, 방치·과부하·내성은 전투와 의존성이 없다. 그래서 11단계를 기다리지 않고 전투 단계와 따로 진행할 수 있다.
