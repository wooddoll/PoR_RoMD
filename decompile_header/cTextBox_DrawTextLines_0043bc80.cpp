/*
                             **************************************************************
                             *                          FUNCTION                          *
                             **************************************************************
                               bool __thiscall cTextBox_DrawTextLines_0043bc80(cTextBox
                               assume FS_OFFSET = 0xffdff000
             bool              AL:1           <RETURN>
             cTextBox *        ECX:4          this
             int               Stack[0x4]:4   param_2
             DDSURFACEDESC2    Stack[-0x7c]   local_7c                                XREF[2]:     0043bc93(*), 
                                                                                                   0043bc9f(W)  
             undefined4        Stack[-0x88]:4 local_88                                XREF[1]:     0043bca7(W)  
                             cTextBox_DrawTextLines_0043bc80                 XREF[1]:     0043c73f(c)  
        0043bc80 81 ec c0        SUB        ESP,0xc0
                 00 00 00
        0043bc86 53              PUSH       EBX
        0043bc87 55              PUSH       EBP
        0043bc88 56              PUSH       ESI
        0043bc89 57              PUSH       EDI
        0043bc8a 8b f1           MOV        ESI,this
        0043bc8c b9 1f 00        MOV        this,0x1f
                 00 00
        0043bc91 33 c0           XOR        EAX,EAX
        0043bc93 8d 7c 24 54     LEA        EDI=>local_7c,[ESP + 0x54]
        0043bc97 f3 ab           STOSD.REP  ES:EDI
        0043bc99 8b 8e c0        MOV        this,dword ptr [ESI + 0x1c0]
                 01 00 00
        0043bc9f c7 44 24        MOV        dword ptr [ESP + local_7c.dwSize],0x7c
                 54 7c 00 
                 00 00
        0043bca7 89 74 24 48     MOV        dword ptr [ESP + local_88],ESI
        0043bcab 8b 01           MOV        EAX,dword ptr [this->vptr]
        0043bcad ff 50 54        CALL       dword ptr [EAX + 0x54]
        0043bcb0 8b 8e a0        MOV        this,dword ptr [ESI + 0x1a0]
                 01 00 00
        0043bcb6 8b d8           MOV        EBX,EAX
        0043bcb8 89 5c 24 44     MOV        dword ptr [ESP + 0x44],EBX
        0043bcbc 8b 11           MOV        EDX,dword ptr [this->vptr]
        0043bcbe ff 52 28        CALL       dword ptr [EDX + 0x28]
        0043bcc1 33 ed           XOR        EBP,EBP
        0043bcc3 8d 4c 24 54     LEA        this,[ESP + 0x54]
        0043bcc7 55              PUSH       EBP
        0043bcc8 55              PUSH       EBP
        0043bcc9 89 44 24 24     MOV        dword ptr [ESP + 0x24],EAX
        0043bccd 8b 03           MOV        EAX,dword ptr [EBX]
        0043bccf 51              PUSH       this
        0043bcd0 55              PUSH       EBP
        0043bcd1 53              PUSH       EBX
        0043bcd2 89 6c 24 2c     MOV        dword ptr [ESP + 0x2c],EBP
        0043bcd6 89 6c 24 48     MOV        dword ptr [ESP + 0x48],EBP
        0043bcda c7 44 24        MOV        dword ptr [ESP + 0x44],0x5
                 44 05 00 
                 00 00
        0043bce2 ff 50 64        CALL       dword ptr [EAX + 0x64]
        0043bce5 8b 7c 24 64     MOV        EDI,dword ptr [ESP + 0x64]
        0043bce9 8b 54 24 78     MOV        EDX,dword ptr [ESP + 0x78]
        0043bced d1 ef           SHR        EDI,0x1
        0043bcef 85 c0           TEST       EAX,EAX
        0043bcf1 89 54 24 40     MOV        dword ptr [ESP + 0x40],EDX
        0043bcf5 89 7c 24 50     MOV        dword ptr [ESP + 0x50],EDI
        0043bcf9 0f 85 f7        JNZ        LAB_0043bef6
                 01 00 00
        0043bcff 8b 84 24        MOV        EAX,dword ptr [ESP + 0xa8]
                 a8 00 00 00
        0043bd06 83 c0 f8        ADD        EAX,-0x8
        0043bd09 83 f8 18        CMP        EAX,0x18
        0043bd0c 77 35           JA         switchD_0043bd16::caseD_9
        0043bd0e 33 c9           XOR        this,this
        0043bd10 8a 88 1c        MOV        this+0x3,byte ptr [EAX + 0x43bf1c]=>PTR_caseD_   = 0043bd3b
                 bf 43 00
                             switchD_0043bd16::switchD
        0043bd16 ff 24 8d        JMP        dword ptr [this->vptr*0x4 + switchD_0043bd16::   = 0043bd1d
                 08 bf 43 00
                             switchD_0043bd16::caseD_8                       XREF[2]:     0043bd16(j), 0043bf08(*)  
        0043bd1d c7 44 24        MOV        dword ptr [ESP + 0x10],0x1
                 10 01 00 
                 00 00
        0043bd25 eb 1c           JMP        switchD_0043bd16::caseD_9
                             switchD_0043bd16::caseD_10                      XREF[2]:     0043bd16(j), 0043bf0c(*)  
        0043bd27 c7 44 24        MOV        dword ptr [ESP + 0x10],0x2
                 10 02 00 
                 00 00
        0043bd2f eb 12           JMP        switchD_0043bd16::caseD_9
                             switchD_0043bd16::caseD_18                      XREF[2]:     0043bd16(j), 0043bf10(*)  
        0043bd31 c7 44 24        MOV        dword ptr [ESP + 0x10],0x3
                 10 03 00 
                 00 00
        0043bd39 eb 08           JMP        switchD_0043bd16::caseD_9
                             switchD_0043bd16::caseD_20                      XREF[2]:     0043bd16(j), 0043bf14(*)  
        0043bd3b c7 44 24        MOV        dword ptr [ESP + 0x10],0x4
                 10 04 00 
                 00 00
                             switchD_0043bd16::caseD_a                       XREF[6]:     0043bd0c(j), 0043bd16(j), 
                             switchD_0043bd16::caseD_b                                    0043bd25(j), 0043bd2f(j), 
                             switchD_0043bd16::caseD_c                                    0043bd39(j), 0043bf18(*)  
                             switchD_0043bd16::caseD_d
                             switchD_0043bd16::caseD_e
                             switchD_0043bd16::caseD_f
                             switchD_0043bd16::caseD_11
                             switchD_0043bd16::caseD_12
                             switchD_0043bd16::caseD_13
                             switchD_0043bd16::caseD_14
                             switchD_0043bd16::caseD_15
                             switchD_0043bd16::caseD_16
                             switchD_0043bd16::caseD_17
                             switchD_0043bd16::caseD_19
                             switchD_0043bd16::caseD_1a
                             switchD_0043bd16::caseD_1b
                             switchD_0043bd16::caseD_1c
                             switchD_0043bd16::caseD_1d
                             switchD_0043bd16::caseD_1e
                             switchD_0043bd16::caseD_1f
                             switchD_0043bd16::caseD_9
        0043bd43 8b 44 24 1c     MOV        EAX,dword ptr [ESP + 0x1c]
        0043bd47 6a 00           PUSH       0x0
        0043bd49 8d 4c 24 58     LEA        this,[ESP + 0x58]
        0043bd4d 6a 00           PUSH       0x0
        0043bd4f 8b 10           MOV        EDX,dword ptr [EAX]
        0043bd51 51              PUSH       this
        0043bd52 6a 00           PUSH       0x0
        0043bd54 50              PUSH       EAX
        0043bd55 ff 52 64        CALL       dword ptr [EDX + 0x64]
        0043bd58 8b 44 24 64     MOV        EAX,dword ptr [ESP + 0x64]
        0043bd5c 8b 54 24 78     MOV        EDX,dword ptr [ESP + 0x78]
        0043bd60 d1 e8           SHR        EAX,0x1
        0043bd62 89 44 24 28     MOV        dword ptr [ESP + 0x28],EAX
        0043bd66 8b 86 90        MOV        EAX,dword ptr [ESI + 0x190]
                 01 00 00
        0043bd6c 85 c0           TEST       EAX,EAX
        0043bd6e 89 54 24 4c     MOV        dword ptr [ESP + 0x4c],EDX
        0043bd72 0f 8e 55        JLE        LAB_0043becd
                 01 00 00
                             LAB_0043bd78                                    XREF[1]:     0043bec3(j)  
        0043bd78 8b 86 8c        MOV        EAX,dword ptr [ESI + 0x18c]
                 01 00 00
        0043bd7e 33 db           XOR        EBX,EBX
        0043bd80 89 5c 24 14     MOV        dword ptr [ESP + 0x14],EBX
        0043bd84 8b 0c a8        MOV        this,dword ptr [EAX + EBP*0x4]
        0043bd87 8d 04 a8        LEA        EAX,[EAX + EBP*0x4]
        0043bd8a 80 39 00        CMP        byte ptr [this->vptr],0x0
        0043bd8d 0f 84 0b        JZ         LAB_0043be9e
                 01 00 00
                             LAB_0043bd93                                    XREF[1]:     0043be98(j)  
        0043bd93 8b 00           MOV        EAX,dword ptr [EAX]
        0043bd95 8b 8e a0        MOV        this,dword ptr [ESI + 0x1a0]
                 01 00 00
        0043bd9b 0f be 04 18     MOVSX      EAX,byte ptr [EAX + EBX*0x1]
        0043bd9f 8b 11           MOV        EDX,dword ptr [this->vptr]
        0043bda1 50              PUSH       EAX
        0043bda2 ff 52 24        CALL       dword ptr [EDX + 0x24]
        0043bda5 8b 10           MOV        EDX,dword ptr [EAX]
        0043bda7 8b 68 08        MOV        EBP,dword ptr [EAX + 0x8]
        0043bdaa 8b 48 04        MOV        this,dword ptr [EAX + 0x4]
        0043bdad 8b 40 0c        MOV        EAX,dword ptr [EAX + 0xc]
        0043bdb0 2b ea           SUB        EBP,EDX
        0043bdb2 2b c1           SUB        EAX,this
        0043bdb4 85 ed           TEST       EBP,EBP
        0043bdb6 0f 8e a1        JLE        LAB_0043be5d
                 00 00 00
        0043bdbc 8b 5c 24 10     MOV        EBX,dword ptr [ESP + 0x10]
        0043bdc0 83 eb 02        SUB        EBX,0x2
        0043bdc3 0f 85 90        JNZ        LAB_0043be59
                 00 00 00
        0043bdc9 0f af 4c        IMUL       this,dword ptr [ESP + 0x28]
                 24 28
        0043bdce 03 ca           ADD        this,EDX
        0043bdd0 8b 54 24 4c     MOV        EDX,dword ptr [ESP + 0x4c]
        0043bdd4 8b 5c 24 30     MOV        EBX,dword ptr [ESP + 0x30]
        0043bdd8 8d 0c 4a        LEA        this,[EDX + this->vptr*0x2]
        0043bddb 8b d7           MOV        EDX,EDI
        0043bddd 0f af 54        IMUL       EDX,dword ptr [ESP + 0x34]
                 24 34
        0043bde2 03 d3           ADD        EDX,EBX
        0043bde4 8b 5c 24 40     MOV        EBX,dword ptr [ESP + 0x40]
        0043bde8 85 c0           TEST       EAX,EAX
        0043bdea 89 4c 24 24     MOV        dword ptr [ESP + 0x24],this
        0043bdee 8d 1c 53        LEA        EBX,[EBX + EDX*0x2]
        0043bdf1 7e 66           JLE        LAB_0043be59
        0043bdf3 89 44 24 20     MOV        dword ptr [ESP + 0x20],EAX
                             LAB_0043bdf7                                    XREF[1]:     0043be57(j)  
        0043bdf7 85 ed           TEST       EBP,EBP
        0043bdf9 7e 3e           JLE        LAB_0043be39
        0043bdfb 8b f9           MOV        EDI,this
        0043bdfd 8b f3           MOV        ESI,EBX
        0043bdff 2b fb           SUB        EDI,EBX
        0043be01 89 6c 24 2c     MOV        dword ptr [ESP + 0x2c],EBP
                             LAB_0043be05                                    XREF[1]:     0043be2f(j)  
        0043be05 66 83 3c        CMP        word ptr [EDI + ESI*0x1],0x0
                 37 00
        0043be0a 74 17           JZ         LAB_0043be23
        0043be0c 68 df 00        PUSH       0xdf
                 00 00
        0043be11 68 9f 00        PUSH       0x9f
                 00 00
        0043be16 6a 29           PUSH       0x29
        0043be18 e8 63 a3        CALL       FUN_00436180                                     ushort FUN_00436180(void)
                 ff ff
        0043be1d 83 c4 0c        ADD        ESP,0xc
        0043be20 66 89 06        MOV        word ptr [ESI],AX
                             LAB_0043be23                                    XREF[1]:     0043be0a(j)  
        0043be23 8b 44 24 2c     MOV        EAX,dword ptr [ESP + 0x2c]
        0043be27 83 c6 02        ADD        ESI,0x2
        0043be2a 48              DEC        EAX
        0043be2b 89 44 24 2c     MOV        dword ptr [ESP + 0x2c],EAX
        0043be2f 75 d4           JNZ        LAB_0043be05
        0043be31 8b 74 24 48     MOV        ESI,dword ptr [ESP + 0x48]
        0043be35 8b 7c 24 50     MOV        EDI,dword ptr [ESP + 0x50]
                             LAB_0043be39                                    XREF[1]:     0043bdf9(j)  
        0043be39 8b 4c 24 24     MOV        this,dword ptr [ESP + 0x24]
        0043be3d 8d 04 3f        LEA        EAX,[EDI + EDI*0x1]
        0043be40 03 d8           ADD        EBX,EAX
        0043be42 8b 44 24 28     MOV        EAX,dword ptr [ESP + 0x28]
        0043be46 03 c0           ADD        EAX,EAX
        0043be48 03 c8           ADD        this,EAX
        0043be4a 8b 44 24 20     MOV        EAX,dword ptr [ESP + 0x20]
        0043be4e 48              DEC        EAX
        0043be4f 89 4c 24 24     MOV        dword ptr [ESP + 0x24],this
        0043be53 89 44 24 20     MOV        dword ptr [ESP + 0x20],EAX
        0043be57 75 9e           JNZ        LAB_0043bdf7
                             LAB_0043be59                                    XREF[2]:     0043bdc3(j), 0043bdf1(j)  
        0043be59 8b 5c 24 14     MOV        EBX,dword ptr [ESP + 0x14]
                             LAB_0043be5d                                    XREF[1]:     0043bdb6(j)  
        0043be5d 8b 86 8c        MOV        EAX,dword ptr [ESI + 0x18c]
                 01 00 00
        0043be63 8b 6c 24 18     MOV        EBP,dword ptr [ESP + 0x18]
        0043be67 8b 8e a0        MOV        this,dword ptr [ESI + 0x1a0]
                 01 00 00
        0043be6d 8b 04 a8        MOV        EAX,dword ptr [EAX + EBP*0x4]
        0043be70 8b 11           MOV        EDX,dword ptr [this->vptr]
        0043be72 8a 04 18        MOV        AL,byte ptr [EAX + EBX*0x1]
        0043be75 50              PUSH       EAX
        0043be76 ff 52 20        CALL       dword ptr [EDX + 0x20]
        0043be79 8b 4c 24 30     MOV        this,dword ptr [ESP + 0x30]
        0043be7d 03 c8           ADD        this,EAX
        0043be7f 43              INC        EBX
        0043be80 89 4c 24 30     MOV        dword ptr [ESP + 0x30],this
        0043be84 8b 8e 8c        MOV        this,dword ptr [ESI + 0x18c]
                 01 00 00
        0043be8a 89 5c 24 14     MOV        dword ptr [ESP + 0x14],EBX
        0043be8e 8b 14 a9        MOV        EDX,dword ptr [this->vptr + EBP*0x4]
        0043be91 8d 04 a9        LEA        EAX,[this->vptr + EBP*0x4]
        0043be94 80 3c 13 00     CMP        byte ptr [EBX + EDX*0x1],0x0
        0043be98 0f 85 f5        JNZ        LAB_0043bd93
                 fe ff ff
                             LAB_0043be9e                                    XREF[1]:     0043bd8d(j)  
        0043be9e 8b 86 40        MOV        EAX,dword ptr [ESI + 0x140]
                 01 00 00
        0043bea4 8b 4c 24 34     MOV        this,dword ptr [ESP + 0x34]
        0043bea8 03 c8           ADD        this,EAX
        0043beaa 8b 86 90        MOV        EAX,dword ptr [ESI + 0x190]
                 01 00 00
        0043beb0 45              INC        EBP
        0043beb1 c7 44 24        MOV        dword ptr [ESP + 0x30],0x5
                 30 05 00 
                 00 00
        0043beb9 3b e8           CMP        EBP,EAX
        0043bebb 89 6c 24 18     MOV        dword ptr [ESP + 0x18],EBP
        0043bebf 89 4c 24 34     MOV        dword ptr [ESP + 0x34],this
        0043bec3 0f 8c af        JL         LAB_0043bd78
                 fe ff ff
        0043bec9 8b 5c 24 44     MOV        EBX,dword ptr [ESP + 0x44]
                             LAB_0043becd                                    XREF[1]:     0043bd72(j)  
        0043becd 8b 0b           MOV        this,dword ptr [EBX]
        0043becf 6a 00           PUSH       0x0
        0043bed1 53              PUSH       EBX
        0043bed2 ff 91 80        CALL       dword ptr [ECX + this+0x80]
                 00 00 00
        0043bed8 8b 44 24 1c     MOV        EAX,dword ptr [ESP + 0x1c]
        0043bedc 6a 00           PUSH       0x0
        0043bede 50              PUSH       EAX
        0043bedf 8b 10           MOV        EDX,dword ptr [EAX]
        0043bee1 ff 92 80        CALL       dword ptr [EDX + 0x80]
                 00 00 00
        0043bee7 5f              POP        EDI
        0043bee8 5e              POP        ESI
        0043bee9 5d              POP        EBP
        0043beea b0 01           MOV        AL,0x1
        0043beec 5b              POP        EBX
        0043beed 81 c4 c0        ADD        ESP,0xc0
                 00 00 00
        0043bef3 c2 04 00        RET        0x4
                             LAB_0043bef6                                    XREF[1]:     0043bcf9(j)  
        0043bef6 5f              POP        EDI
        0043bef7 5e              POP        ESI
        0043bef8 5d              POP        EBP
        0043bef9 32 c0           XOR        AL,AL
        0043befb 5b              POP        EBX
        0043befc 81 c4 c0        ADD        ESP,0xc0
                 00 00 00
        0043bf02 c2 04 00        RET        0x4
        0043bf05 8d              ??         8Dh
        0043bf06 49              ??         49h    I
        0043bf07 00              ??         00h
                             switchD_0043bd16::switchdataD_0043bf08          XREF[1]:     cTextBox_DrawTextLines_0043bc80:
        0043bf08 1d bd 43 00     addr       switchD_0043bd16::caseD_8
        0043bf0c 27 bd 43 00     addr       switchD_0043bd16::caseD_10
        0043bf10 31 bd 43 00     addr       switchD_0043bd16::caseD_18
                             PTR_caseD_20_0043bf14                           XREF[1]:     cTextBox_DrawTextLines_0043bc80:
        0043bf14 3b bd 43 00     addr       switchD_0043bd16::caseD_20
        0043bf18 43 bd 43 00     addr       switchD_0043bd16::caseD_9
                             switchD_0043bd16::switchdataD_0043bf1c
        0043bf1c 00              db         0h
        0043bf1d 04              db         4h
        0043bf1e 04              db         4h
        0043bf1f 04              db         4h
        0043bf20 04              db         4h
        0043bf21 04              db         4h
        0043bf22 04              db         4h
        0043bf23 04              db         4h
        0043bf24 01              db         1h
        0043bf25 04              db         4h
        0043bf26 04              db         4h
        0043bf27 04              db         4h
        0043bf28 04              db         4h
        0043bf29 04              db         4h
        0043bf2a 04              db         4h
        0043bf2b 04              db         4h
        0043bf2c 02              db         2h
        0043bf2d 04              db         4h
        0043bf2e 04              db         4h
        0043bf2f 04              db         4h
        0043bf30 04              db         4h
        0043bf31 04              db         4h
        0043bf32 04              db         4h
        0043bf33 04              db         4h
        0043bf34 03              db         3h
        0043bf35 90              ??         90h
        0043bf36 90              ??         90h
        0043bf37 90              ??         90h
        0043bf38 90              ??         90h
        0043bf39 90              ??         90h
        0043bf3a 90              ??         90h
        0043bf3b 90              ??         90h
        0043bf3c 90              ??         90h
        0043bf3d 90              ??         90h
        0043bf3e 90              ??         90h
        0043bf3f 90              ??         90h
*/

