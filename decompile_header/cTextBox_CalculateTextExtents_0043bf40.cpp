/*
                             **************************************************************
                             *                          FUNCTION                          *
                             **************************************************************
                               bool __thiscall cTextBox_CalculateTextExtents_0043bf40(c
                               assume FS_OFFSET = 0xffdff000
             bool              AL:1           <RETURN>
             cTextBox *        ECX:4          this
             undefined4        Stack[-0x4]:4  local_4                                 XREF[2]:     0043bf5d(W), 
                                                                                                   0043bfed(R)  
             undefined4        Stack[-0x8]:4  local_8                                 XREF[1]:     0043bf52(W)  
                             cTextBox_CalculateTextExtents_0043bf40          XREF[2]:     0043c614(c), 
                                                                                          FUN_0043e9f0:0043e9fd(c)  
        0043bf40 83 ec 08        SUB        ESP,0x8
        0043bf43 53              PUSH       EBX
        0043bf44 55              PUSH       EBP
        0043bf45 56              PUSH       ESI
        0043bf46 8b f1           MOV        ESI,this
        0043bf48 33 db           XOR        EBX,EBX
        0043bf4a 33 c0           XOR        EAX,EAX
        0043bf4c 8b 8e 3c        MOV        this,dword ptr [ESI + 0x13c]
                 01 00 00
        0043bf52 89 44 24 0c     MOV        dword ptr [ESP + local_8],EAX
        0043bf56 3b cb           CMP        this,EBX
        0043bf58 bd 0a 00        MOV        EBP,0xa
                 00 00
        0043bf5d 89 5c 24 10     MOV        dword ptr [ESP + local_4],EBX
        0043bf61 0f 8e 80        JLE        LAB_0043bfe7
                 00 00 00
        0043bf67 57              PUSH       EDI
                             LAB_0043bf68                                    XREF[1]:     0043bfe0(j)  
        0043bf68 8b 86 98        MOV        EAX,dword ptr [ESI + 0x198]
                 01 00 00
        0043bf6e 33 ff           XOR        EDI,EDI
        0043bf70 8b 0c 98        MOV        this,dword ptr [EAX + EBX*0x4]
        0043bf73 8d 04 98        LEA        EAX,[EAX + EBX*0x4]
        0043bf76 80 39 00        CMP        byte ptr [this->vptr],0x0
        0043bf79 74 57           JZ         LAB_0043bfd2
                             LAB_0043bf7b                                    XREF[1]:     0043bfd0(j)  
        0043bf7b 8b 00           MOV        EAX,dword ptr [EAX]
        0043bf7d 8b 8e a0        MOV        this,dword ptr [ESI + 0x1a0]
                 01 00 00
        0043bf83 8a 04 07        MOV        AL,byte ptr [EDI + EAX*0x1]
        0043bf86 8b 11           MOV        EDX,dword ptr [this->vptr]
        0043bf88 50              PUSH       EAX
        0043bf89 ff 52 1c        CALL       dword ptr [EDX + 0x1c]
        0043bf8c 3b 44 24 10     CMP        EAX,dword ptr [ESP + 0x10]
        0043bf90 7e 04           JLE        LAB_0043bf96
        0043bf92 89 44 24 10     MOV        dword ptr [ESP + 0x10],EAX
                             LAB_0043bf96                                    XREF[1]:     0043bf90(j)  
        0043bf96 8b 86 98        MOV        EAX,dword ptr [ESI + 0x198]
                 01 00 00
        0043bf9c 8b 8e a0        MOV        this,dword ptr [ESI + 0x1a0]
                 01 00 00
        0043bfa2 8b 04 98        MOV        EAX,dword ptr [EAX + EBX*0x4]
        0043bfa5 8b 11           MOV        EDX,dword ptr [this->vptr]
        0043bfa7 8a 04 38        MOV        AL,byte ptr [EAX + EDI*0x1]
        0043bfaa 50              PUSH       EAX
        0043bfab ff 52 20        CALL       dword ptr [EDX + 0x20]
        0043bfae 8b 4c 24 14     MOV        this,dword ptr [ESP + 0x14]
        0043bfb2 03 e8           ADD        EBP,EAX
        0043bfb4 8d 45 14        LEA        EAX,[EBP + 0x14]
        0043bfb7 3b c1           CMP        EAX,this
        0043bfb9 7e 04           JLE        LAB_0043bfbf
        0043bfbb 89 44 24 14     MOV        dword ptr [ESP + 0x14],EAX
                             LAB_0043bfbf                                    XREF[1]:     0043bfb9(j)  
        0043bfbf 8b 8e 98        MOV        this,dword ptr [ESI + 0x198]
                 01 00 00
        0043bfc5 47              INC        EDI
        0043bfc6 8b 14 99        MOV        EDX,dword ptr [this->vptr + EBX*0x4]
        0043bfc9 8d 04 99        LEA        EAX,[this->vptr + EBX*0x4]
        0043bfcc 80 3c 17 00     CMP        byte ptr [EDI + EDX*0x1],0x0
        0043bfd0 75 a9           JNZ        LAB_0043bf7b
                             LAB_0043bfd2                                    XREF[1]:     0043bf79(j)  
        0043bfd2 8b 86 3c        MOV        EAX,dword ptr [ESI + 0x13c]
                 01 00 00
        0043bfd8 43              INC        EBX
        0043bfd9 3b d8           CMP        EBX,EAX
        0043bfdb bd 0a 00        MOV        EBP,0xa
                 00 00
        0043bfe0 7c 86           JL         LAB_0043bf68
        0043bfe2 8b 44 24 10     MOV        EAX,dword ptr [ESP + 0x10]
        0043bfe6 5f              POP        EDI
                             LAB_0043bfe7                                    XREF[1]:     0043bf61(j)  
        0043bfe7 8b 96 3c        MOV        EDX,dword ptr [ESI + 0x13c]
                 01 00 00
        0043bfed 8b 4c 24 10     MOV        this,dword ptr [ESP + local_4]
        0043bff1 0f af d0        IMUL       EDX,EAX
        0043bff4 89 86 40        MOV        dword ptr [ESI + 0x140],EAX
                 01 00 00
        0043bffa 89 8e 44        MOV        dword ptr [ESI + 0x144],this
                 01 00 00
        0043c000 89 96 48        MOV        dword ptr [ESI + 0x148],EDX
                 01 00 00
        0043c006 5e              POP        ESI
        0043c007 5d              POP        EBP
        0043c008 b0 01           MOV        AL,0x1
        0043c00a 5b              POP        EBX
        0043c00b 83 c4 08        ADD        ESP,0x8
        0043c00e c3              RET
        0043c00f 90              align      align(1)
*/

