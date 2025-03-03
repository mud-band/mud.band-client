/*
 * Copyright (c) 2024 Weongyo Jeong (weongyo@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jansson.h"
#include "odr.h"
#include "vassert.h"
#include "vhttps.h"
#include "vtc_log.h"

#include "mudband_service.h"

json_t *
MBA_get(void)
{
	char filepath[ODR_BUFSIZ];
	json_t *root;
	const char *default_band_uuid;

	default_band_uuid = MPC_get_default_band_uuid();
	if (default_band_uuid == NULL)
		return (NULL);
	ODR_snprintf(filepath, sizeof(filepath), "%s/admin_%s.json",
	    band_confdir_admin, default_band_uuid);
	root = json_load_file(filepath, 0, NULL);
	if (root == NULL) {
		vtc_log(vl, 0, "BANDEC_00825: Failed to load band admin file.");
		return (NULL);
	}
	return (root);
}

int
MBA_save(const char *band_uuid, const char *jwt)
{
	char filepath[ODR_BUFSIZ];
	json_t *root;

	ODR_snprintf(filepath, sizeof(filepath), "%s/admin_%s.json",
	    band_confdir_admin, band_uuid);
	root = json_object();
	AN(root);
	json_object_set_new(root, "band_uuid", json_string(band_uuid));
	json_object_set_new(root, "jwt", json_string(jwt));
	json_dump_file(root, filepath, 0);
	json_decref(root);
	return (0);
}
