/* Copyright (c) 2013-2021 the Civetweb developers
 * Copyright (c) 2004-2013 Sergey Lyubka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#if defined(_WIN32)

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS /* Disable deprecation warning in VS2005 */
#endif
#if !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE
#endif
#if defined(WIN32_LEAN_AND_MEAN)
#undef WIN32_LEAN_AND_MEAN /* Required for some functions (tray icons, ...) */
#endif

#else

#if defined(__clang__) /* GCC does not (yet) support this pragma */
/* We must set some flags for the headers we include. These flags
 * are reserved ids according to C99, so we need to disable a
 * warning for that. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600 /* For PATH_MAX on linux */
/* This should also be sufficient for "realpath", according to
 * http://man7.org/linux/man-pages/man3/realpath.3.html, but in
 * reality it does not seem to work. */
/* In case this causes a problem, disable the warning:
 * #pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
 * #pragma clang diagnostic ignored "-Wimplicit-function-declaration"
 */
#endif
#endif

#if !defined(IGNORE_UNUSED_RESULT)
#define IGNORE_UNUSED_RESULT(a) ((void)((a) && 1))
#endif

#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define NO_RETURN [[noreturn]]
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define NO_RETURN _Noreturn
#elif defined(__GNUC__)
#define NO_RETURN __attribute((noreturn))
#else
#define NO_RETURN
#endif

/* Use same defines as in civetweb.c before including system headers. */
#if !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE /* For fseeko(), ftello() */
#endif
#if !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64 /* Use 64-bit file offsets by default */
#endif
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS /* <inttypes.h> wants this for C++ */
#endif
#if !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS /* C++ wants that for INT64_MAX */
#endif

#if defined(__clang__)
/* Enable reserved-id-macro warning again. */
#pragma GCC diagnostic pop
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "civetweb.h"
#include "../../inter.h"

#undef printf
#define printf                                                                 \
	DO_NOT_USE_THIS_FUNCTION__USE_fprintf /* Required for unit testing */

#if defined(_WIN32) /* WINDOWS include block */
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501 /* for tdm-gcc so we can use getconsolewindow */
#endif
#undef UNICODE
#include <io.h>
#include <shlobj.h>
#include <windows.h>
#include <winsvc.h>

#define getcwd(a, b) (_getcwd(a, b))
#if !defined(__MINGW32__)
extern char *_getcwd(char *buf, size_t size);
#endif

#if !defined(PATH_MAX)
#define PATH_MAX MAX_PATH
#endif

#if !defined(S_ISDIR)
#define S_ISDIR(x) ((x)&_S_IFDIR)
#endif

#define DIRSEP '\\'
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define sleep(x) (Sleep((x)*1000))
#define WINCDECL __cdecl
#define abs_path(rel, abs, abs_size) (_fullpath((abs), (rel), (abs_size)))

#else /* defined(_WIN32) - WINDOWS / UNIX include                              \
         block */

#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#define DIRSEP '/'
#define WINCDECL
#define abs_path(rel, abs, abs_size) (realpath((rel), (abs)))

#endif /* defined(_WIN32) - WINDOWS / UNIX include                             \
          block */

#if !defined(DEBUG_ASSERT)
#if defined(DEBUG)

#if defined(_MSC_VER)
/* DEBUG_ASSERT has some const conditions */
#pragma warning(disable : 4127)
#endif

