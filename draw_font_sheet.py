import os
from PIL import Image, ImageFont, ImageDraw

def generate_custom_bitmap_font(font_path, font_size, char_list, output_image_path, output_txt_path,
                                padding_left=2, padding_right=2, 
                                fixed_height=16, baseline_below=4,
                                bg_color=(0, 0, 0, 0), fg_color=(255, 255, 255, 255), max_atlas_width=2048):
    """
    TTF 폰트를 받아 안티앨리어싱이 없는 단색 비트맵 폰트를 생성합니다.
    세로 크기는 (baseline_above + baseline_below)로 고정되며, 가로 크기는 가변폭으로 대응합니다.
    결과물로 이미지 파일과 단순 좌표 텍스트 파일을 생성합니다.
    """
    try:
        font = ImageFont.truetype(font_path, font_size)
    except IOError:
        print(f"[오류] 폰트 파일을 찾을 수 없거나 불러올 수 없습니다: {font_path}")
        return

    # 중복 글자 제거 및 순서 유지
    unique_chars = []
    for c in char_list:
        if c not in unique_chars:
            unique_chars.append(c)
            
    # 고정될 총 세로 슬롯 높이 계산
    baseline_above = fixed_height - baseline_below
    
    # 폰트 메트릭 측정을 위한 임시 객체
    dummy_img = Image.new('RGBA', (1, 1))
    dummy_draw = ImageDraw.Draw(dummy_img)
    dummy_draw.fontmode = "1" 
    
    char_data = []
    
    for c in unique_chars:
        # 가변폭(Advance Width)을 계산
        advance_w = int(font.getlength(c))
        
        # 실제 픽셀이 찍히는 영역 계산
        bbox = font.getbbox(c)
        if bbox is None:
            ink_w = advance_w
        else:
            ink_w = bbox[2] - bbox[0]
            
        # 가변폭과 실제 픽셀폭 중 큰 값에 좌우 패딩을 더해 가로 슬롯 크기 결정
        actual_w = max(advance_w, ink_w)
        slot_w = padding_left + actual_w + padding_right
        
        char_data.append({
            'char': c,
            'slot_w': slot_w,
            'slot_h': fixed_height
        })
        
    # 가로줄 단위 패킹 알고리즘
    current_x = 0
    current_y = 0
    packed_chars = []
    
    for data in char_data:
        # 최대 가로폭을 넘어가면 개행
        if current_x + data['slot_w'] > max_atlas_width:
            current_x = 0
            current_y += fixed_height
            
        data['x'] = current_x
        data['y'] = current_y
        packed_chars.append(data)
        
        current_x += data['slot_w']
        
    atlas_width = max_atlas_width if current_y > 0 else current_x
    atlas_height = current_y + fixed_height if current_x > 0 else current_y
    
    # 비트맵 아틀라스 이미지 생성
    atlas_img = Image.new('RGBA', (atlas_width, atlas_height), bg_color)
    draw = ImageDraw.Draw(atlas_img)
    draw.fontmode = "1" # 안티앨리어싱 제거
    
    # 텍스트 메타데이터 저장을 위한 리스트
    txt_lines = []
    
    # 폰트 자체의 내장 메트릭(ascent)을 가져와 오프셋 보정 계산
    # Pillow의 텍스트 드로잉 기준점과 사용자의 커스텀 baseline 매핑용
    try:
        ascent, descent = font.getmetrics()
    except AttributeError:
        ascent = font_size # 하위 버전 호환용 피드백
        
    for data in packed_chars:
        c = data['char']
        
        # 슬롯 내부에서의 렌더링 시작 좌표 계산
        # 좌측 패딩 적용
        draw_x = data['x'] + padding_left
        # Y축: 지정한 baseline_above 위치에 폰트의 실제 baseline이 정렬되도록 오프셋 가감
        draw_y = data['y'] + baseline_above - ascent
        
        # 글자 그리기
        draw.text((draw_x, draw_y), c, fill=fg_color, font=font)
        
        # 요청하신 좌표 데이터 계산 (x0, y0, x1, y1)
        x0 = data['x']
        y0 = data['y']
        x1 = data['x'] + data['slot_w'] - 1
        y1 = data['y'] + data['slot_h'] - 1
        
        # 유니코드 10진수값 또는 문자 자체와 함께 저장 (원하는 형태로 커스텀 가능)
        # 여기서는 [문자코드(유니코드10진수), x0, y0, x1, y1] 형태로 출력합니다.
        cp949char = c.encode("cp949")
        cp949code = int.from_bytes(cp949char, byteorder="big")
        txt_lines.append(f"{cp949code}\t{x0}\t{y0}\t{x1}\t{y1}")
        
    # 파일 저장
    # 파일이 위치할 부모 폴더 경로 추출
    folder_path = os.path.dirname(output_image_path)
    # 폴더가 존재하지 않으면 생성
    if folder_path and not os.path.exists(folder_path):
        os.makedirs(folder_path, exist_ok=True)
    atlas_img.save(output_image_path)
    
    folder_path = os.path.dirname(output_txt_path)
    # 폴더가 존재하지 않으면 생성
    if folder_path and not os.path.exists(folder_path):
        os.makedirs(folder_path, exist_ok=True)
    with open(output_txt_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(txt_lines))
        
    print(f"비트맵 폰트 생성 완료!")
    print(f" - 이미지: {output_image_path} ({atlas_width}x{atlas_height})")
    print(f" - 텍스트 메타데이터: {output_txt_path}")



