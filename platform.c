/* -*- c-basic-offset: 8 -*- */
#include "rdesktop.h"
#include "platform.h"

#ifdef _WIN32
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#include <windows.h>

RD_BOOL
platform_get_local_username(char *buffer, size_t size)
{
	DWORD length;

	if (buffer == NULL || size == 0 || size > MAXDWORD)
		return False;
	length = (DWORD) size;
	if (!GetUserNameA(buffer, &length))
	{
		buffer[0] = '\0';
		return False;
	}
	return buffer[0] != '\0';
}

RD_BOOL
platform_get_local_hostname(char *buffer, size_t size)
{
	DWORD length;

	if (buffer == NULL || size == 0 || size > MAXDWORD)
		return False;
	length = (DWORD) size;
	if (!GetComputerNameA(buffer, &length))
	{
		buffer[0] = '\0';
		return False;
	}
	buffer[length] = '\0';
	return buffer[0] != '\0';
}

const char *
platform_get_home_directory(void)
{
	static char path[MAX_PATH];
	DWORD length;

	length = GetEnvironmentVariableA("USERPROFILE", path, sizeof(path));
	if (length > 0 && length < sizeof(path))
		return path;
	if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, path) != S_OK)
		return NULL;
	return path;
}

RD_BOOL
platform_read_secret(const char *prompt, char *buffer, size_t size)
{
	HANDLE input;
	DWORD mode;
	RD_BOOL console;
	char *newline;

	if (prompt == NULL || buffer == NULL || size == 0 || size > INT_MAX)
		return False;
	input = GetStdHandle(STD_INPUT_HANDLE);
	console = input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &mode);
	if (console)
	{
		fputs(prompt, stderr);
		if (!SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT))
			return False;
	}
	if (fgets(buffer, (int) size, stdin) == NULL)
	{
		if (console)
			SetConsoleMode(input, mode);
		return False;
	}
	newline = strpbrk(buffer, "\r\n");
	if (newline != NULL)
		*newline = '\0';
	if (console)
	{
		SetConsoleMode(input, mode);
		fputc('\n', stderr);
	}
	return True;
}

RD_BOOL
platform_random_bytes(uint8 *buffer, size_t size)
{
	if (buffer == NULL || size > ULONG_MAX)
		return False;
	return BCryptGenRandom(NULL, buffer, (ULONG) size,
			      BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

RD_BOOL
platform_lock_file(int fd, int start, int length)
{
	HANDLE file;
	OVERLAPPED offset;

	if (start < 0 || length < 0)
		return False;
	file = (HANDLE) _get_osfhandle(fd);
	if (file == INVALID_HANDLE_VALUE)
		return False;
	memset(&offset, 0, sizeof(offset));
	offset.Offset = (DWORD) start;
	return LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
			  0, (DWORD) length, 0, &offset) != 0;
}

int
platform_make_directory(const char *path, int mode)
{
	UNUSED(mode);
	return _mkdir(path);
}

void
platform_ignore_sigpipe(void)
{
	/* Winsock reports a failed peer through its API; Windows has no SIGPIPE. */
}

void
platform_sleep_seconds(unsigned int seconds)
{
	Sleep(seconds * 1000U);
}

#else
#include <pwd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

RD_BOOL
platform_get_local_username(char *buffer, size_t size)
{
	struct passwd *pw;

	if (buffer == NULL || size == 0)
		return False;
	pw = getpwuid(getuid());
	if (pw == NULL || pw->pw_name == NULL)
	{
		buffer[0] = '\0';
		return False;
	}
	STRNCPY(buffer, pw->pw_name, size);
	return buffer[0] != '\0';
}

RD_BOOL
platform_get_local_hostname(char *buffer, size_t size)
{
	if (buffer == NULL || size == 0)
		return False;
	if (gethostname(buffer, size) == -1)
	{
		buffer[0] = '\0';
		return False;
	}
	buffer[size - 1] = '\0';
	return buffer[0] != '\0';
}

const char *
platform_get_home_directory(void)
{
	return getenv("HOME");
}

RD_BOOL
platform_read_secret(const char *prompt, char *buffer, size_t size)
{
	struct termios attributes;
	RD_BOOL terminal = False;
	char *newline;

	if (prompt == NULL || buffer == NULL || size == 0 || size > INT_MAX)
		return False;
	if (tcgetattr(STDIN_FILENO, &attributes) == 0)
	{
		fputs(prompt, stderr);
		attributes.c_lflag &= ~ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &attributes);
		terminal = True;
	}
	if (fgets(buffer, (int) size, stdin) == NULL)
	{
		if (terminal)
		{
			attributes.c_lflag |= ECHO;
			tcsetattr(STDIN_FILENO, TCSANOW, &attributes);
		}
		return False;
	}
	newline = strchr(buffer, '\n');
	if (newline != NULL)
		*newline = '\0';
	if (terminal)
	{
		attributes.c_lflag |= ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &attributes);
		fputc('\n', stderr);
	}
	return True;
}

RD_BOOL
platform_random_bytes(uint8 *buffer, size_t size)
{
	int fd;
	ssize_t count;
	size_t offset = 0;

	if (buffer == NULL)
		return False;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
		return False;
	while (offset < size)
	{
		count = read(fd, buffer + offset, size - offset);
		if (count > 0)
			offset += (size_t) count;
		else if (count == -1 && errno == EINTR)
			continue;
		else
			break;
	}
	close(fd);
	return offset == size;
}

RD_BOOL
platform_lock_file(int fd, int start, int length)
{
	struct flock lock;

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = start;
	lock.l_len = length;
	return fcntl(fd, F_SETLK, &lock) != -1;
}

int
platform_make_directory(const char *path, int mode)
{
	return mkdir(path, mode);
}

void
platform_ignore_sigpipe(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_IGN;
	sigemptyset(&action.sa_mask);
	sigaction(SIGPIPE, &action, NULL);
}

void
platform_sleep_seconds(unsigned int seconds)
{
	sleep(seconds);
}
#endif
