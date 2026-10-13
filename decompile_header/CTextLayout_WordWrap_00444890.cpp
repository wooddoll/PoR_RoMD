/*
                             **************************************************************
                             *                          FUNCTION                          *
                             **************************************************************
                               undefined4 __fastcall FUN_00444890(int * param_1)
                               assume FS_OFFSET = 0xffdff000
             undefined4        EAX:4          <RETURN>
             int *             ECX:4          param_1
             undefined4        Stack[-0x8]:4  local_8                                 XREF[1]:     004448a6(W)  
             undefined4        Stack[-0x14]:4 local_14                                XREF[1]:     004448ac(W)  
             undefined4        Stack[-0x18]:4 local_18                                XREF[1]:     004448a2(W)  
             undefined4        Stack[-0x1c]:4 local_1c                                XREF[1]:     0044489e(W)  
                             FUN_00444890                                    XREF[1]:     00444642(c)  
        00444890 83 ec 28        SUB        ESP,0x28
        00444893 53              PUSH       EBX
        00444894 55              PUSH       EBP
        00444895 8b e9           MOV        EBP,param_1
        00444897 56              PUSH       ESI
        00444898 33 f6           XOR        ESI,ESI
        0044489a 57              PUSH       EDI
        0044489b 8b 45 00        MOV        EAX,dword ptr [EBP]
        0044489e 89 74 24 1c     MOV        dword ptr [ESP + local_1c],ESI
        004448a2 89 74 24 20     MOV        dword ptr [ESP + local_18],ESI
        004448a6 89 74 24 30     MOV        dword ptr [ESP + local_8],ESI
        004448aa 33 db           XOR        EBX,EBX
        004448ac c7 44 24        MOV        dword ptr [ESP + local_14],0x1
                 24 01 00 
                 00 00
        004448b4 ff 50 48        CALL       dword ptr [EAX + 0x48]
        004448b7 89 44 24 34     MOV        dword ptr [ESP + 0x34],EAX
        004448bb 8b 85 28        MOV        EAX,dword ptr [EBP + 0x128]
                 01 00 00
        004448c1 89 b5 48        MOV        dword ptr [EBP + 0x148],ESI
                 01 00 00
        004448c7 89 b5 44        MOV        dword ptr [EBP + 0x144],ESI
                 01 00 00
        004448cd 8a 08           MOV        param_1+0x3,byte ptr [EAX]
        004448cf 33 ff           XOR        EDI,EDI
        004448d1 84 c9           TEST       param_1+0x3,param_1+0x3
        004448d3 89 7c 24 28     MOV        dword ptr [ESP + 0x28],EDI
        004448d7 0f 84 b6        JZ         LAB_00444b93
                 02 00 00
        004448dd c7 44 24        MOV        dword ptr [ESP + 0x14],0x4
                 14 04 00 
                 00 00
        004448e5 89 7c 24 18     MOV        dword ptr [ESP + 0x18],EDI
                             LAB_004448e9                                    XREF[1]:     00444b83(j)  
        004448e9 8a 04 30        MOV        AL,byte ptr [EAX + ESI*0x1]
        004448ec 3c 0a           CMP        AL,0xa
        004448ee 0f 85 d4        JNZ        LAB_004449c8
                 00 00 00
        004448f4 8b 4c 24 14     MOV        param_1,dword ptr [ESP + 0x14]
        004448f8 51              PUSH       param_1
        004448f9 e8 ff 5f        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        004448fe 8b 4c 24 1c     MOV        param_1,dword ptr [ESP + 0x1c]
        00444902 8b b5 3c        MOV        ESI,dword ptr [EBP + 0x13c]
                 01 00 00
        00444908 8b d1           MOV        EDX,param_1
        0044490a 8b f8           MOV        EDI,EAX
        0044490c c1 e9 02        SHR        param_1,0x2
        0044490f f3 a5           MOVSD.REP  ES:EDI,ESI
        00444911 8b ca           MOV        param_1,EDX
        00444913 89 44 24 14     MOV        dword ptr [ESP + 0x14],EAX
        00444917 83 e1 03        AND        param_1,0x3
        0044491a f3 a4           MOVSB.REP  ES:EDI,ESI
        0044491c 8b 4c 24 20     MOV        param_1,dword ptr [ESP + 0x20]
        00444920 8b 74 24 18     MOV        ESI,dword ptr [ESP + 0x18]
        00444924 49              DEC        param_1
        00444925 89 4c 06 fc     MOV        dword ptr [ESI + EAX*0x1 + -0x4],param_1
        00444929 8b 95 3c        MOV        EDX,dword ptr [EBP + 0x13c]
                 01 00 00
        0044492f 52              PUSH       EDX
        00444930 e8 bd 5f        CALL       FUN_005ea8f2                                     undefined FUN_005ea8f2(undefined
                 1a 00
        00444935 8b 44 24 18     MOV        EAX,dword ptr [ESP + 0x18]
        00444939 56              PUSH       ESI
        0044493a 89 85 3c        MOV        dword ptr [EBP + 0x13c],EAX
                 01 00 00
        00444940 e8 b8 5f        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444945 8b 4c 24 24     MOV        param_1,dword ptr [ESP + 0x24]
        00444949 8b b5 40        MOV        ESI,dword ptr [EBP + 0x140]
                 01 00 00
        0044494f 8b d1           MOV        EDX,param_1
        00444951 8b f8           MOV        EDI,EAX
        00444953 c1 e9 02        SHR        param_1,0x2
        00444956 f3 a5           MOVSD.REP  ES:EDI,ESI
        00444958 8b ca           MOV        param_1,EDX
        0044495a 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],EAX
        0044495e 83 e1 03        AND        param_1,0x3
        00444961 f3 a4           MOVSB.REP  ES:EDI,ESI
        00444963 8b 4c 24 20     MOV        param_1,dword ptr [ESP + 0x20]
        00444967 89 5c 01 fc     MOV        dword ptr [param_1 + EAX*0x1 + -0x4],EBX
        0044496b 8b 95 40        MOV        EDX,dword ptr [EBP + 0x140]
                 01 00 00
        00444971 52              PUSH       EDX
        00444972 e8 7b 5f        CALL       FUN_005ea8f2                                     undefined FUN_005ea8f2(undefined
                 1a 00
        00444977 8b 44 24 20     MOV        EAX,dword ptr [ESP + 0x20]
        0044497b 83 c4 10        ADD        ESP,0x10
        0044497e 89 85 40        MOV        dword ptr [EBP + 0x140],EAX
                 01 00 00
        00444984 8b 44 24 20     MOV        EAX,dword ptr [ESP + 0x20]
        00444988 3b d8           CMP        EBX,EAX
        0044498a 7e 04           JLE        LAB_00444990
        0044498c 89 5c 24 20     MOV        dword ptr [ESP + 0x20],EBX
                             LAB_00444990                                    XREF[1]:     0044498a(j)  
        00444990 8b 54 24 14     MOV        EDX,dword ptr [ESP + 0x14]
        00444994 8b 4c 24 18     MOV        param_1,dword ptr [ESP + 0x18]
        00444998 8b 74 24 24     MOV        ESI,dword ptr [ESP + 0x24]
        0044499c 8b 7c 24 28     MOV        EDI,dword ptr [ESP + 0x28]
        004449a0 b8 04 00        MOV        EAX,0x4
                 00 00
        004449a5 33 db           XOR        EBX,EBX
        004449a7 03 d0           ADD        EDX,EAX
        004449a9 03 c8           ADD        param_1,EAX
        004449ab 8b 44 24 1c     MOV        EAX,dword ptr [ESP + 0x1c]
        004449af 46              INC        ESI
        004449b0 40              INC        EAX
        004449b1 89 74 24 24     MOV        dword ptr [ESP + 0x24],ESI
        004449b5 89 54 24 14     MOV        dword ptr [ESP + 0x14],EDX
        004449b9 89 4c 24 18     MOV        dword ptr [ESP + 0x18],param_1
        004449bd 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],EAX
        004449c1 8b f0           MOV        ESI,EAX
        004449c3 e9 b1 01        JMP        LAB_00444b79
                 00 00
                             LAB_004449c8                                    XREF[1]:     004448ee(j)  
        004449c8 0f be c8        MOVSX      param_1,AL
        004449cb 51              PUSH       param_1
        004449cc e8 af 0a        CALL       FUN_00445480                                     undefined4 FUN_00445480(int para
                 00 00
        004449d1 83 c4 04        ADD        ESP,0x4
        004449d4 85 c0           TEST       EAX,EAX
        004449d6 75 24           JNZ        LAB_004449fc
        004449d8 85 f6           TEST       ESI,ESI
        004449da 74 1a           JZ         LAB_004449f6
        004449dc 7e 1e           JLE        LAB_004449fc
        004449de 8b 95 28        MOV        EDX,dword ptr [EBP + 0x128]
                 01 00 00
        004449e4 0f be 44        MOVSX      EAX,byte ptr [EDX + ESI*0x1 + -0x1]
                 32 ff
        004449e9 50              PUSH       EAX
        004449ea e8 91 0a        CALL       FUN_00445480                                     undefined4 FUN_00445480(int para
                 00 00
        004449ef 83 c4 04        ADD        ESP,0x4
        004449f2 85 c0           TEST       EAX,EAX
        004449f4 74 06           JZ         LAB_004449fc
                             LAB_004449f6                                    XREF[1]:     004449da(j)  
        004449f6 89 74 24 28     MOV        dword ptr [ESP + 0x28],ESI
        004449fa 8b fe           MOV        EDI,ESI
                             LAB_004449fc                                    XREF[3]:     004449d6(j), 004449dc(j), 
                                                                                          004449f4(j)  
        004449fc 8b 85 28        MOV        EAX,dword ptr [EBP + 0x128]
                 01 00 00
        00444a02 8b 8d 24        MOV        param_1,dword ptr [EBP + 0x124]
                 01 00 00
        00444a08 8a 04 30        MOV        AL,byte ptr [EAX + ESI*0x1]
        00444a0b 8b 11           MOV        EDX,dword ptr [param_1]
        00444a0d 50              PUSH       EAX
        00444a0e ff 52 1c        CALL       dword ptr [EDX + 0x1c]
        00444a11 3b 44 24 30     CMP        EAX,dword ptr [ESP + 0x30]
        00444a15 7e 04           JLE        LAB_00444a1b
        00444a17 89 44 24 30     MOV        dword ptr [ESP + 0x30],EAX
                             LAB_00444a1b                                    XREF[1]:     00444a15(j)  
        00444a1b 8b 85 28        MOV        EAX,dword ptr [EBP + 0x128]
                 01 00 00
        00444a21 8b 8d 24        MOV        param_1,dword ptr [EBP + 0x124]
                 01 00 00
        00444a27 8a 04 30        MOV        AL,byte ptr [EAX + ESI*0x1]
        00444a2a 8b 11           MOV        EDX,dword ptr [param_1]
        00444a2c 50              PUSH       EAX
        00444a2d ff 52 20        CALL       dword ptr [EDX + 0x20]
        00444a30 03 d8           ADD        EBX,EAX
        00444a32 8b 44 24 34     MOV        EAX,dword ptr [ESP + 0x34]
        00444a36 3b d8           CMP        EBX,EAX
        00444a38 0f 8e 36        JLE        LAB_00444b74
                 01 00 00
        00444a3e 3b f7           CMP        ESI,EDI
        00444a40 89 74 24 2c     MOV        dword ptr [ESP + 0x2c],ESI
        00444a44 7c 20           JL         LAB_00444a66
                             LAB_00444a46                                    XREF[1]:     00444a60(j)  
        00444a46 8b 85 28        MOV        EAX,dword ptr [EBP + 0x128]
                 01 00 00
        00444a4c 8b 8d 24        MOV        param_1,dword ptr [EBP + 0x124]
                 01 00 00
        00444a52 8a 04 30        MOV        AL,byte ptr [EAX + ESI*0x1]
        00444a55 8b 11           MOV        EDX,dword ptr [param_1]
        00444a57 50              PUSH       EAX
        00444a58 ff 52 20        CALL       dword ptr [EDX + 0x20]
        00444a5b 2b d8           SUB        EBX,EAX
        00444a5d 4e              DEC        ESI
        00444a5e 3b f7           CMP        ESI,EDI
        00444a60 7d e4           JGE        LAB_00444a46
        00444a62 89 74 24 2c     MOV        dword ptr [ESP + 0x2c],ESI
                             LAB_00444a66                                    XREF[1]:     00444a44(j)  
        00444a66 8b 8d 28        MOV        param_1,dword ptr [EBP + 0x128]
                 01 00 00
        00444a6c 0f be 14 31     MOVSX      EDX,byte ptr [param_1 + ESI*0x1]
        00444a70 52              PUSH       EDX
        00444a71 e8 0a 0a        CALL       FUN_00445480                                     undefined4 FUN_00445480(int para
                 00 00
        00444a76 83 c4 04        ADD        ESP,0x4
        00444a79 85 c0           TEST       EAX,EAX
        00444a7b 74 33           JZ         LAB_00444ab0
                             LAB_00444a7d                                    XREF[1]:     00444aaa(j)  
        00444a7d 8b 95 28        MOV        EDX,dword ptr [EBP + 0x128]
                 01 00 00
        00444a83 8b 8d 24        MOV        param_1,dword ptr [EBP + 0x124]
                 01 00 00
        00444a89 8a 14 32        MOV        DL,byte ptr [EDX + ESI*0x1]
        00444a8c 8b 01           MOV        EAX,dword ptr [param_1]
        00444a8e 52              PUSH       EDX
        00444a8f ff 50 20        CALL       dword ptr [EAX + 0x20]
        00444a92 2b d8           SUB        EBX,EAX
        00444a94 8b 85 28        MOV        EAX,dword ptr [EBP + 0x128]
                 01 00 00
        00444a9a 4e              DEC        ESI
        00444a9b 0f be 0c 30     MOVSX      param_1,byte ptr [EAX + ESI*0x1]
        00444a9f 51              PUSH       param_1
        00444aa0 e8 db 09        CALL       FUN_00445480                                     undefined4 FUN_00445480(int para
                 00 00
        00444aa5 83 c4 04        ADD        ESP,0x4
        00444aa8 85 c0           TEST       EAX,EAX
        00444aaa 75 d1           JNZ        LAB_00444a7d
        00444aac 89 74 24 2c     MOV        dword ptr [ESP + 0x2c],ESI
                             LAB_00444ab0                                    XREF[1]:     00444a7b(j)  
        00444ab0 3b 5c 24 20     CMP        EBX,dword ptr [ESP + 0x20]
        00444ab4 7e 04           JLE        LAB_00444aba
        00444ab6 89 5c 24 20     MOV        dword ptr [ESP + 0x20],EBX
                             LAB_00444aba                                    XREF[1]:     00444ab4(j)  
        00444aba 8b 54 24 14     MOV        EDX,dword ptr [ESP + 0x14]
        00444abe 89 7c 24 1c     MOV        dword ptr [ESP + 0x1c],EDI
        00444ac2 52              PUSH       EDX
        00444ac3 e8 35 5e        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444ac8 8b 4c 24 1c     MOV        param_1,dword ptr [ESP + 0x1c]
        00444acc 8b b5 3c        MOV        ESI,dword ptr [EBP + 0x13c]
                 01 00 00
        00444ad2 8b d1           MOV        EDX,param_1
        00444ad4 8b f8           MOV        EDI,EAX
        00444ad6 c1 e9 02        SHR        param_1,0x2
        00444ad9 f3 a5           MOVSD.REP  ES:EDI,ESI
        00444adb 8b ca           MOV        param_1,EDX
        00444add 89 44 24 14     MOV        dword ptr [ESP + 0x14],EAX
        00444ae1 83 e1 03        AND        param_1,0x3
        00444ae4 f3 a4           MOVSB.REP  ES:EDI,ESI
        00444ae6 8b 74 24 18     MOV        ESI,dword ptr [ESP + 0x18]
        00444aea 8b 4c 24 30     MOV        param_1,dword ptr [ESP + 0x30]
        00444aee 89 4c 06 fc     MOV        dword ptr [ESI + EAX*0x1 + -0x4],param_1
        00444af2 8b 95 3c        MOV        EDX,dword ptr [EBP + 0x13c]
                 01 00 00
        00444af8 52              PUSH       EDX
        00444af9 e8 f4 5d        CALL       FUN_005ea8f2                                     undefined FUN_005ea8f2(undefined
                 1a 00
        00444afe 8b 44 24 18     MOV        EAX,dword ptr [ESP + 0x18]
        00444b02 56              PUSH       ESI
        00444b03 89 85 3c        MOV        dword ptr [EBP + 0x13c],EAX
                 01 00 00
        00444b09 e8 ef 5d        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444b0e 8b 4c 24 24     MOV        param_1,dword ptr [ESP + 0x24]
        00444b12 8b b5 40        MOV        ESI,dword ptr [EBP + 0x140]
                 01 00 00
        00444b18 8b d1           MOV        EDX,param_1
        00444b1a 8b f8           MOV        EDI,EAX
        00444b1c c1 e9 02        SHR        param_1,0x2
        00444b1f f3 a5           MOVSD.REP  ES:EDI,ESI
        00444b21 8b ca           MOV        param_1,EDX
        00444b23 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],EAX
        00444b27 83 e1 03        AND        param_1,0x3
        00444b2a f3 a4           MOVSB.REP  ES:EDI,ESI
        00444b2c 8b 74 24 20     MOV        ESI,dword ptr [ESP + 0x20]
        00444b30 89 5c 06 fc     MOV        dword ptr [ESI + EAX*0x1 + -0x4],EBX
        00444b34 8b 85 40        MOV        EAX,dword ptr [EBP + 0x140]
                 01 00 00
        00444b3a 50              PUSH       EAX
        00444b3b e8 b2 5d        CALL       FUN_005ea8f2                                     undefined FUN_005ea8f2(undefined
                 1a 00
        00444b40 8b 54 24 34     MOV        EDX,dword ptr [ESP + 0x34]
        00444b44 8b 44 24 28     MOV        EAX,dword ptr [ESP + 0x28]
        00444b48 8b 4c 24 20     MOV        param_1,dword ptr [ESP + 0x20]
        00444b4c 8b 7c 24 38     MOV        EDI,dword ptr [ESP + 0x38]
        00444b50 83 c4 10        ADD        ESP,0x10
        00444b53 33 db           XOR        EBX,EBX
        00444b55 42              INC        EDX
        00444b56 83 c6 04        ADD        ESI,0x4
        00444b59 83 c0 04        ADD        EAX,0x4
        00444b5c 89 74 24 14     MOV        dword ptr [ESP + 0x14],ESI
        00444b60 8b 74 24 1c     MOV        ESI,dword ptr [ESP + 0x1c]
        00444b64 89 8d 40        MOV        dword ptr [EBP + 0x140],param_1
                 01 00 00
        00444b6a 89 54 24 24     MOV        dword ptr [ESP + 0x24],EDX
        00444b6e 89 44 24 18     MOV        dword ptr [ESP + 0x18],EAX
        00444b72 eb 05           JMP        LAB_00444b79
                             LAB_00444b74                                    XREF[1]:     00444a38(j)  
        00444b74 46              INC        ESI
        00444b75 89 74 24 1c     MOV        dword ptr [ESP + 0x1c],ESI
                             LAB_00444b79                                    XREF[2]:     004449c3(j), 00444b72(j)  
        00444b79 8b 85 28        MOV        EAX,dword ptr [EBP + 0x128]
                 01 00 00
        00444b7f 80 3c 30 00     CMP        byte ptr [EAX + ESI*0x1],0x0
        00444b83 0f 85 60        JNZ        LAB_004448e9
                 fd ff ff
        00444b89 3b 5c 24 20     CMP        EBX,dword ptr [ESP + 0x20]
        00444b8d 7e 04           JLE        LAB_00444b93
        00444b8f 89 5c 24 20     MOV        dword ptr [ESP + 0x20],EBX
                             LAB_00444b93                                    XREF[2]:     004448d7(j), 00444b8d(j)  
        00444b93 8b 54 24 24     MOV        EDX,dword ptr [ESP + 0x24]
        00444b97 8d 34 95        LEA        ESI,[EDX*0x4 + 0x0]
                 00 00 00 00
        00444b9e 56              PUSH       ESI
        00444b9f 89 74 24 30     MOV        dword ptr [ESP + 0x30],ESI
        00444ba3 e8 55 5d        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444ba8 8d 4e fc        LEA        param_1,[ESI + -0x4]
        00444bab 8b b5 3c        MOV        ESI,dword ptr [EBP + 0x13c]
                 01 00 00
        00444bb1 8b d0           MOV        EDX,EAX
        00444bb3 8b c1           MOV        EAX,param_1
        00444bb5 8b fa           MOV        EDI,EDX
        00444bb7 89 54 24 14     MOV        dword ptr [ESP + 0x14],EDX
        00444bbb c1 e9 02        SHR        param_1,0x2
        00444bbe f3 a5           MOVSD.REP  ES:EDI,ESI
        00444bc0 8b c8           MOV        param_1,EAX
        00444bc2 33 c0           XOR        EAX,EAX
        00444bc4 83 e1 03        AND        param_1,0x3
        00444bc7 f3 a4           MOVSB.REP  ES:EDI,ESI
        00444bc9 8b bd 28        MOV        EDI,dword ptr [EBP + 0x128]
                 01 00 00
        00444bcf 83 c9 ff        OR         param_1,0xffffffff
        00444bd2 f2 ae           SCASB.RE   ES:EDI
        00444bd4 8b 74 24 30     MOV        ESI,dword ptr [ESP + 0x30]
        00444bd8 f7 d1           NOT        param_1
        00444bda 49              DEC        param_1
        00444bdb 89 4c 16 fc     MOV        dword ptr [ESI + EDX*0x1 + -0x4],param_1
        00444bdf 8b 8d 3c        MOV        param_1,dword ptr [EBP + 0x13c]
                 01 00 00
        00444be5 51              PUSH       param_1
        00444be6 e8 07 5d        CALL       FUN_005ea8f2                                     undefined FUN_005ea8f2(undefined
                 1a 00
        00444beb 8b 44 24 28     MOV        EAX,dword ptr [ESP + 0x28]
        00444bef 8b 54 24 18     MOV        EDX,dword ptr [ESP + 0x18]
        00444bf3 8b 4c 24 38     MOV        param_1,dword ptr [ESP + 0x38]
        00444bf7 89 85 44        MOV        dword ptr [EBP + 0x144],EAX
                 01 00 00
        00444bfd 8b 44 24 2c     MOV        EAX,dword ptr [ESP + 0x2c]
        00444c01 89 95 3c        MOV        dword ptr [EBP + 0x13c],EDX
                 01 00 00
        00444c07 8b d0           MOV        EDX,EAX
        00444c09 56              PUSH       ESI
        00444c0a 0f af d1        IMUL       EDX,param_1
        00444c0d 89 8d 4c        MOV        dword ptr [EBP + 0x14c],param_1
                 01 00 00
        00444c13 89 95 48        MOV        dword ptr [EBP + 0x148],EDX
                 01 00 00
        00444c19 89 85 38        MOV        dword ptr [EBP + 0x138],EAX
                 01 00 00
        00444c1f e8 d9 5c        CALL       operator_new                                     void * operator_new(uint param_1)
                 1a 00
        00444c24 8d 4e fc        LEA        param_1,[ESI + -0x4]
        00444c27 8b b5 40        MOV        ESI,dword ptr [EBP + 0x140]
                 01 00 00
        00444c2d 8b d1           MOV        EDX,param_1
        00444c2f 8b f8           MOV        EDI,EAX
        00444c31 c1 e9 02        SHR        param_1,0x2
        00444c34 f3 a5           MOVSD.REP  ES:EDI,ESI
        00444c36 8b ca           MOV        param_1,EDX
        00444c38 89 44 24 1c     MOV        dword ptr [ESP + 0x1c],EAX
        00444c3c 83 e1 03        AND        param_1,0x3
        00444c3f f3 a4           MOVSB.REP  ES:EDI,ESI
        00444c41 8b 4c 24 38     MOV        param_1,dword ptr [ESP + 0x38]
        00444c45 89 5c 01 fc     MOV        dword ptr [param_1 + EAX*0x1 + -0x4],EBX
        00444c49 8b 95 40        MOV        EDX,dword ptr [EBP + 0x140]
                 01 00 00
        00444c4f 52              PUSH       EDX
        00444c50 e8 9d 5c        CALL       FUN_005ea8f2                                     undefined FUN_005ea8f2(undefined
                 1a 00
        00444c55 8b 44 24 20     MOV        EAX,dword ptr [ESP + 0x20]
        00444c59 83 c4 10        ADD        ESP,0x10
        00444c5c 89 85 40        MOV        dword ptr [EBP + 0x140],EAX
                 01 00 00
        00444c62 8b 85 30        MOV        EAX,dword ptr [EBP + 0x130]
                 01 00 00
        00444c68 83 f8 01        CMP        EAX,0x1
        00444c6b 75 0e           JNZ        LAB_00444c7b
        00444c6d 8b 55 00        MOV        EDX,dword ptr [EBP]
        00444c70 8b cd           MOV        param_1,EBP
        00444c72 ff 52 48        CALL       dword ptr [EDX + 0x48]
        00444c75 89 85 44        MOV        dword ptr [EBP + 0x144],EAX
                 01 00 00
                             LAB_00444c7b                                    XREF[1]:     00444c6b(j)  
        00444c7b 8b 85 44        MOV        EAX,dword ptr [EBP + 0x144]
                 01 00 00
        00444c81 85 c0           TEST       EAX,EAX
        00444c83 7e 09           JLE        LAB_00444c8e
        00444c85 83 c0 02        ADD        EAX,0x2
        00444c88 89 85 44        MOV        dword ptr [EBP + 0x144],EAX
                 01 00 00
                             LAB_00444c8e                                    XREF[1]:     00444c83(j)  
        00444c8e 8b 85 48        MOV        EAX,dword ptr [EBP + 0x148]
                 01 00 00
        00444c94 85 c0           TEST       EAX,EAX
        00444c96 7e 09           JLE        LAB_00444ca1
        00444c98 83 c0 02        ADD        EAX,0x2
        00444c9b 89 85 48        MOV        dword ptr [EBP + 0x148],EAX
                 01 00 00
                             LAB_00444ca1                                    XREF[1]:     00444c96(j)  
        00444ca1 5f              POP        EDI
        00444ca2 5e              POP        ESI
        00444ca3 5d              POP        EBP
        00444ca4 b0 01           MOV        AL,0x1
        00444ca6 5b              POP        EBX
        00444ca7 83 c4 28        ADD        ESP,0x28
        00444caa c3              RET
        00444cab 90 90 90        align      align(5)
                 90 90
*/

