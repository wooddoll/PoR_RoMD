/*
 * ============================================================================
 *  CTextLayout::WordWrap  (0x00444890)  ?X  My Decompilation Version
 * ============================================================================
 *
 *  This is my own independent decompilation of the assembly at 0x00444890.
 *  I've traced through every instruction carefully, paying special attention
 *  to the stack frame layout and the dynamic array management pattern.
 *
 *  ?w?w Object Layout (this = EBP) ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
 *    +0x000  vftable
 *    +0x124  cFont*          m_pFont
 *    +0x128  const char*     m_pszText
 *    +0x130  int             m_bUseBoxWidth   (if == 1, override max width)
 *    +0x138  int             m_nLineCount
 *    +0x13C  int*            m_pLineEndIndices
 *    +0x140  int*            m_pLineWidths
 *    +0x144  int             m_nMaxLineWidth
 *    +0x148  int             m_nTotalHeight
 *    +0x14C  int             m_nLineHeight     (max glyph height)
 *
 *  ?w?w Stack Frame (after SUB ESP,0x28 + 4 pushes) ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
 *    [ESP+0x14]  arrayByteSize   (bytes allocated for line arrays)
 *    [ESP+0x18]  byteOffset      (bytes copied so far = elementCount*4)
 *    [ESP+0x1C]  charIdx         (current character index)
 *    [ESP+0x20]  maxLineWidth    (widest line so far)
 *    [ESP+0x24]  lineCount       (number of lines, starts at 1)
 *    [ESP+0x28]  lastSpaceIdx    (index of most recent space)
 *    [ESP+0x2C]  breakPos        (temporary: line break position)
 *    [ESP+0x30]  maxLineHeight   (tallest glyph so far)
 *    [ESP+0x34]  boxMaxWidth     (from vftable+0x48)
 *
 *  ?w?w Registers ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
 *    ESI = charIdx (loop counter)
 *    EDI = lastSpaceIdx
 *    EBX = currentLineWidth
 *    EAX = text pointer (reloaded each iteration)
 *
 *  ?w?w Algorithm Summary ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
 *  1. Get box max width via vftable+0x48
 *  2. Iterate through the text:
 *     a. '\n' ?? force line break: record (charIdx-1) and current width
 *     b. Normal char:
 *        - Track last space position (for word wrapping)
 *        - Accumulate char width
 *        - If width exceeds box max:
 *          i.   Backtrack: subtract widths from charIdx back to lastSpaceIdx
 *          ii.  Skip trailing spaces: subtract their widths too
 *          iii. Record break position and current width as a new line
 *          iv.  Reset width, increment line count, resume at lastSpaceIdx
 *  3. After loop, record final line (end = strlen-1, width = current)
 *  4. Store results: lineCount, lineEnds[], lineWidths[], maxWidth,
 *     totalHeight = lineCount * maxLineHeight, lineHeight = maxLineHeight
 *  5. If m_bUseBoxWidth == 1, override maxWidth with GetMaxWidth()
 *  6. Add +2 padding to maxWidth and totalHeight if positive
 * ============================================================================
 */

#include <cstring>
#include <cstdlib>

// Forward declarations
extern "C" void* operator_new(size_t size);                    // 0x005ea8fd
extern "C" void  operator_delete(void* ptr);                   // 0x005ea8f2
extern "C" int   isspace_00445480(int c);                      // 0x00445480

// Font interface (vtable offsets)
struct cFontVtbl {
    void* dummy[7];            // +0x00 .. +0x18
    int   (*GetCharHeight)(cFontVtbl* self, int code);  // +0x1C
    int   (*GetCharWidth)(cFontVtbl* self, int code);   // +0x20
    void* (*GetCharRect)(cFontVtbl* self, int code);    // +0x24
    void* (*GetSurface)(cFontVtbl* self);               // +0x28
};

struct cFont {
    cFontVtbl* vtable;
};

// Text layout object
struct CTextLayout {
    void* vftable;                       // +0x000
    char  pad_004[0x120];                // +0x004 .. +0x123
    cFont* m_pFont;                      // +0x124
    const char* m_pszText;               // +0x128
    char  pad_12C[4];                    // +0x12C
    int   m_bUseBoxWidth;                // +0x130
    char  pad_134[4];                    // +0x134
    int   m_nLineCount;                  // +0x138
    int*  m_pLineEndIndices;             // +0x13C
    int*  m_pLineWidths;                 // +0x140
    int   m_nMaxLineWidth;               // +0x144
    int   m_nTotalHeight;                // +0x148
    int   m_nLineHeight;                 // +0x14C
};

