#ifndef LIBROMM_AMIGA_H
#define LIBROMM_AMIGA_H
#include "libromm.h"
/* AmigaOS 3.x HTTP-only transport using bsdsocket.library.
   Intended for a trusted LAN proxy. HTTPS is deliberately not implemented. */
romm_transport_t romm_amiga_transport(void);
void romm_amiga_transport_shutdown(void);
void romm_amiga_set_debug(int enabled);
#endif
