import sys
import os
import shutil
from tkinter import Tk, filedialog

from build_fonts import build_font
import sneg_ui_strings

# 출력이 파일/파이프로 갈 때 cp949 로 못 쓰는 글자(이모지)에서 멈추지 않게
try:
    sys.stdout.reconfigure(errors='replace')
except Exception:
    pass


def resource_path(relative_path):
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

embedded_data_path = resource_path('Data')
if not os.path.exists(embedded_data_path) or not os.path.isdir(embedded_data_path):
    print("패치 데이터를 확인할 수 없습니다.")
    sys.exit()


# ======================================================================
#  SNEG 재출시판 (2026, Steam 등): languages\ko 한국어 언어팩으로 설치
#  - 원본 파일은 건드리지 않는다 (Data\Bink 에 자막 .sbv 만 추가)
#  - 게임 설정창의 언어 목록에 '한국어' 로 나오고, 선택하면 다음 실행부터 적용
# ======================================================================
KO_LANG = 'ko'
SNEG_CJK_FONT = 'MaruBuri-Regular.ttf'          # version.dll 이 ImGui CJK 글꼴로 읽는 파일 (languages\ko\)
SNEG_CJK_FONT_LICENSE = 'MaruBuri-OFL.txt'     # 있으면 같이 복사


def is_sneg(gamePath):
    """'sneg_ui' 폴더가 있으면 SNEG 재출시판(2026, Steam 등), 없으면 2001년 원본."""
    return os.path.isdir(os.path.join(gamePath, 'sneg_ui'))


def check_por2(dirpath):
    """SNEG 재출시판 게임 폴더인지 확인. 아니면 이유를 출력하고 False."""
    if not os.path.isdir(dirpath):
        return False
    datapath = os.path.join(dirpath, 'Data')
    for d in ('Bink', 'Dialogue', 'Fonts', 'STR'):
        if not os.path.isdir(os.path.join(datapath, d)):
            return False
    if not os.path.isfile(os.path.join(dirpath, 'por2main.exe')):
        return False
    if not is_sneg(dirpath):
        print("2001년 원본으로 보입니다. 이 인스톨러는 SNEG 재출시판(Steam 등) 전용입니다.")
        return False
    for f in ('sneg.dll', 'sneg_ui_strings.dat'):
        if not os.path.isfile(os.path.join(dirpath, f)):
            print(f"게임 폴더에 {f} 가 없습니다.")
            return False
    return True


def get_folder_path():
    if len(sys.argv) > 1:
        gamePath = os.path.normpath(sys.argv[1])
        if check_por2(gamePath):
            return gamePath

    root = Tk()
    root.withdraw()
    root.attributes('-topmost', True)

    print("\\Pool of Radiance Ruins of Myth Drannor\\ 폴더, 실행파일(por2main.exe)이 있는 곳을 선택하세요.")
    selected_path = filedialog.askdirectory(title="Pool of Radiance 게임 폴더를 선택하세요")
    if not selected_path:
        print("폴더가 선택되지 않아 프로그램을 종료합니다.")
        sys.exit()

    gamePath = os.path.normpath(selected_path)
    if check_por2(gamePath):
        return gamePath

    print(f"{gamePath}는 올바른 Pool of Radiance(SNEG 재출시판) 폴더가 아닙니다.")
    sys.exit()


def askYN(prompt):
    while True:
        user_input = input(f"{prompt} (y/n): ").strip().lower()
        if user_input == 'y':
            return True
        if user_input == 'n':
            return False
        print("잘못된 입력입니다. 'y' 또는 'n'만 입력해 주세요.")


def copyFiles(src_dir, dst_dir, ext):
    os.makedirs(dst_dir, exist_ok=True)
    copied_count = 0
    for file_name in os.listdir(src_dir):
        if file_name.lower().endswith(ext):
            shutil.copy2(os.path.join(src_dir, file_name), os.path.join(dst_dir, file_name))
            copied_count += 1
    return copied_count


def getKoPath(gamePath):
    return os.path.join(gamePath, 'languages', KO_LANG)


