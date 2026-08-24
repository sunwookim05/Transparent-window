# System Transparency

🌍 [English](https://github.com/sunwookim05/Transparent-window/blob/main/README.md) | [한국어](https://github.com/sunwookim05/Transparent-window/blob/main/translations/ko.md)

System Transparency는 Windows Explorer와 일부 시스템 창에 실시간으로 조절 가능한 투명도를 적용해 더 깔끔한 느낌을 주는 가벼운 Windows 트레이 유틸리티입니다.

C와 Win32 API로 작성되었습니다. 앱은 단일 실행 파일로 배포되며, 첫 실행 시 사용자 프로필에 스스로 설치하고, 필요한 인증서를 등록하고, 최고 권한 시작 작업을 만들고, GitHub Releases를 통해 업데이트를 유지할 수 있습니다.

<p align="center">
  <img src="https://img.shields.io/badge/C-100%25-blue?style=flat-square" />
  <img src="https://img.shields.io/badge/Win32-API-blue?style=flat-square&logo=windows" />
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-blue?style=flat-square" />
  <img src="https://img.shields.io/github/v/release/sunwookim05/Transparent-window?label=release&style=flat-square" />
  <img src="https://img.shields.io/github/license/sunwookim05/Transparent-window?style=flat-square" />
</p>

## 기능

- 지원되는 Windows Explorer 및 메뉴 창에 자동 투명도 적용.
- 설정 가능한 단축키로 수동 투명도 조절:
  - 기본값 `Ctrl + Middle Click`: 활성 창에 선택한 프리셋 적용.
  - 기본값 `Win + Middle Click`: 활성 창을 완전 불투명으로 복원.
  - 기본값 `Ctrl + Win + Mouse Wheel`: 활성 창의 투명도를 단계별로 조절.
- 어두운 owner-drawn 스타일의 트레이 메뉴.
- 영어/한국어 UI 언어 지원.
- 투명도 프리셋:
  - `Solid`: 255
  - `Soft`: 200
  - `Glass`: 150
  - `Ghost`: 80
  - `Custom Alpha`: 60부터 255까지 사용자 지정 값.
- Custom Alpha 다이얼로그:
  - 어두운 UI
  - 드래그 가능한 슬라이더
  - 실시간 숫자 값
  - 조절 중 선택한 창에 실시간 미리보기 적용.
- 트레이 설정 메뉴의 어두운 제거 확인 다이얼로그.
- 단일 exe 첫 실행 설정:
  - `%LOCALAPPDATA%\SystemTransparency\SystemTransparency.exe`로 자기 자신 복사
  - 필요 시 UAC 요청
  - 포함된 인증서 등록
  - 최고 권한으로 실행되는 Windows 시작 예약 작업 생성.
- GitHub Releases 기반 자동 업데이트.
- 트레이 메뉴에서 업데이트 로그 바로 열기.

## 설치

1. 최신 릴리즈에서 `SystemTransparency.exe`를 다운로드합니다.
2. 한 번 실행합니다.
3. 앱이 첫 실행 설정을 수행할 때 UAC 프롬프트를 승인합니다.
4. 설정 후 설치된 실행 파일은 다음 위치에서 시작됩니다:

```text
%LOCALAPPDATA%\SystemTransparency\SystemTransparency.exe
```

이후 앱은 시스템 트레이에서 실행되며 다음 로그인부터 자동으로 시작됩니다.

## 사용법

트레이 아이콘을 우클릭해 메뉴를 엽니다.

- `설정 > Explorer 자동 투명화`: 지원되는 Explorer 창의 자동 투명도를 켜거나 끕니다.
- `설정 > 시작 시 실행`: 시작 예약 작업을 켜거나 끕니다.
- `설정 > 단축키...`: 수동 투명도 단축키에 사용할 보조 키를 녹화합니다.
- `설정 > 언어`: 시스템 기본값, English, 한국어 중 선택합니다.
- `설정 > 프리셋`: 자동 모드와 프리셋 적용 단축키에서 사용할 투명도 수준을 선택합니다.
- `설정 > 프리셋 > 사용자 지정 투명도...`: 60부터 255까지 사용자 지정 불투명도 값을 선택합니다.
- `설정 > 제거`: 설치된 실행 파일, 시작 작업, 앱 레지스트리 설정, 포함된 인증서 등록을 제거합니다.
- `업데이트 확인`: 최신 GitHub Release를 수동으로 확인합니다.
- `로그 열기`: 업데이트 로그 파일을 엽니다.

## 업데이트

System Transparency는 GitHub Releases를 확인하고 새 버전의 `SystemTransparency.exe`가 있으면 설치합니다.

업데이트 과정은 단일 exe 배포 모델에 맞춰 설계되었습니다. 실행 중인 앱이 릴리즈 asset을 다운로드하고, 설치된 실행 파일 교체를 예약한 뒤, 업데이트된 버전으로 다시 시작합니다.

## 빌드

이 프로젝트는 GCC/MinGW와 Windows resource compiler를 사용합니다.

```powershell
windres res\app.rc -O coff -o build\app_res.o
gcc src\main.c src\App.c src\Installer.c src\Updater.c src\Settings.c src\Transparency.c src\Tracker.c src\thread.c src\System.c src\Scanner.c src\console.c src\algorithm.c build\app_res.o -Iinc -o SystemTransparency.exe -mwindows -luser32 -lshell32 -lcomctl32 -lpsapi -lwinhttp -lshlwapi -ldwmapi -Wall -Wextra
```

## 참고

- 보호된 창, 게임 창, GPU 가속 창, 보안에 민감한 창은 투명도 변경을 거부할 수 있습니다.
- 앱은 Windows Explorer와 일반적인 셸 창에 최적화되어 있습니다.
- 첫 실행 설정, 인증서 등록, 최고 권한 시작 작업 등록에는 관리자 권한이 필요합니다.

## 라이선스

MIT License. [LICENSE](../LICENSE)를 참고하세요.

## 제작자

- GitHub: [sunwookim05](https://github.com/sunwookim05)
- Repository: [sunwookim05/Transparent-window](https://github.com/sunwookim05/Transparent-window)
