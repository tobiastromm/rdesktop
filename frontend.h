/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_FRONTEND_H
#define RDESKTOP_FRONTEND_H

#include "config.h"

/* Shared configuration/session entry used by the independently linked
   command-line and Win32 GUI frontends. */
int rdesktop_frontend_run(int argc, char **argv, const RDESKTOP_CONFIG *provided_config);
void rdesktop_frontend_prepare_session(void);
void rdesktop_frontend_cancel_session(void);
void rdesktop_frontend_mark_activated(void);
RD_BOOL rdesktop_frontend_was_activated(void);

#endif
/* SPDX-License-Identifier: GPL-3.0-or-later */