# --- 사용 예시 ---
if __name__ == "__main__":
    # 한글 완성형 2,350자 정의
    hangul_2350 = "가각간갇갈갉갊감갑값갓갔강갖갗같갚갛개객갠갤갬갭갯갰갱갸갹갼걀걋걍걔걘걜거걱건걷걸걺검겁것겄겅겆겉겊겋게겐겔겜겝겟겠겡겨격겪견겯결겸겹겻겼경곁계곈곌곕곗고곡곤곧골곪곬곯곰곱곳공곶과곽관괄괆괌괍괏광괘괜괠괩괬괭괴괵괸괼굄굅굇굉교굔굘굡굣구국군굳굴굵굶굻굼굽굿궁궂궈궉권궐궜궝궤궷귀귁귄귈귐귑귓규균귤그극근귿글긁금급긋긍긔기긱긴긷길긺김깁깃깅깆깊까깍깎깐깔깖깜깝깟깠깡깥깨깩깬깰깸깹깻깼깽꺄꺅꺌꺼꺽꺾껀껄껌껍껏껐껑께껙껜껨껫껭껴껸껼꼇꼈꼍꼐꼬꼭꼰꼲꼴꼼꼽꼿꽁꽂꽃꽈꽉꽐꽜꽝꽤꽥꽹꾀꾄꾈꾐꾑꾕꾜꾸꾹꾼꿀꿇꿈꿉꿋꿍꿎꿔꿜꿨꿩꿰꿱꿴꿸뀀뀁뀄뀌뀐뀔뀜뀝뀨끄끅끈끊끌끎끓끔끕끗끙끝끼끽낀낄낌낍낏낑나낙낚난낟날낡낢남납낫났낭낮낯낱낳내낵낸낼냄냅냇냈냉냐냑냔냘냠냥너넉넋넌널넒넓넘넙넛넜넝넣네넥넨넬넴넵넷넸넹녀녁년녈념녑녔녕녘녜녠노녹논놀놂놈놉놋농높놓놔놘놜놨뇌뇐뇔뇜뇝뇟뇨뇩뇬뇰뇹뇻뇽누눅눈눋눌눔눕눗눙눠눴눼뉘뉜뉠뉨뉩뉴뉵뉼늄늅늉느늑는늘늙늚늠늡늣능늦늪늬늰늴니닉닌닐닒님닙닛닝닢다닥닦단닫달닭닮닯닳담답닷닸당닺닻닿대댁댄댈댐댑댓댔댕댜더덕덖던덛덜덞덟덤덥덧덩덫덮데덱덴델뎀뎁뎃뎄뎅뎌뎐뎔뎠뎡뎨뎬도독돈돋돌돎돐돔돕돗동돛돝돠돤돨돼됐되된될됨됩됫됴두둑둔둘둠둡둣둥둬뒀뒈뒝뒤뒨뒬뒵뒷뒹듀듄듈듐듕드득든듣들듦듬듭듯등듸디딕딘딛딜딤딥딧딨딩딪따딱딴딸땀땁땃땄땅땋때땍땐땔땜땝땟땠땡떠떡떤떨떪떫떰떱떳떴떵떻떼떽뗀뗄뗌뗍뗏뗐뗑뗘뗬또똑똔똘똥똬똴뙈뙤뙨뚜뚝뚠뚤뚫뚬뚱뛔뛰뛴뛸뜀뜁뜅뜨뜩뜬뜯뜰뜸뜹뜻띄띈띌띔띕띠띤띨띰띱띳띵라락란랄람랍랏랐랑랒랖랗래랙랜랠램랩랫랬랭랴략랸럇량러럭런럴럼럽럿렀렁렇레렉렌렐렘렙렛렝려력련렬렴렵렷렸령례롄롑롓로록론롤롬롭롯롱롸롼뢍뢨뢰뢴뢸룀룁룃룅료룐룔룝룟룡루룩룬룰룸룹룻룽뤄뤘뤠뤼뤽륀륄륌륏륑류륙륜률륨륩륫륭르륵른를름릅릇릉릊릍릎리릭린릴림립릿링마막만많맏말맑맒맘맙맛망맞맡맣매맥맨맬맴맵맷맸맹맺먀먁먈먕머먹먼멀멂멈멉멋멍멎멓메멕멘멜멤멥멧멨멩며멱면멸몃몄명몇몌모목몫몬몰몲몸몹못몽뫄뫈뫘뫙뫼묀묄묍묏묑묘묜묠묩묫무묵묶문묻물묽묾뭄뭅뭇뭉뭍뭏뭐뭔뭘뭡뭣뭬뮈뮌뮐뮤뮨뮬뮴뮷므믄믈믐믓미믹민믿밀밂밈밉밋밌밍및밑바박밖밗반받발밝밞밟밤밥밧방밭배백밴밸뱀뱁뱃뱄뱅뱉뱌뱍뱐뱝버벅번벋벌벎범법벗벙벚베벡벤벧벨벰벱벳벴벵벼벽변별볍볏볐병볕볘볜보복볶본볼봄봅봇봉봐봔봤봬뵀뵈뵉뵌뵐뵘뵙뵤뵨부북분붇불붉붊붐붑붓붕붙붚붜붤붰붸뷔뷕뷘뷜뷩뷰뷴뷸븀븃븅브븍븐블븜븝븟비빅빈빌빎빔빕빗빙빚빛빠빡빤빨빪빰빱빳빴빵빻빼빽뺀뺄뺌뺍뺏뺐뺑뺘뺙뺨뻐뻑뻔뻗뻘뻠뻣뻤뻥뻬뼁뼈뼉뼘뼙뼛뼜뼝뽀뽁뽄뽈뽐뽑뽕뾔뾰뿅뿌뿍뿐뿔뿜뿟뿡쀼쁑쁘쁜쁠쁨쁩삐삑삔삘삠삡삣삥사삭삯산삳살삵삶삼삽삿샀상샅새색샌샐샘샙샛샜생샤샥샨샬샴샵샷샹섀섄섈섐섕서석섞섟선섣설섦섧섬섭섯섰성섶세섹센셀셈셉셋셌셍셔셕션셜셤셥셧셨셩셰셴셸솅소속솎손솔솖솜솝솟송솥솨솩솬솰솽쇄쇈쇌쇔쇗쇘쇠쇤쇨쇰쇱쇳쇼쇽숀숄숌숍숏숑수숙순숟술숨숩숫숭숯숱숲숴쉈쉐쉑쉔쉘쉠쉥쉬쉭쉰쉴쉼쉽쉿슁슈슉슐슘슛슝스슥슨슬슭슴습슷승시식신싣실싫심십싯싱싶싸싹싻싼쌀쌈쌉쌌쌍쌓쌔쌕쌘쌜쌤쌥쌨쌩썅써썩썬썰썲썸썹썼썽쎄쎈쎌쏀쏘쏙쏜쏟쏠쏢쏨쏩쏭쏴쏵쏸쐈쐐쐤쐬쐰쐴쐼쐽쑈쑤쑥쑨쑬쑴쑵쑹쒀쒔쒜쒸쒼쓩쓰쓱쓴쓸쓺쓿씀씁씌씐씔씜씨씩씬씰씸씹씻씽아악안앉않알앍앎앓암압앗았앙앝앞애액앤앨앰앱앳앴앵야약얀얄얇얌얍얏양얕얗얘얜얠얩어억언얹얻얼얽얾엄업없엇었엉엊엌엎에엑엔엘엠엡엣엥여역엮연열엶엷염엽엾엿였영옅옆옇예옌옐옘옙옛옜오옥온올옭옮옰옳옴옵옷옹옻와왁완왈왐왑왓왔왕왜왝왠왬왯왱외왹왼욀욈욉욋욍요욕욘욜욤욥욧용우욱운울욹욺움웁웃웅워웍원월웜웝웠웡웨웩웬웰웸웹웽위윅윈윌윔윕윗윙유육윤율윰윱윳융윷으윽은을읊음읍읏응읒읓읔읕읖읗의읜읠읨읫이익인일읽읾잃임입잇있잉잊잎자작잔잖잗잘잚잠잡잣잤장잦재잭잰잴잼잽잿쟀쟁쟈쟉쟌쟎쟐쟘쟝쟤쟨쟬저적전절젊점접젓정젖제젝젠젤젬젭젯젱져젼졀졈졉졌졍졔조족존졸졺좀좁좃종좆좇좋좌좍좔좝좟좡좨좼좽죄죈죌죔죕죗죙죠죡죤죵주죽준줄줅줆줌줍줏중줘줬줴쥐쥑쥔쥘쥠쥡쥣쥬쥰쥴쥼즈즉즌즐즘즙즛증지직진짇질짊짐집짓징짖짙짚짜짝짠짢짤짧짬짭짯짰짱째짹짼쨀쨈쨉쨋쨌쨍쨔쨘쨩쩌쩍쩐쩔쩜쩝쩟쩠쩡쩨쩽쪄쪘쪼쪽쫀쫄쫌쫍쫏쫑쫓쫘쫙쫠쫬쫴쬈쬐쬔쬘쬠쬡쭁쭈쭉쭌쭐쭘쭙쭝쭤쭸쭹쮜쮸쯔쯤쯧쯩찌찍찐찔찜찝찡찢찧차착찬찮찰참찹찻찼창찾채책챈챌챔챕챗챘챙챠챤챦챨챰챵처척천철첨첩첫첬청체첵첸첼쳄쳅쳇쳉쳐쳔쳤쳬쳰촁초촉촌촐촘촙촛총촤촨촬촹최쵠쵤쵬쵭쵯쵱쵸춈추축춘출춤춥춧충춰췄췌췐취췬췰췸췹췻췽츄츈츌츔츙츠측츤츨츰츱츳층치칙친칟칠칡침칩칫칭카칵칸칼캄캅캇캉캐캑캔캘캠캡캣캤캥캬캭컁커컥컨컫컬컴컵컷컸컹케켁켄켈켐켑켓켕켜켠켤켬켭켯켰켱켸코콕콘콜콤콥콧콩콰콱콴콸쾀쾅쾌쾡쾨쾰쿄쿠쿡쿤쿨쿰쿱쿳쿵쿼퀀퀄퀑퀘퀭퀴퀵퀸퀼큄큅큇큉큐큔큘큠크큭큰클큼큽킁키킥킨킬킴킵킷킹타탁탄탈탉탐탑탓탔탕태택탠탤탬탭탯탰탱탸턍터턱턴털턺텀텁텃텄텅테텍텐텔템텝텟텡텨텬텼톄톈토톡톤톨톰톱톳통톺톼퇀퇘퇴퇸툇툉툐투툭툰툴툼툽툿퉁퉈퉜퉤튀튁튄튈튐튑튕튜튠튤튬튱트특튼튿틀틂틈틉틋틔틘틜틤틥티틱틴틸팀팁팃팅파팍팎판팔팖팜팝팟팠팡팥패팩팬팰팸팹팻팼팽퍄퍅퍼퍽펀펄펌펍펏펐펑페펙펜펠펨펩펫펭펴편펼폄폅폈평폐폘폡폣포폭폰폴폼폽폿퐁퐈퐝푀푄표푠푤푭푯푸푹푼푿풀풂품풉풋풍풔풩퓌퓐퓔퓜퓟퓨퓬퓰퓸퓻퓽프픈플픔픕픗피픽핀필핌핍핏핑하학한할핥함합핫항해핵핸핼햄햅햇했행햐향허헉헌헐헒험헙헛헝헤헥헨헬헴헵헷헹혀혁현혈혐협혓혔형혜혠혤혭호혹혼홀홅홈홉홋홍홑화확환활홧황홰홱홴횃횅회획횐횔횝횟횡효횬횰횹횻후훅훈훌훑훔훗훙훠훤훨훰훵훼훽휀휄휑휘휙휜휠휨휩휫휭휴휵휸휼흄흇흉흐흑흔흖흗흘흙흠흡흣흥흩희흰흴흼흽힁히힉힌힐힘힙힛힝" 

    #FONT_FILE = "../font/LanaPixel.ttf"
    #FONT_FILE = "../font/Suhgung-ttf/Monospace/Regular/SuhgungMono12.ttf"
    #FONT_FILE = "../font/Tangba-ttf/Monospace/Regular/TangbaMono12.ttf"
    #FONT_FILE = "../font/Limgul-ttf/Monospace/Regular/LimgulMono12.ttf"
    #FONT_FILE = "../font/MonaFont-ttf/Mona/02 Text Only/Mona12TextKR.ttf"
    #FONT_FILE = "../font/Umdot-ttf/Monospace/Regular/UmdotMono12.ttf"   #
    #FONT_FILE = "../font/Umdot-ttf/Proportional/Regular/Umdot14.ttf"    #
    #FONT_FILE = "../font/Umdot-ttf/Proportional/Bold/Umdot20-Bold.ttf"  # 20, 22, 7
    #FONT_FILE = "../font/Galmuri-v2.40.4/Galmuri9.ttf"                  # 10, 10, 4
    #FONT_FILE = "../font/Galmuri-v2.40.4/Galmuri11.ttf"                 # 12, 12, 4
    
    FONT0 = ["../font/Umdot-ttf/Proportional/Regular/Umdot14.ttf", 13, 19, 5]
    FONT1 = ["../font/Umdot-ttf/Proportional/Regular/Umdot12.ttf", 12, 17, 4]
    FONT2 = ["../font/Umdot-ttf/Proportional/Bold/Umdot20-Bold.ttf", 20, 29, 7]
    FONT3 = ["../font/Galmuri-v2.40.4/Galmuri9.ttf", 10, 14, 4]
    FONT4 = ["../font/Galmuri-v2.40.4/Galmuri11.ttf", 12, 16, 4]
    FONTX = FONT4
    
    FONT_FILE = FONTX[0]
    FONT_SIZE = FONTX[1]                # 폰트 크기
    
    # 좌우 여백 설정 (세로는 baseline 옵션으로 고정되므로 좌우만 지정)
    PAD_LEFT = 0
    PAD_RIGHT = 0
    
    # ★ 세로 크기 제어 핵심 변수
    FIXED_HIEGHT = FONTX[2]  # 폰트 고정 높이 (픽셀)
    BASELINE_BELOW = FONTX[3]   # Baseline 기준 아래쪽(받침, 내림글자용)으로 확보할 높이 (픽셀)
    # 결과적으로 모든 글자의 세로 길이는 고정적으로 14 + 4 = 18 픽셀이 됩니다.

    BACKGROUND_COLOR = (0, 0, 0, 255)    # 검은색 배경
    FONT_COLOR = (255, 255, 255, 255)    # 흰색 글씨

    FONT_NAME = os.path.basename(FONT_FILE)
    OUT_NAME = f"{FONT_NAME}_{FIXED_HIEGHT}"
    
    generate_custom_bitmap_font(
            font_path=FONT_FILE,
            font_size=FONT_SIZE,
            char_list=hangul_2350,
            output_image_path=f"./font/{OUT_NAME}.png",
            output_txt_path=f"./font/{OUT_NAME}.txt",
            padding_left=PAD_LEFT,
            padding_right=PAD_RIGHT,
            fixed_height=FIXED_HIEGHT,
            baseline_below=BASELINE_BELOW,
            bg_color=BACKGROUND_COLOR,
            fg_color=FONT_COLOR,
            max_atlas_width=1024
        )
