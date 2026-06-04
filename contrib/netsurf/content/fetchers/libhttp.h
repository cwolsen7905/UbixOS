/*
 * Copyright 2026 The UbixOS Project
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * \file
 * http:// and https:// scheme handler over UbixOS libhttp (BearSSL).
 *
 * Replaces the curl fetcher on UbixOS, which has no libcurl/OpenSSL.  The
 * fetch is synchronous: http_get() runs inside the poll callback and blocks
 * the UI until the response (or an error) arrives — adequate for first paint.
 */

#ifndef NETSURF_CONTENT_FETCHERS_LIBHTTP_H
#define NETSURF_CONTENT_FETCHERS_LIBHTTP_H

#include "utils/errors.h"

/**
 * Register the http and https scheme fetchers.
 *
 * @return NSERROR_OK on success, or an error code on failure.
 */
nserror fetch_libhttp_register(void);

#endif
