# 🪟 System Transparency

**System Transparency**는 Windows 환경에서 탐색기 창 및 메뉴를 자동으로 반투명 처리하여  
작업 환경을 깔끔하고 모던하게 만들어주는 경량 유틸리티입니다.  
트레이 아이콘과 단축키를 활용해 실시간 투명도 조절도 가능합니다.

---

<p align="center">
  <img src="https://img.shields.io/badge/C-100%25-blue?style=flat-square" />
  <img src="https://img.shields.io/badge/GCC-supported-orange?style=flat-square" />
  <img src="https://img.shields.io/badge/Platform-Windows-blue?style=flat-square&logo=windows" />
  <img src="https://img.shields.io/badge/Installer-NSIS-purple?style=flat-square" />
  <a href="https://github.com/sunwookim05/Transparent-window/commits/main">
    <img src="https://img.shields.io/github/commit-activity/m/sunwookim05/Transparent-window?style=flat-square"/>
  </a>
  <a href="https://github.com/sunwookim05/Transparent-window/issues">
    <img src="https://img.shields.io/github/issues/sunwookim05/Transparent-window?style=flat-square" />
  </a>
  <a href="https://github.com/sunwookim05/Transparent-window/pulls">
    <img src="https://img.shields.io/github/issues-pr/sunwookim05/Transparent-window?style=flat-square" />
  </a>
</p>

<p align="center">
  <a href="https://github.com/sunwookim05/Transparent-window/releases">
    <img src="https://img.shields.io/github/v/release/sunwookim05/Transparent-window?label=release&style=flat-square" />
  </a>
  <a href="https://github.com/sunwookim05/Transparent-window/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/sunwookim05/Transparent-window?style=flat-square" />
  </a>
  <img src="https://img.shields.io/github/stars/sunwookim05/Transparent-window?style=flat-square" />
  <img src="https://img.shields.io/github/forks/sunwookim05/Transparent-window?style=flat-square" />
</p>

---

## 📦 Latest Release
- 🔖 **Release Notes & Download**  
  👉 [Releases](https://github.com/sunwookim05/Transparent-window/releases)

---

## 🛠️ Built With
- **Language:** C (Win32 API)  
- **Compiler:** GCC (MinGW)  
- **Platform:** Windows 10 / Windows 11 (x64)  
- **Installer:** NSIS  
- **Type:** Tray-based Desktop Utility

---

## ✨ 주요 기능

### 1️⃣ 자동 투명화
- Windows 탐색기(`CabinetWClass`, `ExploreWClass`) 창 자동 반투명화
- Windows 11 메뉴 및 컨텍스트 메뉴도 지원
- 별도 조작 없이 시작 시 자동 적용

### 2️⃣ 수동 투명화 및 조절
- **Ctrl + 마우스 가운데 클릭** → 현재 활성 창 반투명화  
- **Win + 마우스 가운데 클릭** → 현재 활성 창 불투명화  
- **Ctrl + Win + 마우스 휠** → 창 투명도 단계별 조절 (최소 60, 최대 255)

### 3️⃣ 트레이 아이콘
- 시스템 트레이에 상주하며 쉽게 접근 가능
- 우클릭 메뉴 제공:
  - Explorer 자동 투명화 ON/OFF
  - 투명도 프리셋 선택 (Solid / Soft / Glass / Ghost)
  - 프로그램 정보 확인
  - GitHub 페이지 이동
  - 프로그램 종료

### 4️⃣ 프리셋 투명도
- **Solid:** 255 (완전 불투명)  
- **Soft:** 200  
- **Glass:** 150  
- **Ghost:** 80 (강한 반투명)

---

## ⚙️ 설치 및 실행
1. [Releases 페이지](https://github.com/sunwookim05/Transparent-window/releases)에서 `Setup.exe` 다운로드
2. 설치 후 자동 실행 (Windows 시작 프로그램 등록)
3. 트레이 아이콘 표시 → 자동 투명화 적용
4. 필요 시 단축키/마우스 조합으로 창 투명도 조절 가능

---

## 🔄 Auto Startup
- 설치 시 Windows 시작 프로그램에 자동 등록
- Windows 로그인 시 자동 실행
- 백그라운드 트레이 앱으로 동작

---

## ⚠️ 주의 사항
- 일부 프로그램(게임, 보안 프로그램, GPU 가속 앱)에서는 투명화가 제한될 수 있음
- 기본적으로 Windows 탐색기 및 메뉴에 최적화
- 그 외 창은 수동 조절 권장

---

## 🏷️ Tags
`windows` `windows11` `win32` `transparency` `tray-app`  
`system-utility` `installer` `startup` `autostart` `nsis`

---

## 👨‍💻 개발자 정보
- 개발자: **sunwookim05**  
- GitHub: [sunwookim05/Transparent-window](https://github.com/sunwookim05/Transparent-window)  
- Instagram: [@sunwookim05](https://www.instagram.com/sunwookim05/)  
- Email: sunwookim052@gmail.com  
- License: **MIT**
