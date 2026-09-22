# Actor / SceneGraph 구조

## 변경 목적

월드에 배치되는 객체를 Actor로 통일하고 SceneGraph를 객체와 좌표의 단일 소유자로 사용한다.
기존의 props / enemies / projectiles / loot / combatText 배열과 플레이어·동료의 별도 위치 저장을 제거했다.
게임 규칙은 액터를 조회해 처리하고, 렌더링은 그래프가 수집·정렬한 액터를 그린다.

## 책임 구분

| 파일 | 책임 |
| --- | --- |
| Actor.h / Actor.cpp | ID, 부모·자식, 로컬/월드 위치, 활성/표시 상태, 갱신·렌더 진입점, 삭제 예약 |
| SceneGraph.h / SceneGraph.cpp | 액터 소유, 타입 조회, 부모 변경, 순회, 지연 삭제, 깊은 복사 |
| WorldActors.h / WorldActors.cpp | 지형·장식물·캐릭터·효과 구현, 전투 액터의 생성·렌더 연결 |
| CombatTypes.h | 적·탄환·아이템·전투 텍스트와 성장 수치 |
| ActorRenderer.h | 액터를 그리는 인터페이스. Actor/SceneGraph는 Canvas/OpenGL에 의존하지 않음 |
| GameScene.cpp / GameCombat.cpp / Game.cpp | ActorRenderer 구현과 기존 시각 표현, 화면 UI |
| LevelOne.cpp | 첫 행성의 방·통로·배치 생성과 연결성 검증 |
| NavigationGrid.h / NavigationGrid.cpp | 콜백 기반 격자 연결성 검사와 흐름장 길 찾기 |
| WorldNavigation.cpp | 활성 지형/장식물로 충돌 캐시 구성, 길 찾기 연결, 충돌 이동 |
| WorldScene.cpp | 필수 액터의 ID 기반 위치 접근 |
| Combat.cpp / GameWorld.cpp | 전투·진행·입력·대화 등 월드 규칙 |

## 실제 계층

```text
SceneGraph
├─ Terrain       → 바닥 타일 Actor
├─ Environment   → 건물 / 나무 / 중계기 / 함선 / 주민 / 기억
│                  └─ 지면 그림자·발광, 함선 착륙장 효과
├─ Characters    → 플레이어 / 동료
│                  └─ 플레이어 무기 / 자석 / 선택 링
├─ Enemies       → 드론 / 포탑 / 보스
│                  └─ 공격 예고 효과
├─ Projectiles   → 탄환
├─ Loot          → 전리품
└─ Effects       → 전투 텍스트 / 스캔 / 환경 먼지
```

월드 배치 객체와 그에 부속된 시각 효과를 대상으로 한다.
HUD, 메뉴, 대화창, 지도 UI, 조준 가이드, 전체 화면 배경·후처리는 화면 공간 표현으로 유지한다.
경험치·퀘스트·전투 규칙은 GameWorld에 남긴다. 모든 규칙을 Actor 기본 클래스에 몰아넣지는 않는다.

## 사용 예시

```cpp
// 월드 좌표로 생성. 기본 그룹에 들어가며 Prop의 그림자 효과도 함께 붙는다.
auto& crate = world.Spawn(Prop(PropKind::Crate, {3, 1}, 1, 1, 24));
ActorId crateId = crate.Id();

// SceneGraph::Add는 부모 기준 로컬 좌표로 생성한다.
auto& group = world.scene.Add(Actor("Outpost", {5, 2}));
world.scene.Reparent(crateId, group.Id()); // 기본: 현재 월드 위치 유지
group.SetPosition({6, 2});                 // 상자와 그 자식 효과도 이동

crate.SetLocalPosition({1, 0});
crate.SetPosition({8, 3});                 // 월드 위치를 입력하면 로컬로 환산
crate.SetFootprint(1.5f, 1.5f);            // 충돌 크기 변경 + 캐시 무효화
crate.SetSolid(false);

group.SetVisible(false); // 표시만 끔. 갱신·충돌은 유지
group.SetActive(false);  // 자손의 갱신·렌더·기본 타입 조회·충돌을 제외

for (auto& enemy : world.scene.Actors<Enemy>())
{
    enemy.SetPosition(enemy.Position() + Vec2{.1f, 0});
}

world.scene.Destroy(crateId);   // 자손까지 삭제 예약, Find/새 조회에서는 즉시 제외
world.scene.CollectDestroyed(); // 실제 그래프 등록 해제
```

## 생명주기와 조회 규칙