/////////////////////////////////////////////////////////////////////////////////////////////////////

// param_1은 CTextLayout 또는 CUIStaticText 객체
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
   } 


/////////////////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////////////////

undefined4 __fastcall FUN_00444890(int *param_1)

{
  char cVar1;
  int iVar2;
  char *pcVar3;
  undefined4 *puVar4;
  int iVar5;
  uint uVar6;
  uint uVar7;
  int iVar8;
  undefined4 *puVar9;
  int iVar10;
  int iVar11;
  undefined4 *puVar12;
  uint uStack_24;
  uint uStack_20;
  int local_1c;
  int local_18;
  int local_14;
  int iStack_10;
  int iStack_c;
  
  local_1c = 0;
  local_18 = 0;
  iVar8 = 0;
  local_14 = 1;
  iVar2 = (**(code **)(*param_1 + 0x48))();
  pcVar3 = (char *)param_1[0x4a];
  param_1[0x52] = 0;
  param_1[0x51] = 0;
  iVar10 = 0;
  iStack_10 = 0;
  if (*pcVar3 != '\0') {
    uStack_24 = 4;
    uStack_20 = 0;
    do {
      if (pcVar3[local_1c] == '\n') {
        puVar4 = operator_new(uStack_24);
        puVar9 = (undefined4 *)param_1[0x4f];
        puVar12 = puVar4;
        for (uVar6 = uStack_20 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
          *puVar12 = *puVar9;
          puVar9 = puVar9 + 1;
          puVar12 = puVar12 + 1;
        }
        for (iVar10 = 0; iVar10 != 0; iVar10 = iVar10 + -1) {
          *(undefined1 *)puVar12 = *(undefined1 *)puVar9;
          puVar9 = (undefined4 *)((int)puVar9 + 1);
          puVar12 = (undefined4 *)((int)puVar12 + 1);
        }
        *(int *)((uStack_24 - 4) + (int)puVar4) = local_1c + -1;
        FUN_005ea8f2((undefined *)param_1[0x4f]);
        param_1[0x4f] = (int)puVar4;
        puVar4 = operator_new(uStack_24);
        puVar9 = (undefined4 *)param_1[0x50];
        puVar12 = puVar4;
        for (uVar6 = uStack_20 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
          *puVar12 = *puVar9;
          puVar9 = puVar9 + 1;
          puVar12 = puVar12 + 1;
        }
        for (iVar10 = 0; iVar10 != 0; iVar10 = iVar10 + -1) {
          *(undefined1 *)puVar12 = *(undefined1 *)puVar9;
          puVar9 = (undefined4 *)((int)puVar9 + 1);
          puVar12 = (undefined4 *)((int)puVar12 + 1);
        }
        *(int *)((uStack_24 - 4) + (int)puVar4) = iVar8;
        FUN_005ea8f2((undefined *)param_1[0x50]);
        param_1[0x50] = (int)puVar4;
        if (local_18 < iVar8) {
          local_18 = iVar8;
        }
        iVar8 = 0;
        uStack_24 = uStack_24 + 4;
        uStack_20 = uStack_20 + 4;
        local_14 = local_14 + 1;
        iVar10 = iStack_10;
        local_1c = local_1c + 1;
      }
      else {
        iVar5 = FUN_00445480((int)pcVar3[local_1c]);
        iVar11 = iVar10;
        if ((iVar5 == 0) &&
           ((local_1c == 0 ||
            ((0 < local_1c &&
             (iVar10 = FUN_00445480((int)*(char *)(param_1[0x4a] + -1 + local_1c)), iVar10 != 0)))))
           ) {
          iVar11 = local_1c;
          iStack_10 = local_1c;
        }
        iVar10 = (**(code **)(*(int *)param_1[0x49] + 0x1c))
                           (CONCAT31((int3)((uint)param_1[0x4a] >> 8),
                                     *(undefined1 *)(param_1[0x4a] + local_1c)));
        if (iStack_c < iVar10) {
          iStack_c = iVar10;
        }
        iVar10 = (**(code **)(*(int *)param_1[0x49] + 0x20))
                           (CONCAT31((int3)((uint)param_1[0x4a] >> 8),
                                     *(undefined1 *)(param_1[0x4a] + local_1c)));
        iVar8 = iVar8 + iVar10;
        if (iVar2 < iVar8) {
          for (; iVar11 <= local_1c; local_1c = local_1c + -1) {
            iVar10 = (**(code **)(*(int *)param_1[0x49] + 0x20))
                               (CONCAT31((int3)((uint)param_1[0x4a] >> 8),
                                         *(undefined1 *)(param_1[0x4a] + local_1c)));
            iVar8 = iVar8 - iVar10;
          }
          iVar10 = FUN_00445480((int)*(char *)(param_1[0x4a] + local_1c));
          iStack_c = local_1c;
          while (iVar10 != 0) {
            iVar10 = (**(code **)(*(int *)param_1[0x49] + 0x20))
                               (CONCAT31((int3)((uint)param_1[0x4a] >> 8),
                                         *(undefined1 *)(param_1[0x4a] + iStack_c)));
            iVar8 = iVar8 - iVar10;
            iStack_c = iStack_c + -1;
            iVar10 = FUN_00445480((int)*(char *)(param_1[0x4a] + iStack_c));
          }
          if (local_18 < iVar8) {
            local_18 = iVar8;
          }
          puVar4 = operator_new(uStack_24);
          puVar9 = (undefined4 *)param_1[0x4f];
          puVar12 = puVar4;
          for (uVar6 = uStack_20 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
            *puVar12 = *puVar9;
            puVar9 = puVar9 + 1;
            puVar12 = puVar12 + 1;
          }
          for (iVar10 = 0; iVar10 != 0; iVar10 = iVar10 + -1) {
            *(undefined1 *)puVar12 = *(undefined1 *)puVar9;
            puVar9 = (undefined4 *)((int)puVar9 + 1);
            puVar12 = (undefined4 *)((int)puVar12 + 1);
          }
          *(int *)((uStack_24 - 4) + (int)puVar4) = iStack_c;
          FUN_005ea8f2((undefined *)param_1[0x4f]);
          param_1[0x4f] = (int)puVar4;
          puVar4 = operator_new(uStack_24);
          puVar9 = (undefined4 *)param_1[0x50];
          puVar12 = puVar4;
          for (uVar6 = uStack_20 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
            *puVar12 = *puVar9;
            puVar9 = puVar9 + 1;
            puVar12 = puVar12 + 1;
          }
          for (iVar10 = 0; iVar10 != 0; iVar10 = iVar10 + -1) {
            *(undefined1 *)puVar12 = *(undefined1 *)puVar9;
            puVar9 = (undefined4 *)((int)puVar9 + 1);
            puVar12 = (undefined4 *)((int)puVar12 + 1);
          }
          *(int *)((uStack_24 - 4) + (int)puVar4) = iVar8;
          FUN_005ea8f2((undefined *)param_1[0x50]);
          iVar8 = 0;
          local_14 = local_14 + 1;
          uStack_24 = uStack_24 + 4;
          uStack_20 = uStack_20 + 4;
          param_1[0x50] = (int)puVar4;
          iVar10 = iStack_10;
          local_1c = iVar11;
        }
        else {
          local_1c = local_1c + 1;
          iVar10 = iVar11;
        }
      }
      pcVar3 = (char *)param_1[0x4a];
    } while (pcVar3[local_1c] != '\0');
    if (local_18 < iVar8) {
      local_18 = iVar8;
    }
  }
  uVar6 = local_14 * 4;
  puVar4 = operator_new(uVar6);
  puVar9 = (undefined4 *)param_1[0x4f];
  puVar12 = puVar4;
  for (uVar7 = uVar6 - 4 >> 2; uVar7 != 0; uVar7 = uVar7 - 1) {
    *puVar12 = *puVar9;
    puVar9 = puVar9 + 1;
    puVar12 = puVar12 + 1;
  }
  for (iVar10 = 0; iVar10 != 0; iVar10 = iVar10 + -1) {
    *(undefined1 *)puVar12 = *(undefined1 *)puVar9;
    puVar9 = (undefined4 *)((int)puVar9 + 1);
    puVar12 = (undefined4 *)((int)puVar12 + 1);
  }
  uVar7 = 0xffffffff;
  pcVar3 = (char *)param_1[0x4a];
  do {
    if (uVar7 == 0) break;
    uVar7 = uVar7 - 1;
    cVar1 = *pcVar3;
    pcVar3 = pcVar3 + 1;
  } while (cVar1 != '\0');
  puVar4[local_14 + -1] = ~uVar7 - 1;
  FUN_005ea8f2((undefined *)param_1[0x4f]);
  param_1[0x51] = local_18;
  param_1[0x4f] = (int)puVar4;
  param_1[0x53] = 0;
  param_1[0x52] = 0;
  param_1[0x4e] = local_14;
  puVar4 = operator_new(uVar6);
  puVar9 = (undefined4 *)param_1[0x50];
  puVar12 = puVar4;
  for (uVar6 = uVar6 - 4 >> 2; uVar6 != 0; uVar6 = uVar6 - 1) {
    *puVar12 = *puVar9;
    puVar9 = puVar9 + 1;
    puVar12 = puVar12 + 1;
  }
  for (iVar10 = 0; iVar10 != 0; iVar10 = iVar10 + -1) {
    *(undefined1 *)puVar12 = *(undefined1 *)puVar9;
    puVar9 = (undefined4 *)((int)puVar9 + 1);
    puVar12 = (undefined4 *)((int)puVar12 + 1);
  }
  puVar4[local_14 + -1] = iVar8;
  FUN_005ea8f2((undefined *)param_1[0x50]);
  param_1[0x50] = (int)puVar4;
  if (param_1[0x4c] == 1) {
    iVar8 = (**(code **)(*param_1 + 0x48))();
    param_1[0x51] = iVar8;
  }
  if (0 < param_1[0x51]) {
    param_1[0x51] = param_1[0x51] + 2;
  }
  iVar8 = param_1[0x52];
  if (0 < iVar8) {
    iVar8 = iVar8 + 2;
    param_1[0x52] = iVar8;
  }
  return CONCAT31((int3)((uint)iVar8 >> 8),1);
}