#define DEBUG_ASSERT(cond)                                                     \
	do {                                                                       \
		if (!(cond)) {                                                         \
			fprintf(stderr, "ASSERTION FAILED: %s", #cond);                    \
			exit(2); /* Exit with error */                                     \
		}                                                                      \
	} while (0)

#else
#define DEBUG_ASSERT(cond)
#endif /* DEBUG */
#endif

#if !defined(PATH_MAX)
#define PATH_MAX (1024)
#endif

#define MAX_OPTIONS (50)
#define MAX_CONF_FILE_LINE_SIZE (8 * 1024)

struct tuser_data {
	char *first_message;
};


/* Exit flag for the main loop (read and writen by different threads, thus
 * volatile). */
volatile int g_exit_flag = 0; /* 0 = continue running main loop */


static char g_server_base_name[40]; /* Set by init_server_name() */

static const char *g_server_name; /* Default from init_server_name,
                                   * updated later from the server config */
static const char *g_icon_name;   /* Default from init_server_name,
                                   * updated later from the server config */
static const char *g_website;     /* Default from init_server_name,
                                   * updated later from the server config */
static int g_num_add_domains;     /* Default from init_server_name,
                                   * updated later from the server config */
static const char **g_add_domain; /* Default from init_server_name,
                                   * updated later from the server config */
static int g_hide_tray = 0;       /* Default = do not hide (0),
                                   * updated later from the server config */

static char *g_system_info; /* Set by init_system_info() */
static char g_config_file_name[PATH_MAX] =
    ""; /* Set by
         *  process_command_line_arguments() */

static struct mg_context *g_ctx; /* Set by start_civetweb() */
static struct tuser_data
    g_user_data; /* Passed to mg_start() by start_civetweb() */

#if !defined(CONFIG_FILE)
#define CONFIG_FILE "civetweb.conf"
#endif /* !CONFIG_FILE */

#if !defined(PASSWORDS_FILE_NAME)
#define PASSWORDS_FILE_NAME ".htpasswd"
#endif

/* backup config file */
#if !defined(CONFIG_FILE2) && defined(__linux__)
#define CONFIG_FILE2 "/usr/local/etc/civetweb.conf"
#endif

enum {
	OPTION_TITLE,
	OPTION_ICON,
	OPTION_WEBPAGE,
	OPTION_ADD_DOMAIN,
	OPTION_HIDE_TRAY,
#if defined(DAEMONIZE)
	ENABLE_DAEMONIZE,
#endif

	NUM_MAIN_OPTIONS
};

static struct mg_option main_config_options[] = {
    {"title", MG_CONFIG_TYPE_STRING, NULL},
    {"icon", MG_CONFIG_TYPE_STRING, NULL},
    {"website", MG_CONFIG_TYPE_STRING, NULL},
    {"add_domain", MG_CONFIG_TYPE_STRING_LIST, NULL},
    {"hide_tray", MG_CONFIG_TYPE_BOOLEAN, NULL},
#if defined(DAEMONIZE)
    {"daemonize", MG_CONFIG_TYPE_BOOLEAN, "no"},
#endif

    {NULL, MG_CONFIG_TYPE_UNKNOWN, NULL}};


static void WINCDECL
signal_handler(int sig_num)
{
	g_exit_flag = sig_num;
}


static NO_RETURN void
die(const char *fmt, ...)
{
	va_list ap;
	char msg[512] = "";

	va_start(ap, fmt);
	(void)vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
	msg[sizeof(msg) - 1] = 0;
	va_end(ap);

#if defined(_WIN32)
	MessageBox(NULL, msg, "Error", MB_OK);
#else
	fprintf(stderr, "%s\n", msg);
#endif

	exit(EXIT_FAILURE);
}


static void
warn(const char *fmt, ...)
{
	va_list ap;
	char msg[512] = "";

	va_start(ap, fmt);
	(void)vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
	msg[sizeof(msg) - 1] = 0;
	va_end(ap);

#if defined(_WIN32)
	MessageBox(NULL, msg, "Warning", MB_OK);
#else
	fprintf(stderr, "%s\n", msg);
#endif
}





static void
show_server_name(void)
{
#if defined(BUILD_DATE)
	const char *bd = BUILD_DATE;
#else
	const char *bd = __DATE__;
#endif


	fprintf(stderr, "CivetWeb v%s, built on %s\n", mg_version(), bd);
}


static NO_RETURN void
show_usage_and_exit(const char *exeName)
{
	const struct mg_option *options;
	int i;

	if (exeName == 0 || *exeName == 0) {
		exeName = "civetweb";
	}

	show_server_name();

	fprintf(stderr, "\nUsage:\n");
	fprintf(stderr, "  Start server with a set of options:\n");
	fprintf(stderr, "    %s [config_file]\n", exeName);
	fprintf(stderr, "    %s [-option value ...]\n", exeName);
	fprintf(stderr, "  Run as client:\n");
	fprintf(stderr, "    %s -C url\n", exeName);
	fprintf(stderr, "  Show system information:\n");
	fprintf(stderr, "    %s -I\n", exeName);
	fprintf(stderr, "  Add user/change password:\n");
	fprintf(stderr,
	        "    %s -A <htpasswd_file> <realm> <user> <passwd>\n",
	        exeName);
	fprintf(stderr, "  Remove user:\n");
	fprintf(stderr, "    %s -R <htpasswd_file> <realm> <user>\n", exeName);
	fprintf(stderr, "\nOPTIONS:\n");

	options = mg_get_valid_options();
	for (i = 0; options[i].name != NULL; i++) {
		fprintf(stderr,
		        "  -%s %s\n",
		        options[i].name,
		        ((options[i].default_value == NULL)
		             ? "<empty>"
		             : options[i].default_value));
	}

	options = main_config_options;
	for (i = 0; options[i].name != NULL; i++) {
		fprintf(stderr,
		        "  -%s %s\n",
		        options[i].name,
		        ((options[i].default_value == NULL)
		             ? "<empty>"
		             : options[i].default_value));
	}

	exit(EXIT_FAILURE);
}


#if defined(_WIN32) || defined(USE_COCOA) || defined(MAIN_C_UNIT_TEST)
static const char *config_file_top_comment =
    "# CivetWeb web server configuration file.\n"
    "# For detailed description of every option, visit\n"
    "# https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md\n"
    "# Lines starting with '#' and empty lines are ignored.\n"
    "# To make changes, remove leading '#', modify option values,\n"
    "# save this file and then restart CivetWeb.\n\n";

static const char *
get_url_to_first_open_port(const struct mg_context *ctx)
{
	static char url[128];

#define MAX_PORT_COUNT (32)

	struct mg_server_port ports[MAX_PORT_COUNT];
	int portNum = mg_get_server_ports(ctx, MAX_PORT_COUNT, ports);
	int i;

	memset(url, 0, sizeof(url));

	/* Prefer IPv4 http, ignore redirects */
	for (i = 0; i < portNum; i++) {
		if ((ports[i].protocol == 1) && (ports[i].is_redirect == 0)
		    && (ports[i].is_ssl == 0)) {
			snprintf(url, sizeof(url), "http://localhost:%d/", ports[i].port);
			return url;
		}
	}
	/* Use IPv4 https */
	for (i = 0; i < portNum; i++) {
		if ((ports[i].protocol == 1) && (ports[i].is_redirect == 0)
		    && (ports[i].is_ssl == 1)) {
			snprintf(url, sizeof(url), "https://localhost:%d/", ports[i].port);
			return url;
		}
	}
	/* Try IPv6 http, ignore redirects */
	if (portNum > 0) {
		snprintf(url,
		         sizeof(url),
		         "%s://localhost:%d/",
		         (ports[0].is_ssl ? "https" : "http"),
		         ports[0].port);
	}

#undef MAX_PORT_COUNT

	return url;
}


#if defined(ENABLE_CREATE_CONFIG_FILE) || defined(MAIN_C_UNIT_TEST)
static void
create_config_file(const struct mg_context *ctx, const char *path)
{
	const struct mg_option *options;
	const char *value;
	FILE *fp;
	int i;

	/* Create config file if it is not present yet */
	if ((fp = fopen(path, "r")) != NULL) {
		fclose(fp);
	} else if ((fp = fopen(path, "a+")) != NULL) {
		fprintf(fp, "%s", config_file_top_comment);
		options = mg_get_valid_options();
		for (i = 0; options[i].name != NULL; i++) {
			value = mg_get_option(ctx, options[i].name);
			fprintf(fp,
			        "# %s %s\n",
			        options[i].name,
			        value ? value : "<value>");
		}
		fclose(fp);
	}
}
#endif
#endif


static char *
sdup(const char *str)
{
	size_t len;
	char *p;

	len = strlen(str) + 1;
	p = (char *)malloc(len);

	if (p == NULL) {
		die("Cannot allocate %u bytes", (unsigned)len);
	}

	memcpy(p, str, len);
	return p;
}


#if 0 /* Unused code from "string duplicate with escape" */
static unsigned
hex2dec(char x)
{
    if ((x >= '0') && (x <= '9')) {
        return (unsigned)x - (unsigned)'0';
    }
    if ((x >= 'A') && (x <= 'F')) {
        return (unsigned)x - (unsigned)'A' + 10u;
    }
    if ((x >= 'a') && (x <= 'f')) {
        return (unsigned)x - (unsigned)'a' + 10u;
    }
    return 0;
}


static char *
sdupesc(const char *str)
{
	char *p = sdup(str);

	if (p) {
		char *d = p;
		while ((d = strchr(d, '\\')) != NULL) {
			switch (d[1]) {
			case 'a':
				d[0] = '\a';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'b':
				d[0] = '\b';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'e':
				d[0] = 27;
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'f':
				d[0] = '\f';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'n':
				d[0] = '\n';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'r':
				d[0] = '\r';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 't':
				d[0] = '\t';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'u':
				if (isxdigit(d[2]) && isxdigit(d[3]) && isxdigit(d[4])
				    && isxdigit(d[5])) {
					unsigned short u = (unsigned short)(hex2dec(d[2]) * 4096
					                                    + hex2dec(d[3]) * 256
					                                    + hex2dec(d[4]) * 16
					                                    + hex2dec(d[5]));
					char mbc[16];
					int mbl = wctomb(mbc, (wchar_t)u);
					if ((mbl > 0) && (mbl < 6)) {
						memcpy(d, mbc, (unsigned)mbl);
						memmove(d + mbl, d + 6, strlen(d + 5));
						/* Advance mbl characters (+1 is below) */
						d += (mbl - 1);
					} else {
						/* Invalid multi byte character */
						/* TODO: define what to do */
					}
				} else {
					/* Invalid esc sequence */
					/* TODO: define what to do */
				}
				break;
			case 'v':
				d[0] = '\v';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 'x':
				if (isxdigit(d[2]) && isxdigit(d[3])) {
					d[0] = (char)((unsigned char)(hex2dec(d[2]) * 16
					                              + hex2dec(d[3])));
					memmove(d + 1, d + 4, strlen(d + 3));
				} else {
					/* Invalid esc sequence */
					/* TODO: define what to do */
				}
				break;
			case 'z':
				d[0] = 0;
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case '\\':
				d[0] = '\\';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case '\'':
				d[0] = '\'';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case '\"':
				d[0] = '\"';
				memmove(d + 1, d + 2, strlen(d + 1));
				break;
			case 0:
				if (d == p) {
					/* Line is only \ */
					free(p);
					return NULL;
				}
			/* no break */
			default:
				/* invalid ESC sequence */
				/* TODO: define what to do */
				break;
			}

			/* Advance to next character */
			d++;
		}
	}
	return p;
}
#endif


static const char *
get_option(char **options, const char *option_name)
{
	int i = 0;
	const char *opt_value = NULL;

	/* TODO (low, api makeover): options should be an array of key-value-pairs,
	 * like
	 *     struct {const char * key, const char * value} options[]
	 * but it currently is an array with
	 *     options[2*i] = key, options[2*i + 1] = value
	 * (probably with a MG_LEGACY_INTERFACE definition)
	 */
	while (options[2 * i] != NULL) {
		if (strcmp(options[2 * i], option_name) == 0) {
			opt_value = options[2 * i + 1];
			break;
		}
		i++;
	}
	return opt_value;
}


static int
set_option(char **options, const char *name, const char *value)
{
	int i, type;
	const struct mg_option *default_options = mg_get_valid_options();
	const char *multi_sep = NULL;

	for (i = 0; main_config_options[i].name != NULL; i++) {
		/* These options are evaluated by main.c, not civetweb.c.
		 * Do not add it to options, and return OK */
		if (!strcmp(name, main_config_options[OPTION_TITLE].name)) {
			g_server_name = sdup(value);
			return 1;
		}
		if (!strcmp(name, main_config_options[OPTION_ICON].name)) {

			g_icon_name = sdup(value);
			return 1;
		}
		if (!strcmp(name, main_config_options[OPTION_WEBPAGE].name)) {
			g_website = sdup(value);
			return 1;
		}
		if (!strcmp(name, main_config_options[OPTION_HIDE_TRAY].name)) {
			if (!strcmp(value, "yes")) {
				g_hide_tray = 1;
			} else if (!strcmp(value, "no")) {
				g_hide_tray = 0;
			}
			return 1;
		}
		if (!strcmp(name, main_config_options[OPTION_ADD_DOMAIN].name)) {
			if (g_num_add_domains > 0) {
				g_add_domain = (const char **)realloc(
				    (void *)g_add_domain,
				    sizeof(char *) * ((unsigned)g_num_add_domains + 1u));
				if (!g_add_domain) {
					die("Out of memory");
				}
				g_add_domain[g_num_add_domains] = sdup(value);
				g_num_add_domains++;
			} else {
				g_add_domain = (const char **)malloc(sizeof(char *));
				if (!g_add_domain) {
					die("Out of memory");
				}
				g_add_domain[0] = sdup(value);
				g_num_add_domains = 1;
			}
			return 1;
		}
	}

	/* Not an option of main.c, so check if it is a CivetWeb server option */
	type = MG_CONFIG_TYPE_UNKNOWN; /* type "unknown" means "option not found" */
	for (i = 0; default_options[i].name != NULL; i++) {
		if (!strcmp(default_options[i].name, name)) {
			type = default_options[i].type;
			break; /* no need to search for another option */
		}
	}

	switch (type) {

	case MG_CONFIG_TYPE_UNKNOWN:
		/* unknown option */
		return 0; /* return error */

	case MG_CONFIG_TYPE_NUMBER:
		/* integer number >= 0, e.g. number of threads */
		{
			char *chk = 0;
			unsigned long num = strtoul(value, &chk, 10);
			(void)num; /* do not check value, only syntax */
			if ((chk == NULL) || (*chk != 0) || (chk == value)) {
				/* invalid number */
				return 0;
			}
		}
		break;

	case MG_CONFIG_TYPE_STRING:
		/* any text */
		break;

	case MG_CONFIG_TYPE_STRING_LIST:
		/* list of text items, separated by , */
		multi_sep = ",";
		break;

	case MG_CONFIG_TYPE_STRING_MULTILINE:
		/* lines of text, separated by carriage return line feed */
		multi_sep = "\r\n";
		break;

	case MG_CONFIG_TYPE_BOOLEAN:
		/* boolean value, yes or no */
		if ((0 != strcmp(value, "yes")) && (0 != strcmp(value, "no"))) {
			/* invalid boolean */
			return 0;
		}
		break;

	case MG_CONFIG_TYPE_YES_NO_OPTIONAL:
		/* boolean value, yes or no */
		if ((0 != strcmp(value, "yes")) && (0 != strcmp(value, "no"))
		    && (0 != strcmp(value, "optional"))) {
			/* invalid boolean */
			return 0;
		}
		break;

	case MG_CONFIG_TYPE_FILE:
	case MG_CONFIG_TYPE_DIRECTORY:
		/* TODO (low): check this option when it is set, instead of calling
		 * verify_existence later */
		break;

	case MG_CONFIG_TYPE_EXT_PATTERN:
		/* list of patterns, separated by | */
		multi_sep = "|";
		break;

	default:
		die("Unknown option type - option %s", name);
	}

	for (i = 0; i < MAX_OPTIONS; i++) {
		if (options[2 * i] == NULL) {
			/* Option not set yet. Add new option */
			options[2 * i] = sdup(name);
			options[2 * i + 1] = sdup(value);
			options[2 * i + 2] = NULL;
			break;
		} else if (!strcmp(options[2 * i], name)) {
			if (multi_sep) {
				/* Option already set. Append new value. */
				char *s =
				    (char *)malloc(strlen(options[2 * i + 1])
				                   + strlen(multi_sep) + strlen(value) + 1);
				if (!s) {
					die("Out of memory");
				}
				sprintf(s, "%s%s%s", options[2 * i + 1], multi_sep, value);
				free(options[2 * i + 1]);
				options[2 * i + 1] = s;
			} else {
				/* Option already set. Overwrite */
				free(options[2 * i + 1]);
				options[2 * i + 1] = sdup(value);
			}
			break;
		}
	}

	if (i == MAX_OPTIONS) {
		die("Too many options specified");
	}

	if (options[2 * i] == NULL) {
		die("Out of memory");
	}
	if (options[2 * i + 1] == NULL) {
		die("Illegal escape sequence, or out of memory");
	}

	/* option set correctly */
	return 1;
}


static int
read_config_file(const char *config_file, char **options)
{
	char line[MAX_CONF_FILE_LINE_SIZE], *p;
	FILE *fp = NULL;
	size_t i, j, line_no = 0;

	/* Open the config file */
	fp = fopen(config_file, "r");
	if (fp == NULL) {
		/* Failed to open the file. Keep errno for the caller. */
		return 0;
	}

	/* Load config file settings first */
	fprintf(stdout, "Loading config file %s\n", config_file);

	/* Loop over the lines in config file */
	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!line_no && !memcmp(line, "\xEF\xBB\xBF", 3)) {
			/* strip UTF-8 BOM */
			p = line + 3;
		} else {
			p = line;
		}
		line_no++;

		/* Ignore empty lines and comments */
		for (i = 0; isspace((unsigned char)p[i]);)
			i++;
		if (p[i] == '#' || p[i] == '\0') {
			continue;
		}

		/* Skip spaces, \r and \n at the end of the line */
		for (j = strlen(p); (j > 0)
		                    && (isspace((unsigned char)p[j - 1])
		                        || iscntrl((unsigned char)p[j - 1]));)
			p[--j] = 0;

		/* Find the space character between option name and value */
		for (j = i; !isspace((unsigned char)p[j]) && (p[j] != 0);)
			j++;

		/* Terminate the string - then the string at (p+i) contains the
		 * option name */
		p[j] = 0;
		j++;

		/* Trim additional spaces between option name and value - then
		 * (p+j) contains the option value */
		while (isspace((unsigned char)p[j])) {
			j++;
		}

		/* Set option */
		if (!set_option(options, p + i, p + j)) {
			fprintf(stderr,
			        "%s: line %d is invalid, ignoring it:\n %s",
			        config_file,
			        (int)line_no,
			        p);
		}
	}

	(void)fclose(fp);

	return 1;
}


