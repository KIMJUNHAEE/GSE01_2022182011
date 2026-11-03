# GLFW 프레임워크 전환

## 빌드와 실행

기존 작업 기준 커밋은 `1313dcc`다. GLFW 전환은 그 다음 별도 커밋으로 저장한다.

`SimpleGame.sln`을 열고 `SimpleGame`을 시작 프로젝트로 선택한다. Windows의 C++ 데스크톱 개발 도구와 Windows 10/11 SDK가 필요하다. x86/Win32·x64의 Debug/Release 구성을 지원한다. 설치된 Visual Studio의 기본 도구 집합을 사용하며 GLFW를 따로 설치하거나 빌드 도중 다운로드하지 않는다.

솔루션에서 `GLFW` 정적 라이브러리 프로젝트를 먼저 빌드하고 게임에 연결한다. 게임 프로젝트만 빌드해도 프로젝트 참조를 통해 GLFW를 같이 빌드한다. SDK의 OpenGL과 기존 GLEW는 유지하며 FreeGLUT의 링크와 DLL 복사 의존성은 제거했다.

새 실행 파일 위치:

```text
out/msbuild/bin/x64/Debug/SimpleGame.exe
out/msbuild/bin/x64/Release/SimpleGame.exe
out/msbuild/bin/Win32/Debug/SimpleGame.exe
out/msbuild/bin/Win32/Release/SimpleGame.exe
```

예전 `x64/Debug` 또는 `Debug` 폴더의 실행 파일은 갱신되지 않는다. 이 파일을 직접 실행하면 이전 FreeGLUT 버전이 열린다. 새 경로나 Visual Studio의 시작 프로젝트를 통해 실행한다.

CMake 3.24+와 Ninja를 사용하는 MSVC 경로도 제공한다. 자세한 명령과 라이브러리 패키징 정보는 `SimpleGame/Dependencies/GLFW_INTEGRATION.md`에 있다. 현재 게임의 GDI 글꼴·WinHTTP 등은 Windows 전용이므로 이번 변경이 Linux/macOS 게임 포팅까지 포함하는 것은 아니다.

## 포함한 라이브러리

- `SimpleGame/Dependencies/GLFW/`: 공식 GLFW 3.5.1 전체 소스, 헤더, CMake 파일, 문서, 예제, 테스트, 라이선스. 로컬 Visual Studio 프로젝트는 그 아래 `Build/`에 분리했다.
- `SimpleGame/Dependencies/GLFW.UPSTREAM.json`: 고정 태그·커밋·아카이브 해시와 원본 파일 159개의 SHA-256 기록.
- `SimpleGame/Dependencies/GLEW/bin/Win32/glew32.dll`, `bin/x64/glew32.dll`: 기존 실행에 사용하던 정확한 GLEW DLL을 플랫폼별로 묶었다. 원본 import 라이브러리·헤더는 기존 위치에 유지한다.
- `SimpleGame/Dependencies/GLEW/runtime-manifest.json`: DLL과 import 라이브러리 해시, PE 아키텍처, 출처 기록. 해당 DLL 두 개만 Git 추적 예외를 두어 커밋에 포함한다.

GLFW는 게임과 같은 아키텍처·설정의 정적 라이브러리로 생성한다. 별도 `glfw3.dll`이나 FreeGLUT DLL은 필요 없다. 빌드 후 셰이더·리소스·라이선스와 올바른 아키텍처의 `glew32.dll`만 실행 폴더에 배치한다. Blender 원본·백업과 미리보기는 모든 아트 팩에서 배포 대상에서 제외했다. 원본 파일을 삭제한 것은 아니다.

## 입력·창 처리

- WASD·방향키, 양쪽 Shift 달리기, 좌클릭 발사, 휠 확대/축소, E/Q/M/J와 대화·메뉴 입력을 유지한다.
- 기본 전체화면, F11·Alt+Enter 전환과 창 위치/크기 복원, 현재 창과 가장 많이 겹치는 모니터 선택을 지원한다.
- `--windowed`, `--fullscreen`, `--verify`, `--output`, `--no-monitor`, `--monitor-port`를 유지한다.
- 키 반복 이벤트는 무시하지만 눌림/해제 상태와 짧은 클릭은 유지한다. Shift 두 개를 따로 추적하고, 포커스 상실·최소화 시 입력을 초기화하고 탐험을 일시정지한다.
- 게임은 `Input.h`의 독립 키 열거형을 사용한다. GLFW 숫자 키 코드를 게임 로직에 직접 전달하지 않는다.
- 창의 논리 좌표와 실제 framebuffer 픽셀 좌표를 구분한다. 렌더러와 마우스가 `WindowMetrics.h`의 같은 정수 viewport 계산을 사용하여 DPI 배율·레터박스·홀수 해상도에서 위치가 어긋나지 않게 한다.
- 모니터 작업 영역으로 계산한 창 크기는 생성 후 다시 적용하여 150~200% DPI에서 크기가 이중 확대되지 않게 했다. 숨김 검증 창은 생성 시 DPI 자동 확대를 끈다.
- 최소화나 0 크기 화면에서는 렌더링하지 않는다. 창 크기 변경이나 전체화면 전환으로 메시·텍스처·셰이더 캐시나 내부 1440×900 렌더 타깃을 새로 만들지 않는다.
- GLFW C 콜백에서 발생한 C++ 예외는 저장해 메인 루프에서 처리한다. 종료 시 모니터링 작업과 게임·GPU 리소스를 먼저 정리한 뒤 창/컨텍스트를 파괴한다.