- SceneGraph가 액터를 소유한다. 부모·자식 링크는 약한 참조로 순환 소유를 피한다.
- ActorId는 해당 씬 안에서만 유효하다. 장기 보관은 포인터 대신 ID를 사용하고 Find 결과를 확인한다.
- Actors<T>()는 정확히 T로 등록된 활성 액터의 스냅샷이다. 상속 계층 전체 검색은 아니다.
- Actors<T>(true)는 비활성 액터도 포함하지만 삭제 예약 액터는 제외한다.
- 스냅샷은 순회 중 객체 수명을 보장한다. 생성·삭제로 순회 메모리가 무효화되지 않는다.
  이미 얻은 스냅샷의 구성은 자동 변경되지 않으므로, 순회 중 다른 액터를 비활성화/삭제했다면
  필요에 따라 IsActive()/IsPendingDestroy()를 다시 확인한다.
- Update 순회 중 생성한 액터는 다음 Update부터 갱신한다. 씬 Update 끝과 월드 전투 처리 끝에 삭제를 수거한다.
- 활성/표시 상태와 월드 위치는 조상을 따라 계산한다. Reparent는 순환 계층과 없는 부모를 거부한다.
- Draw는 RenderLayer → 월드 x+y 깊이 순서로 정렬한다. 동일 깊이는 등록 ID 순서를 유지한다.
  GPU 메쉬 캐시·인스턴싱·독립 셰이더 로딩은 기존 Canvas/Renderer가 계속 담당한다.
- SceneGraph/GameWorld 복사는 액터와 부모 링크를 깊게 복제한다. 새 액터 타입은 Clone도 반드시 구현한다.

## 충돌과 길 찾기

지형·고체 Prop의 생성, 이동, 부모 변경, 활성 변경, 삭제가 내비게이션 revision을 변경한다.
Prop의 충돌 크기/고체 여부는 SetFootprint/SetSolid를 사용한다.
충돌 캐시는 revision이 바뀔 때 다시 만들고, NavigationGrid도 다음 필요 시 갱신한다.
길 찾기 클래스에는 GameWorld 포인터를 보관하지 않으며, Walkable/ClearPath 콜백을 호출마다 전달한다.

현재 바닥은 정수 타일 격자다. Terrain 또는 그 부모는 정수 단위로 이동시켜야 시각·충돌 격자가 일치한다.
첫 행성의 길 찾기 범위는 x=-10..20, y=-17..5, 간격 0.5이다.
새 레벨에서는 NavigationGrid의 원점·크기·간격과 레벨 배치 범위를 함께 설정해야 한다.
씬 그래프 도입이 오픈 월드 청크 스트리밍이나 공간 분할 구현을 의미하지는 않는다.

## 확장 범위와 주의점

- 현재 계층 변환은 위치 이동만 지원한다. 회전·스케일 및 범용 3D 행렬 계층은 별도 확장 지점이다.
- 플레이어·동료·함선·중계기·기억·주민·코어는 현재 시나리오가 참조하는 필수 액터다.
  레벨 진행 중 이들을 삭제하려면 퀘스트·카메라의 참조 전환 정책도 추가해야 한다.
  필수 위치 접근 시 객체가 없으면 명시적인 오류를 발생시킨다. 임시 숨김/중지는 상태 API를 사용한다.
- 적의 home/target, 방 중심, 이동 흔적은 배치 좌표 복제본이 아니라 AI/레벨의 목표·이력 데이터다.
  그룹을 이동하며 전투 구역까지 통째로 재배치하려면 이 목표 데이터도 레벨 규칙에 맞춰 갱신해야 한다.
- 새 액터는 Actor 상속, Clone/Draw(필요 시 Update) 구현 후 SceneGraph::Add로 등록한다.
  새로운 시각 타입은 ActorRenderer와 그 구현에 Draw 오버로드를 추가한다.
  GameWorld::Spawn 자동 그룹/부속 효과가 필요하면 타입 분기도 추가한다.

## 검증 상태

PrototypeVerification.cpp의 기존 선택 실행 경로(--verify)에 다음 검증 코드를 추가했다.

- 부모 위치 전파, 월드/로컬 좌표 설정, 부모 교체와 순환 거부
- 활성/표시 상속, 가상 Update, 수명 종료와 자식 삭제
- 스냅샷 수명, 깊은 복사와 이동, 레이어·깊이 렌더 순서
- 액터 생성·삭제·부모 이동·충돌 속성 변경의 충돌 캐시 반영
- 비활성 플레이어/탄환의 이동·사격·피해 중지, 월드 좌표 생성

요청에 따라 빌드·게임 실행·검증 실행은 수행하지 않았다.
코드 정리, 프로젝트 등록/경로 확인, 변경 내용의 정적 점검만 수행했다.