static void
process_command_line_arguments(int argc, char *argv[], char **options)
{
	char *p;
	size_t i, cmd_line_opts_start = 1;
#if defined(CONFIG_FILE2)
	FILE *fp = NULL;
#endif

	/* Should we use a config file ? */
	if ((argc > 1) && (argv[1] != NULL) && (argv[1][0] != '-')
	    && (argv[1][0] != 0)) {
		/* The first command line parameter is a config file name. */
		snprintf(g_config_file_name,
		         sizeof(g_config_file_name) - 1,
		         "%s",
		         argv[1]);
		cmd_line_opts_start = 2;
	} else if ((p = strrchr(argv[0], DIRSEP)) == NULL) {
		/* No config file set. No path in arg[0] found.
		 * Use default file name in the current path. */
		snprintf(g_config_file_name,
		         sizeof(g_config_file_name) - 1,
		         "%s",
		         CONFIG_FILE);
	} else {
		/* No config file set. Path to exe found in arg[0].
		 * Use default file name next to the executable. */
		snprintf(g_config_file_name,
		         sizeof(g_config_file_name) - 1,
		         "%.*s%c%s",
		         (int)(p - argv[0]),
		         argv[0],
		         DIRSEP,
		         CONFIG_FILE);
	}
	g_config_file_name[sizeof(g_config_file_name) - 1] = 0;

#if defined(CONFIG_FILE2)
	fp = fopen(g_config_file_name, "r");

	/* try alternate config file */
	if (fp == NULL) {
		fp = fopen(CONFIG_FILE2, "r");
		if (fp != NULL) {
			strcpy(g_config_file_name, CONFIG_FILE2);
		}
	}
	if (fp != NULL) {
		fclose(fp);
	}
#endif

	/* read all configurations from a config file */
	if (0 == read_config_file(g_config_file_name, options)) {
		if (cmd_line_opts_start == 2) {
			/* If config file was set in command line and open failed, die. */
			/* Errno will still hold the error from fopen. */
			die("Cannot open config file %s: %s",
			    g_config_file_name,
			    strerror(errno));
		}
		/* Otherwise: CivetWeb can work without a config file */
	}

	/* If we're under MacOS and started by launchd, then the second
	   argument is process serial number, -psn_.....
	   In this case, don't process arguments at all. */
	if (argv[1] == NULL || memcmp(argv[1], "-psn_", 5) != 0) {
		/* Handle command line flags.
		   They override config file and default settings. */
		for (i = cmd_line_opts_start; argv[i] != NULL; i += 2) {
			if (argv[i][0] != '-' || argv[i + 1] == NULL) {
				show_usage_and_exit(argv[0]);
			}
			if (!set_option(options, &argv[i][1], argv[i + 1])) {
				fprintf(
				    stderr,
				    "command line option is invalid, ignoring it:\n %s %s\n",
				    argv[i],
				    argv[i + 1]);
			}
		}
	}
}


