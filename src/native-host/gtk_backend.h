#ifndef GTK_BACKEND_H
#define GTK_BACKEND_H

#include <gtk/gtk.h>
#include "sp.h"
#include "backend.h"

backend_t gtk_backend_make(sp_mem_t mem, GtkWindow* window);

#endif
