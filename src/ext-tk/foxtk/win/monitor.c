#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"


static RefGuiHash timer_entry;

static void CALLBACK TimeoutCallback(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    Value *sender_v = GuiHash_get_p(&timer_entry, (const void*)id);

    if (sender_v != NULL) {
        Ref *sender_r = Value_ref(*sender_v);
        Value fn = sender_r->v[INDEX_TIMER_FN];
        Value *stk_top = fg->stk_top;
        int ret_code;
        int result = TRUE;

        fs->Value_push("v", fn);
        *fg->stk_top++ = event_object_new(sender_r);

        ret_code = fs->call_function_obj(1);
        if (fg->error != VALUE_NULL) {
            PostQuitMessage(0);
        }
        if (ret_code) {
            Value vr = fg->stk_top[-1];
            if (vr == VALUE_FALSE) {
                result = FALSE;
            }
        } else {
            result = FALSE;
        }
        // エラーの場合、stk_topが不定
        while (fg->stk_top > stk_top) {
            fs->Value_pop();
        }
        if (!result) {
            sender_r->v[INDEX_TIMER_ID] = VALUE_NULL;

            // カウンタを1減らす
            fs->Value_dec(vp_Value(sender_r));
            KillTimer(NULL, id);

            root_window_count--;
            if (root_window_count <= 0) {
                PostQuitMessage(0);
            }
        }
    } else {
        KillTimer(NULL, id);
    }
}

void native_timer_new(int millisec, Ref *r)
{
    UINT_PTR id = SetTimer(NULL, 0, millisec, TimeoutCallback);
    Value *v_tmp = GuiHash_add_p(&timer_entry, (const void*)id);

    *v_tmp = fs->Value_cp(vp_Value(r));

    r->v[INDEX_TIMER_ID] = int32_Value(id);
    r->rh.nref++;
    // イベントループ監視対象なので1増やす
    root_window_count++;
}
void native_timer_remove(Ref *r)
{
    if (r->v[INDEX_TIMER_ID] != VALUE_NULL) {
        // TODO: INDEX_TIMER_IDは64bit必要
        uintptr_t timer_id = Value_integral(r->v[INDEX_TIMER_ID]);

        KillTimer(NULL, timer_id);
        // カウンタを減らす
        GuiHash_del_p(&timer_entry,(const void*)timer_id);
        r->v[INDEX_TIMER_ID] = VALUE_NULL;

        root_window_count--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct FileMonitor {
    struct FileMonitor *prev;
    struct FileMonitor *next;

    OVERLAPPED overlapped;
    HANDLE hDir;
    int valid;
    
    char buffer[16 * 1024];
} FileMonitor;

enum {
    NOTIFY_FILTER = FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_FILE_NAME,
};

static FileMonitor *s_monitor_head;
static FileMonitor *s_monitor_tail;

static int refresh_filemonitor(FileMonitor *fm, int is_clear);

static void CALLBACK FileMonitorCallback(DWORD errorCode, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    FileMonitor *fm = (FileMonitor*)overlapped;
    size_t offset = 0;

    if (bytesTransfered == 0) {
        return;
    }

    if (errorCode == ERROR_SUCCESS) {
        FILE_NOTIFY_INFORMATION *notify;
        do {
            char *fname;
            int fname_len;
            notify = (FILE_NOTIFY_INFORMATION*)&fm->buffer[offset];
            offset += notify->NextEntryOffset;

            fname_len = WideCharToMultiByte(CP_UTF8, 0, notify->FileName, notify->FileNameLength, NULL, 0, NULL, NULL);
            fname = malloc(fname_len + 1);
            WideCharToMultiByte(CP_UTF8, 0, notify->FileName, notify->FileNameLength, fname, fname_len + 1, NULL, NULL);
            fname[fname_len] = '\0';

            // ...
        } while (notify->NextEntryOffset != 0);
    }
    if (fm->valid) {
        refresh_filemonitor(fm, FALSE);
    }
}
static int refresh_filemonitor(FileMonitor *fm, int is_clear)
{
    return ReadDirectoryChangesW(
        fm->hDir, fm->buffer, sizeof(fm->buffer), FALSE,
        NOTIFY_FILTER, NULL, &fm->overlapped, is_clear ? NULL : FileMonitorCallback) != 0;
}

static FileMonitor *FileMonitor_new(void)
{
    FileMonitor *fm = malloc(sizeof(FileMonitor));
    memset(fm, 0, sizeof(*fm));

    if (s_monitor_head == NULL) {
        s_monitor_head = fm;
    }
    if (s_monitor_tail == NULL) {
        s_monitor_tail = fm;
    } else {
        fm->prev = s_monitor_tail;
        s_monitor_tail->next = fm;
    }
    return fm;
}
static void FileMonitor_del(FileMonitor *fm)
{
    if (fm->prev != NULL) {
        fm->prev->next = fm->next;
    }
    if (fm->next != NULL) {
        fm->next->prev = fm->prev;
    }
    free(fm);
}
int native_filemonitor_new(Ref *r, const char *path)
{
    FileMonitor *fm = FileMonitor_new();

    {
        wchar_t *wpath = filename_to_utf16(path, NULL);
        fm->hDir = CreateFileW(wpath, FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, 
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
        free(wpath);
    }

    if (fm->hDir == INVALID_HANDLE_VALUE) {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        free(fm);
        return FALSE;
    }
    fm->overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    fm->valid = TRUE;

    if (!refresh_filemonitor(fm, FALSE)) {
        CloseHandle(fm->overlapped.hEvent);
        CloseHandle(fm->hDir);
        return FALSE;
    }

    r->v[INDEX_FILEMONITOR_STRUCT] = ptr_Value(fm);

    return TRUE;
}
void native_filemonitor_remove(Ref *r)
{
    FileMonitor *fm = Value_ptr(r->v[INDEX_FILEMONITOR_STRUCT]);

    fm->valid = FALSE;
    CancelIo(fm->hDir);
    refresh_filemonitor(fm, TRUE);

    if (!HasOverlappedIoCompleted(&fm->overlapped)) {
        SleepEx(5, TRUE);
    }
    CloseHandle(fm->hDir);
    CloseHandle(fm->overlapped.hEvent);

    FileMonitor_del(fm);
}

void native_object_remove_all()
{
}