static void
init_system_info(void)
{
	int len = mg_get_system_info(NULL, 0);
	if (len > 0) {
		g_system_info = (char *)malloc((unsigned)len + 1);
		(void)mg_get_system_info(g_system_info, len + 1);
	} else {
		g_system_info = sdup("Not available");
	}
}


static void
init_server_name(void)
{
	DEBUG_ASSERT(sizeof(main_config_options) / sizeof(main_config_options[0])
	             == NUM_MAIN_OPTIONS + 1);
	DEBUG_ASSERT((strlen(mg_version()) + 12) < sizeof(g_server_base_name));
	snprintf(g_server_base_name,
	         sizeof(g_server_base_name),
	         "CivetWeb V%s",
	         mg_version());
	g_server_name = g_server_base_name;
	g_icon_name = NULL;
	g_website = "http://civetweb.github.io/civetweb/";
	g_num_add_domains = 0;
	g_add_domain = NULL;
}


static void
free_system_info(void)
{
	free(g_system_info);
}


static int
log_message(const struct mg_connection *conn, const char *message)
{
	const struct mg_context *ctx = mg_get_context(conn);
	struct tuser_data *ud = (struct tuser_data *)mg_get_user_data(ctx);

	fprintf(stderr, "%s\n", message);

	if (ud->first_message == NULL) {
		ud->first_message = sdup(message);
	}

	return 0;
}