def prepareKoFolder(gamePath):
    koPath = getKoPath(gamePath)
    if os.path.isdir(koPath) and os.listdir(koPath):
        print(f"\n'{koPath}' 폴더가 이미 있습니다.")
        if askYN("기존 내용을 지우고 새로 설치할까요? (n 이면 덮어씁니다)"):
            shutil.rmtree(koPath)
    os.makedirs(koPath, exist_ok=True)
    return koPath


def installDll(gamePath):
    print("한글 출력용 DLL 파일을 복사합니다... ", end='')
    srcPath = os.path.join(embedded_data_path, 'version_sneg.dll')
    if not os.path.isfile(srcPath):
        print("실패: 패치 데이터에 version_sneg.dll 이 없습니다.")
        return False
    shutil.copy2(os.path.join(embedded_data_path, 'minhook.x32.dll'), gamePath)
    shutil.copy2(srcPath, os.path.join(gamePath, 'version.dll'))
    print("ok")
    return True


def snegLanguageIni(gamePath, hiRes):
    print(f"languages/{KO_LANG}/language.ini 를 만듭니다... ", end='')
    with open(os.path.join(getKoPath(gamePath), 'language.ini'), 'w', encoding='ascii', newline='\r\n') as f:
        f.write("[Language]\nCodePage=949\nRelease=Korean patch by wooddoll\n")
        # 글꼴 설정 (version.dll). steam_text_hooks.md 16절 참고
        #   TtfScale/TtfWeight : 게임 텍스트 고해상도 TTF (china_glyph.cpp)
        #   UiFontFile         : SNEG UI(ImGui) 글꼴 파일 (languages\ko 안)
        #   UiLatin            : maru = UI 영문도 마루 부리, retail = 게임 비트맵 영문
        #   [UiFontScale]      : UI 글자 크기 배율(%, 50~300). 역할 키 × 하위 키. 해상도와 무관
        #   [UiStyle]          : 행동 아이콘 툴팁 배경 불투명도(%, 0~100)
        #   [Render] NativeCjk : 1 = sneg 자체 CJK(china) 경로로 한글 출력 (0 = 예전 텍스트 훅)
        #            HiRes     : 1 = 고해상도 TTF 글 (config.ini [Text] ScaledEngineText=1 필요)
        #            CjkFontScale : china 글꼴 높이 배율(%, 50~150)
        f.write(f"\n[Render]\nNativeCjk=1\nHiRes={1 if hiRes else 0}\nCjkFontScale=71\n")
        f.write("\n[Font]\nTtfScale=100\nTtfWeight=400\n"
                "UiFontFile=MaruBuri-Regular.ttf\nUiLatin=maru\n")
        f.write("\n[UiFontScale]\nOverlay=100\nDialogue=100\nMenu=100\nInventory=100\n"
                "Action=100\nPrompt=100\nRadial=100\nSettings=100\n"
                "OverlayActionPreview=110\nOverlayItem=110\nOverlayStatus=110\n"
                "OverlayRollLog=90\nActionIconTooltip=100\n")
        f.write("\n[UiStyle]\nActionIconTooltipOpacity=50\n")
    print("ok")
    return True


def snegData(gamePath):
    koData = os.path.join(getKoPath(gamePath), 'data')
    print(f"languages/{KO_LANG}/data 폴더로 한글 대화/이름 파일을 복사합니다... ", end='')
    n1 = copyFiles(os.path.join(embedded_data_path, 'Dialogue'), os.path.join(koData, 'Dialogue'), '.por')
    n2 = copyFiles(os.path.join(embedded_data_path, 'STR'), os.path.join(koData, 'STR'), '.str')
    shutil.copy2(os.path.join(embedded_data_path, 'sfDat.dat'), os.path.join(getKoPath(gamePath), 'sfDat.dat'))
    print("ok")
    return n1 == 1398 and n2 == 44


