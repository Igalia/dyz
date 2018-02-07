#include "wpe-spi/WPE.loader_interface.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static void* s_relay_library = 0;
static struct wpe_loader_interface* s_relay_loader = 0;

void* relay_load_object(const char* object_name)
{
    if (!s_relay_library) {
        s_relay_library = dlopen("libWPEBackend-fdo.so", RTLD_NOW);
        if (s_relay_library)
            s_relay_loader = dlsym(s_relay_library, "_wpe_loader_interface");
    }

    if (!s_relay_library || !s_relay_loader) {
        fprintf(stderr, "libWPEBackend-dyz: could not load libWPEBackend-fdo\n");
        abort();
    }

    return s_relay_loader->load_object(object_name);
}

__attribute__((visibility("default")))
struct wpe_loader_interface _wpe_loader_interface = {
    relay_load_object,
};
