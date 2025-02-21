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

package band.mud.android.ui.model

import android.app.Application
import android.app.ActivityManager;
import android.content.Context
import androidx.lifecycle.AndroidViewModel
import band.mud.android.MainApplication
import band.mud.android.vpn.MudbandVpnService
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json


@Serializable
data class MudbandConfigInterfaceJson(
    val nat_type: Int,
    val listen_port: Int
)

@Serializable
data class MudbandConfigPeerJson(
    val name: String,
    var private_ip: String,
    var wireguard_pubkey: String
)

@Serializable
data class MudbandConfigLinkJson(
    val name: String,
    val url: String
)

@Serializable
data class MudbandConfigJson(
    val `interface`: MudbandConfigInterfaceJson,
    val peers: Array<MudbandConfigPeerJson>,
    val links: Array<MudbandConfigLinkJson>
)

data class MudbandAppUiStateBand(
    var name: String,
    var band_uuid: String
)

data class MudbandAppUiStateLink(
    var name: String,
    var url: String
)

data class MudbandAppUiStateDevice(
    var name: String,
    var private_ip: String,
    var wireguard_pubkey: String
)

data class MudbandAppUiState(
    var mfaRequired: Boolean = false,
    var mfaUrl: String = "",
    var isBandPublic: Boolean = false,
    var isEnrolled: Boolean = false,
    var isConnected: Boolean = false,
    var isConfigurationReady: Boolean = false,
    var isUserTosAgreed: Boolean = false,
    var activeBandName: String = "",
    var activeDeviceName: String = "",
    var activePrivateIP: String = "",
    var dashboardScreenName: String = "status",
    var topAppBarTitle: String = "Mud.band",
    var bands: List<MudbandAppUiStateBand> = listOf(),
    var devices: List<MudbandAppUiStateDevice> = listOf(),
    var links: List<MudbandAppUiStateLink> = listOf()
)

class MudbandAppViewModel(application: Application) : AndroidViewModel(application) {
    private val app = getApplication<Application>().applicationContext as MainApplication
    private val sharedPreferences = app.getSharedPreferences("Mud.band", Context.MODE_PRIVATE)
    private val _uiState = MutableStateFlow(MudbandAppUiState())
    val uiState: StateFlow<MudbandAppUiState> = _uiState.asStateFlow()

    private fun isServiceRunning(serviceClass: Class<*>): Boolean {
        val manager = app.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        manager?.getRunningServices(Int.MAX_VALUE)?.forEach { service ->
            if (serviceClass.name == service.service.className) {
                return true
            }
        }
        return false
    }

    private fun syncConnectStatus() {
        setConnectStatus(isServiceRunning(MudbandVpnService::class.java))
    }

    fun setEnrollStatus(enrolled: Boolean) {
        _uiState.update { currentState ->
            currentState.copy(
                isEnrolled = enrolled
            )
        }
        if (enrolled) {
            setActiveBandName(app.jni.getActiveBandName())
            setActivePrivateIP(app.jni.getActivePrivateIP())
            setActiveDeviceName(app.jni.getActiveDeviceName())
            setDashboardScreenName("status")
            updateBandList()
            updateDeviceList()
        }
        setBandPublic(app.jni.isBandPublic())
        updateMfaStatus()
        syncUserTosAgreement()
        syncConnectStatus()
    }

    private fun syncUserTosAgreement() {
        var isUserTosAgreed = sharedPreferences.getBoolean("USER_TOS_AGREED", false)
        _uiState.update { currentState ->
            currentState.copy(
                isUserTosAgreed = isUserTosAgreed,
            )
        }
    }

    fun setUserTosAgreement(agreed: Boolean) {
        with(sharedPreferences.edit()) {
            putBoolean("USER_TOS_AGREED", agreed)
            apply()
        }
        syncUserTosAgreement()
    }

    private fun updateMfaStatus() {
        val mfaUrl = sharedPreferences.getString("MFA_URL", null)
        if (mfaUrl != null) {
            _uiState.update { currentState ->
                currentState.copy(
                    mfaRequired = true,
                    mfaUrl = mfaUrl
                )
            }
        }
    }