def snegBink(gamePath):
    # 자막은 영상(.bik)과 같은 폴더에서 찾는다 (version.dll bink_hook). 영상은 원본 Data\Bink 에 있으므로 거기에 추가
    print("Data/Bink 폴더에 한글 자막 파일(.sbv)을 추가합니다... ", end='')
    n = copyFiles(os.path.join(embedded_data_path, 'Bink'), os.path.join(gamePath, 'Data', 'Bink'), '.sbv')
    print("ok")
    return n == 10


def buildFonts(srcFonts, dstFonts):
    os.makedirs(dstFonts, exist_ok=True)
    fonts = [
        ('font0', 'font/Umdot14.ttf_18'),
        ('font1', 'font/Umdot12.ttf_16'),
        ('font2', 'font/Umdot20-Bold.ttf_28'),
        ('font3', 'font/Galmuri9.ttf_13'),
        ('font4', 'font/Galmuri11.ttf_15'),
    ]
    ok = True
    for name, base in fonts:
        ok &= bool(build_font(name, srcFonts,
                              os.path.join(embedded_data_path, base + '.png'),
                              os.path.join(embedded_data_path, base + '.txt'),
                              dstFonts))
    return ok


def snegFont(gamePath):
    # 원본 Data\Fonts 를 바탕으로 한글 병합 폰트를 languages\ko\data\fonts 에 만든다
    srcFonts = os.path.join(gamePath, 'Data', 'Fonts')
    dstFonts = os.path.join(getKoPath(gamePath), 'data', 'fonts')
    print(f"languages/{KO_LANG}/data/fonts 폴더에 한글 폰트를 생성합니다... ")
    ok = buildFonts(srcFonts, dstFonts)

    # 설정창/툴팁 등 새 UI(ImGui)용 한글 글꼴
    print(f"languages/{KO_LANG} 폴더에 UI용 한글 글꼴({SNEG_CJK_FONT})을 복사합니다... ", end='')
    fontDir = os.path.join(embedded_data_path, 'font')
    shutil.copy2(os.path.join(fontDir, SNEG_CJK_FONT), getKoPath(gamePath))
    lic = os.path.join(fontDir, SNEG_CJK_FONT_LICENSE)
    if os.path.isfile(lic):
        shutil.copy2(lic, getKoPath(gamePath))
    shutil.copy2(os.path.join(embedded_data_path, 'sneg_ui_cjk_characters.txt'), getKoPath(gamePath))
    print("ok")
    return ok


def snegUiStrings(gamePath):
    # 게임 루트의 sneg_ui_strings.dat(영어)를 바탕으로, 패치에 넣은 한국어 dat 의 같은 키 값을 덮어쓴다.
    # 패치 dat 에는 번역문만 있다 (번역하지 않은 고유명사/자리표시자 몇 개 제외). 원문 영어는 넣지 않는다.
    # SNEG 업데이트로 게임에 새 키가 생기면 그 키는 게임의 영어 그대로 남는다.
    print(f"languages/{KO_LANG}/sneg_ui_strings.dat 를 만듭니다... ", end='')
    _, items = sneg_ui_strings.load(os.path.join(gamePath, 'sneg_ui_strings.dat'))
    _, koItems = sneg_ui_strings.load(os.path.join(embedded_data_path, 'sneg_ui_strings.dat'))
    ko = {k: (cp, v) for k, cp, v in koItems}
    replaced = 0
    for it in items:
        t = ko.pop(it[0], None)
        if t:
            it[1], it[2] = t
            replaced += 1
    items += [[k, cp, v] for k, (cp, v) in ko.items()]   # 게임에 없는 키 (ui.settings.language.ko 등)
    items.sort(key=lambda it: it[0])
    sneg_ui_strings.save(os.path.join(getKoPath(gamePath), 'sneg_ui_strings.dat'), KO_LANG, items)
    print(f"ok (번역 {replaced}/{len(items)})")
    return True


def writeIni(path, section, key, value):
    import ctypes
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return bool(ctypes.windll.kernel32.WritePrivateProfileStringW(section, key, value, path))


