# 주인공 스프라이트

현재 게임은 Aseprite에서 제작한 대기 이미지와 32프레임 걷기 시트를 사용한다.
기존 `protagonist_sheet.png`와 `protagonist_walk32.png`는 참고 원본으로 남아 있고,
게임에서 재생하는 파일은 다음 두 개다.

- `Textures/protagonist_idle_aseprite.png`: 대기, 512×512.
- `Textures/protagonist_walk_aseprite.png`: 걷기 32장, 8열×4행, 4096×2048, 셀 512×512.
- `Art/Protagonist/protagonist_walk.aseprite`: 편집 가능한 4개 레이어, 대기 1장 + 걷기 32장.
- `Art/Protagonist/protagonist_walk.gif`: 걷기 태그만 내보낸 동작 확인용 GIF.
- `Art/Protagonist/protagonist_rig.json`: 발·무릎 위치와 공통 캔버스/기준점 정보.
- `Art/Protagonist/animation_validation.json`: 실제 Aseprite 출력에 대한 검증 결과.

## 제작 방식

기존 첫 대기 포즈를 Aseprite에 가져와 뒷다리, 앞다리, 몸통/코트, 가까운 팔의 네 레이어로
나눴다. 다리 관절과 발의 접지/들림, 반대 방향의 팔 움직임, 코트 흔들림을 32개 위상으로
제작했다. 현재는 컷아웃 방식의 1차 걷기 동작이며 관절의 수작업 작화 보완은 가능하다.
한 시점의 걷기를 좌우 반전하며 별도의 상하/8방향 시트는 아직 없다.
대화용 초상화와 동료/주민은 기존 리소스를 사용한다.

모든 프레임이 같은 512×512 캔버스, 기준점 (267,455), 기준 신장 396px을 공유한다.
프레임마다 외곽 경계에 맞춰 크기를 다시 늘리지 않는다. 첫 프레임과 마지막 프레임은
같은 주기의 인접 위상이고 동일한 끝 프레임을 중복해서 넣지 않는다.

## 게임 적용

사용자가 새로 빌드한 실행 파일에서 적용된다. WASD/방향키 이동, Shift 달리기.
`Character::animation`이 충돌 처리 후 실제 이동 거리에 맞춰 프레임을 진행한다.
144 화면 단위 이동마다 한 주기이며 달리기/성장 속도에 따라 재생 속도가 바뀐다.
이동하지 않으면 0번(대기), 걸으면 1~32번이다. 좌우 반전은 조준과 별도로 유지한다.
게임 내 출력 높이는 80×zoom이다. 기존 Canvas Quad 메시와 한 번 로드한 텍스처를 재사용한다.

모니터링 화면의 ‘주인공 애니메이션’ 항목과 JSON player.spriteFrame / spriteMoving /
spriteWalkFrames / spriteFlipped로 실제 게임의 재생 상태를 확인할 수 있다.
이 항목은 최신 실행 파일부터 전송한다.

## 직접 확인

루트의 `SpritePreview.cmd` 실행 후 `http://127.0.0.1:8766/SpritePreview.html`을 연다.
대기/걷기/반전/속도/프레임 슬라이더와 GIF로 새 시트를 확인할 수 있다.
`OpenProtagonistInAseprite.cmd`는 로컬에 설치한 Aseprite로 편집 원본을 연다.
자세한 도구 빌드·재생성 절차는 `../../Tools/Aseprite/README.md`를 참고한다.

## 검증 범위

Aseprite 1.3.18.6-dev 로컬 소스 빌드와 CLI 실행에 성공했다.
32개 고유 포즈, 동일한 얼굴/캔버스, 투명 여백, 다리의 실제 픽셀 변화,
양쪽 발의 76px 이동 폭과 마지막→첫 전환을 검증했다. 브라우저에서 실제 재생도 확인했다.
게임 빌드/실행은 사용자 요청에 따라 수행하지 않았으며 C++ 재생 상태 검증은
사용자가 실행할 수 있는 기존 --verify 경로에 추가했다.
