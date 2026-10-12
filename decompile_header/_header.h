struct CharCodeMap {
    int count;                 // +0x00
    int **keys;                // +0x04
    int **values;              // +0x08
};

struct Glyph {
    int charCode;              // +0x00
    int left;                  // +0x04
    int top;                   // +0x08
    int right;                 // +0x0c
    int bottom;                // +0x10
    IDirectDrawSurface *surface;             // +0x14
};

struct cFont {
    void *vtable;              // +0x00

    int glyphCount;            // +0x04
    Glyph **glyphs;            // +0x08
    CharCodeMap *charMap;      // +0x0c

    int unknown_10;            // +0x10

    IDirectDrawSurface *surface; // +0x14

    HDC hdc;                   // +0x18
    HBITMAP dibBitmap;         // +0x1c
    void *dibBits;             // +0x20

    int canvasWidth;            // +0x24
    int canvasHeight;           // +0x28
};

HFONT __thiscall cFont_CreateFontDIB_004356f0(cFont *this,LPCSTR pszFaceName,DWORD dwCharSet,int nHeight);	0
BOOL __thiscall cFont_RasterizeGlyphs_00435950(cFont *this);												4
BOOL __thiscall cFont_UploadDIBToDirectDrawSurface_004359b0(cFont *this)									8
BOOL __thiscall cFont_CreateSurface_00435c10(cFont *this,IDirectDraw *pDD)									c
BOOL __thiscall cFont_Reset_00435c70(cFont *this,LPCSTR pszFaceName,DWORD dwCharSet,int nHeight)			10
BOOL __thiscall cFont_AddGlyph_00435d00(cFont *this,RECT *pRect,int charCode)								14
void __thiscall cFont_SetSurface_00435dd0(cFont *this,IDirectDrawSurface *pSurface)							18
int __thiscall cFont_GetCharHeight_00435e00(cFont *this,char charCode)										1c
int __thiscall cFont_GetCharWidth_00435e30(cFont *this,char charCode)										20
RECT * __thiscall cFont_GetCharRect_00435e60(cFont *this,int charCode)										24
IDirectDrawSurface * __thiscall cFont_GetSurface_005484e0(cFont *this)										28