def askHiRes():
    print(r"""
----------------------------------------------------------------------
🔠 고해상도 글꼴
  켜면 대사·메뉴·이름 등 게임 글자를 화면 해상도에 맞춰 매끈한 TTF(마루 부리)로 그립니다.
  끄면 원작처럼 800×600 비트맵 글꼴(움돋/갈무리)을 확대해서 보여 줍니다.
  (실험 중인 기능이라 화면에 따라 어색한 곳이 있을 수 있습니다)

  나중에 끄려면 아래 두 값을 0 으로 바꾸거나, 인스톨러를 다시 실행해 n 을 고르세요.
    • <게임 폴더>\languages\ko\language.ini   [Render] HiRes=0
    • %LOCALAPPDATA%\SNEG\POR2\config.ini     [Text]   ScaledEngineText=0
----------------------------------------------------------------------""")
    return askYN("고해상도 글꼴을 사용할까요?")


def snegSettings(hiRes):
    # %LOCALAPPDATA%\SNEG\POR2 : config.ini [Text] CodePage=949 / ScaledEngineText, game_settings.ini [Text] Language=ko
    # ScaledEngineText: sneg 고해상도 글(*_ani) 스위치. 게임은 [Text] DefaultsRevision 이 0 이 아닐 때만 이 값을 읽는다.
    base = os.path.join(os.environ.get('LOCALAPPDATA', ''), 'SNEG', 'POR2')
    cfg = os.path.join(base, 'config.ini')
    print("게임 설정에 한국어(코드페이지 949)를 지정합니다... ", end='')
    ok = writeIni(cfg, 'Text', 'CodePage', '949')
    ok &= writeIni(cfg, 'Text', 'ScaledEngineText', '1' if hiRes else '0')
    ok &= writeIni(os.path.join(base, 'game_settings.ini'), 'Text', 'Language', KO_LANG)
    print("ok" if ok else "실패")
    if hiRes:
        import ctypes
        buf = ctypes.create_unicode_buffer(16)
        ctypes.windll.kernel32.GetPrivateProfileStringW('Text', 'DefaultsRevision', '0', buf, 16, cfg)
        if buf.value.strip() in ('', '0'):
            print("  ※ 게임을 아직 한 번도 실행하지 않은 것 같습니다. 고해상도 글꼴이 안 보이면")
            print("    게임을 한 번 실행했다가 끈 뒤 이 인스톨러를 다시 실행해 주세요.")
    return ok


def installSneg(gamePath):
    print("\nlanguages/ko 폴더에 한국어 언어팩을 설치합니다.")
    prepareKoFolder(gamePath)
    hiRes = askHiRes()
    return [
        ('DLL', installDll(gamePath)),
        ('language.ini', snegLanguageIni(gamePath, hiRes)),
        ('data', snegData(gamePath)),
        ('Bink', snegBink(gamePath)),
        ('Fonts', snegFont(gamePath)),
        ('sneg_ui_strings.dat', snegUiStrings(gamePath)),
        ('설정', snegSettings(hiRes)),
    ]