static int
is_path_absolute(const char *path)
{
#if defined(_WIN32)
	return path != NULL
	       && ((path[0] == '\\' && path[1] == '\\') || /* UNC path, e.g.
	                                                      \\server\dir */
	           (isalpha((unsigned char)path[0]) && path[1] == ':'
	            && path[2] == '\\')); /* E.g. X:\dir */
#else
	return path != NULL && path[0] == '/';
#endif
}


static int
verify_existence(char **options, const char *option_name, int must_be_dir)
{
	struct stat st;
	const char *path = get_option(options, option_name);

#if defined(_WIN32)
	wchar_t wbuf[1024];
	char mbbuf[1024];
	int len;

	if (path) {
		memset(wbuf, 0, sizeof(wbuf));
		memset(mbbuf, 0, sizeof(mbbuf));
		len = MultiByteToWideChar(
		    CP_UTF8, 0, path, -1, wbuf, sizeof(wbuf) / sizeof(wbuf[0]) - 1);
		wcstombs(mbbuf, wbuf, sizeof(mbbuf) - 1);
		path = mbbuf;
		(void)len;
	}
#endif

	if (path != NULL
	    && (stat(path, &st) != 0
	        || ((S_ISDIR(st.st_mode) ? 1 : 0) != must_be_dir))) {
		warn("Invalid path for %s: [%s]: (%s). Make sure that path is either "
		     "absolute, or it is relative to civetweb executable.",
		     option_name,
		     path,
		     strerror(errno));
		return 0;
	}
	return 1;
}


