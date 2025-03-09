/*-
 * Copyright (c) 2024 Weongyo Jeong <weongyo@gmail.com>
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

package band.mud.android

class JniWrapper private constructor() {
    companion object {
        @Volatile
        private var instance: JniWrapper? = null

        fun getInstance(): JniWrapper {
            if (instance == null) {
                synchronized(this) {
                    if (instance == null) {
                        instance = JniWrapper()
                    }
                }
            }
            return instance!!
        }

        init {
            System.loadLibrary("mudband");
        }
    }

    external fun isBandPublic(): Boolean
    external fun getBandJWT(): String
    external fun getActiveBandName(): String
    external fun getActiveDeviceName(): String
    external fun getActivePrivateIP(): String?
    external fun getBandConfigEtag(): String?
    external fun getBandConfigString(): String?
    external fun changeEnrollment(uuid: String)
    external fun getBandUUIDs(): Array<String>?
    external fun getBandNameByUUID(uuid: String): String?
    external fun isEnrolled(): Boolean
    external fun createWireguardKeys(): Array<String>?
    external fun getListenPort(): Int
    external fun getIfAddrs(): Array<String>?
    external fun getStunNatType(): Int
    external fun getStunMappedAddr(): String?
    external fun parseEnrollmentResponse(privateKey: String, body: String): Int
    external fun parseConfigResponse(etag: String?, body: String): Int
    external fun parseUnenrollmentResponse(body: String): Int
    external fun getVpnServiceConfig(): String?
    external fun saveBandAdmin(bandUuid: String, jwt: String): Boolean
    external fun getBandAdmin(): String?
    external fun getStatusSnapshotString(): String?
    external fun tunnelInit(tunFd: Int)
    external fun tunnelLoop(): Int
    external fun tunnelFini()

    external fun initJni(rootDir: String?): Int
}