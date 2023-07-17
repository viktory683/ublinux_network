#include <gtk/gtk.h>
#include <stdio.h>

void deviceBoxHandler(GtkComboBoxText* box, gpointer user_data) {
    char* choosen = gtk_combo_box_text_get_active_text(box);
    printf("choosen device: '%s'\n", choosen);
}
