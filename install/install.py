import sys
import os
import shutil
from tkinter import Tk, filedialog

from read_binary_to_json import check_checkdata_in_exe
from build_fonts import build_font


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
        
    jsonpath = os.path.join(embedded_data_path, 'exe_test.json')
    return check_checkdata_in_exe(jsonpath, filepath)

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

def installDll(gamePath, embedded_data_path):
    print("한글 출력용 DLL 파일을 복사합니다... ", end='')
    shutil.copy2(os.path.join(embedded_data_path, 'minhook.x32.dll'), gamePath)
    shutil.copy2(os.path.join(embedded_data_path, 'version.dll'), gamePath)
    print("ok")
    return True

def inputYN():
    while True:
        user_input = input("그래도 진행하시겠습니까? (y/n): ").strip().lower()
        
        if user_input == 'y':
            print("▶️ 작업을 계속 진행합니다.")
            return True
        elif user_input == 'n':
            print("❌ 사용자가 작업을 취소했습니다. 프로그램을 종료합니다.")
            sys.exit()  # 프로그램 즉시 안전하게 종료
        else:
            print("잘못된 입력입니다. 'y' 또는 'n'만 입력해 주세요.")

def installFont(gamePath, embedded_data_path):
    DataPath = os.path.join(gamePath, 'Data')
    FontsPath = os.path.join(DataPath, 'Fonts')
    
    print("Data/Font 폴더를 Data/Font_backup 으로 보관합니다.")
    Fonts_backup = os.path.join(DataPath, 'Fonts_backup')
    if not os.path.exists(Fonts_backup):
        os.rename(FontsPath, Fonts_backup)        
        os.makedirs(FontsPath, exist_ok=True)
    else:
        print(f"⚠️ 경고: '{os.path.join(DataPath, 'Fonts_backup')}' 폴더가 이미 존재합니다.")
        print(f"이전 백업이 유효하다고 가정하고 백업하지 않고 진행합니다.", end='')
        inputYN()
        
    print("Data/Font 폴더에 한글 폰트를 생성합니다... ")
    build_font('font0', Fonts_backup, 
               os.path.join(embedded_data_path, 'font/Umdot14.ttf_18.png'),
               os.path.join(embedded_data_path, 'font/Umdot14.ttf_18.txt'),
               FontsPath)
    build_font('font1', Fonts_backup, 
                os.path.join(embedded_data_path, 'font/Umdot12.ttf_16.png'),
                os.path.join(embedded_data_path, 'font/Umdot12.ttf_16.txt'),
                FontsPath)
    build_font('font2', Fonts_backup, 
                os.path.join(embedded_data_path, 'font/Umdot20-Bold.ttf_28.png'),
                os.path.join(embedded_data_path, 'font/Umdot20-Bold.ttf_28.txt'),
                FontsPath)
    build_font('font3', Fonts_backup, 
                os.path.join(embedded_data_path, 'font/Galmuri9.ttf_13.png'),
                os.path.join(embedded_data_path, 'font/Galmuri9.ttf_13.txt'),
                FontsPath)
    build_font('font4', Fonts_backup, 
                os.path.join(embedded_data_path, 'font/Galmuri11.ttf_15.png'),
                os.path.join(embedded_data_path, 'font/Galmuri11.ttf_15.txt'),
                FontsPath)
    return True

def installDialogue(gamePath, embedded_data_path):
    DataPath = os.path.join(gamePath, 'Data')
    DialoguePath = os.path.join(DataPath, 'Dialogue')
    print("Data/Dialogue 폴더를 Data/Dialogue_backup 으로 보관합니다.")
    if not os.path.exists(os.path.join(DataPath, 'Dialogue_backup')):
        os.rename(DialoguePath, os.path.join(DataPath, 'Dialogue_backup'))        
        os.makedirs(DialoguePath, exist_ok=True)
    else:
        print(f"⚠️ 경고: '{os.path.join(DataPath, 'Dialogue_backup')}' 폴더가 이미 존재합니다.")
        print(f"이전 백업이 유효하다고 가정하고 백업하지 않고 진행합니다.", end='')
        inputYN()
        
    print("Data/Dialogue 폴더로 한글 대화 파일을 복사합니다... ", end='')
    shutil.copy2(os.path.join(DataPath, 'Dialogue_backup/all.db'), DialoguePath)
    shutil.copy2(os.path.join(DataPath, 'Dialogue_backup/all.idx'), DialoguePath)
    
    source_dir = os.path.join(embedded_data_path, 'Dialogue')
    copied_count = 0
    for file_name in os.listdir(source_dir):
        if file_name.lower().endswith('.por'):
            source_path = os.path.join(source_dir, file_name)
            target_path = os.path.join(DialoguePath, file_name)
        
            shutil.copy2(source_path, target_path)
            copied_count += 1
    
    print("ok")
    return copied_count == 1398

