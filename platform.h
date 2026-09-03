/* -*- c-basic-offset: 8 -*- */
#ifndef RDESKTOP_PLATFORM_H
#define RDESKTOP_PLATFORM_H

#include <stddef.h>
#include "types.h"

RD_BOOL platform_get_local_username(char *buffer, size_t size);
RD_BOOL platform_get_local_hostname(char *buffer, size_t size);
const char *platform_get_home_directory(void);
RD_BOOL platform_read_secret(const char *prompt, char *buffer, size_t size);
RD_BOOL platform_random_bytes(uint8 *buffer, size_t size);
RD_BOOL platform_lock_file(int fd, int start, int length);
void platform_ignore_sigpipe(void);
void platform_sleep_seconds(unsigned int seconds);

#endif
