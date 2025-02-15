/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_COVERCACHE_H
#define MYMPD_COVERCACHE_H

#include <stdbool.h>

#include "../../dist/src/sds/sds.h"

bool covercache_write_file(const char *workdir, const char *uri, const char *mime_type, sds binary);
int covercache_clear(const char *workdir, int keepdays);
#endif
