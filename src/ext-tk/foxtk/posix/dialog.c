#include "gui_compat.h"
#include "gui.h"
#include <string.h>
#include <stdlib.h>



void alert_dialog(RefStr *msg, RefStr *title, WndHandle parent)
{
    GtkWidget *dialog;

    if (title != NULL) {
        RefStr *tmp = title;
        title = msg;
        msg = tmp;
    }
    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
            parent != NULL ? GTK_DIALOG_DESTROY_WITH_PARENT : GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "%.*s", msg->size, msg->c);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

    if (title != NULL) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%.*s", title->size, title->c);
    }
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int confirm_dialog(RefStr *msg, RefStr *title, WndHandle parent)
{
    int ret = FALSE;
    GtkWidget *dialog;

    if (title != NULL) {
        RefStr *tmp = title;
        title = msg;
        msg = tmp;
    }

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
            parent != NULL ? GTK_DIALOG_DESTROY_WITH_PARENT : GTK_DIALOG_MODAL,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_YES_NO,
            "%.*s", msg->size, msg->c);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

    if (title != NULL) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%.*s", title->size, title->c);
    }
    ret = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES);
    gtk_widget_destroy(dialog);

    return ret;
}

/**
 * *.bmp;*.jpg -> "*.bmp;*.jpg", "*.bmp;*.jpg"
 * Image file:*.bmp;*.jpg -> "Image file", "*.bmp;*.jpg"
 */
static void split_filter_name(Str *name, Str *filter, Str src)
{
    int i;

    for (i = src.size - 1; i >= 0; i--) {
        if (src.p[i] == ':') {
            name->p = src.p;
            name->size = i;
            filter->p = src.p + i + 1;
            filter->size = src.size - i - 1;
            return;
        }
    }
    *name = src;
    *filter = src;
}

static void file_open_dialog_add_filter(GtkFileChooser *dlg, Str name, Str filter)
{
    GtkFileFilter *fr = gtk_file_filter_new();
    char *name_p = fs->str_dup_p(name.p, name.size, NULL);
    const char *p = filter.p;
    const char *end = filter.p + filter.size;

    gtk_file_filter_set_name(fr, name_p);
    free(name_p);

    while (p < end) {
        const char *top = p;
        while (p < end && *p != ';') {
            p++;
        }
        if (p > top) {
            char *filter_p = fs->str_dup_p(top, p - top, NULL);
            gtk_file_filter_add_pattern(fr, filter_p);
            free(filter_p);
        }
        if (p < end) {
            // ;
            p++;
        }
    }

    gtk_file_chooser_add_filter(dlg, fr);
}

int file_open_dialog(Value *vret, Str title, RefArray *filter, WndHandle parent, int type)
{
    int ret = FALSE;
    char *title_p = fs->str_dup_p(title.p, title.size, NULL);
    GtkWidget *dialog;
    int type_id;
    const char *btn_id = "document-open";

    switch (type) {
    case FILEOPEN_OPEN:
    case FILEOPEN_OPEN_MULTI:
        type_id = GTK_FILE_CHOOSER_ACTION_OPEN;
        break;
    case FILEOPEN_SAVE:
        type_id = GTK_FILE_CHOOSER_ACTION_SAVE;
        btn_id = "document-save";
        break;
    case FILEOPEN_DIR:
        type_id = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        break;
    }

    dialog = gtk_file_chooser_dialog_new(title_p,
            GTK_WINDOW(parent),
            type_id,
            "Cancel", GTK_RESPONSE_CANCEL,
            btn_id, GTK_RESPONSE_ACCEPT,
            NULL);

    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    if (type == FILEOPEN_OPEN_MULTI) {
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    }
    if (type == FILEOPEN_SAVE) {
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    }

    if (filter != NULL) {
        int i;
        for (i = 0; i < filter->size; i++) {
            Str name, ft;
            split_filter_name(&name, &ft, fs->Value_str(filter->p[i]));
            file_open_dialog_add_filter(GTK_FILE_CHOOSER(dialog), name, ft);
        }
    }

    ret = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT);
    if (ret) {
        if (type == FILEOPEN_OPEN_MULTI) {
            GSList *slist = gtk_file_chooser_get_uris(GTK_FILE_CHOOSER(dialog));
            GSList *list = slist;
            RefArray *aret = fs->refarray_new(0);
            *vret = vp_Value(aret);
            while (list != NULL) {
                char *uri = list->data;
                char *filename = g_filename_from_uri(uri, NULL, NULL);
                if (filename != NULL) {
                    Value *vf = fs->refarray_push(aret);
                    *vf = fs->cstr_Value(fs->cls_file, filename, -1);
                    g_free(filename);
                }
                g_free(uri);
                list = list->next;
            }
            g_slist_free(slist);
        } else {
            char *uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
            char *filename = g_filename_from_uri(uri, NULL, NULL);
            if (filename != NULL) {
                *vret = fs->cstr_Value(fs->cls_file, filename, -1);
                g_free(filename);
            }
            g_free(uri);
        }
    }
    gtk_widget_destroy(dialog);
    free(title_p);

    return ret;
}

