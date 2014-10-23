#ifndef _M_EDITOR_H_
#define _M_EDITOR_H_

#include "m_gui.h"
#include <stdint.h>


enum {
    CHARST_SEL    = 1,
    CHARST_ACTIVE = 2,
};
enum {
    INDEX_EDITWND_DATA = INDEX_WIDGET_NUM,
    INDEX_EDITWND_NUM,
};

typedef union CharSt
{
    struct {
        uint8_t submode;
        uint8_t bg, col;
        uint8_t flag;
    } s;
    uint32_t val;  // 比較で使用
} CharSt;
typedef union ChState
{
    const char *p;
    int i;
} ChState;

typedef struct BufLine
{
    char *m_p;
    int m_size, m_max;
    int viewY;      // 表示行
    int closeLen;   // 折りたたみの範囲
    ChState cs;
    uint16_t sub;   // モード混在で切り替わり前の状態
    uint16_t state; // 次の行へ続く状態
    uint8_t mode;   // 次の行へ続くmode
    uint8_t flag;
} BufLine;

typedef struct BufUndo
{
    int x, y;
    int len, type;
    union {
        char w[2];
        char *p;
    } u;
} BufUndo;

typedef struct Buffer
{
    int read_only;
    RefCharset *charset;
    int new_line;
    BufLine *line;
    int line_size, line_max;
} Buffer;

typedef struct VisLine
{
    int x, y;             // 行頭の論理位置
    const char *m_top;    // 行頭
    CharSt *m_p;          // 文字幅・色分け・直前の状態・直前の文字
    int m_size, m_max, m_prevSz, m_prevY;
    uint8_t m_endCol, m_prevEndCol;
    uint8_t m_stt, m_prevStt;
    uint8_t m_mode;
} VisLine;

typedef struct EditWnd
{
    Buffer *buf;
    VisLine *line;
    int line_size, line_max;

    // 設定
    int show_linenum;
    int show_ruler;
} EditWnd;

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxtkStatic *ftk;
extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_editor;
extern RefNode *mod_gui;
extern RefNode *mod_image;
extern RefNode *cls_editor;

#ifdef DEFINE_GLOBALS
#define extern
#endif

void create_editor_pane_window(Value *v, WndHandle parent);

#endif /* _M_EDITOR_H_ */
