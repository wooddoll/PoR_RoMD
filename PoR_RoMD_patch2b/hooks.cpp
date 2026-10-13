#include <windows.h>

void patchBytes(void* ptr, const void* data, size_t bytes) {
    DWORD oldProtect;
    if (VirtualProtect(ptr, bytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(ptr, data, bytes);
        VirtualProtect(ptr, bytes, oldProtect, &oldProtect);
        // 패치한 메모리 영역의 CPU 캐시를 갱신합니다.
        FlushInstructionCache(GetCurrentProcess(), ptr, bytes);
    }
}

// ============================================================
// [패치 1] CFontCharMap::FindGlyphIndex (0x004355c0)
// ============================================================
void PatchFindGlyphIndex()
{
    // 0x00435601: XOR EAX,EAX; NOP (인덱스 검색 실패 시 -1 대신 0 반환)
    const unsigned char patch[] = { 0x31, 0xC0, 0x90 };
    patchBytes(reinterpret_cast<void*>(0x00435601), patch, sizeof(patch));
}

// ============================================================
// [패치 2] cFont::GetCharHeight/GetCharWidth → code cave 이동
// ============================================================
void PatchGetCharWH_CodeCave()
{
    // 원래 함수는 1바이트 charCode만 처리하므로, 2바이트 한글을 처리하기 위해
    // code cave (0x00613800, 0x00613900)에 재구현한 함수로 점프시킵니다.

    // 1. GetCharHeight: JMP 0x00613800 (E9 FB D9 1D 00 + NOP 패딩)
    const unsigned char jmp1[] = { 0xE9, 0xFB, 0xD9, 0x1D, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90 };
    patchBytes(reinterpret_cast<void*>(0x00435E00), jmp1, sizeof(jmp1));

    // 2. GetCharWidth: JMP 0x00613900 (E9 CB DA 1D 00 + NOP 패딩)
    const unsigned char jmp2[] = { 0xE9, 0xCB, 0xDA, 0x1D, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90 };
    patchBytes(reinterpret_cast<void*>(0x00435E30), jmp2, sizeof(jmp2));

    // 3. Code Cave: GetCharHeight 구현 (0x00613800)
    const unsigned char caveH[] = {
        // c1 = charCode & 0xFF; c2 = (charCode >> 8) & 0xFF
        0x8B, 0x44, 0x24, 0x04,             // MOV EAX, [ESP+4]
        0x0F, 0xB6, 0xC0,                   // MOVZX EAX, AL (c1)
        0x8B, 0x54, 0x24, 0x04,             // MOV EDX, [ESP+4]
        0xC1, 0xEA, 0x08,                   // SHR EDX, 8
        0x81, 0xE2, 0xFF, 0x00, 0x00, 0x00, // AND EDX, 0xFF (c2)
        // if ((0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE))
        0x3D, 0xB0, 0x00, 0x00, 0x00,       // CMP EAX, 0xB0
        0x7C, 0x1E,                         // JL check_tail (0x00613839)
        0x3D, 0xC8, 0x00, 0x00, 0x00,       // CMP EAX, 0xC8
        0x7F, 0x17,                         // JG check_tail (0x00613839)
        0x81, 0xFA, 0xA1, 0x00, 0x00, 0x00, // CMP EDX, 0xA1
        0x7C, 0x0F,                         // JL check_tail (0x00613839)
        0x81, 0xFA, 0xFE, 0x00, 0x00, 0x00, // CMP EDX, 0xFE
        0x7F, 0x07,                         // JG check_tail (0x00613839)
        // charCode = (c1 << 8) | c2
        0xC1, 0xE0, 0x08,                   // SHL EAX, 8
        0x0B, 0xC2,                         // OR EAX, EDX
        0xEB, 0x16,                         // JMP do_find (0x0061384F)
        // check_tail: else if (0xA1 <= c1 && c1 <= 0xFE) → return 0
        0x3D, 0xA1, 0x00, 0x00, 0x00,       // CMP EAX, 0xA1
        0x7C, 0x0C,                         // JL do_ascii (0x0061384C)
        0x3D, 0xFE, 0x00, 0x00, 0x00,       // CMP EAX, 0xFE
        0x7F, 0x05,                         // JG do_ascii (0x0061384C)
        0x31, 0xC0,                         // XOR EAX, EAX
        0xC2, 0x04, 0x00,                   // RET 4
        // do_ascii: charCode = (char)c1 (sign extend)
        0x0F, 0xBE, 0xC0,                   // MOVSX EAX, AL
        // do_find: idx = FindCodeIndex(this->charMap, charCode)
        0x56,                               // PUSH ESI
        0x8B, 0xF1,                         // MOV ESI, ECX
        0x50,                               // PUSH EAX
        0x8B, 0x4E, 0x0C,                   // MOV ECX, [ESI+0xC]
        0xE8, 0x65, 0x25, 0xE2, 0xFF,       // CALL 0x004355C0
        // return this->glyphs[idx]->y1 - this->glyphs[idx]->y0
        0x8B, 0x4E, 0x08,                   // MOV ECX, [ESI+0x8]
        0x5E,                               // POP ESI
        0x8B, 0x0C, 0x81,                   // MOV ECX, [ECX+EAX*4]
        0x8B, 0x41, 0x10,                   // MOV EAX, [ECX+0x10] (y1)
        0x8B, 0x51, 0x08,                   // MOV EDX, [ECX+0x8]  (y0)
        0x2B, 0xC2,                         // SUB EAX, EDX
        0xC2, 0x04, 0x00                    // RET 4
    };
    patchBytes(reinterpret_cast<void*>(0x00613800), caveH, sizeof(caveH));

    // 4. Code Cave: GetCharWidth 구현 (0x00613900)
    const unsigned char caveW[] = {
        // c1 = charCode & 0xFF; c2 = (charCode >> 8) & 0xFF
        0x8B, 0x44, 0x24, 0x04,             // MOV EAX, [ESP+4]
        0x0F, 0xB6, 0xC0,                   // MOVZX EAX, AL (c1)
        0x8B, 0x54, 0x24, 0x04,             // MOV EDX, [ESP+4]
        0xC1, 0xEA, 0x08,                   // SHR EDX, 8
        0x81, 0xE2, 0xFF, 0x00, 0x00, 0x00, // AND EDX, 0xFF (c2)
        // if ((0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE))
        0x3D, 0xB0, 0x00, 0x00, 0x00,       // CMP EAX, 0xB0
        0x7C, 0x1E,                         // JL check_tail (0x00613939)
        0x3D, 0xC8, 0x00, 0x00, 0x00,       // CMP EAX, 0xC8
        0x7F, 0x17,                         // JG check_tail (0x00613939)
        0x81, 0xFA, 0xA1, 0x00, 0x00, 0x00, // CMP EDX, 0xA1
        0x7C, 0x0F,                         // JL check_tail (0x00613939)
        0x81, 0xFA, 0xFE, 0x00, 0x00, 0x00, // CMP EDX, 0xFE
        0x7F, 0x07,                         // JG check_tail (0x00613939)
        // charCode = (c1 << 8) | c2
        0xC1, 0xE0, 0x08,                   // SHL EAX, 8
        0x0B, 0xC2,                         // OR EAX, EDX
        0xEB, 0x16,                         // JMP do_find (0x0061394F)
        // check_tail: else if (0xA1 <= c1 && c1 <= 0xFE) → return 0
        0x3D, 0xA1, 0x00, 0x00, 0x00,       // CMP EAX, 0xA1
        0x7C, 0x0C,                         // JL do_ascii (0x0061394C)
        0x3D, 0xFE, 0x00, 0x00, 0x00,       // CMP EAX, 0xFE
        0x7F, 0x05,                         // JG do_ascii (0x0061394C)
        0x31, 0xC0,                         // XOR EAX, EAX
        0xC2, 0x04, 0x00,                   // RET 4
        // do_ascii: charCode = (char)c1 (sign extend)
        0x0F, 0xBE, 0xC0,                   // MOVSX EAX, AL
        // do_find: idx = FindCodeIndex(this->charMap, charCode)
        0x56,                               // PUSH ESI
        0x8B, 0xF1,                         // MOV ESI, ECX
        0x50,                               // PUSH EAX
        0x8B, 0x4E, 0x0C,                   // MOV ECX, [ESI+0xC]
        0xE8, 0x65, 0x22, 0xE2, 0xFF,       // CALL 0x004355C0
        // return this->glyphs[idx]->x1 - this->glyphs[idx]->x0
        0x8B, 0x4E, 0x08,                   // MOV ECX, [ESI+0x8]
        0x5E,                               // POP ESI
        0x8B, 0x0C, 0x81,                   // MOV ECX, [ECX+EAX*4]
        0x8B, 0x41, 0x0C,                   // MOV EAX, [ECX+0xC] (x1)
        0x8B, 0x51, 0x04,                   // MOV EDX, [ECX+0x4] (x0)
        0x2B, 0xC2,                         // SUB EAX, EDX
        0xC2, 0x04, 0x00                    // RET 4
    };
    patchBytes(reinterpret_cast<void*>(0x00613900), caveW, sizeof(caveW));

    // 5. GetCharRect: JMP 0x00613A00 (E9 9B DB 1D 00 + NOP 패딩)
    const unsigned char jmp3[] = { 0xE9, 0x9B, 0xDB, 0x1D, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90 };
    patchBytes(reinterpret_cast<void*>(0x00435E60), jmp3, sizeof(jmp3));

    // 6. Code Cave: GetCharRect 구현 (0x00613A00)
    const unsigned char caveR[] = {
        // c1 = charCode & 0xFF; c2 = (charCode >> 8) & 0xFF
        0x8B, 0x44, 0x24, 0x04,             // MOV EAX, [ESP+4]
        0x0F, 0xB6, 0xC0,                   // MOVZX EAX, AL (c1)
        0x8B, 0x54, 0x24, 0x04,             // MOV EDX, [ESP+4]
        0xC1, 0xEA, 0x08,                   // SHR EDX, 8
        0x81, 0xE2, 0xFF, 0x00, 0x00, 0x00, // AND EDX, 0xFF (c2)
        // if ((0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE))
        0x3D, 0xB0, 0x00, 0x00, 0x00,       // CMP EAX, 0xB0
        0x7C, 0x11,                         // JL check_tail (0x00613A27)
        0x3D, 0xC8, 0x00, 0x00, 0x00,       // CMP EAX, 0xC8
        0x7F, 0x0C,                         // JG check_tail (0x00613A27)
        0x81, 0xFA, 0xA1, 0x00, 0x00, 0x00, // CMP EDX, 0xA1
        0x7C, 0x07,                         // JL check_tail (0x00613A27)
        0x81, 0xFA, 0xFE, 0x00, 0x00, 0x00, // CMP EDX, 0xFE
        0x7F, 0x02,                         // JG check_tail (0x00613A27)
        // charCode = (c1 << 8) | c2
        0xC1, 0xE0, 0x08,                   // SHL EAX, 8
        0x0B, 0xC2,                         // OR EAX, EDX
        0xEB, 0x1C,                         // JMP do_find (0x00613A43)
        // check_tail: else if (0xA1 <= c1 && c1 <= 0xFE) → return &glyphs[0]->x0
        0x3D, 0xA1, 0x00, 0x00, 0x00,       // CMP EAX, 0xA1
        0x7C, 0x17,                         // JL do_ascii (0x00613A57)
        0x3D, 0xFE, 0x00, 0x00, 0x00,       // CMP EAX, 0xFE
        0x7F, 0x10,                         // JG do_ascii (0x00613A57)
        0x56,                               // PUSH ESI
        0x8B, 0xF1,                         // MOV ESI, ECX
        0x8B, 0x4E, 0x08,                   // MOV ECX, [ESI+0x8] (glyphs)
        0x8B, 0x04, 0x81,                   // MOV EAX, [ECX+EAX*4] (glyphs[0])
        0x5E,                               // POP ESI
        0x83, 0xC0, 0x04,                   // ADD EAX, 4 (&glyphs[0]->x0)
        0xC2, 0x04, 0x00,                   // RET 4
        // do_ascii: charCode = (char)c1 (sign extend)
        0x0F, 0xBE, 0xC0,                   // MOVSX EAX, AL
        // do_find: idx = FindCodeIndex(this->charMap, charCode)
        0x56,                               // PUSH ESI
        0x8B, 0xF1,                         // MOV ESI, ECX
        0x50,                               // PUSH EAX
        0x8B, 0x4E, 0x0C,                   // MOV ECX, [ESI+0xC]
        0xE8, 0x1B, 0xE2, 0xFF, 0xFF,       // CALL 0x004355C0
        // return &this->glyphs[idx]->x0 (FindCodeIndex가 0 반환하므로 -1 체크 불필요)
        0x8B, 0x4E, 0x08,                   // MOV ECX, [ESI+0x8]
        0x5E,                               // POP ESI
        0x8B, 0x04, 0x81,                   // MOV EAX, [ECX+EAX*4]
        0x83, 0xC0, 0x04,                   // ADD EAX, 4
        0xC2, 0x04, 0x00                    // RET 4
    };
    patchBytes(reinterpret_cast<void*>(0x00613A00), caveR, sizeof(caveR));
}

// ============================================================
// [패치 3] CTextLayout::WordWrap (0x00444890) 4바이트 읽기
// ============================================================
void PatchWordWrap_4ByteRead()
{
    // MOV AL, [EAX+ESI*1] (8A 04 30) → MOV EAX, [EAX+ESI*1] (8B 04 30)
    // MOV DL, [EDX+ESI*1] (8A 14 32) → MOV EDX, [EDX+ESI*1] (8B 14 32)
    // 각 호출부에서 1바이트만 변경하여 4바이트를 읽어서 전달합니다.
    // GetCharHeight/GetCharWidth 내부에서 리드바이트(0xB0-0xC8) 확인 후
    // 테일바이트(0xA1-0xFE)와 결합하여 사용하고, 테일바이트면 0을 반환합니다.

    const unsigned char patch[] = { 0x8B };

    // 1. GetCharHeight 호출부 (0x00444a08)
    patchBytes(reinterpret_cast<void*>(0x00444a08), patch, sizeof(patch));

    // 2. GetCharWidth 호출부 (0x00444a27)
    patchBytes(reinterpret_cast<void*>(0x00444a27), patch, sizeof(patch));

    // 3. 백트래킹 GetCharWidth 호출부 (0x00444a52)
    patchBytes(reinterpret_cast<void*>(0x00444a52), patch, sizeof(patch));

    // 4. 공백 스킵 GetCharWidth 호출부 (0x00444a89)
    patchBytes(reinterpret_cast<void*>(0x00444a89), patch, sizeof(patch));
}

// ============================================================
// [패치 4] CTextBox::CalculateTextExtents (0x0043bf40) 4바이트 읽기
// ============================================================
void PatchCalculateTextExtents_4ByteRead()
{
    // MOV AL, [EDI+EAX*1] (8A 04 07) → MOV EAX, [EDI+EAX*1] (8B 04 07)
    // MOV AL, [EAX+EDI*1] (8A 04 38) → MOV EAX, [EAX+EDI*1] (8B 04 38)
    // 각 호출부에서 1바이트만 변경하여 4바이트를 읽어서 전달합니다.
    // GetCharHeight/GetCharWidth 내부에서 리드바이트(0xB0-0xC8) 확인 후
    // 테일바이트(0xA1-0xFE)와 결합하여 사용하고, 테일바이트면 0을 반환합니다.

    const unsigned char patch[] = { 0x8B };

    // 1. GetCharHeight 호출부 (0x0043bf83)
    patchBytes(reinterpret_cast<void*>(0x0043bf83), patch, sizeof(patch));

    // 2. GetCharWidth 호출부 (0x0043bfa7)
    patchBytes(reinterpret_cast<void*>(0x0043bfa7), patch, sizeof(patch));
}

// ============================================================
// [패치 5] CTextBox::DrawTextLines (0x0043bd9b) 4바이트 읽기
// ============================================================
void PatchDrawTextLines_CodeCave()
{
    // MOVSX EAX, byte ptr [EAX+EBX*1] (0F BE 04 18) → MOV EAX, [EAX+EBX*1] (8B 04 18) + NOP
    // MOV AL, byte ptr [EAX+EBX*1] (8A 04 18) → MOV EAX, [EAX+EBX*1] (8B 04 18)
    // 각 호출부에서 4바이트를 읽어서 전달합니다.
    // GetCharRect/GetCharWidth 내부에서 리드바이트(0xB0-0xC8) 확인 후
    // 테일바이트(0xA1-0xFE)와 결합하여 사용하고, 테일바이트면 0을 반환합니다.
    // 인덱스 증가(INC EBX)는 항상 +1이지만, 테일바이트에서 0이 반환되므로
    // 그리기/폭 누적이 건너뛰어지고 다음 반복에서 테일바이트 다음으로 이동합니다.

    // 1. GetCharRect 호출부 (0x0043bd9b): MOVSX → MOV + NOP
    const unsigned char patch1[] = { 0x8B, 0x04, 0x18, 0x90 };
    patchBytes(reinterpret_cast<void*>(0x0043bd9b), patch1, sizeof(patch1));

    // 2. GetCharWidth 호출부 (0x0043be72): MOV AL → MOV EAX (1바이트만 변경)
    const unsigned char patch2[] = { 0x8B };
    patchBytes(reinterpret_cast<void*>(0x0043be72), patch2, sizeof(patch2));
}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
const char g_szNewFontPath[] = ".\\Data\\Fonts_kr\\font%d";
const char g_szNewDialgouePath[] = ".\\Data\\Dialogue_kr\\%s";

void PatchResourcePath()
{
    uint32_t newPathAddr = reinterpret_cast<uint32_t>(g_szNewFontPath);
    // 2. 0x0043ffd1 주소의 4바이트 포인터를 문자열 주소로 교체!
    patchBytes(reinterpret_cast<void*>(0x0043ffd1), &newPathAddr, 4);

    newPathAddr = reinterpret_cast<uint32_t>(g_szNewDialgouePath);
    uint32_t pDialpath[] = { 0x005b2b86, 0x005b2ba5, 0x005b35d6, 0x005b35f5, 0x005b3832, 0x005b39d2, 0x005b3a31,0x005b3a6f, 0x005b3e3a, 0x005b44f1, 0x005b4541 };
    int elementCount = sizeof(pDialpath) / sizeof(pDialpath[0]);
    // 2. pDialpath[] 주소들 4바이트 포인터를 문자열 주소로 교체!
    for (int i = 0; i < elementCount; ++i) {
        patchBytes(reinterpret_cast<void*>(pDialpath[i]), &newPathAddr, 4);
    }
}


// ============================================================
// [메모리 패치] 전체 적용
// ============================================================
void ApplyMemoryPatches()
{
    PatchResourcePath();

    PatchFindGlyphIndex();
    PatchGetCharWH_CodeCave();
    PatchWordWrap_4ByteRead();
    PatchCalculateTextExtents_4ByteRead();
    PatchDrawTextLines_CodeCave();
}
