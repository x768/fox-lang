#include "c_posix.c"
#include "c_posix_cl_gui.c"
#include <gtk/gtk.h>

static void messagebox_init()
{
    int argc = 1;
    char *argv_v[] = {(char*)"fox", NULL};
    char **argv = argv_v;
    gtk_init(&argc, &argv);
}
void show_error_message(const char *msg, int msg_size, int warn)
{
    static int mb_init = FALSE;
    GtkWidget *dialog;

    if (!mb_init) {
        messagebox_init();
        mb_init = TRUE;
    }
    if (msg_size < 0) {
        msg_size = strlen(msg);
    }

    dialog = gtk_message_dialog_new(
            NULL,
            GTK_DIALOG_MODAL,
            warn ? GTK_MESSAGE_WARNING : GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "%.*s", msg_size, msg);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int main(int argc, char **argv, char **envp)
{
    init_fox_vm(RUNNING_MODE_GUI);
    init_env(envp);
    return main_fox(argc, (const char**)argv) ? 0 : 1;
}

