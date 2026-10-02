<p align="center">
  <img src="packaging/mikmick.svg" width="96" alt="MikMick logo">
</p>

<h1 align="center">믹믹 (MikMick)</h1>

<p align="center">
  <b>리눅스를 위한 올인원 화면 캡처 · 이미지 편집 · 디자인 도구</b><br>
  캡처하고, 편집하고, 공유하는 일을 한 곳에서.
</p>

<p align="center">
  <a href="https://github.com/KaiHT-Ladiant/MikMick/actions/workflows/ci.yml"><img src="https://github.com/KaiHT-Ladiant/MikMick/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="MIT License"></a>
  <img src="https://img.shields.io/badge/python-3.10%2B-3776AB.svg" alt="Python 3.10+">
  <img src="https://img.shields.io/badge/Qt-PySide6-41CD52.svg" alt="PySide6">
  <img src="https://img.shields.io/badge/platform-Linux-FCC624.svg" alt="Linux">
</p>

---

Windows 에서 오랫동안 사랑받아 온 [픽픽(PicPick)](https://picpick.app/ko/)은 공식적으로 Mac/Linux 버전 계획이 없습니다.
**믹믹(MikMick)** 은 픽픽의 사용 경험(리본 스타일 에디터, 다양한 캡처 모드, 그래픽 도구 모음, 옵션 구성)을
리눅스 데스크톱에서 그대로 누릴 수 있도록 처음부터 새로 만든 **무료 오픈소스** 프로그램입니다.

> 믹믹은 PicPick 및 NGWIN 과 제휴 관계가 없는 독립 프로젝트이며, PicPick 의 코드나 리소스를 사용하지 않습니다.

![믹믹 에디터](docs/screenshots/editor.png)

## 주요 기능

### 화면 캡처 도구
| 모드 | 설명 |
| --- | --- |
| 전체화면 캡처하기 | 다중 모니터를 포함한 전체 화면 |
| 활성화된 윈도우 캡처 | 현재 포커스된 창 (X11: `xdotool`) |
| 윈도우 컨트롤 캡처 | 마우스로 가리킨 창을 하이라이트하여 선택 |
| 자동 스크롤 캡처 | 스크롤하며 여러 장을 찍어 겹치는 부분을 찾아 자동으로 이어 붙임 |
| 영역을 지정하여 캡처 | 드래그로 영역 지정, 화면 확대창(돋보기)과 픽셀 좌표·색상 표시 |
| 고정된 사각 영역 캡처 | 지정한 크기의 사각형을 원하는 위치에 놓고 캡처 (휠로 크기 조절) |
| 내 마음대로 캡처하기 | 자유 곡선으로 영역을 그려 캡처 (바깥은 투명 처리) |
| 마지막 캡처 영역 반복 | 직전에 캡처한 영역을 다시 캡처 |

- 캡처 지연 시간, 마우스 커서 포함, 효과음, 항상 클립보드에 저장
- 캡처 결과: 믹믹 에디터 / 클립보드 / 파일로 저장 / 자동 저장 / FTP 전송 / 외부 프로그램 연결
- 자동 저장 파일 이름 패턴: `%y %m %d %h %n %s %c %u %w %t`

### 이미지 에디터 (리본 UI)
- **파일(백스테이지)**: 시작, 새로 만들기(프리셋·캔버스 크기·배경색), 열기, 저장, 다른 이름으로 저장, 인쇄, 공유, 썸네일, 닫기, 옵션, 정보
- **홈**: 클립보드(붙여넣기/잘라내기/복사), 이미지(효과/크기 조절/회전/자르기), 도구(이동/선택/그리기/채우기/텍스트/스탬프/도형), 선 굵기, 색1·색2, 팔레트
- **공유**: 클립보드, 이메일(`xdg-email`), FTP, 외부 프로그램, 기본 앱으로 열기
- **보기**: 확대/축소, 100%, 화면에 맞추기, 단색/격자 배경, 상태 표시줄, 개체 병합
- 도형·화살표·말풍선·펜·형광펜·텍스트·번호 스탬프는 **개체로 유지**되어 이동/삭제/실행 취소가 가능하고, 저장하거나 효과를 적용할 때 이미지에 병합됩니다.
- 효과: 흐리게, 선명하게, 모자이크, 무채화, 색반전, 밝기/대비, 색조/채도, 테두리, 그림자, 워터마크
- 저장 형식: PNG, JPEG(화질 설정), BMP, GIF, WebP, TIFF, PDF

### 그래픽 도구
| 도구 | 설명 |
| --- | --- |
| 색상 추출 도구 | 화면의 픽셀 색상을 돋보기와 함께 추출, HTML/HEX/RGB/HSB/C++/Delphi 형식 복사 |
| 색상 팔레트 | RGB/HSV 색상 편집, 자주 쓰는 색 저장 |
| 돋보기 | 마우스 주변을 실시간 확대 (휠로 배율 변경) |
| 눈금자 | 가로/세로 전환, 픽셀·인치·센티미터, DPI 설정 |
| 십자선 | 절대 좌표 및 기준점 대비 상대 좌표·거리 |
| 각도기 | 세 점을 찍어 각도 측정 |
| 프리젠테이션 도구 | 바탕 화면 위에 펜·형광펜·도형으로 직접 그리기 (투명/화면/흰색/검정 배경) |

### 기타
- 알림 영역(트레이) 아이콘 메뉴, 단일 인스턴스, 로그인 시 자동 실행
- 전역 단축키 (X11, 기본값은 픽픽과 동일: `PrintScreen`, `Alt+PrintScreen`, `Shift+PrintScreen` …)
- 업데이트 확인 (GitHub Releases)

## 스크린샷

| 시작 화면 | 새로 만들기 |
| --- | --- |
| ![시작](docs/screenshots/start.png) | ![새로 만들기](docs/screenshots/new.png) |

| 영역 지정 캡처 | 옵션 - 단축키 |
| --- | --- |
| ![영역 캡처](docs/screenshots/capture_region.png) | ![옵션 단축키](docs/screenshots/options_hotkeys.png) |

## 설치

### 1. 시스템 패키지

```bash
# Debian / Ubuntu / Kali
sudo apt install python3 python3-venv pipx libxcb-cursor0 xdotool x11-utils

# Fedora
sudo dnf install python3 pipx xcb-util-cursor xdotool xprop xwininfo

# Arch
sudo pacman -S python python-pipx xcb-util-cursor xdotool xorg-xprop xorg-xwininfo
```

> `xdotool`, `xprop`, `xwininfo` 는 X11 에서 활성 창 캡처·창 선택·자동 스크롤 캡처에 사용됩니다.

### 2. 믹믹 설치

```bash
git clone https://github.com/KaiHT-Ladiant/MikMick.git
cd MikMick
./scripts/install.sh          # pipx(없으면 가상환경)로 설치 + 앱 메뉴/아이콘 등록
```

또는 직접:

```bash
pipx install "mikmick[full] @ git+https://github.com/KaiHT-Ladiant/MikMick.git"
```

제거: `./scripts/install.sh --uninstall`

## 사용법

```bash
mikmick                          # 에디터(시작 화면) 실행
mikmick --tray                   # 알림 영역에만 표시
mikmick image.png                # 이미지 열기
mikmick --capture region         # 영역 지정 캡처
mikmick --capture fullscreen --delay 3000
mikmick --tool color_picker      # 색상 추출 도구
mikmick --options                # 옵션
```

캡처 모드: `fullscreen`, `active_window`, `window_control`, `scroll`, `region`, `fixed`, `freehand`, `repeat_last`
그래픽 도구: `editor`, `color_picker`, `palette`, `magnifier`, `ruler`, `crosshair`, `protractor`, `whiteboard`

믹믹이 이미 실행 중이면 명령은 실행 중인 인스턴스로 전달됩니다.

### X11 / Wayland 지원 현황

| 기능 | X11 | Wayland |
| --- | :---: | :---: |
| 전체화면 / 영역 / 고정 영역 / 자유형 캡처 | ✅ | ✅ (xdg-desktop-portal 또는 `grim`, `gnome-screenshot`, `spectacle`) |
| 활성 창 / 창 선택 캡처 | ✅ | ⚠️ 영역 지정으로 대체 |
| 자동 스크롤 캡처 | ✅ | ❌ |
| 실시간 돋보기 | ✅ | ⚠️ 정지 화면 기준 |
| 전역 단축키 | ✅ (`pynput`) | ⚠️ 데스크톱 설정에서 등록 |

**Wayland(GNOME/KDE) 단축키 등록 예시**: 설정 → 키보드 → 사용자 지정 단축키에서
명령 `mikmick --capture region`, 단축키 `Shift+Print` 처럼 등록하세요.
GNOME 은 기본적으로 `Print` 키를 자체 스크린샷에 사용하므로 기존 단축키를 해제해야 할 수 있습니다.

## 설정 파일

`~/.config/mikmick/config.json` 에 저장됩니다. 자동 저장 기본 폴더는 `~/Pictures/MikMick` 입니다.

## 개발

```bash
python3 -m venv .venv && . .venv/bin/activate
pip install -e ".[dev,full]"
QT_QPA_PLATFORM=offscreen pytest -q
ruff check .
python -m mikmick
```

README 스크린샷 다시 만들기: `QT_QPA_PLATFORM=offscreen python scripts/make_screenshots.py`

```
mikmick/
├── app.py            # 진입점, 트레이, 단일 인스턴스(IPC), 캡처 결과 처리
├── capture/          # 화면 획득 백엔드(X11/Wayland), 선택 오버레이, 스크롤 캡처
├── editor/           # 리본 에디터, 백스테이지, 캔버스·개체, 효과
├── tools/            # 색상 추출/팔레트, 돋보기, 눈금자, 십자선, 각도기, 프리젠테이션
├── options.py        # 옵션 대화상자 (8개 페이지)
├── outputs.py        # 저장 / 클립보드 / FTP / 외부 프로그램
└── icons.py          # QPainter 벡터 아이콘
```

## 로드맵

- [ ] 화면 녹화 (PipeWire / ffmpeg)
- [ ] 클라우드 업로드 (Google 드라이브, 드롭박스 등)
- [ ] 개체 크기 조절 핸들, 레이어 순서 변경
- [ ] Flatpak / AppImage / .deb 패키지
- [ ] 다국어(영어) UI

## 만든 사람

- **Kai_HT** ([@KaiHT-Ladiant](https://github.com/KaiHT-Ladiant))

## 라이선스

[MIT](LICENSE) © 2026 Kai_HT
