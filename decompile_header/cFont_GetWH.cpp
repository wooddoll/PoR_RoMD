/*
                             **************************************************************
                             *                          FUNCTION                          *
                             **************************************************************
                               int __thiscall cFont_GetCharHeight_00435e00(cFont * this
                               assume FS_OFFSET = 0xffdff000
             int               EAX:4          <RETURN>
             cFont *           ECX:4          this
             char              Stack[0x4]:1   charCode                                XREF[1]:     00435e00(R)  
                             cFont_GetCharHeight_00435e00                    XREF[1]:     00622528(*)  
        00435e00 0f be 44        MOVSX      EAX,byte ptr [ESP + charCode]
                 24 04
        00435e05 56              PUSH       ESI
        00435e06 8b f1           MOV        ESI,this
        00435e08 50              PUSH       EAX
        00435e09 8b 4e 0c        MOV        this,dword ptr [ESI + 0xc]
        00435e0c e8 af f7        CALL       cCharCodeMap_FindCodeIndex_004355c0              int cCharCodeMap_FindCodeIndex_0
                 ff ff
        00435e11 8b 4e 08        MOV        this,dword ptr [ESI + 0x8]
        00435e14 5e              POP        ESI
        00435e15 8b 0c 81        MOV        this,dword ptr [this->vptr + EAX*0x4]
        00435e18 8b 41 10        MOV        EAX,dword ptr [ECX + this->unknown_10]
        00435e1b 8b 51 08        MOV        EDX,dword ptr [ECX + this->glyphs]
        00435e1e 2b c2           SUB        EAX,EDX
        00435e20 c2 04 00        RET        0x4
        00435e23 90              ??         90h
        00435e24 90              ??         90h
        00435e25 90              ??         90h
        00435e26 90              ??         90h
        00435e27 90              ??         90h
        00435e28 90              ??         90h
        00435e29 90              ??         90h
        00435e2a 90              ??         90h
        00435e2b 90              ??         90h
        00435e2c 90              ??         90h
        00435e2d 90              ??         90h
        00435e2e 90              ??         90h
        00435e2f 90              ??         90h
*/

int __thiscall cFont_GetCharHeight_00435e00(cFont *this,char charCode)

{
  int idx;
  
  idx = cCharCodeMap_FindCodeIndex_004355c0(this->charMap,(int)charCode);
  return this->glyphs[idx]->y1 - this->glyphs[idx]->y0;
}

int __thiscall cFont_GetCharHeight_00435e00_patch(cFont *this, int charCode)
{
  int c1 = charCode & 0xFF;
  int c2 = (charCode >> 8) & 0xFF;

  if ((0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE)) { // 한글 lead & tail
    charCode = (c1 << 8) | c2;
  } else if (0xA1 <= c1 && c1 <= 0xFE) {  // 한글 tail
    return 0;
  } else {  // ASCII
    charCode = (char)c1;
  }

  int idx = cCharCodeMap_FindCodeIndex_004355c0(this->charMap, charCode);
  return this->glyphs[idx]->y1 - this->glyphs[idx]->y0;
}

/*
                             **************************************************************
                             *                          FUNCTION                          *
                             **************************************************************
                               int __thiscall cFont_GetCharWidth_00435e30(cFont * this,
                               assume FS_OFFSET = 0xffdff000
             int               EAX:4          <RETURN>
             cFont *           ECX:4          this
             char              Stack[0x4]:1   charCode                                XREF[1]:     00435e30(R)  
                             cFont_GetCharWidth_00435e30                     XREF[1]:     0062252c(*)  
        00435e30 0f be 44        MOVSX      EAX,byte ptr [ESP + charCode]
                 24 04
        00435e35 56              PUSH       ESI
        00435e36 8b f1           MOV        ESI,this
        00435e38 50              PUSH       EAX
        00435e39 8b 4e 0c        MOV        this,dword ptr [ESI + 0xc]
        00435e3c e8 7f f7        CALL       cCharCodeMap_FindCodeIndex_004355c0              int cCharCodeMap_FindCodeIndex_0
                 ff ff
        00435e41 8b 4e 08        MOV        this,dword ptr [ESI + 0x8]
        00435e44 5e              POP        ESI
        00435e45 8b 0c 81        MOV        this,dword ptr [this->vptr + EAX*0x4]
        00435e48 8b 41 0c        MOV        EAX,dword ptr [ECX + this->charMap]
        00435e4b 8b 51 04        MOV        EDX,dword ptr [ECX + this->glyphCount]
        00435e4e 2b c2           SUB        EAX,EDX
        00435e50 c2 04 00        RET        0x4
        00435e53 90              ??         90h
        00435e54 90              ??         90h
        00435e55 90              ??         90h
        00435e56 90              ??         90h
        00435e57 90              ??         90h
        00435e58 90              ??         90h
        00435e59 90              ??         90h
        00435e5a 90              ??         90h
        00435e5b 90              ??         90h
        00435e5c 90              ??         90h
        00435e5d 90              ??         90h
        00435e5e 90              ??         90h
        00435e5f 90              ??         90h
*/

