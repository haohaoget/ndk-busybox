/* vi: set sw=4 ts=4: */
/*
 * prioritynames[] and facilitynames[]
 *
 * Copyright (C) 2008 by Denys Vlasenko <vda.linux@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "common_bufsiz.h"
#define SYSLOG_NAMES
#define SYSLOG_NAMES_CONST
#include <syslog.h>

#if defined(__ANDROID__)
/* Android Bionic's unified sysroot lacks SYSLOG_NAMES */
typedef struct _code {
	char *c_name;
	int c_val;
} CODE;
static const CODE prioritynames[] = {
    { "alert", LOG_ALERT }, { "crit", LOG_CRIT }, { "debug", LOG_DEBUG },
    { "emerg", LOG_EMERG }, { "err", LOG_ERR }, { "error", LOG_ERR },
    { "info", LOG_INFO }, { "none", -1 }, { "notice", LOG_NOTICE },
    { "panic", LOG_EMERG }, { "warn", LOG_WARNING }, { "warning", LOG_WARNING },
    { NULL, -1 }
};
static const CODE facilitynames[] = {
    { "auth", LOG_AUTH }, { "authpriv", LOG_AUTHPRIV }, { "cron", LOG_CRON },
    { "daemon", LOG_DAEMON }, { "ftp", LOG_FTP }, { "kern", LOG_KERN },
    { "lpr", LOG_LPR }, { "mail", LOG_MAIL }, { "mark", (24<<3) }, /* INTERNAL_MARK */
    { "news", LOG_NEWS }, { "security", LOG_AUTH }, { "syslog", LOG_SYSLOG },
    { "user", LOG_USER }, { "uucp", LOG_UUCP }, { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 }, { "local2", LOG_LOCAL2 }, { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 }, { "local5", LOG_LOCAL5 }, { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 }, { NULL, -1 }
};
#ifndef INTERNAL_NOPRI
#define INTERNAL_NOPRI 0x10
#endif
#ifndef INTERNAL_MARK
#define INTERNAL_MARK (24<<3)
#endif
#endif

#if 0
/* For the record: with SYSLOG_NAMES <syslog.h> defines
 * (not declares) the following:
 */
typedef struct _code {
	/*const*/ char *c_name;
	int c_val;
} CODE;
/*const*/ CODE prioritynames[] = {
    { "alert", LOG_ALERT },
...
    { NULL, -1 }
};
/* same for facilitynames[] */

/* This MUST occur only once per entire executable,
 * therefore we can't just do it in syslogd.c and logger.c -
 * there will be two copies of it.
 *
 * We cannot even do it in separate file and then just reference
 * prioritynames[] from syslogd.c and logger.c - bare <syslog.h>
 * will not emit extern decls for prioritynames[]! Attempts to
 * emit "matching" struct _code declaration defeat the whole purpose
 * of <syslog.h>.
 *
 * For now, syslogd.c and logger.c are simply compiled into
 * one object file.
 */
#endif

/* musl decided to be funny and it implements these as giant defines
 * of the form: ((CODE *)(const CODE []){ ... })
 * Which works, but causes _every_ function using them
 * to have a copy on stack (at least with gcc-6.3.0).
 * If we reference them just once, this saves 150 bytes.
 * The pointers themselves are optimized out
 * (no size change on uclibc).
 */
static const CODE *const bb_prioritynames = prioritynames;
static const CODE *const bb_facilitynames = facilitynames;


#if ENABLE_SYSLOGD
#include "syslogd.c"
#endif

#if ENABLE_LOGGER
#include "logger.c"
#endif
