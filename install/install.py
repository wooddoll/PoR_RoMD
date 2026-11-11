import sys
import os
import shutil
from tkinter import Tk, filedialog

from read_binary_to_json import check_checkdata_in_exe
from build_fonts import build_font
import sneg_ui_strings


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

def check_por2(dirpath):
    if not os.path.isdir(dirpath):
        return False
    
    datapath = os.path.join(dirpath, 'Data')
    if not os.path.isdir(os.path.join(datapath, 'Bink')): return False
    if not os.path.isdir(os.path.join(datapath, 'Dialogue')): return False
    if not os.path.isdir(os.path.join(datapath, 'Fonts')): return False
    if not os.path.isdir(os.path.join(datapath, 'STR')): return False
    
    filepath = os.path.join(dirpath, 'por2main.exe')
    if not os.path.isfile(filepath):
        return False

    # SNEG 재출시판(Steam 등): 실행 파일이 원본과 달라 exe 검사 대신 sneg.dll 존재로 확인
    if is_sneg(dirpath):
        return os.path.isfile(os.path.join(dirpath, 'sneg.dll'))

    jsonpath = os.path.join(embedded_data_path, 'exe_test.json')
    return check_checkdata_in_exe(jsonpath, filepath)


def is_sneg(gamePath):
    """'sneg_ui' 폴더가 있으면 SNEG 재출시판(2026, Steam 등), 없으면 2001년 원본."""
    return os.path.isdir(os.path.join(gamePath, 'sneg_ui'))

def get_folder_path():
    if len(sys.argv) > 1:
        path = sys.argv[1] # sys.argv[1]로 인자 인덱스 수정
        gamePath = os.path.normpath(path)
        if check_por2(gamePath):
            return gamePath
    
    root = Tk()
    root.withdraw()
    root.attributes('-topmost', True)
    
    print("\\Pool of Radiance\\ 폴더, 실행파일(por2main.exe)이 있는 곳을 선택하세요.")
    selected_path = filedialog.askdirectory(title="\\Pool of Radiance\\ 폴더를 선택하세요")
    if not selected_path:
        print("폴더가 선택되지 않아 프로그램을 종료합니다.")
        sys.exit()
    
    gamePath = os.path.normpath(selected_path)
    if check_por2(gamePath):
        return gamePath
    
    print(f"{gamePath}는 올바른 PoR2 폴더가 아닙니다.")
    sys.exit()

def installDll(gamePath, embedded_data_path, sneg):
    # 버전에 맞는 version.dll 을 골라 version.dll 이름으로 복사
    src = 'version_sneg.dll' if sneg else 'version_2001.dll'
    print(f"한글 출력용 DLL 파일을 복사합니다 ({src})... ", end='')
    srcPath = os.path.join(embedded_data_path, src)
    if not os.path.isfile(srcPath):
        print(f"실패: 패치 데이터에 {src} 가 없습니다.")
        return False
    shutil.copy2(os.path.join(embedded_data_path, 'minhook.x32.dll'), gamePath)
    shutil.copy2(srcPath, os.path.join(gamePath, 'version.dll'))
    print("ok")
    return True

KR_DIR_NAME = 'Data_kr'     # version.dll(data_redirect)이 읽는 폴더. language.ini [Settings] DataPath 기본값과 같아야 함

LEGACY_DIRS = ['Fonts', 'Dialogue', 'STR']


def askYN(prompt):
    while True:
        user_input = input(f"{prompt} (y/n): ").strip().lower()
        if user_input == 'y':
            return True
        if user_input == 'n':
            return False
        print("잘못된 입력입니다. 'y' 또는 'n'만 입력해 주세요.")


def getKrPath(gamePath):
    return os.path.join(gamePath, KR_DIR_NAME)