int __thiscall cFont_GetCharWidth_00435e30(cFont *this, char charCode)
{
  int idx;
  
  idx = cCharCodeMap_FindCodeIndex_004355c0(this->charMap,(int)charCode);
  return this->glyphs[idx]->x1 - this->glyphs[idx]->x0;
}

int __thiscall cFont_GetCharWidth_00435e30_patch(cFont *this, int charCode)
{
  int c1 = charCode & 0xFF;
  int c2 = (charCode >> 8) & 0xFF;

  if ((0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE)) { // 한글 lead & tail
    charCode = (c1 << 8) | c2;
  } else if (0xA1 <= c1 && c1 <= 0xFE) {  // 한글 tail
    return 0;
  } else {  // ASCII
    charCode = (char)c1;
  }

  int idx = cCharCodeMap_FindCodeIndex_004355c0(this->charMap,(int)charCode);
  return this->glyphs[idx]->x1 - this->glyphs[idx]->x0;
}




/*
                             **************************************************************
                             *                          FUNCTION                          *
                             **************************************************************
                               RECT * __thiscall cFont_GetCharRect_00435e60(cFont * thi
                               assume FS_OFFSET = 0xffdff000
             RECT *            EAX:4          <RETURN>
             cFont *           ECX:4          this
             int               Stack[0x4]:4   charCode                                XREF[1]:     00435e60(R)  
                             cFont_GetCharRect_00435e60                      XREF[1]:     00622530(*)  
        00435e60 8b 44 24 04     MOV        EAX,dword ptr [ESP + charCode]
        00435e64 56              PUSH       ESI
        00435e65 8b f1           MOV        ESI,this
        00435e67 50              PUSH       EAX
        00435e68 8b 4e 0c        MOV        this,dword ptr [ESI + 0xc]
        00435e6b e8 50 f7        CALL       cCharCodeMap_FindCodeIndex_004355c0              int cCharCodeMap_FindCodeIndex_0
                 ff ff
        00435e70 83 f8 ff        CMP        EAX,-0x1
        00435e73 74 0d           JZ         LAB_00435e82
        00435e75 8b 4e 08        MOV        this,dword ptr [ESI + 0x8]
        00435e78 5e              POP        ESI
        00435e79 8b 04 81        MOV        EAX,dword ptr [this->vptr + EAX*0x4]
        00435e7c 83 c0 04        ADD        EAX,0x4
        00435e7f c2 04 00        RET        0x4
                             LAB_00435e82                                    XREF[1]:     00435e73(j)  
        00435e82 33 c0           XOR        EAX,EAX
        00435e84 5e              POP        ESI
        00435e85 c2 04 00        RET        0x4
        00435e88 90              ??         90h
        00435e89 90              ??         90h
        00435e8a 90              ??         90h
        00435e8b 90              ??         90h
        00435e8c 90              ??         90h
        00435e8d 90              ??         90h
        00435e8e 90              ??         90h
        00435e8f 90              ??         90h
*/

RECT * __thiscall cFont_GetCharRect_00435e60(cFont *this, int charCode)
{
  int iVar1;
  
  iVar1 = cCharCodeMap_FindCodeIndex_004355c0(this->charMap,charCode);  // charMap에 charCode이 없으면 0을 반환
  if (iVar1 != -1) {
    return (RECT *)&this->glyphs[iVar1]->x0;
  }
  return (RECT *)0x0;
}

RECT * __thiscall cFont_GetCharRect_00435e60_patch(cFont *this,int charCode)
{
  int c1 = charCode & 0xFF;
  int c2 = (charCode >> 8) & 0xFF;

  if ((0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE)) { // 한글 lead & tail
    charCode = (c1 << 8) | c2;
  } else if (0xA1 <= c1 && c1 <= 0xFE) {  // 한글 tail
    return 0;
  } else {  // ASCII
    charCode = (char)c1;
  }

  int idx = cCharCodeMap_FindCodeIndex_004355c0(this->charMap,charCode);
  if (idx != -1) {
    return (RECT *)&this->glyphs[idx]->x0;
  }
  return (RECT *)0x0;
}