// vftable+0x48: GetMaxWidth()
typedef int (__thiscall* tGetMaxWidth)(CTextLayout* self);

// ============================================================================
//  Dynamic array append helper (matches the assembly's realloc+copy pattern)
// ============================================================================
static int* AppendToIntArray(int* pOldArray, int oldByteOffset, int newValue)
{
    // oldByteOffset = number of bytes already in the array
    int newByteSize = oldByteOffset + 4;

    int* pNewArray = (int*)operator_new(newByteSize);
    if (pOldArray)
    {
        memcpy(pNewArray, pOldArray, oldByteOffset);
        operator_delete(pOldArray);
    }

    // Store new element at the end
    pNewArray[oldByteOffset / 4] = newValue;
    return pNewArray;
}

// ============================================================================
//  Main function
// ============================================================================
BOOL __fastcall CTextLayout_WordWrap_00444890(CTextLayout* this)
{
    // ?w?w Stack frame variables (mapped to assembly) ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    int   arrayByteSize  = 4;    // [ESP+0x14] ?X starts at 4 bytes (1 element)
    int   byteOffset     = 0;    // [ESP+0x18] ?X bytes copied so far
    int   charIdx        = 0;    // [ESP+0x1C] / ESI ?X current char index
    int   maxLineWidth   = 0;    // [ESP+0x20] ?X widest line so far
    int   lineCount      = 1;    // [ESP+0x24] ?X number of lines
    int   lastSpaceIdx   = 0;    // [ESP+0x28] / EDI ?X most recent space index
    int   breakPos       = 0;    // [ESP+0x2C] ?X temporary break position
    int   maxLineHeight  = 0;    // [ESP+0x30] ?X tallest glyph so far
    int   currentLineWidth = 0;  // EBX ?X current line's accumulated width

    // ?w?w Get box max width via vftable+0x48 ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    tGetMaxWidth fnGetMaxWidth = (tGetMaxWidth)((void***)this)[0][0x48 / 4];
    int boxMaxWidth = fnGetMaxWidth(this);   // [ESP+0x34]

    const char* szText = this->m_pszText;    // [EBP+0x128]

    // ?w?w Initialize object fields ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    this->m_nTotalHeight = 0;    // [EBP+0x148]
    this->m_nMaxLineWidth = 0;   // [EBP+0x144]

    // ?w?w Main loop ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    if (szText[0] != '\0')
    {
        do
        {
            char c = szText[charIdx];

            // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
            // Case 1: Newline character ?? force line break
            // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
            if (c == '\n')
            {
                // Append (charIdx - 1) to lineEndIndices array
                this->m_pLineEndIndices = AppendToIntArray(
                    this->m_pLineEndIndices, byteOffset, charIdx - 1);

                // Append currentLineWidth to lineWidths array
                this->m_pLineWidths = AppendToIntArray(
                    this->m_pLineWidths, byteOffset, currentLineWidth);

                // Update max line width
                if (maxLineWidth < currentLineWidth)
                    maxLineWidth = currentLineWidth;

                // Reset for next line
                currentLineWidth = 0;
                arrayByteSize += 4;
                byteOffset    += 4;
                lineCount++;
                charIdx++;
            }
            // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
            // Case 2: Normal character
            // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
            else
            {
                // Track last space position for word wrapping.
                // Condition: current char is NOT space, AND
                //   (charIdx == 0 OR previous char IS space)
                if (isspace_00445480(c) == 0 &&
                    (charIdx == 0 ||
                     isspace_00445480(szText[charIdx - 1]) != 0))
                {
                    lastSpaceIdx = charIdx;
                }

                // Get character height, track max
                int charH = this->m_pFont->vtable->GetCharHeight(
                    this->m_pFont, (int)c);
                if (maxLineHeight < charH)
                    maxLineHeight = charH;

                // Get character width, accumulate
                int charW = this->m_pFont->vtable->GetCharWidth(
                    this->m_pFont, (int)c);
                currentLineWidth += charW;

                // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
                // Word wrap: current line exceeds box max width
                // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
                if (currentLineWidth > boxMaxWidth)
                {
                    // breakPos starts at current charIdx
                    breakPos = charIdx;

                    // ?w?w Step 1: Backtrack to last space ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
                    // Subtract widths from charIdx back to lastSpaceIdx
                    if (charIdx >= lastSpaceIdx)
                    {
                        do
                        {
                            int w = this->m_pFont->vtable->GetCharWidth(
                                this->m_pFont, (int)szText[charIdx]);
                            currentLineWidth -= w;
                            charIdx--;
                        } while (charIdx >= lastSpaceIdx);
                        breakPos = charIdx;
                    }

                    // ?w?w Step 2: Skip trailing spaces ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
                    // Subtract widths of any remaining space characters
                    if (isspace_00445480((int)szText[charIdx]) != 0)
                    {
                        do
                        {
                            int w = this->m_pFont->vtable->GetCharWidth(
                                this->m_pFont, (int)szText[charIdx]);
                            currentLineWidth -= w;
                            charIdx--;
                        } while (isspace_00445480((int)szText[charIdx]) != 0);
                        breakPos = charIdx;
                    }

                    // Update max line width
                    if (maxLineWidth < currentLineWidth)
                        maxLineWidth = currentLineWidth;

                    // Append breakPos to lineEndIndices array
                    this->m_pLineEndIndices = AppendToIntArray(
                        this->m_pLineEndIndices, byteOffset, breakPos);

                    // Append currentLineWidth to lineWidths array
                    this->m_pLineWidths = AppendToIntArray(
                        this->m_pLineWidths, byteOffset, currentLineWidth);

                    // Reset for next line
                    currentLineWidth = 0;
                    lineCount++;
                    arrayByteSize += 4;
                    byteOffset    += 4;

                    // Resume from last space position
                    charIdx = lastSpaceIdx;
                }
                // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
                // Line fits within box ?? advance to next char
                // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
                else
                {
                    charIdx++;
                }
            }
        } while (szText[charIdx] != '\0');

        // Update max line width with final line
        if (maxLineWidth < currentLineWidth)
            maxLineWidth = currentLineWidth;
    }

    // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    // Final array allocation: lineCount elements
    // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    int finalByteSize = lineCount * 4;

    // Allocate final lineEndIndices array
    int* pFinalEnds = (int*)operator_new(finalByteSize);
    if (this->m_pLineEndIndices)
    {
        // Copy only lineCount-1 elements (the last one is overwritten below)
        memcpy(pFinalEnds, this->m_pLineEndIndices, finalByteSize - 4);
        operator_delete(this->m_pLineEndIndices);
    }

    // Final line end = strlen(szText)
    // (Note: the assembly uses SCASB to compute strlen, then stores it directly.
    //  This is an exclusive end index �X one past the last character.)
    pFinalEnds[lineCount - 1] = (int)strlen(szText);

    // Store results
    this->m_pLineEndIndices = pFinalEnds;
    this->m_nMaxLineWidth   = maxLineWidth;
    this->m_nLineHeight     = maxLineHeight;   // [EBP+0x14C]
    this->m_nTotalHeight    = lineCount * maxLineHeight;  // [EBP+0x148]
    this->m_nLineCount      = lineCount;

    // Allocate final lineWidths array
    int* pFinalWidths = (int*)operator_new(finalByteSize);
    if (this->m_pLineWidths)
    {
        memcpy(pFinalWidths, this->m_pLineWidths, finalByteSize - 4);
        operator_delete(this->m_pLineWidths);
    }

    // Final line width = currentLineWidth
    pFinalWidths[lineCount - 1] = currentLineWidth;
    this->m_pLineWidths = pFinalWidths;

    // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    // If m_bUseBoxWidth == 1, override max width with GetMaxWidth()
    // ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    if (this->m_bUseBoxWidth == 1)
    {
        this->m_nMaxLineWidth = fnGetMaxWidth(this);
    }

    // ?w?w Add +2 padding if positive ?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w?w
    if (this->m_nMaxLineWidth > 0)
        this->m_nMaxLineWidth += 2;

    if (this->m_nTotalHeight > 0)
        this->m_nTotalHeight += 2;

    return TRUE;
}