bool __thiscall cTextBox_CalculateTextExtents_0043bf40(cTextBox *this)

{
  char cVar1;
  char **ppcVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int local_8;
  int local_4;
  
  iVar5 = 0;
  iVar3 = 0;
  local_8 = 0;
  local_4 = 0;
  if (0 < this->m_nLineCount) {
    do {
      iVar6 = 10;
      ppcVar2 = this->m_ppLines;
      iVar7 = 0;
      cVar1 = *ppcVar2[iVar5];
      iVar3 = local_8;
      while (local_8 = iVar3, cVar1 != '\0') {
        iVar4 = (*this->pFont->vptr->cFont_GetCharHeight_00435e00)
                          (this->pFont,ppcVar2[iVar5][iVar7]);
        if (iVar3 < iVar4) {
          local_8 = iVar4;
        }
        iVar3 = (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                          (this->pFont,this->m_ppLines[iVar5][iVar7]);
        iVar6 = iVar6 + iVar3;
        if (local_4 < iVar6 + 0x14) {
          local_4 = iVar6 + 0x14;
        }
        ppcVar2 = this->m_ppLines;
        iVar7 = iVar7 + 1;
        iVar3 = local_8;
        cVar1 = ppcVar2[iVar5][iVar7];
      }
      iVar5 = iVar5 + 1;
    } while (iVar5 < this->m_nLineCount);
  }
  this->m_rcBounds_left = iVar3;
  this->m_nTotalWidth = local_4;
  this->m_rcBounds_top = this->m_nLineCount * iVar3;
  return true;
}

