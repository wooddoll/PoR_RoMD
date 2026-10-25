
                               BOOL __thiscall cTextLayout_WordWrap_00444890(cTextLayou
                               assume FS_OFFSET = 0xffdff000
             BOOL              EAX:4          <RETURN>
             cTextLayout *     ECX:4          this
             int               EBX:4          nCurrentLineWidth                       XREF[1]:     004448aa(W)  
             int               EAX:4          nBoxMaxWidth                            XREF[1]:     004448b4(W)  
             char *            EAX:4          szFullText                              XREF[1]:     004448e9(W)  
             int               EDI:4          nLastSpaceIdx                           XREF[1]:     004449fc(W)  
             int               EAX:4          charH                                   XREF[1]:     00444a0e(W)  
             int               EAX:4          charW_1                                 XREF[1]:     00444a2d(W)  
             int               EAX:4          charW_2                                 XREF[1]:     00444a58(W)  
             undefined4        EAX:4          charW_3                                 XREF[1]:     00444a8f(W)  
             undefined4        Stack[-0x8]:4  local_8                                 XREF[1]:     004448a6(W)  
             undefined4        Stack[-0xc]:4  nMaxLineHeight
             undefined4        Stack[-0x14]:4 local_14                                XREF[1]:     004448ac(W)  
             undefined4        Stack[-0x18]:4 local_18                                XREF[1]:     004448a2(W)  
             int               Stack[-0x1c]:4 nCharIdx                                XREF[1]:     0044489e(W)  
                             cTextLayout_WordWrap_00444890                   XREF[1]:     00444642(c)  
        00444890 83 ec 28        SUB        ESP,0x28
        00444893 53              PUSH       EBX
        00444894 55              PUSH       EBP
        00444895 8b e9           MOV        EBP,this
        00444897 56              PUSH       ESI
        00444898 33 f6           XOR        ESI,ESI
        0044489a 57              PUSH       EDI
        0044489b 8b 45 00        MOV        EAX,dword ptr [EBP]
        0044489e 89 74 24 1c     MOV        dword ptr [ESP + nCharIdx],ESI
        004448a2 89 74 24 20     MOV        dword ptr [ESP + local_18],ESI
        004448a6 89 74 24 30     MOV        dword ptr [ESP + local_8],ESI
        004448aa 33 db           XOR        nCurrentLineWidth,nCurrentLineWidth
        004448ac c7 44 24        MOV        dword ptr [ESP + local_14],0x1
                 24 01 00 
                 00 00
        004448b4 ff 50 48        CALL       dword ptr [nBoxMaxWidth + 0x48]
        004448b7 89 44 24 34     MOV        dword ptr [ESP + 0x34],nBoxMaxWidth
        004448bb 8b 85 28        MOV        nBoxMaxWidth,dword ptr [EBP + 0x128]
                 01 00 00
        004448c1 89 b5 48        MOV        dword ptr [EBP + 0x148],ESI
                 01 00 00
        004448c7 89 b5 44        MOV        dword ptr [EBP + 0x144],ESI
                 01 00 00
        004448cd 8a 08           MOV        this+0x3,byte ptr [nBoxMaxWidth]
        004448cf 33 ff           XOR        EDI,EDI
        004448d1 84 c9           TEST       this+0x3,this+0x3
        004448d3 89 7c 24 28     MOV        dword ptr [ESP + 0x28],EDI
        004448d7 0f 84 b6        JZ         LAB_00444b93
                 02 00 00
        004448dd c7 44 24        MOV        dword ptr [ESP + 0x14],0x4
                 14 04 00 
                 00 00
        004448e5 89 7c 24 18     MOV        dword ptr [ESP + 0x18],EDI
                             LAB_004448e9                                    XREF[1]:     00444b83(j)  
        004448e9 8a 04 30        MOV        szFullText,byte ptr [szFullText + ESI*0x1]
        004448ec 3c 0a           CMP        szFullText+0x3,0xa
        004448ee 0f 85 d4        JNZ        LAB_004449c8
                 00 00 00
        004448f4 8b 4c 24 14     MOV        this,dword ptr [ESP + 0x14]
        004448f8 51              PUSH       this
        004448f9 e8 ff 5f        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        004448fe 8b 4c 24 1c     MOV        this,dword ptr [ESP + 0x1c]
        00444902 8b b5 3c        MOV        ESI,dword ptr [EBP + 0x13c]
                 01 00 00
        00444908 8b d1           MOV        EDX,this
        0044490a 8b f8           MOV        EDI,szFullText
        0044490c c1 e9 02        SHR        this,0x2
        0044490f f3 a5           MOVSD.REP  ES:EDI,ESI
        00444911 8b ca           MOV        this,EDX
        00444913 89 44 24 14     MOV        dword ptr [ESP + 0x14],szFullText
        00444917 83 e1 03        AND        this,0x3
        0044491a f3 a4           MOVSB.REP  ES:EDI,ESI
        0044491c 8b 4c 24 20     MOV        this,dword ptr [ESP + 0x20]
        00444920 8b 74 24 18     MOV        ESI,dword ptr [ESP + 0x18]
        00444924 49              DEC        this
        00444925 89 4c 06 fc     MOV        dword ptr [ESI + szFullText*0x1 + -0x4],this
        00444929 8b 95 3c        MOV        EDX,dword ptr [EBP + 0x13c]
                 01 00 00
        0044492f 52              PUSH       EDX
        00444930 e8 bd 5f        CALL       delete_005ea8f2                                  void delete_005ea8f2(void * ptr)
                 1a 00
        00444935 8b 44 24 18     MOV        szFullText,dword ptr [ESP + 0x18]
        00444939 56              PUSH       ESI
        0044493a 89 85 3c        MOV        dword ptr [EBP + 0x13c],szFullText
                 01 00 00
        00444940 e8 b8 5f        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444945 8b 4c 24 24     MOV        this,dword ptr [ESP + 0x24]
        00444949 8b b5 40        MOV        ESI,dword ptr [EBP + 0x140]
                 01 00 00
        0044494f 8b d1           MOV        EDX,this
        00444951 8b f8           MOV        EDI,szFullText
        00444953 c1 e9 02        SHR        this,0x2
        00444956 f3 a5           MOVSD.REP  ES:EDI,ESI
        00444958 8b ca           MOV        this,EDX
        0044495a 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],szFullText
        0044495e 83 e1 03        AND        this,0x3
        00444961 f3 a4           MOVSB.REP  ES:EDI,ESI
        00444963 8b 4c 24 20     MOV        this,dword ptr [ESP + 0x20]
        00444967 89 5c 01 fc     MOV        dword ptr [this->vptr + szFullText*0x1 + -0x4]
        0044496b 8b 95 40        MOV        EDX,dword ptr [EBP + 0x140]
                 01 00 00
        00444971 52              PUSH       EDX
        00444972 e8 7b 5f        CALL       delete_005ea8f2                                  void delete_005ea8f2(void * ptr)
                 1a 00
        00444977 8b 44 24 20     MOV        szFullText,dword ptr [ESP + 0x20]
        0044497b 83 c4 10        ADD        ESP,0x10
        0044497e 89 85 40        MOV        dword ptr [EBP + 0x140],szFullText
                 01 00 00
        00444984 8b 44 24 20     MOV        szFullText,dword ptr [ESP + 0x20]
        00444988 3b d8           CMP        nCurrentLineWidth,szFullText
        0044498a 7e 04           JLE        LAB_00444990
        0044498c 89 5c 24 20     MOV        dword ptr [ESP + 0x20],nCurrentLineWidth
                             LAB_00444990                                    XREF[1]:     0044498a(j)  
        00444990 8b 54 24 14     MOV        EDX,dword ptr [ESP + 0x14]
        00444994 8b 4c 24 18     MOV        this,dword ptr [ESP + 0x18]
        00444998 8b 74 24 24     MOV        ESI,dword ptr [ESP + 0x24]
        0044499c 8b 7c 24 28     MOV        EDI,dword ptr [ESP + 0x28]
        004449a0 b8 04 00        MOV        szFullText,0x4
                 00 00
        004449a5 33 db           XOR        nCurrentLineWidth,nCurrentLineWidth
        004449a7 03 d0           ADD        EDX,szFullText
        004449a9 03 c8           ADD        this,szFullText
        004449ab 8b 44 24 1c     MOV        szFullText,dword ptr [ESP + 0x1c]
        004449af 46              INC        ESI
        004449b0 40              INC        szFullText
        004449b1 89 74 24 24     MOV        dword ptr [ESP + 0x24],ESI
        004449b5 89 54 24 14     MOV        dword ptr [ESP + 0x14],EDX
        004449b9 89 4c 24 18     MOV        dword ptr [ESP + 0x18],this
        004449bd 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],szFullText
        004449c1 8b f0           MOV        ESI,szFullText
        004449c3 e9 b1 01        JMP        LAB_00444b79
                 00 00
                             LAB_004449c8                                    XREF[1]:     004448ee(j)  
        004449c8 0f be c8        MOVSX      this,szFullText+0x3
        004449cb 51              PUSH       this
        004449cc e8 af 0a        CALL       isspace_00445480                                 undefined isspace_00445480(void)
                 00 00
        004449d1 83 c4 04        ADD        ESP,0x4
        004449d4 85 c0           TEST       szFullText,szFullText
        004449d6 75 24           JNZ        LAB_004449fc
        004449d8 85 f6           TEST       ESI,ESI
        004449da 74 1a           JZ         LAB_004449f6
        004449dc 7e 1e           JLE        LAB_004449fc
        004449de 8b 95 28        MOV        EDX,dword ptr [EBP + 0x128]
                 01 00 00
        004449e4 0f be 44        MOVSX      szFullText,byte ptr [EDX + ESI*0x1 + -0x1]
                 32 ff
        004449e9 50              PUSH       szFullText
        004449ea e8 91 0a        CALL       isspace_00445480                                 undefined isspace_00445480(void)
                 00 00
        004449ef 83 c4 04        ADD        ESP,0x4
        004449f2 85 c0           TEST       szFullText,szFullText
        004449f4 74 06           JZ         LAB_004449fc
                             LAB_004449f6                                    XREF[1]:     004449da(j)  
        004449f6 89 74 24 28     MOV        dword ptr [ESP + 0x28],ESI
        004449fa 8b fe           MOV        EDI,ESI
                             LAB_004449fc                                    XREF[3]:     004449d6(j), 004449dc(j), 
                                                                                          004449f4(j)  
        004449fc 8b 85 28        MOV        szFullText,dword ptr [EBP + 0x128]
                 01 00 00
        00444a02 8b 8d 24        MOV        this,dword ptr [EBP + 0x124]
                 01 00 00
        00444a08 8a 04 30        MOV        szFullText+0x3,byte ptr [szFullText + ESI*0x1]
        00444a0b 8b 11           MOV        EDX,dword ptr [this->vptr]
        00444a0d 50              PUSH       szFullText
                             [★★★ 패치 포인트 A: GetCharHeight (+0x1c)]
        00444a0e ff 52 1c        CALL       dword ptr [EDX + 0x1c]
        00444a11 3b 44 24 30     CMP        charH,dword ptr [ESP + 0x30]
        00444a15 7e 04           JLE        LAB_00444a1b
        00444a17 89 44 24 30     MOV        dword ptr [ESP + 0x30],charH
                             LAB_00444a1b                                    XREF[1]:     00444a15(j)  
        00444a1b 8b 85 28        MOV        charH,dword ptr [EBP + 0x128]
                 01 00 00
        00444a21 8b 8d 24        MOV        this,dword ptr [EBP + 0x124]
                 01 00 00
        00444a27 8a 04 30        MOV        charH+0x3,byte ptr [charH + ESI*0x1]
        00444a2a 8b 11           MOV        EDX,dword ptr [this->vptr]
        00444a2c 50              PUSH       charH
                             [★★★ 패치 포인트 B: GetCharWidth (+0x20)]
        00444a2d ff 52 20        CALL       dword ptr [EDX + 0x20]
        00444a30 03 d8           ADD        nCurrentLineWidth,charW_1
        00444a32 8b 44 24 34     MOV        charW_1,dword ptr [ESP + 0x34]
        00444a36 3b d8           CMP        nCurrentLineWidth,charW_1
        00444a38 0f 8e 36        JLE        LAB_00444b74
                 01 00 00
        00444a3e 3b f7           CMP        ESI,nLastSpaceIdx
        00444a40 89 74 24 2c     MOV        dword ptr [ESP + 0x2c],ESI
        00444a44 7c 20           JL         LAB_00444a66
                             LAB_00444a46                                    XREF[1]:     00444a60(j)  
        00444a46 8b 85 28        MOV        charW_1,dword ptr [EBP + 0x128]
                 01 00 00
        00444a4c 8b 8d 24        MOV        this,dword ptr [EBP + 0x124]
                 01 00 00
        00444a52 8a 04 30        MOV        charW_1+0x3,byte ptr [charW_1 + ESI*0x1]
        00444a55 8b 11           MOV        EDX,dword ptr [this->vptr]
        00444a57 50              PUSH       charW_1
                             [★★★ 패치 포인트 C: GetCharWidth (+0x20)]
        00444a58 ff 52 20        CALL       dword ptr [EDX + 0x20]
        00444a5b 2b d8           SUB        nCurrentLineWidth,charW_2
        00444a5d 4e              DEC        ESI
        00444a5e 3b f7           CMP        ESI,nLastSpaceIdx
        00444a60 7d e4           JGE        LAB_00444a46
        00444a62 89 74 24 2c     MOV        dword ptr [ESP + 0x2c],ESI
                             LAB_00444a66                                    XREF[1]:     00444a44(j)  
        00444a66 8b 8d 28        MOV        this,dword ptr [EBP + 0x128]
                 01 00 00
        00444a6c 0f be 14 31     MOVSX      EDX,byte ptr [this->vptr + ESI*0x1]
        00444a70 52              PUSH       EDX
        00444a71 e8 0a 0a        CALL       isspace_00445480                                 undefined isspace_00445480(void)
                 00 00
        00444a76 83 c4 04        ADD        ESP,0x4
        00444a79 85 c0           TEST       charW_2,charW_2
        00444a7b 74 33           JZ         LAB_00444ab0
                             LAB_00444a7d                                    XREF[1]:     00444aaa(j)  
        00444a7d 8b 95 28        MOV        EDX,dword ptr [EBP + 0x128]
                 01 00 00
        00444a83 8b 8d 24        MOV        this,dword ptr [EBP + 0x124]
                 01 00 00
        00444a89 8a 14 32        MOV        DL,byte ptr [EDX + ESI*0x1]
        00444a8c 8b 01           MOV        charW_2,dword ptr [this->vptr]
        00444a8e 52              PUSH       EDX
                             [★★★ 패치 포인트 D: GetCharWidth (+0x20)]
        00444a8f ff 50 20        CALL       dword ptr [charW_3 + 0x20]
        00444a92 2b d8           SUB        nCurrentLineWidth,charW_3
        00444a94 8b 85 28        MOV        charW_3,dword ptr [EBP + 0x128]
                 01 00 00
        00444a9a 4e              DEC        ESI
        00444a9b 0f be 0c 30     MOVSX      this,byte ptr [charW_3 + ESI*0x1]
        00444a9f 51              PUSH       this
        00444aa0 e8 db 09        CALL       isspace_00445480                                 undefined isspace_00445480(void)
                 00 00
        00444aa5 83 c4 04        ADD        ESP,0x4
        00444aa8 85 c0           TEST       charW_3,charW_3
        00444aaa 75 d1           JNZ        LAB_00444a7d
        00444aac 89 74 24 2c     MOV        dword ptr [ESP + 0x2c],ESI
                             LAB_00444ab0                                    XREF[1]:     00444a7b(j)  
        00444ab0 3b 5c 24 20     CMP        nCurrentLineWidth,dword ptr [ESP + 0x20]
        00444ab4 7e 04           JLE        LAB_00444aba
        00444ab6 89 5c 24 20     MOV        dword ptr [ESP + 0x20],nCurrentLineWidth
                             LAB_00444aba                                    XREF[1]:     00444ab4(j)  
        00444aba 8b 54 24 14     MOV        EDX,dword ptr [ESP + 0x14]
        00444abe 89 7c 24 1c     MOV        dword ptr [ESP + 0x1c],nLastSpaceIdx
        00444ac2 52              PUSH       EDX
        00444ac3 e8 35 5e        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444ac8 8b 4c 24 1c     MOV        this,dword ptr [ESP + 0x1c]
        00444acc 8b b5 3c        MOV        ESI,dword ptr [EBP + 0x13c]
                 01 00 00
        00444ad2 8b d1           MOV        EDX,this
        00444ad4 8b f8           MOV        nLastSpaceIdx,charW_3
        00444ad6 c1 e9 02        SHR        this,0x2
        00444ad9 f3 a5           MOVSD.REP  ES:nLastSpaceIdx,ESI
        00444adb 8b ca           MOV        this,EDX
        00444add 89 44 24 14     MOV        dword ptr [ESP + 0x14],charW_3
        00444ae1 83 e1 03        AND        this,0x3
        00444ae4 f3 a4           MOVSB.REP  ES:nLastSpaceIdx,ESI
        00444ae6 8b 74 24 18     MOV        ESI,dword ptr [ESP + 0x18]
        00444aea 8b 4c 24 30     MOV        this,dword ptr [ESP + 0x30]
        00444aee 89 4c 06 fc     MOV        dword ptr [ESI + charW_3*0x1 + -0x4],this
        00444af2 8b 95 3c        MOV        EDX,dword ptr [EBP + 0x13c]
                 01 00 00
        00444af8 52              PUSH       EDX
        00444af9 e8 f4 5d        CALL       delete_005ea8f2                                  void delete_005ea8f2(void * ptr)
                 1a 00
        00444afe 8b 44 24 18     MOV        charW_3,dword ptr [ESP + 0x18]
        00444b02 56              PUSH       ESI
        00444b03 89 85 3c        MOV        dword ptr [EBP + 0x13c],charW_3
                 01 00 00
        00444b09 e8 ef 5d        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444b0e 8b 4c 24 24     MOV        this,dword ptr [ESP + 0x24]
        00444b12 8b b5 40        MOV        ESI,dword ptr [EBP + 0x140]
                 01 00 00
        00444b18 8b d1           MOV        EDX,this
        00444b1a 8b f8           MOV        nLastSpaceIdx,charW_3
        00444b1c c1 e9 02        SHR        this,0x2
        00444b1f f3 a5           MOVSD.REP  ES:nLastSpaceIdx,ESI
        00444b21 8b ca           MOV        this,EDX
        00444b23 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],charW_3
        00444b27 83 e1 03        AND        this,0x3
        00444b2a f3 a4           MOVSB.REP  ES:nLastSpaceIdx,ESI
        00444b2c 8b 74 24 20     MOV        ESI,dword ptr [ESP + 0x20]
        00444b30 89 5c 06 fc     MOV        dword ptr [ESI + charW_3*0x1 + -0x4],nCurrentL
        00444b34 8b 85 40        MOV        charW_3,dword ptr [EBP + 0x140]
                 01 00 00
        00444b3a 50              PUSH       charW_3
        00444b3b e8 b2 5d        CALL       delete_005ea8f2                                  void delete_005ea8f2(void * ptr)
                 1a 00
        00444b40 8b 54 24 34     MOV        EDX,dword ptr [ESP + 0x34]
        00444b44 8b 44 24 28     MOV        charW_3,dword ptr [ESP + 0x28]
        00444b48 8b 4c 24 20     MOV        this,dword ptr [ESP + 0x20]
        00444b4c 8b 7c 24 38     MOV        nLastSpaceIdx,dword ptr [ESP + 0x38]
        00444b50 83 c4 10        ADD        ESP,0x10
        00444b53 33 db           XOR        nCurrentLineWidth,nCurrentLineWidth
        00444b55 42              INC        EDX
        00444b56 83 c6 04        ADD        ESI,0x4
        00444b59 83 c0 04        ADD        charW_3,0x4
        00444b5c 89 74 24 14     MOV        dword ptr [ESP + 0x14],ESI
        00444b60 8b 74 24 1c     MOV        ESI,dword ptr [ESP + 0x1c]
        00444b64 89 8d 40        MOV        dword ptr [EBP + 0x140],this
                 01 00 00
        00444b6a 89 54 24 24     MOV        dword ptr [ESP + 0x24],EDX
        00444b6e 89 44 24 18     MOV        dword ptr [ESP + 0x18],charW_3
        00444b72 eb 05           JMP        LAB_00444b79
                             [★★★ 패치 포인트 E:다음 글자로 이동]
                             원래 코드: 1바이트 전진 (nCharIdx++)
                             한글인 경우: 2바이트 전진 (nCharIdx += 2) 이어야 함!
                             LAB_00444b74                                    XREF[1]:     00444a38(j)  
        00444b74 46              INC        ESI
        00444b75 89 74 24 1c     MOV        dword ptr [ESP + 0x1c],ESI
                             LAB_00444b79                                    XREF[2]:     004449c3(j), 00444b72(j)  
        00444b79 8b 85 28        MOV        charW_3,dword ptr [EBP + 0x128]
                 01 00 00
        00444b7f 80 3c 30 00     CMP        byte ptr [charW_3 + ESI*0x1],0x0
        00444b83 0f 85 60        JNZ        LAB_004448e9
                 fd ff ff
        00444b89 3b 5c 24 20     CMP        nCurrentLineWidth,dword ptr [ESP + 0x20]
        00444b8d 7e 04           JLE        LAB_00444b93
        00444b8f 89 5c 24 20     MOV        dword ptr [ESP + 0x20],nCurrentLineWidth
                             LAB_00444b93                                    XREF[2]:     004448d7(j), 00444b8d(j)  
        00444b93 8b 54 24 24     MOV        EDX,dword ptr [ESP + 0x24]
        00444b97 8d 34 95        LEA        ESI,[EDX*0x4 + 0x0]
                 00 00 00 00
        00444b9e 56              PUSH       ESI
        00444b9f 89 74 24 30     MOV        dword ptr [ESP + 0x30],ESI
        00444ba3 e8 55 5d        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444ba8 8d 4e fc        LEA        this,[ESI + -0x4]
        00444bab 8b b5 3c        MOV        ESI,dword ptr [EBP + 0x13c]
                 01 00 00
        00444bb1 8b d0           MOV        EDX,charW_3
        00444bb3 8b c1           MOV        charW_3,this
        00444bb5 8b fa           MOV        nLastSpaceIdx,EDX
        00444bb7 89 54 24 14     MOV        dword ptr [ESP + 0x14],EDX
        00444bbb c1 e9 02        SHR        this,0x2
        00444bbe f3 a5           MOVSD.REP  ES:nLastSpaceIdx,ESI
        00444bc0 8b c8           MOV        this,charW_3
        00444bc2 33 c0           XOR        charW_3,charW_3
        00444bc4 83 e1 03        AND        this,0x3
        00444bc7 f3 a4           MOVSB.REP  ES:nLastSpaceIdx,ESI
        00444bc9 8b bd 28        MOV        nLastSpaceIdx,dword ptr [EBP + 0x128]
                 01 00 00
        00444bcf 83 c9 ff        OR         this,0xffffffff
        00444bd2 f2 ae           SCASB.RE   ES:nLastSpaceIdx
        00444bd4 8b 74 24 30     MOV        ESI,dword ptr [ESP + 0x30]
        00444bd8 f7 d1           NOT        this
        00444bda 49              DEC        this
        00444bdb 89 4c 16 fc     MOV        dword ptr [ESI + EDX*0x1 + -0x4],this
        00444bdf 8b 8d 3c        MOV        this,dword ptr [EBP + 0x13c]
                 01 00 00
        00444be5 51              PUSH       this
        00444be6 e8 07 5d        CALL       delete_005ea8f2                                  void delete_005ea8f2(void * ptr)
                 1a 00
        00444beb 8b 44 24 28     MOV        charW_3,dword ptr [ESP + 0x28]
        00444bef 8b 54 24 18     MOV        EDX,dword ptr [ESP + 0x18]
        00444bf3 8b 4c 24 38     MOV        this,dword ptr [ESP + 0x38]
        00444bf7 89 85 44        MOV        dword ptr [EBP + 0x144],charW_3
                 01 00 00
        00444bfd 8b 44 24 2c     MOV        charW_3,dword ptr [ESP + 0x2c]
        00444c01 89 95 3c        MOV        dword ptr [EBP + 0x13c],EDX
                 01 00 00
        00444c07 8b d0           MOV        EDX,charW_3
        00444c09 56              PUSH       ESI
        00444c0a 0f af d1        IMUL       EDX,this
        00444c0d 89 8d 4c        MOV        dword ptr [EBP + 0x14c],this
                 01 00 00
        00444c13 89 95 48        MOV        dword ptr [EBP + 0x148],EDX
                 01 00 00
        00444c19 89 85 38        MOV        dword ptr [EBP + 0x138],charW_3
                 01 00 00
        00444c1f e8 d9 5c        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444c24 8d 4e fc        LEA        this,[ESI + -0x4]
        00444c27 8b b5 40        MOV        ESI,dword ptr [EBP + 0x140]
                 01 00 00
        00444c2d 8b d1           MOV        EDX,this
        00444c2f 8b f8           MOV        nLastSpaceIdx,charW_3
        00444c31 c1 e9 02        SHR        this,0x2
        00444c34 f3 a5           MOVSD.REP  ES:nLastSpaceIdx,ESI
        00444c36 8b ca           MOV        this,EDX
        00444c38 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],charW_3
        00444c3c 83 e1 03        AND        this,0x3
        00444c3f f3 a4           MOVSB.REP  ES:nLastSpaceIdx,ESI
        00444c41 8b 4c 24 38     MOV        this,dword ptr [ESP + 0x38]
        00444c45 89 5c 01 fc     MOV        dword ptr [this->vptr + charW_3*0x1 + -0x4],nC
        00444c49 8b 95 40        MOV        EDX,dword ptr [EBP + 0x140]
                 01 00 00
        00444c4f 52              PUSH       EDX
        00444c50 e8 9d 5c        CALL       delete_005ea8f2                                  void delete_005ea8f2(void * ptr)
                 1a 00
        00444c55 8b 44 24 20     MOV        charW_3,dword ptr [ESP + 0x20]
        00444c59 83 c4 10        ADD        ESP,0x10
        00444c5c 89 85 40        MOV        dword ptr [EBP + 0x140],charW_3
                 01 00 00
        00444c62 8b 85 30        MOV        charW_3,dword ptr [EBP + 0x130]
                 01 00 00
        00444c68 83 f8 01        CMP        charW_3,0x1
        00444c6b 75 0e           JNZ        LAB_00444c7b
        00444c6d 8b 55 00        MOV        EDX,dword ptr [EBP]
        00444c70 8b cd           MOV        this,EBP
        00444c72 ff 52 48        CALL       dword ptr [EDX + 0x48]
        00444c75 89 85 44        MOV        dword ptr [EBP + 0x144],charW_3
                 01 00 00
                             LAB_00444c7b                                    XREF[1]:     00444c6b(j)  
        00444c7b 8b 85 44        MOV        charW_3,dword ptr [EBP + 0x144]
                 01 00 00
        00444c81 85 c0           TEST       charW_3,charW_3
        00444c83 7e 09           JLE        LAB_00444c8e
        00444c85 83 c0 02        ADD        charW_3,0x2
        00444c88 89 85 44        MOV        dword ptr [EBP + 0x144],charW_3
                 01 00 00
                             LAB_00444c8e                                    XREF[1]:     00444c83(j)  
        00444c8e 8b 85 48        MOV        charW_3,dword ptr [EBP + 0x148]
                 01 00 00
        00444c94 85 c0           TEST       charW_3,charW_3
        00444c96 7e 09           JLE        LAB_00444ca1
        00444c98 83 c0 02        ADD        charW_3,0x2
        00444c9b 89 85 48        MOV        dword ptr [EBP + 0x148],charW_3
                 01 00 00
                             LAB_00444ca1                                    XREF[1]:     00444c96(j)  
        00444ca1 5f              POP        nLastSpaceIdx
        00444ca2 5e              POP        ESI
        00444ca3 5d              POP        EBP
        00444ca4 b0 01           MOV        charW_3+0x3,0x1
        00444ca6 5b              POP        nCurrentLineWidth
        00444ca7 83 c4 28        ADD        ESP,0x28
        00444caa c3              RET
        00444cab 90 90 90        align      align(5)
                 90 90