def restoreLegacyInstall(gamePath, embedded_data_path):
    """v1.0 인스톨러는 Data\\X 를 Data\\X_backup 으로 옮기고 한글 파일을 Data\\X 에 덮었다.
    이번 버전은 원본을 건드리지 않으므로, 그 흔적이 있으면 원본으로 되돌린다."""
    DataPath = os.path.join(gamePath, 'Data')
    sfDatPath = os.path.join(gamePath, 'sfDat.dat')

    found = [d for d in LEGACY_DIRS if os.path.isdir(os.path.join(DataPath, d + '_backup'))]
    hasSfDatBackup = os.path.isfile(sfDatPath + '.backup')
    if not found and not hasSfDatBackup:
        return True

    print("\n이전 버전(v1.0) 한글패치 설치 흔적이 있습니다:")
    for d in found:
        print(f"  Data/{d}_backup")
    if hasSfDatBackup:
        print("  sfDat.dat.backup")
    print("이번 버전은 원본 파일을 그대로 두고 Data_kr 폴더에 설치합니다.")
    if not askYN("백업해 둔 원본 파일을 원래 위치로 되돌릴까요?"):
        print("원본 복구를 건너뜁니다. (폰트는 *_backup 폴더의 원본으로 만듭니다)")
        return False

    for d in found:
        cur = os.path.join(DataPath, d)
        bak = os.path.join(DataPath, d + '_backup')
        if os.path.isdir(cur):
            shutil.rmtree(cur)
        os.rename(bak, cur)
        print(f"  Data/{d} 복구 ok")

    if hasSfDatBackup:
        if os.path.isfile(sfDatPath):
            os.remove(sfDatPath)
        os.rename(sfDatPath + '.backup', sfDatPath)
        print("  sfDat.dat 복구 ok")

    # v1.0 은 자막(.sbv)을 Data/Bink 에 바로 넣었다
    binkPath = os.path.join(DataPath, 'Bink')
    srcBink = os.path.join(embedded_data_path, 'Bink')
    removed = 0
    if os.path.isdir(binkPath) and os.path.isdir(srcBink):
        for file_name in os.listdir(srcBink):
            target = os.path.join(binkPath, file_name)
            if file_name.lower().endswith('.sbv') and os.path.isfile(target):
                os.remove(target)
                removed += 1
    if removed:
        print(f"  Data/Bink 의 이전 자막 파일 {removed}개 삭제 ok")
    return True


def prepareKrFolder(gamePath):
    krPath = getKrPath(gamePath)
    if os.path.isdir(krPath) and os.listdir(krPath):
        print(f"\n'{krPath}' 폴더가 이미 있습니다.")
        if askYN("기존 내용을 지우고 새로 설치할까요? (n 이면 덮어씁니다)"):
            shutil.rmtree(krPath)
    os.makedirs(krPath, exist_ok=True)
    return krPath


def installFont(gamePath, embedded_data_path):
    DataPath = os.path.join(gamePath, 'Data')
    # 원본 폰트: v1.0 백업이 남아 있으면 그쪽이 원본이다
    srcFonts = os.path.join(DataPath, 'Fonts_backup')
    if not os.path.isdir(srcFonts):
        srcFonts = os.path.join(DataPath, 'Fonts')
    dstFonts = os.path.join(getKrPath(gamePath), 'Fonts')
    print(f"{KR_DIR_NAME}/Fonts 폴더에 한글 폰트를 생성합니다... ")
    return buildFonts(srcFonts, dstFonts, embedded_data_path)


def buildFonts(srcFonts, dstFonts, embedded_data_path):
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


def copyFiles(src_dir, dst_dir, ext):
    os.makedirs(dst_dir, exist_ok=True)
    copied_count = 0
    for file_name in os.listdir(src_dir):
        if file_name.lower().endswith(ext):
            shutil.copy2(os.path.join(src_dir, file_name), os.path.join(dst_dir, file_name))
            copied_count += 1
    return copied_count


def installDialogue(gamePath, embedded_data_path):
    print(f"{KR_DIR_NAME}/Dialogue 폴더로 한글 대화 파일을 복사합니다... ", end='')
    n = copyFiles(os.path.join(embedded_data_path, 'Dialogue'),
                  os.path.join(getKrPath(gamePath), 'Dialogue'), '.por')
    print("ok")
    return n == 1398


def installSTR(gamePath, embedded_data_path):
    print(f"{KR_DIR_NAME}/STR 폴더로 한글 이름 파일을 복사합니다... ", end='')
    n = copyFiles(os.path.join(embedded_data_path, 'STR'),
                  os.path.join(getKrPath(gamePath), 'STR'), '.str')
    print("ok")
    return n == 44


def installBink(gamePath, embedded_data_path):
    print(f"{KR_DIR_NAME}/Bink 폴더로 한글 자막 파일을 복사합니다... ", end='')
    n = copyFiles(os.path.join(embedded_data_path, 'Bink'),
                  os.path.join(getKrPath(gamePath), 'Bink'), '.sbv')
    print("ok")
    return n == 10


