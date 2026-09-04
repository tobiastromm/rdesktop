#define main rdesktop_program_main
#include "../rdesktop.h"
#undef main
#include "../win32_i18n.h"
#include <wchar.h>

static int write_catalog(const char *directory,const char *locale,const char *text)
{
	char path[MAX_PATH*2]; FILE *file;
	snprintf(path,sizeof(path),"%s\\%s.lang",directory,locale);
	file=fopen(path,"wb"); if(!file)return 0;
	fwrite(text,1,strlen(text),file); fclose(file); return 1;
}

int main(void)
{
	char root[MAX_PATH],directory[MAX_PATH*2];
	if(!GetTempPathA(sizeof(root),root))return 1;
	snprintf(directory,sizeof(directory),"%srdesktop-i18n-%lu",root,(unsigned long)GetCurrentProcessId());
	if(!CreateDirectoryA(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 2;
	if(!write_catalog(directory,"en-US","label.server=Base Server\nbutton.connect=Connect\n"))return 3;
	if(!write_catalog(directory,"pt-BR","app.subtitle=Configurações\napp.subtitle=Conexão\n"))return 4;
	if(!win32_i18n_load_from_directory("pt-BR",directory))return 5;
	if(strcmp(win32_i18n_get("app.subtitle"),"Conexão")||strcmp(win32_i18n_get("label.server"),"Base Server"))return 6;
	if(wcscmp(win32_i18n_get_w("app.subtitle"),L"Conexão"))return 7;
	if(win32_i18n_load_from_directory("missing",directory)||strcmp(win32_i18n_locale(),"en-US"))return 8;
	if(!write_catalog(directory,"broken","malformed line\n"))return 9;
	if(win32_i18n_load_from_directory("broken",directory)||strcmp(win32_i18n_get("label.server"),"Base Server"))return 10;
	{char path[MAX_PATH*3];snprintf(path,sizeof(path),"%s\\en-US.lang",directory);DeleteFileA(path);snprintf(path,sizeof(path),"%s\\pt-BR.lang",directory);DeleteFileA(path);snprintf(path,sizeof(path),"%s\\broken.lang",directory);DeleteFileA(path);}RemoveDirectoryA(directory);
	puts("Win32 localization catalog tests: PASS");return 0;
}
