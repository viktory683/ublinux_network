#include <gtk/gtk.h>
#include <stdio.h>

/**
 * @todo update all UI based on device info
 */
void deviceBoxHandler(GtkComboBoxText* box, gpointer user_data) {
    char* choosen = gtk_combo_box_text_get_active_text(box);
    printf("choosen device: '%s'\n", choosen);
}