def installSTR(gamePath, embedded_data_path):
    DataPath = os.path.join(gamePath, 'Data')
    STRPath = os.path.join(DataPath, 'STR')
    print("Data/STR 폴더를 Data/STR_backup 으로 보관합니다.")
    if not os.path.exists(os.path.join(DataPath, 'STR_backup')):
        os.rename(STRPath, os.path.join(DataPath, 'STR_backup'))        
        os.makedirs(STRPath, exist_ok=True)
    else:
        print(f"⚠️ 경고: '{os.path.join(DataPath, 'STR_backup')}' 폴더가 이미 존재합니다.")
        print(f"이전 백업이 유효하다고 가정하고 백업하지 않고 진행합니다.", end='')
        inputYN()
        
    print("Data/STR 폴더로 한글 이름 파일을 복사합니다... ", end='')
    
    source_dir = os.path.join(embedded_data_path, 'STR')
    copied_count = 0
    for file_name in os.listdir(source_dir):
        if file_name.lower().endswith('.str'):
            source_path = os.path.join(source_dir, file_name)
            target_path = os.path.join(STRPath, file_name)
        
            shutil.copy2(source_path, target_path)
            copied_count += 1

    print("ok")
    return copied_count == 44

def installBink(gamePath, embedded_data_path):
    DataPath = os.path.join(gamePath, 'Data')
    STRPath = os.path.join(DataPath, 'Bink')

    print("Data/Bink 폴더로 한글 자막 파일을 복사합니다... ", end='')
    
    source_dir = os.path.join(embedded_data_path, 'Bink')
    copied_count = 0
    for file_name in os.listdir(source_dir):
        if file_name.lower().endswith('.sbv'):
            source_path = os.path.join(source_dir, file_name)
            target_path = os.path.join(STRPath, file_name)
        
            shutil.copy2(source_path, target_path)
            copied_count += 1

    print("ok")
    return copied_count == 10

if __name__ == "__main__":
    print("""
======================================================================
  🕹️  Pool of Radiance: Ruins of Myth Drannor 유저 한글 패치 v1.0
======================================================================

⚠️  [중요: 버전 호환성 안내]
본 한글패치는 2001년 원본(Original) 버전에서만 정상 동작합니다.
2026년 11월에 출시 예정인 새로운 릴리즈 버전에서는 동작하지 않으니 
설치 전 본인의 게임 버전을 반드시 확인해 주시기 바랍니다.


수정 및 업데이트 버전은 항상 https://github.com/wooddoll/PoR_RoMD 에서 배포하고 있습니다.

🌿 게임 중 출력되는 모든 대사와 아이템 이름, 오브젝트 이름이 번역되었습니다.
🌿 삽입된 시네마틱 동영상에 자막이 추가되었습니다.
(그 외 메뉴 및 일부 UI는 영문 그대로 출력됩니다.)

======================================================================
📢 [필독 및 동의 안내]
본 한글패치 프로그램을 설치 및 사용하는 것은 
아래의 모든 이용약관 및 주의사항에 동의한 것으로 간주됩니다. 
내용을 반드시 읽고 동의하시는 경우에만 설치를 진행해 주시기 바랍니다.

✔ 특정 버전 전용 (2001년 원본)
본 패치는 오직 2001년 발매된 원본 게임 환경에 맞춰 제작되었습니다. 
2026년 11월 새 릴리즈 버전과는 아직 호환되지 않으며, 
새 버전 구동 시 발생하는 오류에 대해서는 지원하지 않습니다.

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

    installDll(gamePath, embedded_data_path)
    installBink(gamePath, embedded_data_path)
    installFont(gamePath, embedded_data_path)
    installDialogue(gamePath, embedded_data_path)
    installSTR(gamePath, embedded_data_path)

    print("\n✨ 모든 패치가 잘 끝났습니다.")
    print("📝 문제가 있으면 https://github.com/wooddoll/PoR_RoMD 에 issue로 올려주세요.")
    os.system("pause")