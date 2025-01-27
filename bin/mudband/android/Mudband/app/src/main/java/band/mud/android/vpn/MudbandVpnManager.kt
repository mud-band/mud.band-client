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

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ResultReceiver
import android.util.Log
import band.mud.android.MainApplication

class MudbandVpnManager(private val mApp: MainApplication) {
    private val mTag = "Mud.band"

    fun connect(resultReceiver: ResultReceiver?): Int {
        val mHandler = Handler(Looper.getMainLooper())
        mHandler.post {
            mApp.startService(serviceIntentForConnect(resultReceiver))
        }
        return (0)
    }

    fun disconnect(resultReceiver: ResultReceiver): Int {
        val mHandler = Handler(Looper.getMainLooper())
        mHandler.post {
            mApp.startService(serviceIntentForDisconnect(resultReceiver))
        }
        return (0)
    }

    private fun serviceIntentForDisconnect(resultReceiver: ResultReceiver): Intent {
        val prefix = mApp.packageName
        return Intent(mApp, MudbandVpnService::class.java)
            .putExtra("$prefix.RESULT_RECEIVER", resultReceiver)
            .setAction("DISCONNECT")
    }

    private fun serviceIntentForConnect(resultReceiver: ResultReceiver?): Intent {
        val prefix = mApp.packageName
        val intent = Intent(mApp, MudbandVpnService::class.java)
        if (resultReceiver == null) {
            return intent
        }
        return intent.putExtra("$prefix.RESULT_RECEIVER", resultReceiver)
    }
}