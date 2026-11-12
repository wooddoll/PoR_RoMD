// Pool of Radiance: Ruins of Myth Drannor (Steam/SNEG) - apply analysis results to Ghidra.
// Run once per program: por2main.exe, sneg.dll, ddraw.dll (the script picks the table by program name).
// Safe to re-run: structures are replaced, functions keep existing user names unless they are default (FUN_/DAT_).
// See steam_text_hooks.md and ghidra_scan/README.md.
//@category PoR_RoMD
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.util.exception.DuplicateNameException;
import java.util.*;

public class ApplyPoRAnnotations extends GhidraScript {

    static final CategoryPath CAT = new CategoryPath("/PoR");
    DataTypeManager dtm;
    long base;          // image base of the current program
    long refBase;       // image base used in the tables below
    int nFunc, nSig, nCmt, nLbl, nErr;

    // ------------------------------------------------------------------ types
    DataType t(String spec) {
        spec = spec.trim();
        if (spec.endsWith("*")) {
            return new PointerDataType(t(spec.substring(0, spec.length() - 1)), dtm);
        }
        switch (spec) {
            case "void":   return VoidDataType.dataType;
            case "int":    return IntegerDataType.dataType;
            case "uint":   return UnsignedIntegerDataType.dataType;
            case "short":  return ShortDataType.dataType;
            case "ushort": return UnsignedShortDataType.dataType;
            case "char":   return CharDataType.dataType;
            case "byte":   return ByteDataType.dataType;
            case "bool":   return BooleanDataType.dataType;
            case "float":  return FloatDataType.dataType;
            case "dword":  return DWordDataType.dataType;
            case "word":   return WordDataType.dataType;
            case "wchar":  return WideChar16DataType.dataType;
        }
        DataType dt = dtm.getDataType(CAT, spec);
        if (dt == null) {
            throw new IllegalArgumentException("unknown type " + spec);
        }
        return dt;
    }

    // name, type, offset, comment
    static Object[] f(int off, String type, String name) { return new Object[]{off, type, name, null}; }
    static Object[] f(int off, String type, String name, String cmt) { return new Object[]{off, type, name, cmt}; }

