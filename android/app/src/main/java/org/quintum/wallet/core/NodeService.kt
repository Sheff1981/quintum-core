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
    @Volatile private var destroyed = false
    private val lifecycleLock = Any()

    private fun recordStatus(message: String) {
        getSharedPreferences("node_status", MODE_PRIVATE).edit()
            .putString("last_error", message).apply()
    }

    override fun onCreate() {
        super.onCreate()
        recordStatus("")
        createChannel()
        try {
            startForeground(NOTIFICATION_ID, notification("Starting QUINTUM node"))
        } catch (e: RuntimeException) {
            android.util.Log.e("QUINTUM-Node", "Foreground service startup rejected", e)
            recordStatus("Android rejected foreground service: ${e.javaClass.simpleName}")
            stopSelf()
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (startRequested) return START_NOT_STICKY
        startRequested = true
        scope.launch {
            try {
                val result = synchronized(lifecycleLock) {
                    if (destroyed) -1 else {
                        val dataDir = filesDir.resolve("core")
                        if (!dataDir.isDirectory && !dataDir.mkdirs()) {
                            android.util.Log.e("QUINTUM-Node", "Unable to create core data directory")
                            recordStatus("Cannot create node data directory")
                            -2
                        } else NativeCore.nativeStart(dataDir.absolutePath)
                    }
                }
                if (result != 0 && result != 2 && result != -1) {
                    android.util.Log.e("QUINTUM-Node", "Node startup failed: $result")
                    recordStatus("Core startup failed (code $result)")
                    stopSelf(startId)
                }
            } catch (e: Throwable) {
                android.util.Log.e("QUINTUM-Node", "Node startup exception", e)
                recordStatus("Core exception: ${e.javaClass.simpleName}")
                stopSelf(startId)
            }
        }

        return START_NOT_STICKY
    }

    override fun onDestroy() {
        destroyed = true
        scope.cancel()
        Thread {
            synchronized(lifecycleLock) { NativeCore.nativeStop() }
        }.start()
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
