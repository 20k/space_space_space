#ifndef FILE_EDITOR_HPP_INCLUDED
#define FILE_EDITOR_HPP_INCLUDED

#include <stdint.h>
#include <imgui/imgui.h>

struct cpu_file;

struct file_editor
{
    // Settings
    bool            Open;                                   // = true   // set to false when DrawWindow() was closed. ignore if not using DrawWindow().
    bool            ReadOnly;                               // = false  // disable any editing.
    int             Cols;                                   // = 16     // number of columns to display.
    bool            OptShowOptions;                         // = true   // display options button/context menu. when disabled, options will be locked unless you provide your own UI for them.
    bool            OptShowDataPreview;                     // = false  // display a footer previewing the decimal/binary/hex/float representation of the currently selected bytes.
    bool            OptShowHexII;                           // = false  // display values in HexII representation instead of regular hexadecimal: hide null/zero bytes, ascii values as ".X".
    bool            OptShowAscii;                           // = true   // display ASCII representation on the right side.
    bool            OptGreyOutZeroes;                       // = true   // display null/zero bytes using the TextDisabled color.
    bool            OptUpperCaseHex;                        // = true   // display hexadecimal values as "FF" instead of "ff".
    int             OptMidColsCount;                        // = 8      // set to 0 to disable extra spacing between every mid-cols.
    int             OptAddrDigitsCount;                     // = 0      // number of addr digits to display (default calculated based on maximum displayed addr).
    unsigned int    HighlightColor;                         //          // background color of highlighted bytes.

    // [Internal State]
    bool            ContentsWidthChanged;
    size_t          DataPreviewAddr;
    size_t          DataEditingAddr;
    bool            DataEditingTakeFocus;
    char            DataInputBuf[32];
    char            AddrInputBuf[32];
    size_t          GotoAddr;
    size_t          HighlightMin, HighlightMax;
    int             PreviewEndianess;

    int             DataRenderWidth = 5;

    file_editor()
    {
        // Settings
        Open = true;
        ReadOnly = false;
        Cols = 16;
        OptShowOptions = true;
        OptShowDataPreview = false;
        OptShowHexII = false;
        OptShowAscii = true;
        OptGreyOutZeroes = true;
        OptUpperCaseHex = true;
        OptMidColsCount = 8;
        OptAddrDigitsCount = 0;
        HighlightColor = IM_COL32(255, 255, 255, 50);

        // State/Internals
        ContentsWidthChanged = false;
        DataPreviewAddr = DataEditingAddr = (size_t)-1;
        DataEditingTakeFocus = false;
        memset(DataInputBuf, 0, sizeof(DataInputBuf));
        memset(AddrInputBuf, 0, sizeof(AddrInputBuf));
        GotoAddr = (size_t)-1;
        HighlightMin = HighlightMax = (size_t)-1;
        PreviewEndianess = 0;
    }


    void render(cpu_file& fle);
};

#endif // FILE_EDITOR_HPP_INCLUDED