static void
set_absolute_path(char *options[],
                  const char *option_name,
                  const char *path_to_civetweb_exe)
{
	char path[PATH_MAX] = "", absolute[PATH_MAX] = "";
	const char *option_value;
	const char *p;

	/* Check whether option is already set */
	option_value = get_option(options, option_name);

	/* If option is already set and it is an absolute path,
	   leave it as it is -- it's already absolute. */
	if (option_value != NULL && !is_path_absolute(option_value)) {
		/* Not absolute. Use the directory where civetweb executable lives
		   be the relative directory for everything.
		   Extract civetweb executable directory into path. */
		if ((p = strrchr(path_to_civetweb_exe, DIRSEP)) == NULL) {
			IGNORE_UNUSED_RESULT(getcwd(path, sizeof(path)));
		} else {
			snprintf(path,
			         sizeof(path) - 1,
			         "%.*s",
			         (int)(p - path_to_civetweb_exe),
			         path_to_civetweb_exe);
			path[sizeof(path) - 1] = 0;
		}

		strncat(path, "/", sizeof(path) - strlen(path) - 1);
		strncat(path, option_value, sizeof(path) - strlen(path) - 1);

		/* Absolutize the path, and set the option */
		IGNORE_UNUSED_RESULT(abs_path(path, absolute, sizeof(absolute)));
		set_option(options, option_name, absolute);
	}
}


#if defined(USE_LUA)

#include "civetweb_private_lua.h"

#endif


#if defined(USE_DUKTAPE)

#include "duktape.h"

static int
run_duktape(const char *file_name)
{
	duk_context *ctx = NULL;

	ctx = duk_create_heap_default();
	if (!ctx) {
		fprintf(stderr, "Failed to create a Duktape heap.\n");
		goto finished;
	}

	if (duk_peval_file(ctx, file_name) != 0) {
		fprintf(stderr, "%s\n", duk_safe_to_string(ctx, -1));
		goto finished;
	}
	duk_pop(ctx); /* ignore result */

finished:
	duk_destroy_heap(ctx);

	return 0;
}
#endif


#if defined(__MINGW32__) || defined(__MINGW64__)
/* For __MINGW32/64_MAJOR/MINOR_VERSION define */
#include <_mingw.h>
#endif


