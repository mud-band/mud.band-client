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

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Intent
import android.net.VpnService
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.os.ParcelFileDescriptor
import android.os.ResultReceiver
import android.util.Log
import android.util.Pair
import android.widget.Toast
import band.mud.android.MainActivity
import band.mud.android.R
import band.mud.android.util.MudbandLog
import java.io.IOException
import java.util.concurrent.atomic.AtomicReference

class MudbandVpnService : VpnService(), Handler.Callback {
    private class Connection(thread: Thread?, pfd: ParcelFileDescriptor?) :
        Pair<Thread?, ParcelFileDescriptor?>(thread, pfd)

    private var mHandler: Handler? = null
    private val mConnectingThread = AtomicReference<Thread?>()
    private val mConnection = AtomicReference<Connection?>()
    private var mConfigureIntent: PendingIntent? = null
    private var mResultReceiver: ResultReceiver? = null

    override fun onCreate() {
        if (mHandler == null) {
            mHandler = Handler(this)
        }
        mConfigureIntent = PendingIntent.getActivity(this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    override fun onStartCommand(intent: Intent, flags: Int, startId: Int): Int {
        mResultReceiver = intent.getParcelableExtra("$packageName.RESULT_RECEIVER");
        if ("DISCONNECT" == intent.action) {
            disconnect()
            return (START_NOT_STICKY)
        }
        connect()
        return (START_STICKY)
    }

    override fun onDestroy() {
        disconnect()
    }

    fun reportResult(status: Int, msg: String) {
        val bundle = Bundle()
        bundle.putString("msg", msg)
        mResultReceiver?.send(status, bundle)
    }

    override fun handleMessage(message: Message): Boolean {
        Toast.makeText(this, message.what, Toast.LENGTH_SHORT).show()
        updateForegroundNotification(message.what)
        if (message.what == R.string.connected) {
            reportResult(200, "Connected")
        } else if (message.what == R.string.disconnected) {
            reportResult(201, "Disconnected")
        } else if (message.what == R.string.connecting) {
            reportResult(202, "Connecting")
        }
        return (true)
    }

    private fun connect() {
        updateForegroundNotification(R.string.connecting)
        mHandler?.sendEmptyMessage(R.string.connecting)

        val connection = MudbandVpnConnection(this)
        val thread = Thread(connection, "MudbandVpnThread")
        setConnectingThread(thread)
        connection.setConfigureIntent(mConfigureIntent)
        connection.setOnEstablishListener { tunInterface ->
            mHandler?.sendEmptyMessage(R.string.connected)
            mConnectingThread.compareAndSet(thread, null)
            setConnection(Connection(thread, tunInterface))
        }
        thread.start()
    }

    private fun setConnectingThread(thread: Thread?) {
        val oldThread = mConnectingThread.getAndSet(thread)
        oldThread?.interrupt()
    }

    private fun setConnection(connection: Connection?) {
        val oldConnection = mConnection.getAndSet(connection)
        if (oldConnection != null) {
            try {
                oldConnection.first!!.interrupt()
                oldConnection.second!!.close()
            } catch (e: IOException) {
                MudbandLog.e("BANDEC_00175: Closing VPN interface", e)
            }
        }
    }

    private fun disconnect() {
        mHandler?.sendEmptyMessage(R.string.disconnected)
        setConnectingThread(null)
        setConnection(null)
        stopForeground(true)
        stopSelf();
    }

    private fun updateForegroundNotification(message: Int) {
        val id = "Mud.band"
        val notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.createNotificationChannel(
            NotificationChannel(id, id, NotificationManager.IMPORTANCE_DEFAULT)
        )
        startForeground(1, Notification.Builder(this, id)
            .setSmallIcon(R.drawable.mudband_95x95)
            .setContentText(getString(message))
            .setContentIntent(mConfigureIntent)
            .build())
    }
}