/* // param_1은 CTextLayout 또는 CUIStaticText 객체
   BOOL __fastcall CTextLayout_WordWrap_00444890(CTextLayout* this)
   {
       int   nCharIdx       = 0;  // local_1c: 전체 문자열을 순회하는 글자 인덱스
       int   nMaxLineWidth  = 0;  // local_18: 여러 줄 중 가장 긴 줄의 가로 폭
       int   nCurrentLineWidth = 0; // iVar8: 현재 작업 중인 줄의 가로 폭 누적합
       int   nLineCount     = 1;  // local_14: 계산된 총 라인 수
       int   nMaxLineHeight = 0;  // iStack_c: 줄의 최대 글자 높이
       int   nLastSpaceIdx  = 0;  // iStack_10 / iVar11: 가장 최근에 발견한
   '공백(띄어쓰기)' 인덱스
   
       // 박스의 최대 허용 가로폭 획득 (예: GetMaxWidth())
       int nBoxMaxWidth = this->GetMaxWidth(); // vftable + 0x48
   
       const char* szFullText = this->m_pszText; // param_1[0x4a]: 전체 출력할 원본 문자열
   
       this->m_nTotalHeight = 0; // param_1[0x52]
       this->m_nTotalWidth  = 0; // param_1[0x51]
   
       if (szFullText[0] != '\0') 
       {
           do {
               char c = szFullText[nCharIdx];
   
               // -------------------------------------------------------------
               // [경우 1] 개행 문자('\n')를 만났을 때 -> 강제 줄바꿈
               // -------------------------------------------------------------
               if (c == '\n') 
               {
                   // 현재 라인의 끝 인덱스(nCharIdx - 1)와 폭(nCurrentLineWidth)을
   동적 배열에 추가
                   this->m_pLineEndIndices = AppendToArray(this->m_pLineEndIndices, nLineCount,
   nCharIdx - 1);
                   this->m_pLineWidths     = AppendToArray(this->m_pLineWidths, nLineCount,
   nCurrentLineWidth);
   
                   if (nMaxLineWidth < nCurrentLineWidth)
                       nMaxLineWidth = nCurrentLineWidth;
   
                   nCurrentLineWidth = 0;   // 다음 줄을 위해 폭 초기화
                   nLineCount++;            // 줄 수 증가
                   nCharIdx++;              // '\n' 다음 글자로 이동
               }
               // -------------------------------------------------------------
               // [경우 2] 일반 문자를 만났을 때
               // -------------------------------------------------------------
               else 
               {
                   // 공백 문자(띄어쓰기) 검사: 단어 단위 줄바꿈을 위해
   마지막 공백 위치 기억
                   if (isspace_00445480(c) == 0 && isspace_00445480(szFullText[nCharIdx - 1]) != 0)
   {
                       nLastSpaceIdx = nCharIdx;
                   }
   
                   // [★★★ 패치 포인트 A: GetCharHeight (+0x1c)]
                   // 글자 높이 측정
                   int h = this->m_pFont->GetCharHeight(c); // vftable + 0x1c
                   if (nMaxLineHeight < h)
                       nMaxLineHeight = h;
   
                   // [★★★ 패치 포인트 B: GetCharWidth (+0x20)]
                   // 글자 폭 측정 및 현재 줄 폭에 누적
                   int w = this->m_pFont->GetCharWidth(c);  // vftable + 0x20
                   nCurrentLineWidth += w;
   
                   // ---------------------------------------------------------
                   // 현재 줄 폭이 박스 최대 폭을 초과했는가? -> Word Wrap(자동
   줄바꿈) 발동!
                   // ---------------------------------------------------------
                   if (nCurrentLineWidth > nBoxMaxWidth) 
                   {
                       // 초과했으므로, 가장 최근 공백(nLastSpaceIdx) 위치까지
   글자 폭들을 빼면서 백트래킹(되감기)
                       for (; nLastSpaceIdx <= nCharIdx; nCharIdx--) {
                           int charW = this->m_pFont->GetCharWidth(szFullText[nCharIdx]);
                           nCurrentLineWidth -= charW;
                       }
   
                       // 공백 문자들 자체의 폭도 추가로 차감
                       // ... (공백 문자 너비 차감 루프) ...
   
                       // 현재 줄 저장 (동적 배열 추가)
                       this->m_pLineEndIndices = AppendToArray(this->m_pLineEndIndices, nLineCount,
   nLastSpaceIdx);
                       this->m_pLineWidths     = AppendToArray(this->m_pLineWidths, nLineCount,
   nCurrentLineWidth);
   
                       nCurrentLineWidth = 0;
                       nLineCount++;
                       nCharIdx = nLastSpaceIdx; // 다음 줄은 공백 다음 단어부터 시작!
                   }
                   else 
                   {
                       // [★★★ 패치 포인트 C: 다음 글자로 이동]
                       // 원래 코드: 1바이트 전진 (nCharIdx++)
                       // 한글인 경우: 2바이트 전진 (nCharIdx += 2) 이어야 함!
                       nCharIdx++;
                   }
               }
           } while (szFullText[nCharIdx] != '\0');
   
           if (nMaxLineWidth < nCurrentLineWidth)
               nMaxLineWidth = nCurrentLineWidth;
       }
   
       // 마지막 줄 정보 저장 및 최종 텍스트 레이아웃 정보 세팅
       this->m_nLineCount   = nLineCount;      // param_1[0x4e]
       this->m_nTotalWidth  = nMaxLineWidth;   // param_1[0x51]
       this->m_nTotalHeight = ...;             // param_1[0x52]
   
       return TRUE;
   } */

BOOL __thiscall cTextLayout_WordWrap_00444890(cTextLayout *this)