static int
run_client(const char *url_arg)
{
	/* connection data */
	char *url = sdup(url_arg); /* OOM will cause program to exit */
	char *host;
	char *resource;
	int is_ssl = 0;
	unsigned long port = 0;
	size_t sep;
	char *endp = 0;
	char empty[] = "";

	/* connection object */
	struct mg_connection *conn;
	char ebuf[1024] = {0};

#if 0 /* Unreachable code, since sdup will never return NULL */
    /* Check out of memory */
    if (!url) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
#endif

	/* Check parameter */
	if (!strncmp(url, "http://", 7)) {
		host = url + 7;
		port = 80;
	} else if (!strncmp(url, "https://", 8)) {
		host = url + 8;
		is_ssl = 1;
		port = 443;
	} else {
		fprintf(stderr, "URL must start with http:// or https://\n");
		free(url);
		return 0;
	}
	if ((host[0] <= 32) || (host[0] > 126) || (host[0] == '/')
	    || (host[0] == ':')) {
		fprintf(stderr, "Invalid host\n");
		free(url);
		return 0;
	}

	sep = strcspn(host, "/:");
	switch (host[sep]) {
	case 0:
		resource = empty;
		break;
	case '/':
		host[sep] = 0;
		resource = host + sep + 1;
		break;
	case ':':
		host[sep] = 0;
		port = strtoul(host + sep + 1, &endp, 10);
		if (!endp || (*endp != '/' && *endp != 0) || (port < 1)
		    || (port > 0xFFFF)) {
			fprintf(stderr, "Invalid port\n");
			free(url);
			return 0;
		}
		if (*endp) {
			*endp = 0;
			resource = endp + 1;
		} else {
			resource = empty;
		}
		break;
	default:
		fprintf(stderr, "Syntax error\n");
		free(url);
		return 0;
	}

	fprintf(stdout, "Protocol: %s\n", is_ssl ? "https" : "http");
	fprintf(stdout, "Host: %s\n", host);
	fprintf(stdout, "Port: %lu\n", port);
	fprintf(stdout, "Resource: %s\n", resource);

	/* Initialize library */
	if (is_ssl) {
		mg_init_library(MG_FEATURES_TLS);
	} else {
		mg_init_library(MG_FEATURES_DEFAULT);
	}

	/* Connect to host */
	conn = mg_connect_client(host, (int)port, is_ssl, ebuf, sizeof(ebuf));
	if (conn) {
		/* Connecting to server worked */
		char buf[1024] = {0};
		int ret;

		fprintf(stdout, "Connected to %s\n", host);

		/* Send GET request */
		mg_printf(conn,
		          "GET /%s HTTP/1.1\r\n"
		          "Host: %s\r\n"
		          "Connection: close\r\n"
		          "\r\n",
		          resource,
		          host);

		/* Wait for server to respond with a HTTP header */
		ret = mg_get_response(conn, ebuf, sizeof(ebuf), 10000);

		if (ret >= 0) {
			const struct mg_response_info *ri = mg_get_response_info(conn);

			fprintf(stdout,
			        "Response info: %i %s\n",
			        ri->status_code,
			        ri->status_text);

			/* Respond reader read. Read body (if any) */
			ret = mg_read(conn, buf, sizeof(buf));
			while (ret > 0) {
				fwrite(buf, 1, (unsigned)ret, stdout);
				ret = mg_read(conn, buf, sizeof(buf));
			}

			fprintf(stdout, "Closing connection to %s\n", host);

		} else {
			/* Server did not reply to HTTP request */
			fprintf(stderr, "Got no response from %s:\n%s\n", host, ebuf);
		}
		mg_close_connection(conn);

	} else {
		/* Connecting to server failed */
		fprintf(stderr, "Error connecting to %s:\n%s\n", host, ebuf);
	}

	/* Free memory and exit library */
	free(url);
	mg_exit_library();
	return 1;
}


static int
sanitize_options(char *options[] /* server options */,
                 const char *arg0 /* argv[0] */)
{
	int ok = 1;
	/* Make sure we have absolute paths for files and directories */
	set_absolute_path(options, "document_root", arg0);
	set_absolute_path(options, "put_delete_auth_file", arg0);
	set_absolute_path(options, "cgi_interpreter", arg0);
	set_absolute_path(options, "access_log_file", arg0);
	set_absolute_path(options, "error_log_file", arg0);
	set_absolute_path(options, "global_auth_file", arg0);
#if defined(USE_LUA)
	set_absolute_path(options, "lua_preload_file", arg0);
#endif
	set_absolute_path(options, "ssl_certificate", arg0);

	/* Make extra verification for certain options */
	if (!verify_existence(options, "document_root", 1))
		ok = 0;
	if (!verify_existence(options, "cgi_interpreter", 0))
		ok = 0;
	if (!verify_existence(options, "ssl_certificate", 0))
		ok = 0;
	if (!verify_existence(options, "ssl_ca_path", 1))
		ok = 0;
	if (!verify_existence(options, "ssl_ca_file", 0))
		ok = 0;
#if defined(USE_LUA)
	if (!verify_existence(options, "lua_preload_file", 0))
		ok = 0;
#endif
	return ok;
}