    void struct(String name, int size, String cmt, Object[]... fields) {
        StructureDataType s = new StructureDataType(CAT, name, size, dtm);
        if (cmt != null) s.setDescription(cmt);
        for (Object[] fd : fields) {
            int off = (Integer) fd[0];
            DataType dt = arrayOrType((String) fd[1]);
            s.replaceAtOffset(off, dt, dt.getLength(), (String) fd[2], (String) fd[3]);
        }
        dtm.addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER);
    }

    // "char[16]" style arrays
    DataType arrayOrType(String spec) {
        int b = spec.indexOf('[');
        if (b > 0 && spec.endsWith("]")) {
            DataType el = t(spec.substring(0, b));
            int n = Integer.decode(spec.substring(b + 1, spec.length() - 1));
            return new ArrayDataType(el, n, el.getLength(), dtm);
        }
        return t(spec);
    }

    void createStructures() {
        struct("RECT", 0x10, "Win32 RECT",
            f(0x0, "int", "left"), f(0x4, "int", "top"), f(0x8, "int", "right"), f(0xC, "int", "bottom"));
        struct("MsvcString", 0x18, "MSVC std::string (SSO: buf used when capacity < 16)",
            f(0x00, "char[16]", "buf_or_ptr"), f(0x10, "uint", "size"), f(0x14, "uint", "capacity"));

        // ---- game font
        struct("cGlyph", 0x18, "glyph entry of cFont",
            f(0x00, "int", "charCode"), f(0x04, "RECT", "rect", "atlas rect (right/bottom exclusive)"),
            f(0x14, "void*", "pSurface", "IDirectDrawSurface7*"));
        struct("cCharCodeMap", 0x0C, "code -> glyph index (FindGlyphIndex_004355c0)",
            f(0x0, "int", "count"), f(0x4, "int*", "keys"), f(0x8, "int*", "values"));
        struct("cFont_vtable", 0x2C, "vtable @ por2main 0x0062250C",
            f(0x00, "void*", "CreateFontDIB", "(face, charset, height)"),
            f(0x04, "void*", "RasterizeGlyphs"),
            f(0x08, "void*", "UploadDIBToDirectDrawSurface"),
            f(0x0C, "void*", "CreateSurface"),
            f(0x10, "void*", "RebuildFromFace", "(face, charset, height, arg) - sneg GothicTTF_load uses this"),
            f(0x14, "void*", "AddGlyph"),
            f(0x18, "void*", "SetSurface"),
            f(0x1C, "void*", "GetCharHeight", "0x00435e00"),
            f(0x20, "void*", "GetCharWidth", "0x00435e30"),
            f(0x24, "void*", "GetCharRect", "0x00435e60 (NULL if glyph missing)"),
            f(0x28, "void*", "GetSurface"));
        struct("cFont", 0x2C, "game bitmap font (font0~4). keys: signed char for 1-byte, (c1<<8)|c2 for Hangul",
            f(0x00, "cFont_vtable*", "vtable"),
            f(0x04, "int", "glyphCount"),
            f(0x08, "cGlyph**", "pGlyphs"),
            f(0x0C, "cCharCodeMap*", "pCharMap", "sneg HeadingFace swaps this"),
            f(0x10, "int", "unknown_10"),
            f(0x14, "void*", "pSurface", "IDirectDrawSurface7*, swapped by HeadingFace"),
            f(0x18, "void*", "hdc"),
            f(0x1C, "void*", "dibBitmap"),
            f(0x20, "void*", "dibBits"),
            f(0x24, "int", "atlasWidth"),
            f(0x28, "int", "atlasHeight"));
        struct("cGlyphResult", 0x14, "sneg china_pass result",
            f(0x00, "uint", "charCode"), f(0x04, "uint", "byteLength"), f(0x08, "int", "width"),
            f(0x0C, "int", "height"), f(0x10, "char[4]", "originalText", "[1] = 1 if TTF (china) glyph"));
        struct("SnegCharMetric", 0x1C, "sneg per-char layout entry (builders -> Wrap_ItemsCJK / Wrap_ItemsWord)",
            f(0x00, "int", "byteOffset"), f(0x04, "int", "byteLength"), f(0x08, "int", "width", "china_pass width (bitmap px)"),
            f(0x0C, "int", "height"), f(0x10, "uint", "charCode", "unicode for TTF glyphs, else byte"),
            f(0x14, "float", "hiResAdvance", "_1_china (retail TTF advance), <=0 -> width"),
            f(0x18, "float", "kerning", "_2_china"));
        struct("SnegCharItem", 0x3C, "sneg positioned glyph (HiRes_DrawCharItems input)",
            f(0x00, "RECT", "src", "font atlas rect, or {0,0,w,h} for TTF glyph"),
            f(0x10, "byte", "code", "1-byte char code"), f(0x11, "byte", "isTTF", "1 -> GetChinaFontBitmap"),
            f(0x14, "cGlyphResult", "glyph"), f(0x28, "RECT", "dst", "bitmap px, x scaled at draw"),
            f(0x38, "float", "subpixelX"));
        struct("RetailInkHeader", 0x0C, "por_retail_font%u[_*].ink: 'PRIK', version 1, fontHeight, then u32 hash[256] "
            + "(FNV-1a of original bitmap glyph mask per byte code). File size must be 0x40C",
            f(0x0, "dword", "magic", "0x4B495250 'PRIK'"), f(0x4, "uint", "version"), f(0x8, "uint", "fontHeight"));
        struct("RetailArialInkHeader", 0x10, "por_retail_font%u_arial.ink: 'PRIA', version 1, fontHeight, count, "
            + "then count x {u32 code, u32 unicode, u32 hash} sorted by code",
            f(0x0, "dword", "magic", "0x41495250 'PRIA'"), f(0x4, "uint", "version"), f(0x8, "uint", "fontHeight"),
            f(0xC, "uint", "count"));

        // ---- game UI objects (partial)
        struct("cRenderTarget", 0x130, "vtable +0x54 = GetTargetSurface",
            f(0x000, "void*", "vftable"), f(0x128, "int", "m_nWidth"), f(0x12C, "int", "m_nHeight"));
        struct("CWidgetBase", 0x124, null,
            f(0x000, "void*", "vftable"), f(0x038, "int", "m_nX"), f(0x03C, "int", "m_nY"),
            f(0x0BC, "RECT", "m_rcScreen"), f(0x0CC, "RECT", "m_rcFrame"), f(0x0DC, "RECT", "m_rcClip"),
            f(0x0EC, "void*", "m_pImage"), f(0x0F0, "int", "m_nImageArg"), f(0x0F4, "byte", "m_bVisible"));
        struct("CKeyInput", 0x1B4, "text input widget (0x0043a4d0 Draw)",
            f(0x000, "CWidgetBase", "base"), f(0x124, "int", "m_nBgFlag"), f(0x134, "cFont*", "m_pFont"),
            f(0x138, "char*", "m_pszText"), f(0x13C, "int", "m_nStopIdx"), f(0x140, "int", "m_nPadX"),
            f(0x144, "int", "m_nPadY"), f(0x148, "int", "m_nBoxW"), f(0x14C, "int", "m_nBoxH"),
            f(0x150, "byte", "m_bCursorOn"), f(0x158, "dword", "m_dwLastTick"),
            f(0x15C, "int[20]", "m_aBreakCache"), f(0x1AC, "int", "m_nCursorAlpha"),
            f(0x1B0, "byte", "m_bCursorFalling"), f(0x1B1, "byte", "m_bRightAlign"));
        struct("cTextLayout", 0x168, "word-wrapped text (WordWrap 0x00444890, RenderShadow 0x00444d40)",
            f(0x000, "void*", "vftable", "+0x48 GetMaxWidth, +0x54 GetTargetSurface"),
            f(0x124, "cFont*", "m_pFont"), f(0x128, "char*", "m_pszText"), f(0x130, "int", "m_nAlign", "1 = center"),
            f(0x138, "int", "m_nLineCount"), f(0x13C, "int*", "m_pLineEndIndices"), f(0x140, "int*", "m_pLineWidths"),
            f(0x144, "int", "m_nMaxLineWidth"), f(0x148, "int", "m_nTotalHeight"), f(0x14C, "int", "m_nLineHeight"),
            f(0x150, "word", "m_wTextColor"), f(0x154, "word", "m_wShadowColor"), f(0x164, "byte", "m_bShadow"));
        struct("cTextLabel", 0x1AC, "label (DrawText 0x00442590, CalcExtents 0x004428b0)",
            f(0x000, "void*", "vftable"), f(0x144, "int", "m_nLineHeight"), f(0x148, "int", "m_nTotalHeight"),
            f(0x14C, "int", "m_nBoxWidth"), f(0x158, "int", "m_nLineCount"), f(0x15C, "char**", "m_ppLines"),
            f(0x164, "cFont*", "m_pFont"), f(0x168, "word", "m_wColor"), f(0x170, "void*", "m_pRecolorSurface"),
            f(0x174, "cRenderTarget*", "m_pTarget"), f(0x1A8, "int", "m_nLeft"));
        struct("cTextManager", 0x1E4, "menu / text box (DrawMenu 0x0043b4f0, DrawTextLines 0x0043bc80, HMenu 0x0043f1b0)",
            f(0x000, "void*", "vftable"), f(0x030, "int", "m_nWidth30", "'>' right align base"),
            f(0x0EC, "void*", "m_pOwnerSurface", "compared with backbuffer by sneg scale checks"),
            f(0x128, "int", "m_nWidth"), f(0x12C, "int", "m_nHeight"),
            f(0x13C, "int", "m_nLineCountB"), f(0x140, "int", "m_nLineHeight"), f(0x144, "int", "m_nBoxWidth"),
            f(0x148, "int", "m_nTotalHeight"), f(0x178, "word", "m_wNormalColor"), f(0x17A, "word", "m_wSelColor"),
            f(0x18C, "char**", "m_ppLinesA", "DrawTextLines"), f(0x190, "int", "m_nLineCountA"),
            f(0x198, "char**", "m_ppLinesB", "menu lines"), f(0x1A0, "cFont*", "m_pFont"),
            f(0x1A4, "byte*", "m_pTruncFlags"), f(0x1B4, "cRenderTarget*[2]", "m_pPart"),
            f(0x1BC, "cRenderTarget*", "m_pPartBottom"), f(0x1C0, "cRenderTarget*", "m_pTarget"),
            f(0x1CC, "byte*", "m_pSelFlags"), f(0x1DC, "int", "m_nColumnWidth", "HMenu"),
            f(0x1E0, "int", "m_nColumnMaxBytes", "HMenu"));

        // ---- SNEG
        struct("SnegLanguageEntry", 0x1C, "sneg g_Languages element (languages\\<name>\\language.ini)",
            f(0x00, "MsvcString", "name", "folder name, [A-Za-z0-9_-]{1,15}"),
            f(0x18, "int", "codepage", "[Language] CodePage"));
        struct("SnegLanguagesPollItem", 0x18, null,
            f(0x00, "char[16]", "name"), f(0x10, "int", "codepage"),
            f(0x14, "uint", "flags", "bit0 = active (applied at startup), bit1 = selected for next launch"));
        struct("SnegLanguagesPoll", 400, "SNEG_LanguagesPoll out (version 1, size 400)",
            f(0x00, "int", "version"), f(0x04, "int", "size"), f(0x08, "int", "count"), f(0x0C, "int", "reserved"),
            f(0x10, "SnegLanguagesPollItem[16]", "items"));
        struct("SnegTextSettings", 0x54, "SNEG_TextSettingsPoll out (version 2)",
            f(0x00, "int", "version"), f(0x04, "int", "size"),
            f(0x08, "uint", "flags", "1 NativeFont, 2 ?, 4 ?, 8 ?, 0x10 ?"),
            f(0x0C, "int", "fontScale"), f(0x10, "int", "nativeFontCharSet"), f(0x14, "char[64]", "nativeFontFace"));
        struct("SuisHeader", 0x08, "sneg_ui_strings.dat: 'SUIS', version 1, then len-prefixed string, u32 count, "
            + "entries {u16 keyLen, key, u32 codepage, u16 valLen, value}",
            f(0x0, "dword", "magic", "0x53495553 'SUIS'"), f(0x4, "uint", "version"));
    }

    // ------------------------------------------------------------------ helpers
    Address a(long addr) { return toAddr(addr - refBase + base); }

    boolean isDefaultName(String n) {
        return n.startsWith("FUN_") || n.startsWith("DAT_") || n.startsWith("LAB_") || n.startsWith("thunk_FUN_");
    }

    /** cc: "__thiscall", "__cdecl", "__stdcall", "__fastcall" or null (name/comment only).
        params: "type name" ; for __thiscall the first param is 'this' (passed in ECX). */
    void fn(long addr, String name, String cc, String ret, String params, String comment) {
        Address ad = a(addr);
        try {
            Function func = getFunctionAt(ad);
            if (func == null) {
                func = createFunction(ad, name);
                if (func == null) { println("  ! no function at " + ad + " (" + name + ")"); nErr++; return; }
            }
            if (isDefaultName(func.getName())) {
                try {
                    func.setName(name, SourceType.USER_DEFINED);
                } catch (DuplicateNameException e) {
                    func.setName(name + "_" + ad, SourceType.USER_DEFINED);
                }
                nFunc++;
            }
            if (cc != null && ret != null) {
                applySignature(func, cc, ret, params);
                nSig++;
            }
            if (comment != null) {
                func.setComment(comment);
                nCmt++;
            }
        } catch (Exception e) {
            println("  ! " + ad + " " + name + ": " + e);
            nErr++;
        }
    }

    List<String[]> parseParams(String params) {
        List<String[]> out = new ArrayList<>();
        if (params == null || params.isBlank() || params.equals("void")) return out;
        for (String p : params.split(",")) {
            p = p.trim();
            int sp = p.lastIndexOf(' ');
            String type = p.substring(0, sp).trim();
            String nm = p.substring(sp + 1).trim();
            while (nm.startsWith("*")) { type += "*"; nm = nm.substring(1); }
            out.add(new String[]{type, nm});
        }
        return out;
    }

    void applySignature(Function func, String cc, String ret, String params) throws Exception {
        List<String[]> ps = parseParams(params);
        if (!cc.equals("__thiscall")) {
            FunctionDefinitionDataType fd = new FunctionDefinitionDataType(func.getName(), dtm);
            fd.setReturnType(t(ret));
            ParameterDefinition[] defs = new ParameterDefinition[ps.size()];
            for (int i = 0; i < ps.size(); i++) {
                defs[i] = new ParameterDefinitionImpl(ps.get(i)[1], t(ps.get(i)[0]), null);
            }
            fd.setArguments(defs);
            fd.setCallingConvention(cc);
            ApplyFunctionSignatureCmd cmd = new ApplyFunctionSignatureCmd(func.getEntryPoint(), fd, SourceType.USER_DEFINED);
            if (!cmd.applyTo(currentProgram, monitor)) throw new Exception(cmd.getStatusMsg());
            return;
        }
        // __thiscall: custom storage so 'this' keeps its real type without a class namespace
        Register ecx = currentProgram.getRegister("ECX");
        List<Parameter> list = new ArrayList<>();
        int stack = 4;
        for (int i = 0; i < ps.size(); i++) {
            DataType dt = t(ps.get(i)[0]);
            VariableStorage vs;
            if (i == 0) {
                vs = new VariableStorage(currentProgram, ecx);
            } else {
                int len = Math.max(4, dt.getLength());
                vs = new VariableStorage(currentProgram, stack, dt.getLength());
                stack += len;
            }
            list.add(new ParameterImpl(ps.get(i)[1], dt, vs, currentProgram, SourceType.USER_DEFINED));
        }
        DataType rdt = t(ret);
        VariableStorage rvs;
        if (rdt instanceof VoidDataType) rvs = VariableStorage.VOID_STORAGE;
        else if (rdt.getLength() == 1) rvs = new VariableStorage(currentProgram, currentProgram.getRegister("AL"));
        else if (rdt instanceof FloatDataType) rvs = new VariableStorage(currentProgram, currentProgram.getRegister("ST0"));
        else rvs = new VariableStorage(currentProgram, currentProgram.getRegister("EAX"));
        ReturnParameterImpl rp = new ReturnParameterImpl(rdt, rvs, currentProgram);
        func.setCustomVariableStorage(true);
        func.updateFunction("__thiscall", rp, list, Function.FunctionUpdateType.CUSTOM_STORAGE, true,
            SourceType.USER_DEFINED);
    }

    void lbl(long addr, String name, String eol) {
        Address ad = a(addr);
        try {
            Symbol s = getSymbolAt(ad);
            if (s == null || isDefaultName(s.getName()) || s.getSource() == SourceType.DEFAULT) {
                createLabel(ad, name, true, SourceType.USER_DEFINED);
                nLbl++;
            }
            if (eol != null) { setEOLComment(ad, eol); nCmt++; }
        } catch (Exception e) {
            println("  ! label " + ad + " " + name + ": " + e);
            nErr++;
        }
    }

    /** Clear a wrongly-set "No Return" flag and repair call-site fallthrough / caller bodies.
        Disable Auto Analysis "Non-Returning Functions - Discovered" first, or it marks them again. */
    void noReturnOff(long addr, String name) {
        Address ad = a(addr);
        Function f = getFunctionAt(ad);
        if (f == null) { println("  ! noReturnOff: no function at " + ad); nErr++; return; }
        if (isDefaultName(f.getName()) && name != null) {
            try { f.setName(name, SourceType.USER_DEFINED); } catch (Exception e) { }
        }
        if (!f.hasNoReturn()) return;
        f.setNoReturn(false);
        Set<Function> callers = new HashSet<>();
        for (Reference r : currentProgram.getReferenceManager().getReferencesTo(ad)) {
            if (!r.getReferenceType().isCall()) continue;
            Instruction ins = getInstructionAt(r.getFromAddress());
            if (ins == null) continue;
            ins.setFlowOverride(FlowOverride.NONE);
            ins.clearFallThroughOverride();
            Address ft = ins.getDefaultFallThrough();
            if (ft != null && getInstructionAt(ft) == null && getDataAt(ft) == null) {
                new ghidra.app.cmd.disassemble.DisassembleCommand(ft, null, true).applyTo(currentProgram, monitor);
            }
            Function c = getFunctionContaining(r.getFromAddress());
            if (c != null) callers.add(c);
        }
        for (Function c : callers) {
            try { ghidra.app.cmd.function.CreateFunctionCmd.fixupFunctionBody(currentProgram, c, monitor); }
            catch (Exception e) { println("  ! fixup " + c.getName() + ": " + e); }
        }
        println("  noReturn cleared: " + f.getName() + " (" + callers.size() + " callers)");
        nFunc++;
    }

    void eol(long addr, String text) {
        try { setEOLComment(a(addr), text); nCmt++; }
        catch (Exception e) { println("  ! eol " + Long.toHexString(addr) + ": " + e); nErr++; }
    }

    void typed(long addr, String type, String name, String eol) {
        Address ad = a(addr);
        try {
            DataType dt = arrayOrType(type);
            clearListing(ad, ad.add(dt.getLength() - 1));
            createData(ad, dt);
        } catch (Exception e) {
            println("  ! data " + ad + " " + type + ": " + e);
            nErr++;
        }
        lbl(addr, name, eol);
    }

    // ------------------------------------------------------------------ por2main.exe
    void applyExe() {
        refBase = 0x00400000L;
        String K = "[PoR] ";
        fn(0x004355C0, "cCharCodeMap_FindGlyphIndex", "__thiscall", "int", "cCharCodeMap* this, int code",
            K + "glyph index for code, -1 if missing. Only called by GetCharHeight/Width/Rect.");
        fn(0x00435620, "cFont_ctor", null, null, null, K + "cFont constructor (vtable 0x0062250C). sneg LoadFonts also uses it.");
        fn(0x00435E00, "cFont_GetCharHeight", "__thiscall", "int", "cFont* this, int code", K + "vtable +0x1C. 0 if missing.");
        fn(0x00435E30, "cFont_GetCharWidth", "__thiscall", "int", "cFont* this, int code", K + "vtable +0x20. 0 if missing.");
        fn(0x00435E60, "cFont_GetCharRect", "__thiscall", "RECT*", "cFont* this, int code",
            K + "vtable +0x24. Returns NULL if glyph missing. Patch replaces vtable slot with stateless Hangul-byte handler (FontMetrics.cpp).");
        fn(0x00435E90, "cFont_LoadFont", null, null, null, K + "load ./data/fonts/fontN.bmp/.dff");
        fn(0x0043A4D0, "CKeyInput_Draw", "__thiscall", "bool", "CKeyInput* this",
            K + "sneg: KeyInputDraw_5952c600 (RegHooks_KeyInput). Patch: CKeyInput_Draw_Hook on sneg side.");
        fn(0x0043B4F0, "cTextManager_DrawMenu", "__thiscall", "bool", "cTextManager* this",
            K + "menu draw, 1 byte per char, truncates at 140px with '...' in local copy, '>' right-aligned. "
            + "sneg: DrawMenu_59529ea0 (only if ScaledEngineText or isChina). Patch: cTextManager_MenuDraw_Hook.");
        fn(0x0043B8E0, "cTextManager_DrawMenuMask", "__thiscall", "bool", "cTextManager* this",
            K + "draws value 1 where glyph pixels are set into m_pPartBottom; writes '...' into the SOURCE string. "
            + "Not hooked by sneg. Patch: cTextManager_DrawMenuMask_Hook (MenuExtra.cpp).");
        fn(0x0043BC80, "cTextManager_DrawTextLines", "__thiscall", "bool", "cTextManager* this, int unused",
            K + "RET 4. sneg: DrawTextLines_59529d90. Patch: cTextManager_DrawTextLines_Hook.");
        fn(0x0043BF40, "cTextManager_CalculateTextExtents", "__thiscall", "bool", "cTextManager* this",
            K + "box size from m_ppLinesB (start 10, +20). Not hooked by sneg. Patch: cTextManager_CalculateTextExtents_Hook.");
        fn(0x0043F1B0, "cTextManager_DrawHMenu", "__thiscall", "bool", "cTextManager* this, int unused",
            K + "RET 4. horizontal menu: first word of each item centered in m_nColumnWidth, y=0. "
            + "sneg: FUN_59529c50. Patch: cTextManager_DrawHMenu_Hook.");
        fn(0x00442590, "cTextLabel_DrawText", "__thiscall", "bool", "cTextLabel* this, void* pDstSurface",
            K + "sneg: DrawTextTextLabel_59527720 -> DrawText_59527cd0 / DrawText_ani_59528020. Patch: cTextLabel_DrawText_Hook.");
        fn(0x004428B0, "cTextLabel_CalcExtents", "__thiscall", "bool", "cTextLabel* this",
            K + "Not hooked by sneg. Patch: cTextLabel_CalcExtents_Hook.");
        fn(0x00444890, "cTextLayout_WordWrap", "__thiscall", "bool", "cTextLayout* this",
            K + "sneg: WordWrap_594a7f70 (always). Patch: cTextLayout_WordWrap_Hook.");
        fn(0x00444D40, "cTextLayout_RenderShadow", "__thiscall", "bool", "cTextLayout* this",
            K + "sneg: RenderShadow_59515aa0. Patch: cTextLayout_RenderShadow_Hook.");
        fn(0x00445480, "IsSpaceChar", "__cdecl", "int", "int c", K + "9..13 or 0x20");
        fn(0x00436A50, "CWidget_UpdateTooltip", null, null, null,
            K + "tooltip box width measured 1 byte per char via vtable GetCharRect. sneg: FUN_59576cc0.");
        fn(0x0043DFC0, "cTextManager_HoverTooltip", null, null, null,
            K + "menu item hover tooltip, width per byte. sneg: FUN_59576ae0.");
        fn(0x00476820, "PartyTooltip_SprintfMeasure", null, null, null,
            K + "party tooltip width per byte (clamped 200..300). sneg: SprintfMeasure_5955cb10.");
        fn(0x0043FFA0, "LoadFonts", null, null, null, K + "creates font0..4. sneg: LoadFonts_59514810.");
        fn(0x00406D60, "KeyInput_OnChar", null, null, null, K + "patch: call at 0x0049EDCE redirected to Input_Hook (Hangul backspace).");

        typed(0x0062250C, "cFont_vtable", "cFont_vtable", "[PoR] cFont vtable; slots +0x1C/+0x20/+0x24 patched by FontMetrics.cpp");
        lbl(0x00727290, "g_pGame", "[PoR] game global (+0x18 gfx context, +0x2c text manager with fonts at +0x13c)");
    }

    // ------------------------------------------------------------------ sneg.dll
    void applySneg() {
        refBase = 0x59460000L;
        String K = "[PoR] ";
        // CRT functions wrongly marked noreturn -> decompiler cut off after every delete/new
        noReturnOff(0x595D28FA, "operator_delete_sized");
        noReturnOff(0x59467911, null);   // thunk of operator_delete_sized
        noReturnOff(0x595D28CA, "operator_new");
        noReturnOff(0x594A6C50, null);   // MakeChinaFont
        fn(0x595DF551, "_invalid_parameter_noinfo_noreturn", null, null, null, null);
        // load / hook registration
        fn(0x594A42F0, "SNEG_Load", "__cdecl", "void", "uint* param", K + "called from ddraw.dll DllMain right after LoadLibraryA(sneg.dll)");
        fn(0x594A5840, "Sneg_Init", null, null, null, K + "SNEG_Load body: settings, language, then Sneg_RegisterAllHooks");
        fn(0x594A4680, "Sneg_RegisterAllHooks", null, null, null, K + "calls every RegHooks_* (runs inside ddraw DllMain, before our InitHookThread)");
        fn(0x594A4420, "Sneg_InstallHook", "__cdecl", "uint", "int gameOffset, void* hook, void** orig",
            K + "MinHook CreateHook+Enable on por2main base + gameOffset");
        fn(0x5952A050, "RegHooks_TextManager", null, null, null,
            K + "0x3b4f0 DrawMenu, 0x3bc80 DrawTextLines, 0x3f1b0 HMenu, 0x3d1f0, 0x3f090. Only if ScaledEngineText || isChina. "
            + "Patch ret at entry is ineffective (already ran).");
        fn(0x59527C70, "RegHooks_TextLabel", null, null, null, K + "0x42590, 0x429b0, 0x421f0. If ScaledEngineText || HeadingFaceScaled");
        fn(0x5952CC00, "RegHooks_KeyInput", null, null, null, K + "0x3a4d0. If ScaledEngineText || HeadingFaceScaled");
        fn(0x594A7C80, "RegHook_WordWrap", null, null, null, K + "0x44890 always");
        fn(0x59515F80, "RegHook_RenderShadow", null, null, null, K + "0x44d40 if 596443ca");
        fn(0x595149E0, "RegHook_LoadFonts", null, null, null, K + "0x3ffa0 always");
        fn(0x59515490, "RegHook_Blend4451f0_A", null, null, null, K + "0x451f0 -> FUN_59515300 (not text)");
        fn(0x59515A70, "RegHook_Blend4451f0_B", null, null, null, K + "0x451f0 -> FUN_59515760 (not text)");
        fn(0x59576F30, "RegHooks_Tooltips", null, null, null, K + "0x390e0, 0x36a50 -> FUN_59576cc0, 0x3dfc0 -> FUN_59576ae0");
        fn(0x5955CE80, "RegHooks_SprintfMeasure", null, null, null, K + "0x73b20, 0x76820 -> SprintfMeasure_5955cb10");

        // text replacements
        fn(0x59529EA0, "Sneg_DrawMenu", "__thiscall", "bool", "cTextManager* this",
            K + "game 0x43b4f0. scale>0 -> subDrawMenu_ani (china), else subDrawMenu_5952ac20. Patch: hooked whole.");
        fn(0x59529D90, "Sneg_DrawTextLines", "__thiscall", "bool", "cTextManager* this, int unused",
            K + "game 0x43bc80. else -> FUN_5952a4d0. Patch: hooked whole.");
        fn(0x59529C50, "Sneg_DrawHMenu", "__thiscall", "bool", "cTextManager* this, int unused",
            K + "game 0x43f1b0, RET 4. else -> FUN_5952a840. Patch: cTextManager_DrawHMenu_Hook.");
        fn(0x595297F0, "Sneg_TM_Draw43d1f0", "__thiscall", "bool", "cTextManager* this", K + "game 0x43d1f0 (blit only)");
        fn(0x59529A40, "Sneg_TM_Draw43f090", "__thiscall", "bool", "cTextManager* this", K + "game 0x43f090 (blit only)");
        fn(0x5952AC20, "Sneg_subDrawMenu", "__thiscall", "bool", "cTextManager* this", K + "non-scaled menu blit (MakeChinaFont)");
        fn(0x5952A4D0, "Sneg_DrawTextLines_Blit", "__cdecl", "bool", "cTextManager* tm", K + "non-scaled text lines blit");
        fn(0x5952A840, "Sneg_DrawHMenu_Blit", "__cdecl", "bool", "cTextManager* tm", K + "non-scaled horizontal menu blit (china_pass)");
        fn(0x5952A130, "Sneg_TM_UpdateHiResCache", null, null, null,
            K + "ani render only if GetMenuTextScale>0 or cache flag (+0x20/+0x21) set");
        fn(0x595293B0, "Sneg_DrawMenuTrunc", "__cdecl", "int", "cTextManager* tm, int lineIdx, char* buf, char* truncFlag", K + "140px '...'");
        fn(0x59527720, "Sneg_DrawTextTextLabel", "__thiscall", "bool", "cTextLabel* this, void* pDstSurface",
            K + "game 0x42590 -> DrawText_59527cd0 or DrawText_ani_59528020 (GetLabelTextScale)");
        fn(0x59527CD0, "Sneg_TextLabel_DrawText", "__cdecl", "bool", "cTextLabel* label, void* pDstSurface",
            K + "Patch: cTextLabel_DrawText_Hook");
        fn(0x59528020, "Sneg_TextLabel_DrawText_ani", "__cdecl", "bool", "cTextLabel* label, void* pDstSurface, float scale",
            K + "china hi-res path, blocked by GetLabelTextScale patch");
        fn(0x595278A0, "Sneg_TextLabel_Recolor429b0", "__thiscall", "bool", "cTextLabel* this, void* pSrc", K + "game 0x429b0 (color only)");
        fn(0x59527480, "Sneg_TextLabel_Blit421f0", "__thiscall", "bool", "cTextLabel* this", K + "game 0x421f0 (blit only)");
        fn(0x5952C600, "Sneg_KeyInputDraw", "__thiscall", "bool", "CKeyInput* this", K + "game 0x3a4d0. Patch: CKeyInput_Draw_Hook");
        fn(0x594A7F70, "Sneg_WordWrap", "__thiscall", "bool", "cTextLayout* this", K + "game 0x44890. Patch: cTextLayout_WordWrap_Hook");
        fn(0x59515AA0, "Sneg_RenderShadow", "__thiscall", "bool", "cTextLayout* this", K + "game 0x44d40. Patch: cTextLayout_RenderShadow_Hook");
        fn(0x59576CC0, "Sneg_Tooltip_Widget", "__thiscall", "void", "void* this", K + "game 0x36a50, width per byte (200..300)");
        fn(0x59576AE0, "Sneg_Tooltip_MenuHover", "__thiscall", "bool", "cTextManager* this, int x, int y",
            K + "game 0x3dfc0, width per byte (100..800)");
        fn(0x5955CB10, "Sneg_SprintfMeasure", "__thiscall", "void", "void* this", K + "game 0x76820, party tooltip width per byte");
        fn(0x594A68D0, "china_pass", "__cdecl", "cGlyphResult*", "cGlyphResult* out, cFont* font, char* text, int len",
            K + "decode 1 char (GBK/Big5 aware) and measure");

        // scale
        fn(0x595258B0, "GetMenuTextScale", "__cdecl", "float", "void",
            K + ">0 enables *_ani (china) paths for menus. Patch: fldz;ret");
        fn(0x59525830, "GetLabelTextScale", "__cdecl", "float", "cFont* font",
            K + ">0 enables DrawText_ani for labels. Patch: fldz;ret");
        eol(0x595258B0, "PoR_RoMD patch: D9 EE C3 (fldz; ret)");
        eol(0x59525830, "PoR_RoMD patch: D9 EE C3 (fldz; ret)");
        eol(0x5952A050, "PoR_RoMD patch: C3 (ret) - ineffective, SNEG_Load already registered");

        // fonts
        fn(0x59514810, "Sneg_LoadFonts", "__thiscall", "bool", "void* this", K + "game 0x3ffa0. font0..4 via cFont_ctor, then HeadingFace TTF");
        fn(0x59514C00, "Sneg_ApplyHeadingFaceTTF", "__cdecl", "void", "cFont** fonts, int count",
            K + "if HeadingFace && !NativeFont && !Legacy && (AlternativeFont || codepage in {0,1252,1250}). "
            + "Returns early if gfx context (g_pGame+0x18 +0x48) not ready.");
        fn(0x59514DB0, "Sneg_BuildFontFromTTF", "__cdecl", "bool", "cFont** fonts, int idx, char* face, float scale, uint charset, int arg",
            K + "AddFontResourceExA(sneg_ui/fonts/*.ttf, FR_PRIVATE) then cFont vtable+0x10. Replaces our Hangul atlas!");
        fn(0x59514FB0, "Sneg_ApplyFontSettings", null, null, null, K + "swap original <-> TTF atlas when settings change");
        fn(0x59524690, "Sneg_GetHiResGlyphCache", null, null, null, K + "hi-res glyph cache for *_ani paths (retail fonts / heading face)");
        fn(0x5951B000, "Sneg_LoadRetailFonts", null, null, null, K + "sneg_ui/fonts/retail/por_retail_font%u.ttf/.ink, or %WINDIR%\\Fonts\\arial.ttf");

        // retail fonts (.ink = per-byte fingerprint of the original bitmap glyph, NOT glyph data)
        fn(0x5951B890, "RetailFont_LoadMain", null, null, null, K + "por_retail_font%u.ttf + .ink");
        fn(0x59519240, "RetailFont_LoadTtfInk", null, null, null, K + "'PRIK' ink, size 0x40C, u32 hash[256]; then TTF -> variant list (+0x08)");
        fn(0x5951A950, "RetailFont_LoadArialTtf", null, null, null, K + "%WINDIR%\\Fonts\\arial.ttf");
        fn(0x5951ABD0, "RetailFont_LoadArialInk", null, null, null, K + "'PRIA' ink {code, unicode, hash} list (+0x1C), resets 256-byte check cache (+0x2C)");
        fn(0x5951A6A0, "GlyphMask_Hash", "__cdecl", "uint", "int* mask", K + "FNV-1a over {w, h, 1-bit mask bytes}");
        fn(0x59524460, "GlyphMask_FromFont", null, null, null, K + "cut glyph rect from cFont surface into {w, h, mask}");
        fn(0x59519DF0, "RetailFont_RenderGlyph", null, null, null,
            K + "code>0xFF fails. Use retail TTF only if GlyphMask_Hash(game glyph) == ink hash[code] (unmodified font); else bitmap fallback");
        fn(0x59519490, "RetailFont_Advance", null, null, null, K + "retail TTF advance for unicode");
        fn(0x5951A710, "RetailFont_Kerning", null, null, null, K + "retail TTF kerning pair");
        fn(0x59523AD0, "Byte_ToUnicode", "__cdecl", "bool", "byte code, uint* out", K + "MultiByteToWideChar(codepage, 1 byte) - DBCS lead bytes fail");
        fn(0x59524070, "HiRes_GetGlyphBitmap", null, null, null, K + "1-byte glyph: retail TTF (RetailFont_RenderGlyph) or scaled bitmap (FUN_59517040)");
        fn(0x5952BC90, "HiRes_DrawCharItems", null, null, null,
            K + "draw SnegCharItem list at (dst.x+subpixelX)*scale; isTTF -> GetChinaFontBitmap else HiRes_GetGlyphBitmap. Layout already done.");
        fn(0x594A63D0, "GetChinaFontBitmap", null, null, null,
            K + "DIB (width*scale x height*scale), TextOutW at x=0 (left aligned, clipped to china_pass width)");
        fn(0x594A66A0, "ChinaFont_GetByHeight", null, null, null, K + "GDI font cache by pixel height (CreateFontW JhengHei/YaHei)");
        fn(0x595272A0, "GameFontBridge_GetGlyph", null, null, null, K + "SNEG_RetailFontGlyphPoll: gamefont mode 1-bit glyph table, font 0..7 x code 0..255");
        fn(0x59526D10, "GameFontBridge_Build", null, null, null, K + "build 256 glyphs x 0x220 per font via BridgeChineae_English");

        // char item builders / wrap
        fn(0x5952CC30, "KeyInput_BuildItemsChina", null, null, null,
            K + "china_pass per char -> SnegCharMetric; hi-res: advance = RetailFont_Advance; wrap via Wrap_ItemsCJK (isChina) / Wrap_ItemsWord");
        fn(0x5952D410, "KeyInput_BuildItems", null, null, null, K + "non-china KeyInput builder");
        fn(0x5952D630, "CharItem_Push", null, null, null, K + "metric -> SnegCharItem (src = GetCharRect, or {0,0,w,h} if TTF)");
        fn(0x594A71E0, "Wrap_ItemsCJK", "__cdecl", "void*", "void* outLines, void* metrics, uint byteLen, int maxWidth",
            K + "break anywhere if charCode > 0x2E7F (Hangul too!), kinsoku tables 5960b118/5960b12c");
        fn(0x594A7690, "Wrap_ItemsWord", "__cdecl", "void*", "void* outLines, void* metrics, uint count, int maxWidth",
            K + "break at spaces. Loops metric index < count (caller passes byte length: unsafe with DBCS)");

        // settings
        fn(0x5957F090, "Sneg_LoadTextSettings", null, null, null,
            K + "[Text] NativeFont, HeadingFace, AlternativeFont, HeadingFaceScaled, DefaultsRevision+ScaledEngineText, "
            + "GameFontOutlines, CodePage, NativeFontGroups, NativeFontFace, NativeFontScalePercent, NativeFontCharSet (+ POR_* env)");
        String[][] getters = {
            {"595815B0", "Cfg_AlternativeFontActive"}, {"595815D0", "Cfg_59699262"}, {"595815E0", "Cfg_596443c8"},
            {"595815F0", "Cfg_59699263"}, {"59581620", "Cfg_GameFontOutlinesActive"}, {"59581640", "Cfg_ChangeCounter"},
            {"59581650", "Cfg_HeadingFace"}, {"59581660", "Cfg_HeadingFaceScaledActive"}, {"59581680", "Cfg_HeadingFaceScaled"},
            {"59581690", "Cfg_596443cb"}, {"595816A0", "Cfg_5969925c"}, {"595816B0", "Cfg_NativeFont"},
            {"595816D0", "Cfg_5969925b"}, {"59581700", "Cfg_LegacyLock"}, {"59581710", "Cfg_5969926d"},
            {"59581720", "Cfg_AlternativeFont"}, {"59581730", "Cfg_596443ca"}, {"59581740", "Cfg_ScaledEngineTextActive"},
            {"59581760", "Cfg_ScaledEngineText"},
            {"59581770", "Set_AlternativeFont"}, {"595817B0", "Set_CodePageOverride"}, {"595817C0", "Set_GameFontOutlines"},
            {"59581800", "Set_HeadingFace"}, {"59581810", "Set_HeadingFaceScaled"}, {"59581850", "Set_LanguageCodePage"},
            {"59581860", "Set_596443cb"}, {"59581870", "Set_NativeFontCharSet"}, {"59581880", "Set_NativeFont"},
            {"59581900", "Set_LegacyLock"}, {"59581940", "Set_5969926d"}, {"59581950", "Set_596443ca"},
            {"59581960", "Set_ScaledEngineText"}};
        for (String[] g : getters) fn(Long.parseLong(g[0], 16), g[1], null, null, null, null);
        fn(0x59581600, "GetCodepage", "__cdecl", "int", "void", K + "[Text] CodePage override, else active language codepage");
        fn(0x594A6690, "IsChineseCodepage", "__cdecl", "bool", "void", K + "GetCodepage() == 950");
        lbl(0x596443C8, "g_Cfg_596443c8", null);
        lbl(0x596443C9, "g_ScaledEngineText", "[PoR] .data default 1 but overwritten by Sneg_LoadTextSettings");
        lbl(0x596443CA, "g_Cfg_596443ca", "[PoR] RenderShadow hook flag");
        lbl(0x596443CB, "g_Cfg_596443cb", null);
        lbl(0x59699258, "g_NativeFont", null);
        lbl(0x59699259, "g_HeadingFace", null);
        lbl(0x5969925A, "g_HeadingFaceScaled", null);
        lbl(0x59699260, "g_GameFontOutlines", null);
        lbl(0x59699261, "g_AlternativeFont", null);
        lbl(0x59699264, "g_CodePageOverride", "[PoR] [Text] CodePage");
        lbl(0x59699268, "g_LanguageCodePage", "[PoR] active language codepage");
        lbl(0x5969926C, "g_LegacyLock", null);
        lbl(0x59699270, "g_SettingsChangeCounter", null);
        lbl(0x596131EC, "g_HeadingFaceTable", "[PoR] {key, name, face, ttf, 0} x2 stride 0x14: PoR Gothic, Source Sans 3");

        // language
        fn(0x59509040, "Lang_ScanFolders", null, null, null,
            K + "languages\\*\\language.ini [Language] CodePage (valid) -> g_Languages (max 16, entry 0 = built-in 1252)");
        fn(0x5950A400, "Lang_Init", "__cdecl", "void", "char* gameDir, char* languageName",
            K + "scan folders, pick active by name, build per-language file map, read [Dialogue] TextField / [Spells] LevelLast");
        fn(0x5950D4F0, "Lang_ResolveFile", "__cdecl", "bool", "char* name, char* outPath, uint outSize",
            K + "-> languages\\<active>\\<name> if the language folder has that file");
        fn(0x5950E6C0, "Lang_GetSelectedName", "__cdecl", "void", "char* buf, uint size",
            K + "cmdline Language, env POR_LANGUAGE, settings [Text] Language");
        fn(0x59508F80, "Lang_GetActiveCodepage", "__cdecl", "int", "void", null);
        fn(0x5950B090, "Lang_GetList", null, null, null, K + "returns &g_Languages");
        fn(0x59508AF0, "Lang_GetActiveIndex", null, null, null, K + "language applied at startup (SNEG_LanguagesPoll flags bit0)");
        fn(0x5950DA90, "Lang_GetSelectedIndex", null, null, null, K + "language selected for next launch (flags bit1)");
        fn(0x59580880, "Sneg_InitLanguageAndUiStrings", null, null, null, K + "Lang_Init then UiStrings_Load(sneg_ui_strings.dat)");
        fn(0x59513400, "UiStrings_Load", "__cdecl", "void", "char* path", K + "SUIS format, see SuisHeader");
        fn(0x59513BD0, "UiText_Poll", "__cdecl", "int", "int* request, int* result",
            K + "kind 1 = sfDat id, kind 2 = key; missing key -> \"[[key]]\" cp65001");
        fn(0x594D0E00, "SNEG_LanguagesPoll", "__cdecl", "int", "SnegLanguagesPoll* out", null);
        fn(0x594D0B50, "SNEG_TextSettingsPoll", "__cdecl", "int", "SnegTextSettings* out", null);
        lbl(0x59677EB8, "g_Languages", "[PoR] std::vector<SnegLanguageEntry>");
        lbl(0x59677EEC, "g_ActiveLanguageIndex", null);
        lbl(0x59677EF4, "g_LanguageFileMap", "[PoR] files present in active language folder");
        lbl(0x59677F34, "g_UiStrings", "[PoR] std::map key -> {value, codepage}");
    }

    // ------------------------------------------------------------------ ddraw.dll
    void applyDdraw() {
        refBase = 0x596E0000L;
        String K = "[PoR] ";
        fn(0x596E2AD0, "ddraw_DllMain", null, null, null, K + "calls LoadSnegDll on DLL_PROCESS_ATTACH");
        fn(0x596FCE90, "LoadSnegDll", null, null, null,
            K + "GetPrivateProfileStringA(\"Plugin\",\"GameDll\",\"sneg.dll\") -> LoadLibraryA -> SNEG_Load (inside DllMain)");
        fn(0x596F89B0, "GetUiCodepage", "__cdecl", "int", "void", K + "ini [Text] CodePage if >0, else active language codepage");
        fn(0x596F89D0, "SetActiveLanguageCodepage", "__cdecl", "void", "int codepage", K + "from SNEG_LanguagesPoll item with flags bit0");
        fn(0x5971D860, "SelectUiBaseFont", null, null, null,
            K + "\"builtin\" / \"gamefont\" / PoRGothic (codepage 0/1250/1252 only) / custom path");
        fn(0x5972B2D0, "AddCjkFont", "__cdecl", "void", "void* atlas, char* fontsDir, int codepage, void* ranges",
            K + "msjh.ttc (fallback msyh.ttf) at 48px with glyphs from BuildCjkGlyphSet. codepage 949 -> TexDesiredWidth 4096");
        fn(0x5972BA20, "BuildCjkGlyphSet", "__cdecl", "void*", "void* outSet, int codepage",
            K + "always seed \"繁體中文日本語\"; only codepage 949 (0x3B5) adds languages/zh-hant/sneg_ui_cjk_characters.txt. "
            + "Chinese is 950 in sneg -> Chinese UI shows '?'");
        fn(0x597B7A30, "ImFontAtlas_AddFontFromFileTTF", null, null, null, K + "Dear ImGui (atlas, path, size, cfg, ranges)");
        fn(0x597B6C70, "ImFontConfig_ctor", null, null, null, null);
        fn(0x5970A580, "Settings_BuildLanguageList", null, null, null, K + "label key \"ui.settings.language.\" + name");
        lbl(0x5982DA50, "g_CjkSeedChars", "[PoR] UTF-16 \"繁體中文日本語\"");
        lbl(0x5982DA60, "s_CjkCharListPath", null);
        lbl(0x599F101C, "g_UiCodepageIni", null);
        lbl(0x599F102C, "g_UiCodepageActiveLanguage", null);
        lbl(0x599F118C, "g_SnegLoadState", null);
    }

    // ------------------------------------------------------------------ anchors
    // All table addresses are converted with (address - table base + current image base), so a rebased
    // Ghidra program is fine. These checks make sure the binary itself is the same build.

    /** RVA of an exported/global symbol must match. */
    boolean anchorSymbolRva(String sym, long expectRva) {
        for (Symbol s : currentProgram.getSymbolTable().getGlobalSymbols(sym)) {
            long rva = s.getAddress().getOffset() - base;
            boolean ok = rva == expectRva;
            println(String.format("  anchor %s rva=0x%x expected 0x%x %s", sym, rva, expectRva, ok ? "OK" : "MISMATCH"));
            return ok;
        }
        println("  anchor " + sym + " not found");
        return false;
    }

    /** dword at tableAddr must point to tableTarget (both rebased). */
    boolean anchorPointer(long tableAddr, long tableTarget, String what) {
        try {
            long v = getInt(a(tableAddr)) & 0xFFFFFFFFL;
            long expect = a(tableTarget).getOffset();
            boolean ok = v == expect;
            println(String.format("  anchor %s [%s]=0x%x expected 0x%x %s", what, a(tableAddr), v, expect, ok ? "OK" : "MISMATCH"));
            return ok;
        } catch (Exception e) {
            println("  anchor " + what + ": " + e);
            return false;
        }
    }

    /** bytes at tableAddr must equal hex. */
    boolean anchorBytes(long tableAddr, String hex, String what) {
        try {
            byte[] got = getBytes(a(tableAddr), hex.length() / 2);
            StringBuilder sb = new StringBuilder();
            for (byte b : got) sb.append(String.format("%02x", b & 0xff));
            boolean ok = sb.toString().equalsIgnoreCase(hex);
            println(String.format("  anchor %s @%s = %s expected %s %s", what, a(tableAddr), sb, hex, ok ? "OK" : "MISMATCH"));
            return ok;
        } catch (Exception e) {
            println("  anchor " + what + ": " + e);
            return false;
        }
    }

    boolean checkAnchors(String name) {
        if (name.startsWith("por2main")) {
            refBase = 0x00400000L;
            return anchorPointer(0x00622528, 0x00435E00, "cFont_vtable+0x1C -> GetCharHeight")
                && anchorPointer(0x00622530, 0x00435E60, "cFont_vtable+0x24 -> GetCharRect")
                && anchorBytes(0x0043B4F0, "81ec", "DrawMenu prologue (SUB ESP)");
        }
        if (name.startsWith("sneg")) {
            refBase = 0x59460000L;
            return anchorSymbolRva("SNEG_Load", 0x442F0)
                && anchorSymbolRva("SNEG_LanguagesPoll", 0x70E00)
                && anchorSymbolRva("SNEG_UiTextPoll", 0x79250);
        }
        if (name.startsWith("ddraw")) {
            refBase = 0x596E0000L;
            return anchorSymbolRva("DirectDrawCreate", 0x2CE0)
                && anchorSymbolRva("POR_BuildEdition", 0x4C960);
        }
        return true;
    }

    // ------------------------------------------------------------------ main
    @Override
    public void run() throws Exception {
        dtm = currentProgram.getDataTypeManager();
        base = currentProgram.getImageBase().getOffset();
        String name = currentProgram.getName().toLowerCase();
        println("ApplyPoRAnnotations: " + name + " base=" + Long.toHexString(base));

        boolean force = getScriptArgs().length > 0 && getScriptArgs()[0].equalsIgnoreCase("force");
        if (!force && !checkAnchors(name)) {
            println("ABORT: anchors do not match this binary (different version?). Nothing applied. "
                + "Run with script argument 'force' to apply anyway.");
            return;
        }

        createStructures();
        if (name.startsWith("por2main")) applyExe();
        else if (name.startsWith("sneg")) applySneg();
        else if (name.startsWith("ddraw")) applyDdraw();
        else { println("unknown program, only structures were created"); }

        println(String.format("done: renamed %d, signatures %d, comments %d, labels %d, errors %d",
            nFunc, nSig, nCmt, nLbl, nErr));
    }
}
