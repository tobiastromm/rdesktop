#define main rdesktop_program_main
#include "../win32_profile.h"
#undef main
RDP_VERSION g_rdp_version=RDP_V4;char g_hostname[16];int g_tcp_port_rdp;char*g_username;
uint32 g_requested_session_width,g_requested_session_height;window_size_type_t g_window_size_type;
int g_xpos,g_ypos,g_pos,g_dpi,g_server_depth;char g_keymapname[PATH_MAX],g_password[64];
RD_BOOL g_fullscreen,g_show_connection_bar,g_rdpsnd;RDESKTOP_WINDOW_MODE g_window_mode;
void logger(log_subject_t s,log_level_t l,char*f,...){UNUSED(s);UNUSED(l);UNUSED(f);}void*xmalloc(int n){return malloc(n);}void xfree(void*p){free(p);}
static void make(RDESKTOP_CONFIG*c,const char*s){rdesktop_config_init(c);STRNCPY(c->server,s,sizeof(c->server));c->port=3389;c->rdp_version=RDP_V5;c->colour_depth=16;STRNCPY(c->keymap,"en-us",sizeof(c->keymap));}
static int contains(const void*p,size_t n,const char*s){const unsigned char*b=p;size_t z=strlen(s),i;for(i=0;i+z<=n;i++)if(!memcmp(b+i,s,z))return 1;return 0;}
int main(void){RDESKTOP_CONFIG c,out;WIN32_PROFILE p,list[WIN32_PROFILE_MAX];int i;char n[16];memset(list,0,sizeof(list));
	for(i=1;i<=6;i++){sprintf(n,"server%d",i);make(&c,n);if(!win32_profile_from_config(&p,&c))return 1;win32_profile_promote(list,&p);}if(strcmp(list[0].server,"server6")||strcmp(list[4].server,"server2"))return 2;
	make(&c,"SERVER4");rdesktop_config_set_password(&c,"must-not-persist",False);if(!win32_profile_from_config(&p,&c))return 3;win32_profile_promote(list,&p);if(strcmp(list[0].server,"SERVER4")||contains(&p,sizeof(p),"must-not-persist"))return 4;
	if(!win32_profile_apply(&p,&out)||out.password_present||out.password[0])return 5;
	p.port=0;if(win32_profile_valid(&p)||win32_profile_apply(&p,&out))return 6;
	if(win32_profile_should_commit(False)||!win32_profile_should_commit(True))return 7;
	puts("Win32 profile model tests: PASS");return 0;}