    fun resetMfaStatus() {
        _uiState.update { currentState ->
            currentState.copy(
                mfaRequired = false,
                mfaUrl = ""
            )
        }
        with(sharedPreferences.edit()) {
            remove("MFA_URL")
            apply()
        }
    }

    private fun setBandPublic(isPublic: Boolean) {
        _uiState.update { currentState ->
            currentState.copy(
                isBandPublic = isPublic
            )
        }
    }

    private fun updateBandList() {
        val bands: MutableList<MudbandAppUiStateBand> = ArrayList()
        val band_uuids = app.jni.getBandUUIDs() ?: return
        band_uuids.forEach {
            val name = app.jni.getBandNameByUUID(it)
            if (name != null) {
                bands += MudbandAppUiStateBand(name, it)
            }
        }
        _uiState.update { currentState ->
            currentState.copy(
                bands = bands
            )
        }
    }

    private fun updateDeviceList() {
        val body = app.jni.getBandConfigString()
        if (body == null) {
            _uiState.update { currentState ->
                currentState.copy(
                    isConfigurationReady = false
                )
            }
            return
        }
        val jsonWithUnknownKeys = Json { ignoreUnknownKeys = true }
        val obj = jsonWithUnknownKeys.decodeFromString<MudbandConfigJson>(body)
        val devices: MutableList<MudbandAppUiStateDevice> = ArrayList()
        obj.peers.forEach {
            devices += MudbandAppUiStateDevice(it.name, it.private_ip, it.wireguard_pubkey)
        }
        _uiState.update { currentState ->
            currentState.copy(
                devices = devices,
                isConfigurationReady = true
            )
        }
    }

    private fun updateLinks() {
        val body = app.jni.getBandConfigString() ?: return
        val jsonWithUnknownKeys = Json { ignoreUnknownKeys = true }
        val obj = jsonWithUnknownKeys.decodeFromString<MudbandConfigJson>(body)
        val links: MutableList<MudbandAppUiStateLink> = ArrayList()
        obj.links.forEach {
            links += MudbandAppUiStateLink(it.name, it.url)
        }
        _uiState.update { currentState ->
            currentState.copy(
                links = links
            )
        }
    }

    fun setConnectStatus(connected: Boolean) {
        if (connected) {
            setActivePrivateIP(app.jni.getActivePrivateIP())
            setActiveDeviceName(app.jni.getActiveDeviceName())
        }
        _uiState.update { currentState ->
            currentState.copy(
                isConnected = connected
            )
        }
    }

    fun setDashboardScreenName(screenName: String) {
        _uiState.update { currentState ->
            currentState.copy(
                dashboardScreenName = screenName
            )
        }
        if (screenName == "devices") {
            updateDeviceList()
        }
        if (screenName == "links") {
            updateLinks()
        }
    }

    fun changeEnrollment(uuid: String) {
        app.jni.changeEnrollment(uuid)
        refreshEnrollStatus()
    }

    fun setTopAppBarTitle(title: String) {
        _uiState.update { currentState ->
            currentState.copy(
                topAppBarTitle = title
            )
        }
    }

    private fun setActiveBandName(bandName: String) {
        _uiState.update { currentState ->
            currentState.copy(
                activeBandName = bandName
            )
        }
    }

    private fun setActivePrivateIP(ip: String?) {
        if (ip == null) {
            return
        }
        _uiState.update { currentState ->
            currentState.copy(
                activePrivateIP = ip
            )
        }
    }

    private fun setActiveDeviceName(deviceName: String?) {
        if (deviceName == null) {
            return
        }
        _uiState.update { currentState ->
            currentState.copy(
                activeDeviceName = deviceName
            )
        }
    }

    fun refreshEnrollStatus() {
        setEnrollStatus(app.jni.isEnrolled())
        setDashboardScreenName("status")
    }

    init {
        refreshEnrollStatus()
    }
}
