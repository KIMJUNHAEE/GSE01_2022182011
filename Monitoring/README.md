# 로컬 월드 모니터

## 실행

1. Python 3.9 이상이 설치된 PC에서 저장소 루트의 **Monitor.cmd**를 실행한다.
   외부 패키지 설치는 필요 없다. 터미널에서 `python -B Monitoring/server.py`로 실행해도 된다.
2. 브라우저에서 **http://127.0.0.1:8765/** 를 연다.
3. 사용자가 Visual Studio에서 게임을 빌드한 뒤 실행한다.
   새 실행 파일이 0.5초마다 JSON을 전송한다. 기존 바이너리에는 송신 기능이 없다.
4. 수신기 종료는 실행한 콘솔에서 Ctrl+C. 게임을 먼저 실행해도 서버가 켜지면 자동 재접속한다.

HTML 파일을 더블클릭하는 file:// 방식이 아니라 위 주소로 접속한다.
JS/CSS/글꼴/CDN 다운로드 없이 로컬에서 작동한다. 게임은 Debug/Release 모두 송신한다.
모니터링을 끄려면 게임 실행 인수에 `--no-monitor`를 넣는다.
게임의 `--verify` 경로는 네트워크 송신기를 시작하지 않는다.

포트를 바꾸려면 양쪽을 맞춘다.

```text
Monitor.cmd --port 8877
SimpleGame.exe --monitor-port 8877
```

## 모니터링 항목

- NPC: 주민 Prop(Citizen) + 동료 Character. 플레이어와 적은 NPC 수에 포함하지 않는다.
  ID, 역할, 대화/동행/대기/비활성 상태, 위치, 소속 구역, 표시/활성 여부.
- 몹: 생존 중인 Enemy. 드론/포탑/보스, 체력, 대기/이동/공격 예고 상태, 캠프와 위치.
  NPC/몹 모두 활성 개수와 비활성 포함 개수를 구분한다.
- 마을/지역: 기존 5개 구역을 관측 단위로 사용한다. 지역별 NPC/몹 수, 중계기 복구,
  디자이너가 설정한 성향, 실제 위협·복구 상태로 추정한 분위기를 표시한다.
  경계는 LevelOne의 District 사각형이며, 구역 밖 개체는 따로 집계한다.
- 사건: 월드 생성, 탐사 시작, 주민 대화, 중계기 복구, 기억 발견, 레벨 업, 적 처치,
  플레이어 패배, 보스 시작/격파, 스토리 선택.
- 확장 지표: FPS, 실제 glDraw 호출의 평균/직전 프레임 수, 평균 인스턴스 수,
  액터·탄환·아이템 개수, 플레이어 체력/레벨/무기 강화, 시드/퀘스트/세션, 연결 신선도.
- HTML: 액터 검색/지역 필터, 사건 출처/등급 필터, 월드 좌표 지도, FPS 추이, 원문 JSON,
  외부 사건 JSON 입력 창.

### 현재 게임에 없는 기능은 구분한다

주민·동료 HP, 선악 평판 시뮬레이션, 해적 세력 AI, NPC 살인과 소문 전파 시스템은 아직 없다.
HP는 null/미구현으로, 성향은 설정값으로 표시한다.
현재 주거구역은 Civilian, 착륙장/정원/기록보관소는 Neutral, 중앙 첨탑은 Unknown이다.
Pirate도 지원하지만 기존 마을을 임의로 해적 마을로 바꾸지는 않는다.

`MonitoringState::villages`에서 이름/성향을 설정할 수 있다.
분위기 계산은 `BuildMonitoringSnapshot`에 있으며 보스 교전 → 위기,
활성 몹 존재 → 경계, 복구 또는 보스 격파 → 회복 중, 그 외 → 조용함 순서다.
몬스터의 이동 표시는 현재 위치와 home 차이에서 파생한 상태이며 정식 AI FSM 상태가 아니다.
지역 경계는 현재 레벨 배치 메타데이터 기준이다. 레벨 전체를 이동/재구성하면 District도 갱신해야 한다.

살인 소문 등은 아래 JSON API로 신고할 수 있고 반드시 **외부 미확인 제보**로 표시한다.
이는 모니터의 사건 피드만 바꾸며 게임의 NPC, AI, 성향이나 퀘스트를 변경하지 않는다.
경비 기계 처치를 NPC 살인 사건으로 오인해 기록하지 않는다.

## 연결 구조와 스레드

```text
GameWorld + SceneGraph + RenderStats
    → 게임 스레드: 0.5초마다 UTF-8 JSON 스냅샷 생성
    → MonitorPublisher: 최신 스냅샷 1개를 유지하는 메일박스
    → 별도 스레드: WinHTTP POST /api/snapshot
    → Python loopback 수신 서버
    → 브라우저: GET /api/state (약 0.5초 간격)
```

네트워크 스레드는 GameWorld/Actor/OpenGL에 접근하지 않는다.
수신기 부재 시 게임을 멈추지 않고 재시도한다. 각 HTTP 단계 타임아웃은 500ms이며
종료 시 진행 중인 요청이 끝날 때까지 짧게 기다릴 수 있다. 스냅샷 생성 자체는 게임 스레드 비용이다.

게임 이벤트는 GameWorld 안에 최근 256개, 외부 제보는 서버 메모리에 최근 128개,
차트는 서버 메모리에 최근 120개 표본을 보관한다. 영구 저장/무손실 전송은 아니다.
수신기 단절 중 256개를 넘는 게임 이벤트는 오래된 것부터 사라진다.
게임 재시작 시 sessionId가 바뀌고 서버는 제보/차트를 초기화한다.
서버 재시작 시 보관 데이터가 지워지며 다음 게임 스냅샷에서 현재 상태/최근 게임 사건을 복구한다.
한 수신기는 게임 한 세션을 대상으로 한다. 새 세션은 이전 세션을 대체하고 최근 종료 세션의 지연 메시지는 거부한다.

