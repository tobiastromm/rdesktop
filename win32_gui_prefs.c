/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "win32_gui_prefs.h"
#include "win32_profile.h"
#ifdef _WIN32
#define PREFS_KEY "Software\\rdesktop\\Win32 GUI"
static RD_BOOL read_profile(HKEY k,int i,WIN32_PROFILE*p){char n[16];DWORD t=REG_BINARY,s=sizeof(*p);sprintf(n,"Profile%d",i);memset(p,0,sizeof(*p));return RegQueryValueExA(k,n,NULL,&t,(BYTE*)p,&s)==ERROR_SUCCESS&&t==REG_BINARY&&s==sizeof(*p)&&win32_profile_valid(p);}
static void write_profile(HKEY k,int i,const WIN32_PROFILE*p){char n[16];sprintf(n,"Profile%d",i);RegSetValueExA(k,n,0,REG_BINARY,(const BYTE*)p,sizeof(*p));}
void win32_gui_prefs_load_servers(char s[WIN32_GUI_MRU_COUNT][256]){HKEY k;WIN32_PROFILE p;int i;memset(s,0,WIN32_GUI_MRU_COUNT*256);if(RegOpenKeyExA(HKEY_CURRENT_USER,PREFS_KEY,0,KEY_QUERY_VALUE,&k)!=ERROR_SUCCESS)return;for(i=0;i<WIN32_GUI_MRU_COUNT;i++)if(read_profile(k,i,&p))STRNCPY(s[i],p.server,256);RegCloseKey(k);}
RD_BOOL win32_gui_prefs_load_profile(const char*s,RDESKTOP_CONFIG*c){HKEY k;int i;WIN32_PROFILE p;RD_BOOL f=False;if(!s||!*s||RegOpenKeyExA(HKEY_CURRENT_USER,PREFS_KEY,0,KEY_QUERY_VALUE,&k)!=ERROR_SUCCESS)return False;for(i=0;i<WIN32_GUI_MRU_COUNT;i++)if(read_profile(k,i,&p)&&!_stricmp(p.server,s)){f=win32_profile_apply(&p,c);break;}RegCloseKey(k);return f;}
void win32_gui_prefs_remember_profile(const RDESKTOP_CONFIG*c){WIN32_PROFILE list[WIN32_PROFILE_MAX],p;HKEY k;DWORD d;int i;if(!win32_profile_from_config(&p,c))return;memset(list,0,sizeof(list));if(RegCreateKeyExA(HKEY_CURRENT_USER,PREFS_KEY,0,NULL,0,KEY_QUERY_VALUE|KEY_SET_VALUE,NULL,&k,&d)!=ERROR_SUCCESS)return;for(i=0;i<WIN32_PROFILE_MAX;i++)read_profile(k,i,&list[i]);win32_profile_promote(list,&p);for(i=0;i<WIN32_PROFILE_MAX;i++){char n[16];sprintf(n,"Profile%d",i);if(win32_profile_valid(&list[i]))write_profile(k,i,&list[i]);else RegDeleteValueA(k,n);}RegCloseKey(k);}
void win32_gui_prefs_remember_server(const char*s){RDESKTOP_CONFIG c;if(!s||!*s)return;rdesktop_config_init(&c);STRNCPY(c.server,s,sizeof(c.server));win32_gui_prefs_remember_profile(&c);}
void win32_gui_prefs_load_global(char*l,size_t z,RD_BOOL*e){HKEY k;DWORD t=REG_SZ,s=(DWORD)z,d=0,dz=sizeof(d);STRNCPY(l,"en-US",z);*e=False;if(RegOpenKeyExA(HKEY_CURRENT_USER,PREFS_KEY,0,KEY_QUERY_VALUE,&k)!=ERROR_SUCCESS)return;RegQueryValueExA(k,"Language",NULL,&t,(BYTE*)l,&s);l[z-1]=0;if(RegQueryValueExA(k,"Expanded",NULL,&t,(BYTE*)&d,&dz)==ERROR_SUCCESS)*e=d?True:False;RegCloseKey(k);}
void win32_gui_prefs_save_global(const char*l,RD_BOOL e){HKEY k;DWORD d,v=e?1:0;if(RegCreateKeyExA(HKEY_CURRENT_USER,PREFS_KEY,0,NULL,0,KEY_SET_VALUE,NULL,&k,&d)!=ERROR_SUCCESS)return;RegSetValueExA(k,"Language",0,REG_SZ,(const BYTE*)l,(DWORD)strlen(l)+1);RegSetValueExA(k,"Expanded",0,REG_DWORD,(const BYTE*)&v,sizeof(v));RegCloseKey(k);}
#endif