if __name__ == "__main__":
    print("""
======================================================================
  🕹️  Pool of Radiance: Ruins of Myth Drannor(SNEG 재출시판) 
                    유저 한글 패치 v2026.11.14
======================================================================

⚠️  [중요: 버전 호환성 안내]
본 한글패치는 2026년 SNEG 재출시판(Steam 등) 전용입니다.
선택한 게임 폴더에 'sneg_ui' 폴더가 있어야 설치됩니다. (2001년 원본은 지원하지 않습니다)


수정 및 업데이트 버전은 항상 https://github.com/wooddoll/PoR_RoMD 에서 배포하고 있습니다.

🌿 게임 중 출력되는 모든 대사와 아이템 이름, 오브젝트 이름이 번역되었습니다.
🌿 설정창, 툴팁, 튜토리얼 등 재출시판의 새 UI 문구도 번역되었습니다.
🌿 삽입된 시네마틱 동영상에 자막이 추가되었습니다.

======================================================================
📢 [필독 및 동의 안내]
본 한글패치 프로그램을 설치 및 사용하는 것은
아래의 모든 이용약관 및 주의사항에 동의한 것으로 간주됩니다.
내용을 반드시 읽고 동의하시는 경우에만 설치를 진행해 주시기 바랍니다.

✔ 지원 버전
2026년 SNEG 재출시판에 맞춰 제작되었습니다.
이후 게임 업데이트로 내부 구조가 바뀌면 동작하지 않을 수 있습니다.

✔ 정품 소유자 전용
본 한글패치는 해당 게임의 정품 패키지 또는 디지털 에디션 등을
정상적으로 구매한 사용자만 이용할 수 있습니다.

✔ 상업적 이용 및 유료 판매 금지
본 한글패치는 순수한 비영리 목적으로 제작된 창작물입니다.
본 패치 파일의 전체 또는 일부를 유료로 판매, 대여하거나
상업적인 목적으로 이용하는 행위는 엄격히 금지됩니다.

✔ 저작권 준수 안내
본 패치 파일은 자체 제작한 기술 코드와
순수 번역 데이터, 오픈소스 폰트로만 구성되어 있습니다.
이외 원본 게임의 데이터 등 타인의 저작물은 일절 포함하지 않았음을 밝힙니다.

✔ 면책 조항
본 패치는 비공식 유저 번역이므로, 시스템 환경이나 패치에 따라
구동이 불가해지거나 오류가 발생할 수 있습니다.
본 패치 설치 및 사용 과정에서 발생할 수 있는
게임 데이터 손상, 세이브 파일 충돌, 시스템 오류 등의 문제에 대해
제작자는 어떠한 법적·물질적 책임도 지지 않습니다.
(설치 전 원본 파일 및 세이브 데이터 백업을 권장합니다.)
======================================================================

✒️  사용 폰트 안내
• 움돋 (Umdot, https://monadabxy.com/fonts/umdot/) - SIL Open Font License 1.1
• 갈무리 (Galmuri, https://quiple.dev/font/galmuri) - SIL Open Font License 1.1
• 마루 부리 (MaruBuri, https://hangeul.naver.com/font) - SIL Open Font License 1.1


📁  설치 방식
• 게임의 언어팩 구조를 이용해 languages/ko 폴더에 한국어 언어팩을 설치합니다.
  원본 파일은 수정하지 않고, Data/Bink 에 자막 파일(.sbv)만 추가합니다.
• 게임 설정을 한국어(코드페이지 949)로 지정합니다. 설정창 언어 목록에서 '한국어' 를 볼 수 있습니다.
• 패치 제거: languages/ko 폴더, version.dll, minhook.x32.dll 을 지우고 설정창에서 다른 언어를 고르면 됩니다.

🛠️  기술 안내 및 알림
• 백신 오진 관련 안내
본 패치는 게임 실행 중 텍스트와 자막을 렌더링하기 위해 메모리 후킹(Hooking)방식을 사용합니다.
이로 인해 일부 백신 프로그램에서 악성코드로 오진하여 파일(version.dll)을 차단할 수 있으니,
실행이 안 될 경우 백신 검사 예외 처리를 해주시기 바랍니다.
• 오픈소스 및 직접 빌드 안내
본 패치의 모든 소스 코드와 번역 데이터는 공개되어 있습니다.
인스톨러의 안전성이 우려되거나 직접 수정하고 싶으신 분들은 상단의 공식 깃허브 저장소를 방문하여
코드를 직접 확인하거나 개별적으로 다운로드/빌드하여 사용하실 수 있습니다.

======================================================================
🔔 [최종 동의 확인]
  정상적인 이용을 위해 위 안내 사항을 반드시 숙지해 주시기 바랍니다.
  본 프로그램을 계속 설치 및 이용하는 것은 위 내용에 동의함을 의미합니다.
======================================================================
          """)
    os.system("pause")

    gamePath = get_folder_path()

    results = installSneg(gamePath)
    failed = [name for name, ok in results if not ok]
    if failed:
        print("\n⚠️ 확인이 필요한 항목: " + ", ".join(failed))

    print("\n✨ 모든 패치가 잘 끝났습니다.")
    print("📝 문제가 있으면 https://github.com/wooddoll/PoR_RoMD 에 issue로 올려주세요.")
    os.system("pause")