def installSFDat(gamePath, embedded_data_path):
    print(f"{KR_DIR_NAME}/sfDat.dat 파일을 복사합니다... ", end='')
    shutil.copy2(os.path.join(embedded_data_path, 'sfDat.dat'),
                 os.path.join(getKrPath(gamePath), 'sfDat.dat'))
    print("ok")
    return True


# ======================================================================
#  SNEG 재출시판 (2026, Steam 등): languages\ko 한국어 언어팩으로 설치
#  - 원본 파일은 건드리지 않는다 (Data\Bink 에 자막 .sbv 만 추가)
#  - 게임 설정창의 언어 목록에 '한국어' 로 나오고, 선택하면 다음 실행부터 적용
# ======================================================================
KO_LANG = 'ko'
SNEG_CJK_FONT = 'MaruBuri-Regular.ttf'          # version.dll 이 ImGui CJK 글꼴로 읽는 파일 (languages\ko\)
SNEG_CJK_FONT_LICENSE = 'MaruBuri-OFL.txt'     # 있으면 같이 복사


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


def snegLanguageIni(gamePath):
    print(f"languages/{KO_LANG}/language.ini 를 만듭니다... ", end='')
    with open(os.path.join(getKoPath(gamePath), 'language.ini'), 'w', encoding='ascii', newline='\r\n') as f:
        f.write("[Language]\nCodePage=949\nRelease=Korean patch by wooddoll\n")
    print("ok")
    return True


def snegData(gamePath, embedded_data_path):
    koData = os.path.join(getKoPath(gamePath), 'data')
    print(f"languages/{KO_LANG}/data 폴더로 한글 대화/이름 파일을 복사합니다... ", end='')
    n1 = copyFiles(os.path.join(embedded_data_path, 'Dialogue'), os.path.join(koData, 'Dialogue'), '.por')
    n2 = copyFiles(os.path.join(embedded_data_path, 'STR'), os.path.join(koData, 'STR'), '.str')
    shutil.copy2(os.path.join(embedded_data_path, 'sfDat.dat'), os.path.join(getKoPath(gamePath), 'sfDat.dat'))
    print("ok")
    return n1 == 1398 and n2 == 44


def snegBink(gamePath, embedded_data_path):
    # 자막은 영상(.bik)과 같은 폴더에서 찾는다 (version.dll bink_hook). 영상은 원본 Data\Bink 에 있으므로 거기에 추가
    print("Data/Bink 폴더에 한글 자막 파일(.sbv)을 추가합니다... ", end='')
    n = copyFiles(os.path.join(embedded_data_path, 'Bink'), os.path.join(gamePath, 'Data', 'Bink'), '.sbv')
    print("ok")
    return n == 10


def snegFont(gamePath, embedded_data_path):
    # 원본 Data\Fonts 를 바탕으로 한글 병합 폰트를 languages\ko\data\fonts 에 만든다
    srcFonts = os.path.join(gamePath, 'Data', 'Fonts')
    dstFonts = os.path.join(getKoPath(gamePath), 'data', 'fonts')
    print(f"languages/{KO_LANG}/data/fonts 폴더에 한글 폰트를 생성합니다... ")
    ok = buildFonts(srcFonts, dstFonts, embedded_data_path)

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
    # 루트 sneg_ui_strings.dat (영어) 를 바탕으로 언어 코드 ko + 언어 이름 '한국어' 를 넣어 만든다
    print(f"languages/{KO_LANG}/sneg_ui_strings.dat 를 만듭니다... ", end='')
    src = os.path.join(gamePath, 'sneg_ui_strings.dat')
    if not os.path.isfile(src):
        print("실패: 게임 폴더에 sneg_ui_strings.dat 가 없습니다.")
        return False
    _, items = sneg_ui_strings.load(src)
    key = f'ui.settings.language.{KO_LANG}'
    items = [it for it in items if it[0] != key]
    items.append([key, 65001, '한국어'.encode('utf-8')])
    items.sort(key=lambda it: it[0])
    sneg_ui_strings.save(os.path.join(getKoPath(gamePath), 'sneg_ui_strings.dat'), KO_LANG, items)
    print("ok")
    return True


def writeIni(path, section, key, value):
    import ctypes
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return bool(ctypes.windll.kernel32.WritePrivateProfileStringW(section, key, value, path))


