/*-
 * Copyright (c) 2022 Weongyo Jeong <weongyo@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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

#ifndef _MUDBAND_SERVICE_H_
#define	_MUDBAND_SERVICE_H_

#include "jansson.h"

/* mudband_service.c */
extern char *band_confdir_enroll;
extern char *band_confdir_root;
extern char *band_confdir_admin;
struct vtclog;
extern struct vtclog *vl;

/* mudband_service_bandadmin.c */
int	MBA_save(const char *band_uuid, const char *jwt);
json_t *MBA_get(void);

/* mudband_service_cmdctl.c */
int	CMD_execute(int wait, const char *fmt, ...);
void	CMD_init(void);

/* mudband_service_confmgr.c */
json_t *CNF_get_active_conf(void);

/* mudband_service_enroll.c */
int	MBE_get_enrollment_count(void);
ssize_t	MBE_enroll(char *out, size_t outmax,const char *token,
	    const char *name, const char *secret);
json_t *MBE_get_active_band(void);
json_t *MBE_get_enrollment_list(void);
int	MBE_unenroll(const char *band_uuid);

/* mudband_service_progconf.c */
void	MPC_init(void);
void	MPC_set_default_band_uuid(const char *band_uuid);
const char *
	MPC_get_default_band_uuid(void);
void	MPC_remove_default_band_uuid(void);

#endif /* _MUDBAND_SERVICE_H_ */
