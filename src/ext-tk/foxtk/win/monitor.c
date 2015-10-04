#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"


static RefGuiHash timer_entry;

static void CALLBACK timeout_callback(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
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
        }
    } else {
        KillTimer(NULL, id);
    }
}

void native_timer_new(int millisec, Ref *r)
{
    UINT_PTR id = SetTimer(NULL, 0, millisec, timeout_callback);
    Value *v_tmp = GuiHash_add_p(&timer_entry, (const void*)id);

    *v_tmp = fs->Value_cp(vp_Value(r));

    r->v[INDEX_TIMER_ID] = int32_Value(id);
    r->rh.nref++;
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
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct FileMonitor {
    OVERLAPPED overlapped;

    Ref *ref;
    HANDLE hDir;
    int valid;

    char buffer[16 * 1024];
} FileMonitor;

static int refresh_dirmonitor(FileMonitor *fm, int is_clear);

static Value wstr_to_file(RefStr *base_dir, const wchar_t *wp, int size)
{
    int fname_len = utf16_to_utf8(NULL, wp, size);
    int basedir_last_sep = (base_dir->c[base_dir->size - 1] == SEP_C);
    int total_len = base_dir->size + (basedir_last_sep ? 0 : 1) + fname_len;
    RefStr *rs_name = fs->refstr_new_n(fs->cls_file, total_len);
    char *dst = rs_name->c;

    memcpy(dst, base_dir->c, base_dir->size);
    dst += base_dir->size;
    if (!basedir_last_sep) {
        *dst++ = SEP_C;
    }
    utf16_to_utf8(dst, wp, size);

    return vp_Value(rs_name);
}

static void CALLBACK file_monitor_callback(DWORD errorCode, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    // overlappedがFileMonitorの先頭にあることが前提
    FileMonitor *fm = (FileMonitor*)overlapped;

    if (bytesTransfered == 0) {
        return;
    }
    if (errorCode == ERROR_SUCCESS) {
        Ref *sender_r = fm->ref;
        size_t offset = 0;
        FILE_NOTIFY_INFORMATION *notify = (FILE_NOTIFY_INFORMATION*)&fm->buffer[offset];
        Value v_other = VALUE_NULL;

        do {
            Value v_tmp;
            Value *evt;
            int ret_code;
            const char *action_val = NULL;
            notify = (FILE_NOTIFY_INFORMATION*)&fm->buffer[offset];
            offset += notify->NextEntryOffset;

            switch (notify->Action) {
            case FILE_ACTION_ADDED:
            case FILE_ACTION_RENAMED_NEW_NAME:
                action_val = "created";
                break;
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
                action_val = "deleted";
                break;
            case FILE_ACTION_MODIFIED:
                action_val = "changed";
                break;
            default:
                goto CONTINUE;
            }

            fs->Value_push("v", sender_r->v[INDEX_FILEMONITOR_FN]);
            evt = fg->stk_top++;
            *evt = event_object_new(sender_r);

            v_tmp = fs->cstr_Value(fs->cls_str, action_val, -1);
            event_object_add(*evt, "action", v_tmp);
            fs->Value_dec(v_tmp);

            v_tmp = wstr_to_file(Value_vp(sender_r->v[INDEX_FILEMONITOR_FILE]), notify->FileName, notify->FileNameLength / sizeof(wchar_t));
            event_object_add(*evt, "file", v_tmp);
            fs->Value_dec(v_tmp);

            ret_code = fs->call_function_obj(1);
            if (!ret_code && fg->error != VALUE_NULL) {
                //
            }
            fs->Value_pop();

        CONTINUE:
            ;
        } while (notify->NextEntryOffset != 0);
        fs->Value_dec(v_other);
    }
    if (fm->valid) {
        refresh_dirmonitor(fm, FALSE);
    }
}
static int refresh_dirmonitor(FileMonitor *fm, int is_clear)
{
    enum {
        NOTIFY_FILTER = FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
    };

    return ReadDirectoryChangesW(
        fm->hDir, fm->buffer, sizeof(fm->buffer), FALSE,
        NOTIFY_FILTER, NULL, &fm->overlapped, is_clear ? NULL : file_monitor_callback) != 0;
}

static FileMonitor *FileMonitor_new(void)
{
    FileMonitor *fm = malloc(sizeof(FileMonitor));
    memset(fm, 0, sizeof(*fm));
    return fm;
}
static void FileMonitor_del(FileMonitor *fm)
{
    free(fm);
}
int native_dirmonitor_new(Ref *r, const char *path)
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
    fm->ref = r;
    fm->overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    fm->valid = TRUE;

    if (!refresh_dirmonitor(fm, FALSE)) {
        CloseHandle(fm->overlapped.hEvent);
        CloseHandle(fm->hDir);
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        return FALSE;
    }

    r->v[INDEX_FILEMONITOR_STRUCT] = ptr_Value(fm);

    return TRUE;
}
void native_dirmonitor_remove(Ref *r)
{
    FileMonitor *fm = Value_ptr(r->v[INDEX_FILEMONITOR_STRUCT]);

    if (fm != NULL) {
        fm->valid = FALSE;
        CancelIo(fm->hDir);
        refresh_dirmonitor(fm, TRUE);

        if (!HasOverlappedIoCompleted(&fm->overlapped)) {
            SleepEx(5, TRUE);
        }
        CloseHandle(fm->hDir);
        CloseHandle(fm->overlapped.hEvent);

        FileMonitor_del(fm);
        r->v[INDEX_FILEMONITOR_STRUCT] = VALUE_NULL;
    }
}