3초 이상 새 스냅샷이 없으면 마지막 상태를 보존하면서 ‘게임 수신 끊김’으로 표시한다.
서버 자체가 꺼지면 ‘수신 서버 연결 끊김’으로 표시한다.
게임은 기존 규칙상 포커스를 잃으면 Pause가 된다. 브라우저를 클릭해 일시정지되어도
Timer가 실행되는 동안 스냅샷은 계속 전달되므로 수신 중단과 구분할 수 있다.

## JSON API v1

모든 POST는 `Content-Type: application/json`, UTF-8, Content-Length를 사용한다.
최대 본문 2 MiB. 성공은 HTTP 202, 유효하지 않은 JSON/스키마/세션/순서는 400이다.
수신기는 `127.0.0.1`에만 바인딩하고, Host/Origin을 확인하며 크로스 오리진 요청은 차단한다.
게임 송신기는 프록시와 HTTP 리다이렉트를 사용하지 않는다.
이 기능은 신뢰하는 로컬 개발 도구용이다. 인증 서버가 아니며 같은 PC의 다른 프로세스가
접근할 수 있다. 인터넷에 프록시/포트포워딩하거나 공용 서버로 노출하지 않는다.

### POST /api/snapshot — 게임 상태 수신

`BuildMonitoringSnapshot`가 생성하며 최상위 필드는 다음과 같다.

| 필드 | 의미 |
| --- | --- |
| schemaVersion / type | 1 / world_snapshot |
| sessionId / sequence | 게임 인스턴스 식별자 / 증가하는 10진 문자열 순번 |
| mapSeed / gameTime / view | 시드 / 게임 내부 시간 / explore, pause 등의 화면 |
| counts | NPC, 몹, 비활성 포함 개수, 탄환, 전리품, 전체 액터, 구역 밖 개수 |
| player / progress / performance | 성장·체력 / 퀘스트 / 기존 렌더링 통계 |
| villages | id, name, alignment, mood, 설정·추정 출처, counts, bounds, relayRestored |
| npcs / mobs | id, name, role, state, active, visible, villageId, position, health 등 |
| events / eventsTotal | 최근 게임 사건 목록 / 세션의 누적 사건 개수 |

ActorId/이벤트 ID/순번/누적 이벤트 수는 JavaScript 정수 정밀도를 위해 문자열이다.
미구현 값과 측정 준비 전 성능 값은 null이며 0으로 가장하지 않는다.
FPS는 기존 RenderStats 통계이며 GPU 실행 시간 측정값이 아니다.
전체 유효 예제는 `test_server.py::snapshot`에 있다. 이 테스트 데이터는 실제 화면에 자동 주입되지 않는다.

### POST /api/events — 사건/소문 제보 수신

```json
{
    "schemaVersion": 1,
    "type": "world_event",
    "sessionId": "GET /api/state에서 받은 snapshot.sessionId",
    "kind": "murder_rumor",
    "severity": "warning",
    "villageId": 1,
    "message": "주거구역에서 살인 사건이 있었다는 미확인 제보"
}
```

kind: murder_rumor / theft_rumor / disturbance / observation
severity: info / warning / danger
villageId: 현재 스냅샷의 지역 ID 또는 -1(구역 밖)
message: 1~500자

confirmed/source를 임의로 보내도 서버는 confirmed=false, source=external로 강제한다.
현재 sessionId가 없거나 다른 세션이면 거부한다.
HTML ‘예시 채우기’는 입력창만 채우며 ‘JSON 전송’을 눌러야 실제 제보가 저장된다.

PowerShell 예시:

```powershell
$monitorState = Invoke-RestMethod http://127.0.0.1:8765/api/state
$monitorReport = @{
    schemaVersion = 1
    type = 'world_event'
    sessionId = $monitorState.snapshot.sessionId
    kind = 'murder_rumor'
    severity = 'warning'
    villageId = 1
    message = '수동 테스트: 확인되지 않은 살인 소문'
} | ConvertTo-Json
Invoke-RestMethod http://127.0.0.1:8765/api/events -Method Post -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($monitorReport))
```

### GET /api/state — HTML 수신

snapshot, externalEvents, history, receivedAt(UTC), ageSeconds, stale를 반환한다.
게임이 아직 연결되지 않았으면 snapshot=null, stale=true다.

### 게임 내 사건 확장

미래에 실제 소문 시스템을 구현하면 그 시스템의 사건 발생 지점에서 호출한다.

```cpp
world.monitoring.Record(world.time, "murder_rumor", L"주민이 미확인 소문을 전달했습니다.",
                        1, witnessId, "warning", false);
```

확인되지 않은 소문은 confirmed=false로 남긴다. Record는 게임 스레드에서만 호출한다.

## 검증

수신기 전용 테스트(게임 빌드/실행 없음):

```text
python -B -m unittest discover -s Monitoring -p test_server.py -v
node --check Monitoring/dashboard.js
```

JSON 유효성, 한글/특수문자, 잘못된 Host/Origin, 정적 파일 허용 목록, 요청 크기,
순서 역전, 세션 재시작, 외부 제보의 비확정 처리, 제한된 버퍼, stale 표시 조건을 점검한다.
C++ 스냅샷 필드/이스케이프/버퍼/세션 검증은 기존 --verify 코드에 추가했다.
사용자 요청에 따라 게임 빌드·실행 및 C++ 검증 실행은 수행하지 않았다.