{
  newFunction **ppnVar1;
  undefined uVar2;
  int nBoxMaxWidth;
  char *szFullText;
  undefined4 *puVar3;
  int charH;
  int charW_1;
  int charW_2;
  int charW_3;
  undefined3 extraout_var;
  uint uVar4;
  uint uVar5;
  int nCurrentLineWidth;
  undefined4 *puVar6;
  int nLastSpaceIdx;
  undefined4 *puVar7;
  cTextLayout_vtable *pcVar8;
  uint uStack_24;
  uint uStack_20;
  int nCharIdx;
  int local_18;
  int local_14;
  int iStack_10;
  int local_8;
  
  nCharIdx = 0;
  local_18 = 0;
  local_8 = 0;
  nCurrentLineWidth = 0;
  local_14 = 1;
  nBoxMaxWidth._0_1_ = (*this->vptr[3].unknown0c)();
  nBoxMaxWidth = CONCAT31(nBoxMaxWidth._1_3_,(undefined)nBoxMaxWidth);
  szFullText = (char *)this[1].vptr;
  this[1].dummy[7] = 0;
  this[1].dummy[6] = 0;
  iStack_10 = 0;
  if (*(char *)&((cTextLayout_vtable *)szFullText)->unknown00 != '\0') {
    uStack_24 = 4;
    uStack_20 = 0;
    nLastSpaceIdx = 0;
    do {
      if (szFullText[nCharIdx] == '\n') {
        puVar3 = operator_new(uStack_24);
        puVar6 = (undefined4 *)this[1].dummy[4];
        puVar7 = puVar3;
        for (uVar4 = uStack_20 >> 2; uVar4 != 0; uVar4 = uVar4 - 1) {
          *puVar7 = *puVar6;
          puVar6 = puVar6 + 1;
          puVar7 = puVar7 + 1;
        }
        for (charW_3 = 0; charW_3 != 0; charW_3 = charW_3 + -1) {
          *(undefined1 *)puVar7 = *(undefined1 *)puVar6;
          puVar6 = (undefined4 *)((int)puVar6 + 1);
          puVar7 = (undefined4 *)((int)puVar7 + 1);
        }
        *(int *)((uStack_24 - 4) + (int)puVar3) = nCharIdx + -1;
        delete_005ea8f2((void *)this[1].dummy[4]);
        this[1].dummy[4] = (int)puVar3;
        puVar3 = operator_new(uStack_24);
        puVar6 = (undefined4 *)this[1].dummy[5];
        puVar7 = puVar3;
        for (uVar4 = uStack_20 >> 2; uVar4 != 0; uVar4 = uVar4 - 1) {
          *puVar7 = *puVar6;
          puVar6 = puVar6 + 1;
          puVar7 = puVar7 + 1;
        }
        for (charW_3 = 0; charW_3 != 0; charW_3 = charW_3 + -1) {
          *(undefined1 *)puVar7 = *(undefined1 *)puVar6;
          puVar6 = (undefined4 *)((int)puVar6 + 1);
          puVar7 = (undefined4 *)((int)puVar7 + 1);
        }
        *(int *)((uStack_24 - 4) + (int)puVar3) = nCurrentLineWidth;
        delete_005ea8f2((void *)this[1].dummy[5]);
        this[1].dummy[5] = (int)puVar3;
        if (local_18 < nCurrentLineWidth) {
          local_18 = nCurrentLineWidth;
        }
        nCurrentLineWidth = 0;
        uStack_24 = uStack_24 + 4;
        uStack_20 = uStack_20 + 4;
        local_14 = local_14 + 1;
        nCharIdx = nCharIdx + 1;
        charW_3 = iStack_10;
      }
      else {
        charW_3 = isspace_00445480();
        if ((charW_3 == 0) &&
           ((nCharIdx == 0 || ((0 < nCharIdx && (charW_3 = isspace_00445480(), charW_3 != 0)))))) {
          nLastSpaceIdx = nCharIdx;
          iStack_10 = nCharIdx;
        }
                    /* [★★★ 패치 포인트 A: GetCharHeight (+0x1c)] */
        charH = (*this->pFont->vptr->cFont_GetCharHeight_00435e00)
                          (this->pFont,*(char *)((int)&(this[1].vptr)->unknown00 + nCharIdx));
        if (local_8 < charH) {
          local_8 = charH;
        }
                    /* [★★★ 패치 포인트 B: GetCharWidth (+0x20)] */
        charW_1 = (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                            (this->pFont,*(char *)((int)&(this[1].vptr)->unknown00 + nCharIdx));
        nCurrentLineWidth = nCurrentLineWidth + charW_1;
        if (nBoxMaxWidth < nCurrentLineWidth) {
          for (; nLastSpaceIdx <= nCharIdx; nCharIdx = nCharIdx + -1) {
                    /* [★★★ 패치 포인트 C: GetCharWidth (+0x20)] */
            charW_2 = (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                                (this->pFont,*(char *)((int)&(this[1].vptr)->unknown00 + nCharIdx));
            nCurrentLineWidth = nCurrentLineWidth - charW_2;
          }
          charW_3 = isspace_00445480();
          while (charW_3 != 0) {
                    /* [★★★ 패치 포인트 D: GetCharWidth (+0x20)] */
            charW_3 = (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                                (this->pFont,*(char *)((int)&(this[1].vptr)->unknown00 + nCharIdx));
            nCurrentLineWidth = nCurrentLineWidth - charW_3;
            nCharIdx = nCharIdx + -1;
            charW_3 = isspace_00445480();
          }
          if (local_18 < nCurrentLineWidth) {
            local_18 = nCurrentLineWidth;
          }
          puVar3 = operator_new(uStack_24);
          puVar6 = (undefined4 *)this[1].dummy[4];
          puVar7 = puVar3;
          for (uVar4 = uStack_20 >> 2; uVar4 != 0; uVar4 = uVar4 - 1) {
            *puVar7 = *puVar6;
            puVar6 = puVar6 + 1;
            puVar7 = puVar7 + 1;
          }
          for (charW_3 = 0; charW_3 != 0; charW_3 = charW_3 + -1) {
            *(undefined1 *)puVar7 = *(undefined1 *)puVar6;
            puVar6 = (undefined4 *)((int)puVar6 + 1);
            puVar7 = (undefined4 *)((int)puVar7 + 1);
          }
          *(int *)((uStack_24 - 4) + (int)puVar3) = nCharIdx;
          delete_005ea8f2((void *)this[1].dummy[4]);
          this[1].dummy[4] = (int)puVar3;
          puVar3 = operator_new(uStack_24);
          puVar6 = (undefined4 *)this[1].dummy[5];
          puVar7 = puVar3;
          for (uVar4 = uStack_20 >> 2; uVar4 != 0; uVar4 = uVar4 - 1) {
            *puVar7 = *puVar6;
            puVar6 = puVar6 + 1;
            puVar7 = puVar7 + 1;
          }
          for (charW_3 = 0; charW_3 != 0; charW_3 = charW_3 + -1) {
            *(undefined1 *)puVar7 = *(undefined1 *)puVar6;
            puVar6 = (undefined4 *)((int)puVar6 + 1);
            puVar7 = (undefined4 *)((int)puVar7 + 1);
          }
          *(int *)((uStack_24 - 4) + (int)puVar3) = nCurrentLineWidth;
          delete_005ea8f2((void *)this[1].dummy[5]);
          nCurrentLineWidth = 0;
          local_14 = local_14 + 1;
          uStack_24 = uStack_24 + 4;
          uStack_20 = uStack_20 + 4;
          this[1].dummy[5] = (int)puVar3;
          charW_3 = iStack_10;
          nCharIdx = nLastSpaceIdx;
        }
        else {
                    /* [★★★ 패치 포인트 E:다음 글자로 이동]
                       원래 코드: 1바이트 전진 (nCharIdx++)
                       한글인 경우: 2바이트 전진 (nCharIdx += 2) 이어야 함! */
          nCharIdx = nCharIdx + 1;
          charW_3 = nLastSpaceIdx;
        }
      }
      szFullText = (char *)this[1].vptr;
      nLastSpaceIdx = charW_3;
    } while (*(char *)((int)&((cTextLayout_vtable *)szFullText)->unknown00 + nCharIdx) != '\0');
    if (local_18 < nCurrentLineWidth) {
      local_18 = nCurrentLineWidth;
    }
  }
  uVar4 = local_14 * 4;
  puVar3 = operator_new(uVar4);
  puVar6 = (undefined4 *)this[1].dummy[4];
  puVar7 = puVar3;
  for (uVar5 = uVar4 - 4 >> 2; uVar5 != 0; uVar5 = uVar5 - 1) {
    *puVar7 = *puVar6;
    puVar6 = puVar6 + 1;
    puVar7 = puVar7 + 1;
  }
  for (charW_3 = 0; charW_3 != 0; charW_3 = charW_3 + -1) {
    *(undefined1 *)puVar7 = *(undefined1 *)puVar6;
    puVar6 = (undefined4 *)((int)puVar6 + 1);
    puVar7 = (undefined4 *)((int)puVar7 + 1);
  }
  uVar5 = 0xffffffff;
  pcVar8 = this[1].vptr;
  do {
    if (uVar5 == 0) break;
    uVar5 = uVar5 - 1;
    ppnVar1 = &pcVar8->unknown00;
    pcVar8 = (cTextLayout_vtable *)((int)&pcVar8->unknown00 + 1);
  } while (*(char *)ppnVar1 != '\0');
  puVar3[local_14 + -1] = ~uVar5 - 1;
  delete_005ea8f2((void *)this[1].dummy[4]);
  this[1].dummy[6] = local_18;
  this[1].dummy[4] = (int)puVar3;
  this[1].dummy[8] = local_8;
  this[1].dummy[7] = local_14 * local_8;
  this[1].dummy[3] = local_14;
  puVar3 = operator_new(uVar4);
  puVar6 = (undefined4 *)this[1].dummy[5];
  puVar7 = puVar3;
  for (uVar4 = uVar4 - 4 >> 2; uVar4 != 0; uVar4 = uVar4 - 1) {
    *puVar7 = *puVar6;
    puVar6 = puVar6 + 1;
    puVar7 = puVar7 + 1;
  }
  for (charW_3 = 0; charW_3 != 0; charW_3 = charW_3 + -1) {
    *(undefined1 *)puVar7 = *(undefined1 *)puVar6;
    puVar6 = (undefined4 *)((int)puVar6 + 1);
    puVar7 = (undefined4 *)((int)puVar7 + 1);
  }
  puVar3[local_14 + -1] = nCurrentLineWidth;
  delete_005ea8f2((void *)this[1].dummy[5]);
  this[1].dummy[5] = (int)puVar3;
  if (this[1].dummy[1] == 1) {
    uVar2 = (*this->vptr[3].unknown0c)(this);
    this[1].dummy[6] = CONCAT31(extraout_var,uVar2);
  }
  if (0 < this[1].dummy[6]) {
    this[1].dummy[6] = this[1].dummy[6] + 2;
  }
  charW_3 = this[1].dummy[7];
  if (0 < charW_3) {
    charW_3 = charW_3 + 2;
    this[1].dummy[7] = charW_3;
  }
  return CONCAT31((int3)((uint)charW_3 >> 8),1);
}
















/* BOOL __thiscall CTextBox::CalculateTextExtents(CTextBox* this)
   {
       int maxBoxWidth = 0;
       int maxLineHeight = 0;
   
       if (this->m_nLineCount > 0) 
       {
           for (int lineIdx = 0; lineIdx < this->m_nLineCount; ++lineIdx) 
           {
               int currentLineWidth = 10; // 왼쪽 마진 (10px)
               const char* szLine = this->m_ppLines[lineIdx];
   
               for (int charIdx = 0; szLine[charIdx] != '\0'; ++charIdx) 
               {
                   char c = szLine[charIdx];
   
                   // +0x1c: cFont::GetCharHeight(c)
                   int charHeight = this->m_pFont->GetCharHeight(c);
                   if (maxLineHeight < charHeight) {
                       maxLineHeight = charHeight;
                   }
   
                   // +0x20: cFont::GetCharWidth(c)
                   int charWidth = this->m_pFont->GetCharWidth(c);
                   currentLineWidth += charWidth;
   
                   // 20px(0x14)는 오른쪽 마진/패딩
                   if (maxBoxWidth < currentLineWidth + 20) {
                       maxBoxWidth = currentLineWidth + 20;
                   }
               }
           }
       }
   
       this->m_rcBounds.left   = 0;
       this->m_rcBounds.right  = maxBoxWidth; // 계산된 최대 가로 크기 저장
       this->m_rcBounds.top    = 0;
   
       return TRUE;
   } */

BOOL __thiscall cTextManager_CalculateTextExtents_0043bf40(cTextManager *this)

{
  int iVar1;
  int charHeight_Width;
  int maxLineHeight;
  int lineIdx;
  int currentLineWidth;
  int charIdx;
  int local_8;
  int maxBoxWidth;
  int temp_5f01419038;
  char c;
  char *szLine;
  
  lineIdx = 0;
  charHeight_Width = 0;
  local_8 = 0;
  maxBoxWidth = 0;
  if (0 < this->dummy[0x4e]) {
    do {
      currentLineWidth = 10;
      charIdx = 0;
      szLine = (char *)(this->dummy[0x65] + lineIdx * 4);
      c = **(char **)(this->dummy[0x65] + lineIdx * 4);
      charHeight_Width = local_8;
      while (local_8 = charHeight_Width, c != '\0') {
        iVar1 = (*this->pFont->vptr->cFont_GetCharHeight_00435e00)
                          (this->pFont,*(char *)(charIdx + *(int *)szLine));
        if (charHeight_Width < iVar1) {
          local_8 = iVar1;
        }
                    /* this + x1a0 : cFont // cFont + x20 : cFont_GetCharWidth_00435e30 */
        charHeight_Width =
             (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                       (this->pFont,*(char *)(*(int *)(this->dummy[0x65] + lineIdx * 4) + charIdx));
        currentLineWidth = currentLineWidth + charHeight_Width;
        if (maxBoxWidth < currentLineWidth + 0x14) {
          maxBoxWidth = currentLineWidth + 0x14;
        }
        charIdx = charIdx + 1;
        szLine = (char *)(this->dummy[0x65] + lineIdx * 4);
        charHeight_Width = local_8;
        c = *(char *)(charIdx + *(int *)(this->dummy[0x65] + lineIdx * 4));
      }
      lineIdx = lineIdx + 1;
    } while (lineIdx < this->dummy[0x4e]);
  }
  this->dummy[0x4f] = charHeight_Width;
  this->dummy[0x50] = maxBoxWidth;
  this->dummy[0x51] = this->dummy[0x4e] * charHeight_Width;
  return CONCAT31((int3)((uint)charHeight_Width >> 8),1);
}




















/* WARNING: Removing unreachable block (ram,0x0043bdc9) */
/* WARNING: Removing unreachable block (ram,0x0043bdf3) */
/* WARNING: Removing unreachable block (ram,0x0043bdf7) */
/* WARNING: Removing unreachable block (ram,0x0043bdfb) */
/* WARNING: Removing unreachable block (ram,0x0043be05) */
/* WARNING: Removing unreachable block (ram,0x0043be0c) */
/* WARNING: Removing unreachable block (ram,0x0043be23) */
/* WARNING: Removing unreachable block (ram,0x0043be31) */
/* WARNING: Removing unreachable block (ram,0x0043be39) */
/* BOOL __fastcall CTextBox_DrawTextLines_0043bc80(CTextBox* pTextBox)
   {
       DDSURFACEDESC2 ddsdDst;
       ZeroMemory(&ddsdDst, sizeof(ddsdDst));
       ddsdDst.dwSize = sizeof(ddsdDst);
   
       // 타겟 서피스(텍스트 박스 서피스) Lock
       IDirectDrawSurface7* pDstSurface = pTextBox->m_pTargetSurfaceBox->GetSurface();
       if (pDstSurface->Lock(NULL, &ddsdDst, DDLOCK_WAIT, NULL) != DD_OK) {
           return FALSE;
       }
   
       // 폰트 서피스(아틀라스) 포인터 획득 (vftable + 0x28)
       IDirectDrawSurface7* pFontSurface = pTextBox->m_pFont->GetSurface();
   
       // 폰트 서피스 Lock
       DDSURFACEDESC2 ddsdSrc;
       // ... FontSurface Lock ...
   
       // 라인 단위 렌더링 루프
       for (int lineIdx = 0; lineIdx < pTextBox->m_nLineCount; ++lineIdx)
       {
           const char* pLine = pTextBox->m_ppLines[lineIdx];
           int penX = 0; // 또는 시작 X 오프셋
   
           for (int charIdx = 0; pLine[charIdx] != '\0'; ++charIdx)
           {
               char c = pLine[charIdx];
   
               // 글리프 영역 구하기: cFont::GetCharRect(c) (vftable + 0x24)
               RECT* pGlyphRect = pTextBox->m_pFont->GetCharRect((int)c);
   
               if (pGlyphRect && (pGlyphRect->right - pGlyphRect->left > 0))
               {
                   // [Ghidra WARNING으로 가려졌던 블록]:
                   // pFontSurface(ddsdSrc)의 pGlyphRect 영역 픽셀들을
                   // pDstSurface(ddsdDst)의 (penX, currentY) 위치로 픽셀 복사(Blt)
               }
   
               // 글자 너비만큼 커서 X 이동: cFont::GetCharWidth(c) (vftable + 0x20)
               penX += pTextBox->m_pFont->GetCharWidth(c);
           }
       }
   
       // 언락
       pDstSurface->Unlock(NULL);
       pFontSurface->Unlock(NULL);
   
       return TRUE;
   } */

BOOL __thiscall cTextManager_DrawTextLines_0043bc80(cTextManager *this)

{
  int *piVar1;
  int *piVar2;
  uint uVar3;
  undefined4 uVar4;
  int iVar5;
  int *unaff_EBP;
  int piVar5;
  undefined4 *puVar6;
  int *piVar8;
  int iVar9;
  undefined1 auStack_90 [4];
  int *piStack_8c;
  cTextManager *local_88;
  undefined4 local_7c [16];
  undefined4 uStack_3c;
  char cVar1;
  int piVar7;
  
  puVar6 = local_7c;
  for (iVar5 = 0x1f; iVar5 != 0; iVar5 = iVar5 + -1) {
    *puVar6 = 0;
    puVar6 = puVar6 + 1;
  }
  local_7c[0] = 0x7c;
  local_88 = this;
  piVar1 = (int *)(**(code **)(*(int *)this[1].dummy[6] + 0x54))();
  piStack_8c = piVar1;
  piVar2 = (*this->pFont->vptr->cFont_GetSurface_005484e0)();
  piVar5 = 0;
  iVar5 = 0;
  piVar7 = (int)piVar1;
  uVar3 = (**(code **)(*piVar1 + 100))(piVar1,0,local_7c,0,0);
  if (uVar3 != 0) {
    return uVar3 & 0xffffff00;
  }
  switch(uStack_3c) {
  case 8:
    break;
  case 0x10:
    break;
  case 0x18:
    break;
  case 0x20:
  }
  (**(code **)(*unaff_EBP + 100))(unaff_EBP,0,auStack_90,0);
  if (0 < this->dummy[99]) {
    do {
      piVar7 = 0;
      piVar1 = (int *)(this->dummy[0x62] + piVar5 * 4);
      cVar1 = **(char **)(this->dummy[0x62] + piVar5 * 4);
      while (cVar1 != '\0') {
        piVar8 = (int *)piVar7;
        piVar1 = (int *)(*this->pFont->vptr->cFont_GetCharRect_00435e60)
                                  (this->pFont,(int)*(char *)(*piVar1 + piVar7));
        if (0 < piVar1[2] - *piVar1) {
          piVar7 = (int)piVar8;
        }
        iVar9 = iVar5;
        (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                  (this->pFont,*(char *)(*(int *)(this->dummy[0x62] + iVar5 * 4) + piVar7));
        piVar7 = piVar7 + 1;
        piVar1 = (int *)(this->dummy[0x62] + iVar5 * 4);
        cVar1 = *(char *)(piVar7 + *(int *)(this->dummy[0x62] + iVar5 * 4));
        piVar5 = iVar5;
        iVar5 = iVar9;
      }
      piVar5 = piVar5 + 1;
      piVar1 = piVar2;
      iVar5 = piVar5;
    } while (piVar5 < this->dummy[99]);
  }
  (**(code **)(*piVar1 + 0x80))(piVar1,0);
  uVar4 = (**(code **)(*(int *)piVar7 + 0x80))(piVar7,0);
  return CONCAT31((int3)((uint)uVar4 >> 8),1);
}