def snegSettings():
    # %LOCALAPPDATA%\SNEG\POR2 : config.ini [Text] CodePage=949, game_settings.ini [Text] Language=ko
    base = os.path.join(os.environ.get('LOCALAPPDATA', ''), 'SNEG', 'POR2')
    print("게임 설정에 한국어(코드페이지 949)를 지정합니다... ", end='')
    ok = writeIni(os.path.join(base, 'config.ini'), 'Text', 'CodePage', '949')
    ok &= writeIni(os.path.join(base, 'game_settings.ini'), 'Text', 'Language', KO_LANG)
    print("ok" if ok else "실패")
    return ok


def installSneg(gamePath):
    print("\nSNEG 재출시판으로 확인되었습니다. languages/ko 한국어 언어팩으로 설치합니다.")
    prepareKoFolder(gamePath)
    return [
        ('DLL', installDll(gamePath, embedded_data_path, True)),
        ('language.ini', snegLanguageIni(gamePath)),
        ('data', snegData(gamePath, embedded_data_path)),
        ('Bink', snegBink(gamePath, embedded_data_path)),
        ('Fonts', snegFont(gamePath, embedded_data_path)),
        ('sneg_ui_strings.dat', snegUiStrings(gamePath)),
        ('설정', snegSettings()),
    ]


def install2001(gamePath):
    print("\n2001년 원본으로 확인되었습니다. Data_kr 폴더로 설치합니다.")
    restoreLegacyInstall(gamePath, embedded_data_path)
    prepareKrFolder(gamePath)
    return [
        ('DLL', installDll(gamePath, embedded_data_path, False)),
        ('Bink', installBink(gamePath, embedded_data_path)),
        ('Fonts', installFont(gamePath, embedded_data_path)),
        ('sfDat.dat', installSFDat(gamePath, embedded_data_path)),
        ('Dialogue', installDialogue(gamePath, embedded_data_path)),
        ('STR', installSTR(gamePath, embedded_data_path)),
    ]


if __name__ == "__main__":
    print("""
======================================================================
  🕹️  Pool of Radiance: Ruins of Myth Drannor 유저 한글 패치 v1.0
======================================================================

⚠️  [중요: 버전 호환성 안내]
본 한글패치는 2001년 원본(Original) 버전과 2026년 SNEG 재출시판(Steam 등)을 지원합니다.
선택한 게임 폴더에 'sneg_ui' 폴더가 있으면 재출시판, 없으면 2001년 원본으로 보고 설치합니다.


수정 및 업데이트 버전은 항상 https://github.com/wooddoll/PoR_RoMD 에서 배포하고 있습니다.

🌿 게임 중 출력되는 모든 대사와 아이템 이름, 오브젝트 이름이 번역되었습니다.
🌿 삽입된 시네마틱 동영상에 자막이 추가되었습니다.
(그 외 메뉴 및 일부 UI는 영문 그대로 출력됩니다.)

======================================================================
📢 [필독 및 동의 안내]
본 한글패치 프로그램을 설치 및 사용하는 것은 
아래의 모든 이용약관 및 주의사항에 동의한 것으로 간주됩니다. 
내용을 반드시 읽고 동의하시는 경우에만 설치를 진행해 주시기 바랍니다.

✔ 지원 버전
2001년 원본과 2026년 SNEG 재출시판에 맞춰 제작되었습니다. 
재출시판은 이후 업데이트로 내부 구조가 바뀌면 동작하지 않을 수 있습니다.

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
• 마루 부리 (MaruBuri, 재출시판 새 UI용) - SIL Open Font License 1.1


📁  설치 방식 (2001년 원본)
• 원본 Data 폴더와 sfDat.dat 는 수정하지 않고, 한글 파일은 모두 Data_kr 폴더에 설치합니다.
  게임은 version.dll 이 Data_kr 에 있는 파일을 원본 대신 읽도록 해 줍니다.
• 이전 버전(v1.0)으로 설치한 흔적(*_backup)이 있으면 원본으로 되돌릴지 묻습니다.
• 패치 제거: 게임 폴더의 Data_kr 폴더, version.dll, minhook.x32.dll 을 지우면 됩니다.

📁  설치 방식 (SNEG 재출시판)
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

    results = installSneg(gamePath) if is_sneg(gamePath) else install2001(gamePath)
    failed = [name for name, ok in results if not ok]
    if failed:
        print("\n⚠️ 확인이 필요한 항목: " + ", ".join(failed))

    print("\n✨ 모든 패치가 잘 끝났습니다.")
    print("📝 문제가 있으면 https://github.com/wooddoll/PoR_RoMD 에 issue로 올려주세요.")
    os.system("pause")