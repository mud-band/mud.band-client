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

package band.mud.android.vpn

import android.app.PendingIntent
import android.content.Context
import android.content.pm.PackageManager
import android.net.VpnService
import android.os.ParcelFileDescriptor
import android.util.Log
import band.mud.android.MainApplication
import band.mud.android.util.MudbandLog
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer

typealias OnEstablishListener = (ParcelFileDescriptor?) -> Unit

@Serializable
data class MudbandConfigInterfaceRequestData(
    @SerialName("listen_port")
    var listenPort: Int,
    var addresses: Array<String>
)

@Serializable
data class MudbandConfigRequestData(
    @SerialName("fetch_type")
    var fetchType: String,
    @SerialName("stun_mapped_addr")
    val stunMappedAddr: String,
    @SerialName("stun_nattype")
    val stunNatType: Int,
    var `interface`: MudbandConfigInterfaceRequestData
)

@Serializable
data class MudbandConfigResponseData(
    val status: Int,
    val msg: String? = null,
    val sso_url: String? = null
)

data class MudbandConfigResult(
    var status: Int,
    var msg: String
)

class MudbandVpnConnection(
    private val mService: MudbandVpnService
) : Runnable {
    private var mConfigureIntent: PendingIntent? = null
    private var mOnEstablishListener: OnEstablishListener? = null
    private var mConfigFetcherThread: Thread? = null

    fun setConfigureIntent(intent: PendingIntent?) {
        mConfigureIntent = intent
    }

    fun setOnEstablishListener(listener: OnEstablishListener?) {
        mOnEstablishListener = listener
    }

    private fun fetchResult(status: Int, msg: String): MudbandConfigResult {
        if (status != 0) {
            MudbandLog.e(msg)
        }
        return MudbandConfigResult(status, msg)
    }

    private fun fetchConfAndConnect(fetchType: String): MudbandConfigResult {
        MudbandLog.i("Fetching the band config: $fetchType")
        val app = MainApplication.applicationContext() as MainApplication
        var ifaddrs = app.jni.getIfAddrs()
            ?: return fetchResult(-8, "BANDEC_00176: app.jni.getIfAddrs() failed.")
        val iface = MudbandConfigInterfaceRequestData(app.jni.getListenPort(), ifaddrs)
        val stunNatType = app.jni.getStunNatType()
        val stunMappedAddr = app.jni.getStunMappedAddr()
            ?: return fetchResult(-9, "BANDEC_00177: app.jni.getStunMappedAddr() failed.")
        val data = MudbandConfigRequestData(fetchType, stunMappedAddr, stunNatType, iface)
        val mediaType = "application/json; charset=utf-8".toMediaType()
        val post = Json.encodeToString(data).toRequestBody(mediaType)
        val client = OkHttpClient()
        var builder = Request.Builder()
            .url("https://www.mud.band/api/band/conf")
        builder = builder.addHeader("Authorization", app.jni.getBandJWT())
        val etag = app.jni.getBandConfigEtag()
        if (etag != null) {
            builder = builder.addHeader("If-None-Match", etag)
        }
        val request = builder.post(post)
            .build()
        try {
            val response = client.newCall(request).execute()
            if (!response.isSuccessful) {
                if (response.code == 304) {
                    return fetchResult(0, "Okay")
                }
                return fetchResult(-4, "BANDEC_00178: Unexpected status ${response.code}")
            }
            val etag = response.headers["Etag"]
            val responseData = response.body?.string()
                ?: return fetchResult(-2, "BANDEC_00179: Failed to get the response body.")
            val jsonWithUnknownKeys = Json { ignoreUnknownKeys = true }
            val obj = jsonWithUnknownKeys.decodeFromString<MudbandConfigResponseData>(responseData)
            if (obj.status != 200) {
                if (obj.status == 301 /* SSO_URL */) {
                    return fetchResult(-10, obj.sso_url ?: "https://mud.band/api/error/BANDEC_00495")
                }
                return fetchResult(-5, obj.msg ?: "BANDEC_00180: msg is null")
            }
            var r = app.jni.parseConfigResponse(etag, responseData)
            if (r != 0) {
                return fetchResult(-7, "BANDEC_00181: app.jni.parseConfigResponse() failed.")
            }
            return fetchResult(0, "Okay")
        } catch (e: Exception) {
            e.printStackTrace()
            return fetchResult(-3, "BANDEC_00182: Exception ${e.message}")
        }
    }

    private fun reportError(status: Int, msg: String) {
        mService.reportResult(status, msg)
        MudbandLog.e(msg)
    }

    private fun reportError(status: Int, msg: String, e: Exception) {
        mService.reportResult(status, msg)
        MudbandLog.e(msg, e)
    }

    private fun saveMfaUrlIntoPref(url: String) {
        val app = MainApplication.applicationContext() as MainApplication
        val sharedPreferences = app.getSharedPreferences("Mud.band", Context.MODE_PRIVATE)
        with(sharedPreferences.edit()) {
            putString("MFA_URL", url)
            apply()
        }
    }

    override fun run() {
        val app = MainApplication.applicationContext() as MainApplication
        try {
            val result = fetchConfAndConnect("when_it_gots_a_event")
            if (result.status != 0) {
                if (result.status == -10 /* SSO_URL */) {
                    reportError(401, result.msg)
                    return
                }
                reportError(301, "BANDEC_00183: fetchConfAndConnect() failed: ${result.status}")
                return
            }
            val conf = app.jni.getVpnServiceConfig()
            if (conf == null) {
                reportError(302, "BANDEC_00184: app.jni.getVpnServiceConfig() failed.")
                return
            }
            val iface = configure(conf)
            if (iface == null) {
                reportError(303, "BANDEC_00185: configure() failed.")
                return
            }
            var isAborted = false
            app.jni.tunnelInit(iface.fd)
            while (!isAborted) {
                when (val r = app.jni.tunnelLoop()) {
                    in 101..500 -> {
                        MudbandLog.w("BANDEC_00186: Got a error code $r. Aborting the loop.")
                        isAborted = true
                    }
                    1 -> {
                        mConfigFetcherThread?.interrupt()
                        mConfigFetcherThread = Thread {
                            var result = fetchConfAndConnect("when_it_gots_a_event")
                            if (result.status != 0) {
                                if (result.status == -10 /* SSO_URL */) {
                                    saveMfaUrlIntoPref(result.msg)
                                } else {
                                    MudbandLog.e("BANDEC_00503: fetchConfAndConnect() failed.")
                                }
                            }
                        }
                        mConfigFetcherThread?.start();
                    }
                    0 -> {
                        /* do nothing */
                    }
                    else -> MudbandLog.e("BANDEC_00187: Unexpected return code.")
                }
            }
            MudbandLog.i("MainLoop is aborted.")
        } catch (e: IOException) {
            reportError(304, "BANDEC_00188: Connection failed, exiting", e)
        } catch (e: InterruptedException) {
            reportError(305, "BANDEC_00189: Connection failed, exiting", e)
        } catch (e: IllegalArgumentException) {
            reportError(306, "BANDEC_00190: Connection failed, exiting", e)
        } catch (e : Exception) {
            reportError(307, "BANDEC_00191: Unexpected exception", e)
        } finally {
            app.jni.tunnelFini()
        }
    }

    @Throws(IllegalArgumentException::class)
    private fun configure(parameters: String): ParcelFileDescriptor? {
        val builder = mService.Builder()
        for (parameter in parameters.split(" ").dropLastWhile { it.isEmpty() }
            .toTypedArray()) {
            val fields = parameter.split(",").dropLastWhile { it.isEmpty() }.toTypedArray()
            try {
                when (fields[0][0]) {
                    'm' -> builder.setMtu(fields[1].toShort().toInt())
                    'a' -> builder.addAddress(fields[1], fields[2].toInt())
                    'r' -> builder.addRoute(fields[1], fields[2].toInt())
                    'd' -> builder.addDnsServer(fields[1])
                    's' -> builder.addSearchDomain(fields[1])
                }
            } catch (e: NumberFormatException) {
                throw IllegalArgumentException("Bad parameter: $parameter")
            }
        }
        val vpnInterface: ParcelFileDescriptor?
        builder.setSession("Mud.band").setConfigureIntent(mConfigureIntent!!)
        synchronized(mService) {
            vpnInterface = builder.establish()
            mOnEstablishListener?.let { it(vpnInterface) }
        }
        MudbandLog.i("New interface: $vpnInterface ($parameters)")
        return vpnInterface
    }
}