static void
start_civetweb(int argc, char *argv[])
{
	struct mg_callbacks callbacks;
	char *options[2 * MAX_OPTIONS + 1];
	int i;

	/* Start option -I:
	 * Show system information and exit
	 * This is very useful for diagnosis. */
	if (argc > 1 && !strcmp(argv[1], "-I")) {


		fprintf(stdout,
		        "\n%s (%s)\n%s\n",
		        g_server_base_name,
		        g_server_name,
		        g_system_info);

		exit(EXIT_SUCCESS);
	}

	/* Edit passwords file: Add user or change password, if -A option is
	 * specified */
	if (argc > 1 && !strcmp(argv[1], "-A")) {
		if (argc != 6) {
			show_usage_and_exit(argv[0]);
		}
		exit(mg_modify_passwords_file(argv[2], argv[3], argv[4], argv[5])
		         ? EXIT_SUCCESS
		         : EXIT_FAILURE);
	}

	/* Edit passwords file: Remove user, if -R option is specified */
	if (argc > 1 && !strcmp(argv[1], "-R")) {
		if (argc != 5) {
			show_usage_and_exit(argv[0]);
		}
		exit(mg_modify_passwords_file(argv[2], argv[3], argv[4], NULL)
		         ? EXIT_SUCCESS
		         : EXIT_FAILURE);
	}

	/* Client mode */
	if (argc > 1 && !strcmp(argv[1], "-C")) {
		if (argc != 3) {
			show_usage_and_exit(argv[0]);
		}

		exit(run_client(argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	/* Call Lua with additional CivetWeb specific Lua functions, if -L option
	 * is specified */
	if (argc > 1 && !strcmp(argv[1], "-L")) {

#if defined(USE_LUA)
		if (argc != 3) {
			show_usage_and_exit(argv[0]);
		}

		exit(run_lua(argv[2]));
#else
		show_server_name();
		fprintf(stderr, "\nError: Lua support not enabled\n");
		exit(EXIT_FAILURE);
#endif
	}

	/* Call Duktape, if -E option is specified */
	if (argc > 1 && !strcmp(argv[1], "-E")) {

#if defined(USE_DUKTAPE)
		if (argc != 3) {
			show_usage_and_exit(argv[0]);
		}

		exit(run_duktape(argv[2]));
#else
		show_server_name();
		fprintf(stderr, "\nError: Ecmascript support not enabled\n");
		exit(EXIT_FAILURE);
#endif
	}

	/* Show usage if -h or --help options are specified */
	if (argc == 2
	    && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-H")
	        || !strcmp(argv[1], "--help"))) {
		show_usage_and_exit(argv[0]);
	}

	/* Initialize options structure */
	memset(options, 0, sizeof(options));
	set_option(options, "document_root", ".");

	/* Update config based on command line arguments */
	process_command_line_arguments(argc, argv, options);

	i = sanitize_options(options, argv[0]);
	if (!i) {
		die("Invalid options");
	}

	/* Setup signal handler: quit on Ctrl-C */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

#if defined(DAEMONIZE)
	/* Daemonize */
	for (i = 0; options[i] != NULL; i++) {
		if (strcmp(options[i], "daemonize") == 0) {
			if (options[i + 1] != NULL) {
				if (mg_strcasecmp(options[i + 1], "yes") == 0) {
					fprintf(stdout, "daemonize.\n");
					if (daemon(0, 0) != 0) {
						fprintf(stdout, "Faild to daemonize main process.\n");
						exit(EXIT_FAILURE);
					}
					FILE *fp;
					if ((fp = fopen(PID_FILE, "w")) == 0) {
						fprintf(stdout, "Can not open %s.\n", PID_FILE);
						exit(EXIT_FAILURE);
					}
					fprintf(fp, "%d", getpid());
					fclose(fp);
				}
			}
			break;
		}
	}
#endif

	/* Initialize user data */
	memset(&g_user_data, 0, sizeof(g_user_data));

	/* Start Civetweb */
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.log_message = &log_message;
	g_ctx = mg_start(&callbacks, &g_user_data, (const char **)options);

	/* mg_start copies all options to an internal buffer.
	 * The options data field here is not required anymore. */
	for (i = 0; options[i] != NULL; i++) {
		free(options[i]);
	}

	/* If mg_start fails, it returns NULL */
	if (g_ctx == NULL) {
		die("Failed to start %s:\n%s",
		    g_server_name,
		    ((g_user_data.first_message == NULL) ? "unknown reason"
		                                         : g_user_data.first_message));
		/* TODO: Edit file g_config_file_name */
	}

#if defined(MG_EXPERIMENTAL_INTERFACES)
	for (i = 0; i < g_num_add_domains; i++) {

		int j;
		memset(options, 0, sizeof(options));
		set_option(options, "document_root", ".");

		if (0 == read_config_file(g_add_domain[i], options)) {
			die("Cannot open config file %s: %s",
			    g_add_domain[i],
			    strerror(errno));
		}

		j = sanitize_options(options, argv[0]);
		if (!j) {
			die("Invalid options");
		}

		j = mg_start_domain(g_ctx, (const char **)options);
		if (j < 0) {
			die("Error loading domain file %s: %i", g_add_domain[i], j);
		} else {
			fprintf(stdout, "Domain file %s loaded\n", g_add_domain[i]);
		}

		for (j = 0; options[j] != NULL; j++) {
			free(options[j]);
		}
	}
#endif
}


static void
stop_civetweb(void)
{
	mg_stop(g_ctx);
	free(g_user_data.first_message);
	g_user_data.first_message = NULL;
}




/* main for Linux (and others) */
int
fmain(t_webserver *x)
{
	//init_server_name();
	//init_system_info();
	
	const char *options[] = {
       "document_root", "D:/00server",
       "listening_ports", "8080",
       NULL
     };
	 
	 struct mg_callbacks callbacks;
	 memset(&callbacks, 0, sizeof(callbacks));
     struct mg_context *ctx = mg_start(&callbacks, NULL, options);
	
	//post("%s", options);
	
	
	
	
	//start_civetweb(argc, argv);
	
	
	fprintf(stdout,
	        "%s started on port(s) %s with web root [%s]\n",
	        g_server_name,
	        mg_get_option(g_ctx, "listening_ports"),
	        mg_get_option(g_ctx, "document_root"));
			
	

	while (x->g_exit_flag == 0) {
		sleep(1);
	}

	fprintf(stdout,
	        "Exiting on signal %d, waiting for all threads to finish...",
	        g_exit_flag);
	fflush(stdout);
	stop_civetweb();
	fprintf(stdout, "%s", " done.\n");

	free_system_info();


	pthread_exit(NULL);
	return EXIT_SUCCESS;
	
	
}
//#endif /* _WIN32 */
#undef printf
