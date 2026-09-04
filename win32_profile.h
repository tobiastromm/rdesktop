/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDESKTOP_WIN32_PROFILE_H
#define RDESKTOP_WIN32_PROFILE_H
#include "config.h"
#define WIN32_PROFILE_SCHEMA_VERSION 2
#define WIN32_PROFILE_MAX 5
typedef struct {
	uint32 version; char server[256],username[256],domain[256]; uint16 port;
	uint32 width,height; window_size_type_t geometry_type; int x,y,position,dpi;
	RD_BOOL geometry_set; RDESKTOP_WINDOW_MODE window_mode; int depth; RDP_VERSION protocol;
	char keymap[PATH_MAX]; RD_BOOL numlock_sync,clipboard_enabled,show_connection_bar;
	RDESKTOP_AUDIO_MODE audio_mode; char audio_driver_options[256]; uint32 performance_flags;
	RD_BOOL send_motion,compression,bitmap_cache,console_session,retry_on_network_error;
	char tls_version[4],hostname[16],codepage[16],shell[256],directory[256],title[64];
} WIN32_PROFILE;
RD_BOOL win32_profile_from_config(WIN32_PROFILE*,const RDESKTOP_CONFIG*);
RD_BOOL win32_profile_apply(const WIN32_PROFILE*,RDESKTOP_CONFIG*);
RD_BOOL win32_profile_valid(const WIN32_PROFILE*);
void win32_profile_promote(WIN32_PROFILE[WIN32_PROFILE_MAX],const WIN32_PROFILE*);
RD_BOOL win32_profile_should_commit(RD_BOOL session_activated);
#endif