/* WARNING: Removing unreachable block (ram,0x0043bdc9) */
/* WARNING: Removing unreachable block (ram,0x0043bdf3) */
/* WARNING: Removing unreachable block (ram,0x0043bdf7) */
/* WARNING: Removing unreachable block (ram,0x0043bdfb) */
/* WARNING: Removing unreachable block (ram,0x0043be05) */
/* WARNING: Removing unreachable block (ram,0x0043be0c) */
/* WARNING: Removing unreachable block (ram,0x0043be23) */
/* WARNING: Removing unreachable block (ram,0x0043be31) */
/* WARNING: Removing unreachable block (ram,0x0043be39) */

bool __thiscall cTextBox_DrawTextLines_0043bc80(cTextBox *this,int param_2)

{
  char cVar1;
  int *piVar2;
  int *piVar3;
  int iVar4;
  RECT *pRVar5;
  int iVar6;
  int *unaff_EBP;
  DDSURFACEDESC2 *pDVar7;
  int *piVar8;
  int *piVar9;
  int iVar10;
  undefined1 auStack_90 [4];
  int *piStack_8c;
  cTextBox *local_88;
  DDSURFACEDESC2 local_7c;
  
  pDVar7 = &local_7c;
  for (iVar6 = 0x1f; iVar6 != 0; iVar6 = iVar6 + -1) {
    pDVar7->dwSize = 0;
    pDVar7 = (DDSURFACEDESC2 *)&pDVar7->dwFlags;
  }
  local_7c.dwSize = 0x7c;
  local_88 = this;
  piVar2 = (int *)(**(code **)(**(int **)(this[1].pad_00 + 0x14) + 0x54))();
  piStack_8c = piVar2;
  piVar3 = (*this->pFont->vptr->cFont_GetSurface_005484e0)(this->pFont);
  iVar6 = 0;
  iVar10 = 0;
  piVar8 = piVar2;
  iVar4 = (**(code **)(*piVar2 + 100))(piVar2,0,&local_7c,0,0);
  if (iVar4 != 0) {
    return false;
  }
  switch(local_7c.ddpfPixelFormat) {
  case 8:
    break;
  case 0x10:
    break;
  case 0x18:
    break;
  case 0x20:
  }
  (**(code **)(*unaff_EBP + 100))(unaff_EBP,0,auStack_90,0);
  if (0 < this->unknown190) {
    do {
      piVar8 = (int *)0x0;
      piVar2 = (int *)(this->unknown18c + iVar6 * 4);
      cVar1 = **(char **)(this->unknown18c + iVar6 * 4);
      while (cVar1 != '\0') {
        piVar9 = piVar8;
        pRVar5 = (*this->pFont->vptr->cFont_GetCharRect_00435e60)
                           (this->pFont,(int)*(char *)(*piVar2 + (int)piVar8));
        if (0 < pRVar5->right - pRVar5->left) {
          piVar8 = piVar9;
        }
        iVar4 = iVar10;
        (*this->pFont->vptr->cFont_GetCharWidth_00435e30)
                  (this->pFont,*(char *)(*(int *)(this->unknown18c + iVar10 * 4) + (int)piVar8));
        piVar8 = (int *)((int)piVar8 + 1);
        piVar2 = (int *)(this->unknown18c + iVar10 * 4);
        cVar1 = *(char *)((int)piVar8 + *(int *)(this->unknown18c + iVar10 * 4));
        iVar6 = iVar10;
        iVar10 = iVar4;
      }
      iVar6 = iVar6 + 1;
      piVar2 = piVar3;
      iVar10 = iVar6;
    } while (iVar6 < this->unknown190);
  }
  (**(code **)(*piVar2 + 0x80))(piVar2,0);
  (**(code **)(*piVar8 + 0x80))(piVar8,0);
  return true;
}

