package org.quintum.wallet.core

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

/**
 * Owns the lifetime of QUINTUM Core while the user explicitly keeps the node
 * running. The service is private to this application and exposes no Binder
 * API to other apps.
 */
class NodeService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    @Volatile private var startRequested = false

    override fun onCreate() {
        super.onCreate()
        createChannel()
        startForeground(NOTIFICATION_ID, notification("Starting QUINTUM node"))
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (startRequested) return START_STICKY
        startRequested = true
        scope.launch {
            val result = NativeCore.nativeStart(filesDir.resolve("core").absolutePath)
        if (result != 0 && result != 2) {
            stopSelf(startId)
        }
        }

        return START_STICKY
    }

    override fun onDestroy() {
        scope.cancel()
        Thread { NativeCore.nativeStop() }.start()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                "QUINTUM node",
                NotificationManager.IMPORTANCE_LOW,
            ),
        )
    }

    private fun notification(text: String): Notification =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setContentTitle("QUINTUM Testnet")
            .setContentText(text)
            .setOngoing(true)
            .build()

    private companion object {
        const val CHANNEL_ID = "quintum-node"
        const val NOTIFICATION_ID = 1001
    }
}
