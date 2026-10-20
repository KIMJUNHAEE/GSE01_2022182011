# Aseprite 로컬 제작 도구

사용자가 지정한 [aseprite-builder](https://github.com/a1393323447/aseprite-builder)의 Windows
워크플로를 확인하고 로컬 PowerShell 환경에 맞춰 빌드했다. 확인한 빌더 커밋은
`6b15bb5f992fc9fa176dae0ae6bd367fa9564bf1`이다.
GitHub 계정의 포크/Actions/Release는 만들지 않았다.

- 공식 Aseprite `v1.3.18.6` 소스 ZIP, SHA-256을 고정하여 확인.
- 빌더와 동일한 공식 Skia `m124-08a5439a6b`, Windows x64 Release.
- Visual Studio 2026 C++ / Windows SDK 10.0.26100.0 / CMake / Ninja.
- Release, Skia 백엔드, Lua scripting 활성화. 게임 프로젝트 빌드와는 별개다.
- 로컬 결과의 버전 표기는 `Aseprite 1.3.18.6-dev`다.

개인 PC용 실행 파일은 `%LOCALAPPDATA%\StarDrifterTools\Aseprite\1.3.18.6\aseprite.exe`에
설치되어 있다. 빌드 파일과 의존성은 `%TEMP%\star-drifter-aseprite`에 있다.
실행 파일은 프로젝트 Git에 포함하지 않으며 설치 폴더에 원본 EULA와 라이선스 자료를 유지한다.

## 다시 제작

프로젝트 루트의 PowerShell에서:

```powershell
./Tools/Aseprite/RebuildProtagonist.ps1
```

다른 설치 파일을 사용하려면 `-AsepritePath '설치 경로/aseprite.exe'`를 지정한다.
기본 참조는 기존 3×3 시트의 첫 대기 포즈다. 생성 스크립트는 현재 스프라이트 출력들을
다시 만들므로 원본을 수작업으로 편집한 경우 별도 파일로 먼저 보관한다.
개인 편집 원본은 루트의 `OpenProtagonistInAseprite.cmd`로 열 수 있다.

`BuildProtagonist.lua`에서 다리 마스크, 관절, 보폭, 발 들림, 팔/코트 움직임을 조절한다.
스크립트는 PNG를 Aseprite로 읽고 레이어별 33프레임 문서를 작성하고 walk 태그의
32프레임 PNG+JSON 및 GIF를 내보낸다. 중간 파일은 한글 경로 문제가 없는 임시 폴더에서
만든 뒤 프로젝트에 복사한다.
Windows GUI 프로그램인 Aseprite는 Start-Process -Wait로 종료와 오류 코드를 확인한다.

`ValidateProtagonist.lua`는 실제 Aseprite 문서를 다시 열어 레이어·태그 범위, 포즈 중복,
고정 캔버스, 투명 여백, 얼굴 일관성, 다리 픽셀 변화와 주기 경계를 점검한다.
검증에 실패하면 프로젝트 리소스 복사를 중단한다.

## 도구 재빌드와 설치

```powershell
./Tools/Aseprite/BuildAseprite.ps1
./Tools/Aseprite/InstallLocal.ps1
```

다운로드와 컴파일에 시간이 걸리며 C++ 데스크톱 개발 도구가 필요하다.
게임은 이 빌드에 포함하지 않는다. 도구는 개인 로컬 사용용이며 실행 파일을 재배포하지 않는다.

공식 참고: [빌드 안내](https://github.com/aseprite/aseprite/blob/v1.3.18.6/INSTALL.md),
[CLI](https://aseprite.org/docs/cli/), [스크립팅 API](https://aseprite.org/api/).
