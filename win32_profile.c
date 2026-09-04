/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "win32_profile.h"
static RD_BOOL term(const char*v,size_t n){return memchr(v,'\0',n)!=NULL;}
RD_BOOL win32_profile_valid(const WIN32_PROFILE*p){
	if(!p||p->version!=WIN32_PROFILE_SCHEMA_VERSION||!term(p->server,sizeof(p->server))||!term(p->username,sizeof(p->username))||!term(p->domain,sizeof(p->domain))||!term(p->keymap,sizeof(p->keymap))||!p->server[0]||!p->port)return False;
	if(p->protocol!=RDP_V4&&p->protocol!=RDP_V5)return False;
	if(p->depth!=-1&&p->depth!=8&&p->depth!=15&&p->depth!=16&&p->depth!=24&&p->depth!=32)return False;
	if(p->window_mode<RDESKTOP_WINDOW_CLIENT_SIZE||p->window_mode>RDESKTOP_WINDOW_FULLSCREEN||p->audio_mode<RDESKTOP_AUDIO_DISABLED||p->audio_mode>RDESKTOP_AUDIO_LOCAL)return False;
	if(p->geometry_set&&(!p->width||!p->height))return False;
	return True;
}
RD_BOOL win32_profile_from_config(WIN32_PROFILE*p,const RDESKTOP_CONFIG*c){
	if(!p||!c||!c->server[0])return False;
	memset(p,0,sizeof(*p)); p->version=WIN32_PROFILE_SCHEMA_VERSION;
	STRNCPY(p->server,c->server,sizeof(p->server));STRNCPY(p->username,c->username,sizeof(p->username));STRNCPY(p->domain,c->domain,sizeof(p->domain));p->port=c->port;
	p->width=c->width;p->height=c->height;p->geometry_type=c->geometry_type;p->x=c->x;p->y=c->y;p->position=c->position;p->dpi=c->dpi;p->geometry_set=c->geometry_set;p->window_mode=c->window_mode;p->depth=c->colour_depth;p->protocol=c->rdp_version;
	STRNCPY(p->keymap,c->keymap,sizeof(p->keymap));p->numlock_sync=c->numlock_sync;p->clipboard_enabled=c->clipboard_enabled;p->show_connection_bar=c->show_connection_bar;p->audio_mode=c->audio_mode;STRNCPY(p->audio_driver_options,c->audio_driver_options,sizeof(p->audio_driver_options));
	p->performance_flags=c->performance_flags;p->send_motion=c->send_motion;p->compression=c->compression;p->bitmap_cache=c->bitmap_cache;p->console_session=c->console_session;p->retry_on_network_error=c->retry_on_network_error;
	STRNCPY(p->tls_version,c->tls_version,sizeof(p->tls_version));STRNCPY(p->hostname,c->hostname,sizeof(p->hostname));STRNCPY(p->codepage,c->codepage,sizeof(p->codepage));STRNCPY(p->shell,c->shell,sizeof(p->shell));STRNCPY(p->directory,c->directory,sizeof(p->directory));STRNCPY(p->title,c->title,sizeof(p->title));return win32_profile_valid(p);
}
RD_BOOL win32_profile_apply(const WIN32_PROFILE*p,RDESKTOP_CONFIG*c){
	if(!win32_profile_valid(p)||!c)return False;
	rdesktop_config_init(c);STRNCPY(c->server,p->server,sizeof(c->server));STRNCPY(c->username,p->username,sizeof(c->username));c->username_set=c->username[0]!=0;STRNCPY(c->domain,p->domain,sizeof(c->domain));c->port=p->port;
	c->width=p->width;c->height=p->height;c->geometry_type=p->geometry_type;c->x=p->x;c->y=p->y;c->position=p->position;c->dpi=p->dpi;c->geometry_set=p->geometry_set;c->window_mode=p->window_mode;c->fullscreen=p->window_mode==RDESKTOP_WINDOW_FULLSCREEN;c->colour_depth=p->depth;c->rdp_version=p->protocol;
	STRNCPY(c->keymap,p->keymap,sizeof(c->keymap));c->numlock_sync=p->numlock_sync;c->clipboard_enabled=p->clipboard_enabled;c->show_connection_bar=p->show_connection_bar;c->audio_mode=p->audio_mode;STRNCPY(c->audio_driver_options,p->audio_driver_options,sizeof(c->audio_driver_options));
	c->performance_flags=p->performance_flags;c->send_motion=p->send_motion;c->compression=p->compression;c->bitmap_cache=p->bitmap_cache;c->console_session=p->console_session;c->retry_on_network_error=p->retry_on_network_error;STRNCPY(c->tls_version,p->tls_version,sizeof(c->tls_version));STRNCPY(c->hostname,p->hostname,sizeof(c->hostname));STRNCPY(c->codepage,p->codepage,sizeof(c->codepage));STRNCPY(c->shell,p->shell,sizeof(c->shell));STRNCPY(c->directory,p->directory,sizeof(c->directory));STRNCPY(c->title,p->title,sizeof(c->title));rdesktop_config_clear_password(c);return True;
}
void win32_profile_promote(WIN32_PROFILE list[WIN32_PROFILE_MAX],const WIN32_PROFILE*p){WIN32_PROFILE out[WIN32_PROFILE_MAX];int i,n=1;memset(out,0,sizeof(out));if(!win32_profile_valid(p))return;out[0]=*p;for(i=0;i<WIN32_PROFILE_MAX&&n<WIN32_PROFILE_MAX;i++)if(win32_profile_valid(&list[i])&&_stricmp(list[i].server,p->server))out[n++]=list[i];memcpy(list,out,sizeof(out));}
RD_BOOL win32_profile_should_commit(RD_BOOL session_activated){return session_activated?True:False;}