## 성능 확인

기존 16ms 타이머를 화면 교체 동기화와 함께 기다리는 구조를 제거했다. 활성 창은 이벤트 처리 → 게임 갱신 → 렌더링 → VSync 화면 교체를 한 번씩 수행한다. 포커스를 잃거나 최소화되면 이벤트 대기를 사용해 바쁜 루프와 불필요한 GPU 작업을 피하며, 모니터링은 기존 500ms 제한에 따라 계속 갱신한다.

기존 드로우콜 집계·배칭·CPU/GPU 메시 캐시와 OpenGL 3.3 compatibility profile은 유지한다. FPS 측정은 FreeGLUT 시간 함수 대신 단조 증가 시계를 사용한다. 성능 로그에 다음 항목을 추가했다.

깊이 버퍼는 기존 장면 FBO에만 두고, 최종 2D 출력용 기본 framebuffer에는 별도 depth/stencil/MSAA를 요구하지 않는다. GLFW의 기본 depth/stencil 설정 때문에 고해상도 화면에서 불필요한 버퍼가 추가되지 않도록 했다.

```text
[Perf] FPS: ... DrawCalls/frame(avg): ... Frame(ms): ... CPU(ms): ... Swap/wait(ms): ...
```

`CPU(ms)`는 이벤트 처리·게임 갱신·모니터링 snapshot·렌더 명령 제출 시간을 포함한다. GPU 자체의 실행 시간이나 GPU 타이머 결과는 아니다. `Swap/wait(ms)`는 화면 교체/동기화 호출에 걸린 시간이다. 포커스 전환 시 집계 구간을 초기화해 백그라운드에 머문 시간을 FPS 저하로 오인하지 않게 한다.

사용자 빌드 후 성능을 확인할 때는 같은 아키텍처의 **Release**와 같은 해상도·장면·줌을 사용한다. 로딩 직후를 제외하고 여러 집계 구간의 FPS, CPU 시간, 드로우콜을 비교한다. VSync는 기본으로 켜져 있다. GPU 여유 성능을 확인할 때만 `--no-vsync`를 사용한다. 이 옵션은 프레임 상한을 없애므로 GPU 사용률이 높아질 수 있다. 고정 프레임 대기와 VSync를 동시에 넣지 않는다.

이 작업 중에는 사용자 요청에 따라 **게임 빌드·실행, CMake configure, 컴파일러 실행과 FPS 실측을 하지 않았다**. 성능 저하가 없다고 실측 보장하는 것은 아니며, 구조 점검과 아래 정적 검사를 수행했다.

## 검증

빌드 없이 수행할 수 있는 검사:

```powershell
python -B Tools/VerifyGlfwMigration.py
python -B Tools/VerifyCompanionMotion.py
python -B Tools/VerifyLowPolyAssets.py
python -B Tools/VerifyPlanetLayout.py
```

GLFW 검사에는 원본 파일 해시, 플랫폼별 GLEW PE/해시, 프로젝트 구성과 소스 목록 일치, FreeGLUT 활성 참조 제거, 오프라인 의존성 구성, 컨텍스트 수명·루프 순서의 정적 점검과 DPI/viewport의 독립 수치 모델 검사를 포함한다. `--require-tracked`를 붙이면 모든 GLFW/GLEW 파일이 Git에 포함됐는지도 확인한다. 이 검사는 컴파일이나 실제 창 동작을 대신하지 않는다.

사용자 빌드 후 `SimpleGame.exe --verify --no-monitor`로 실제 C++/OpenGL 검증을 실행한다. 키 매핑·DPI 좌표와 GLFW resize 검사를 기존 검증 경로에 연결했다. 전체화면 전환, 마우스 조준, 키/버튼 해제, Alt+Tab·최소화·복원은 창을 보며 별도로 확인한다.

빌드에서 제외된 과거 예제 `SimpleGame/SimpleGame.cpp`와 예전 FreeGLUT 라이브러리 파일은 보존했다. 활성 게임은 `Main.cpp`를 사용하며 해당 예제를 링크하지 않는다.

## 기준 자료

- [GLFW 공식 배포](https://www.glfw.org/download)
- [GLFW 창·framebuffer·전체화면 가이드](https://www.glfw.org/docs/latest/window_guide.html)
- [GLFW 입력·이벤트 처리 가이드](https://www.glfw.org/docs/latest/input_guide.html)
- [GLFW 컨텍스트 수명 가이드](https://www.glfw.org/docs/latest/context_guide.html)
- [GLFW 소스 빌드 가이드](https://www.glfw.org/docs/latest/compile_guide.html)
