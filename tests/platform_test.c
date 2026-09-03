#define main rdesktop_program_main
#include "../rdesktop.h"
#undef main
#include "../platform.h"

int
main(void)
{
	char username[256];
	char hostname[256];
	uint8 random[32];
	size_t index;
	RD_BOOL differs = False;

	if (platform_get_local_username(NULL, 0))
		return 1;
	if (platform_get_local_hostname(NULL, 0))
		return 2;
	if (!platform_get_local_username(username, sizeof(username)) || username[0] == '\0')
		return 3;
	if (!platform_get_local_hostname(hostname, sizeof(hostname)) || hostname[0] == '\0')
		return 4;
	if (platform_get_home_directory() == NULL || platform_get_home_directory()[0] == '\0')
		return 7;
	if (!platform_random_bytes(random, sizeof(random)))
		return 5;
	for (index = 1; index < sizeof(random); index++)
		if (random[index] != random[0])
			differs = True;
	if (!differs)
		return 6;
	return 0;
}
