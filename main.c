#include <stdio.h>
#include <gtk/gtk.h>

#define UI_FILE "design.glade"

GtkBuilder *builder;
GtkWidget *topWindow;
GtkComboBox *deviceBox;

enum
{
    COLUMN_ID,
    COLUMN_LABEL,
    NUM_COLUMNS
};

// void onExit(GtkWidget *widget)
// {
//     // TODO test destroying top window
//     printf("! onExit called");
//     gtk_main_quit();
// }

// G_MODULE_EXPORT void onBtnClicked(GtkButton * btn, gpointer data) {
//     gtk_button_set_label(GTK_BUTTON(btn), "Woof");
// }

void deviceBoxHandler(GtkComboBox *box, gpointer user_data)
{
    printf("deviceBoxHandler called");
}

void updateComboBoxItems(GtkComboBox *box, int itemc, char *items[])
{
    GtkTreeModel *model = gtk_combo_box_get_active(box);

    gtk_list_store_clear(GTK_LIST_STORE(model));

    for (int i = 0; i < itemc; i++)
    {
        printf("adding: %s\n", items[i]);
        gtk_list_store_insert_with_values(GTK_LIST_STORE(model), NULL, -1, COLUMN_ID, items[i], COLUMN_LABEL, "label", -1);
    }

    gtk_combo_box_set_active(box, 0);
}

int main(int argc, char *argv[])
{
    GError *err = NULL;

    // GTK+ initialization
    gtk_init(&argc, &argv);

    builder = gtk_builder_new();

    // * UI file should be placed to the same directory as the application
    if (!gtk_builder_add_from_file(builder, UI_FILE, &err))
    {
        g_warning(err->message);
        g_free(err);
        return 1;
    }

    deviceBox = GTK_COMBO_BOX(gtk_builder_get_object(builder, "deviceBox"));

    // TODO scan for devices from nmcli and pass it as arguments to function
    char *items[] = {"abc", "bcd"};
    updateComboBoxItems(deviceBox, 2, items);
    topWindow = GTK_WIDGET(gtk_builder_get_object(builder, "topWindow"));

    g_signal_connect(G_OBJECT(topWindow), "destroy", gtk_main_quit, NULL);
    g_signal_connect(G_OBJECT(deviceBox), "changed", deviceBoxHandler, NULL);

    // gtk_builder_connect_signals(builder, "NULL");

    // freeing up memory
    g_object_unref(G_OBJECT(builder));

    gtk_widget_show(topWindow);

    gtk_main();

    return 0;
}