#ifndef GTK_BACKEND_H
#define GTK_BACKEND_H

#include <gtk/gtk.h>
#include "sp.h"
#include "spry/backend/backend.h"

typedef sp_str_t (*gtk_request_fn_t)(void* ctx, sp_str_t action, sp_str_t body);

backend_t gtk_backend_make(sp_mem_t mem, GtkWindow* window, host_iface_t host, gtk_request_fn_t resolver, void* resolver_ctx);

#endif
