package org.quintum.wallet.core

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.PowerManager
import android.os.Build
import android.os.BatteryManager
import android.content.IntentFilter
import android.os.IBinder
import android.os.SystemClock
import org.quintum.wallet.MainActivity
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay

/**
 * Owns the lifetime of QUINTUM Core while the user explicitly keeps the node
 * running. The service is private to this application and exposes no Binder
 * API to other apps.
 */
class NodeService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    @Volatile private var startRequested = false
    @Volatile private var destroyed = false
    private var wakeLock: PowerManager.WakeLock? = null
    @Volatile private var thermalStopIssued = false

    private fun diagnosticEvent(name: String) {
        diagnosticExecutor.execute {
            runCatching {
                NativeCore.nativeInitializeDiagnostics(filesDir.resolve("core").absolutePath)
                NativeCore.nativeServiceEvent(name)
            }
        }
    }

    private fun recordStatus(message: String) {
        getSharedPreferences("node_status", MODE_PRIVATE).edit()
            .putString("last_error", message).apply()
    }

    override fun onCreate() {
        super.onCreate()
        diagnosticEvent("created")
        recordStatus("")
        createChannel()
        try {
            startForeground(NOTIFICATION_ID, notification("Starting QUINTUM node"))
            scope.launch { enforceMiningThermalSafety() }
            wakeLock = getSystemService(PowerManager::class.java)
                .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "QUINTUM:Node").apply {
                    setReferenceCounted(false)
                    acquire()
                }
        } catch (e: RuntimeException) {
            android.util.Log.e("QUINTUM-Node", "Foreground service startup rejected", e)
            destroyed = true
            diagnosticEvent("start_failed")
            recordStatus("Android rejected foreground service: ${e.javaClass.simpleName}")
            stopSelf()
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        diagnosticEvent("start_request")
        if (destroyed) return START_NOT_STICKY
        if (startRequested) return START_STICKY
        startRequested = true
        recordStatus("Initializing native QUINTUM Core")
        scope.launch {
            val startedAt = SystemClock.elapsedRealtime()
            try {
                NativeCore.nativeInitializeDiagnostics(filesDir.resolve("core").absolutePath)
                val result = ownership.start(this@NodeService, { destroyed }) {
                        val dataDir = filesDir.resolve("core")
                        if (!dataDir.isDirectory && !dataDir.mkdirs()) {
                            android.util.Log.e("QUINTUM-Node", "Unable to create core data directory")
                            recordStatus("Cannot create node data directory")
                            -2
                        } else NativeCore.nativeStart(dataDir.absolutePath)
                }
                val elapsedMs = SystemClock.elapsedRealtime() - startedAt
                android.util.Log.i("QUINTUM-Node", "Native startup result=$result duration_ms=$elapsedMs")
                getSharedPreferences("node_status", MODE_PRIVATE).edit()
                    .putLong("last_start_duration_ms", elapsedMs)
                    .putInt("last_start_result", result)
                    .apply()
                if (result != -1) diagnosticEvent(when (result) { 0 -> "core_started"; 2 -> "core_already_running"; else -> "start_failed" })
                if (result == 0 || result == 2) {
                    recordStatus("")
                }
                if (result != 0 && result != 2 && result != -1) {
                    android.util.Log.e("QUINTUM-Node", "Node startup failed: $result")
                    recordStatus("Core startup failed (code $result)")
                    stopSelf(startId)
                }
            } catch (e: Throwable) {
                android.util.Log.e("QUINTUM-Node", "Node startup exception", e)
                diagnosticEvent("start_failed")
                recordStatus("Core exception: ${e.javaClass.simpleName}")
                stopSelf(startId)
            }
        }

        return START_STICKY
    }

    /** Thermal enforcement belongs to the foreground service, not a visible screen.
     * Closing the mining page must never disable overheat protection.
     */
    private suspend fun enforceMiningThermalSafety() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        val power = getSystemService(PowerManager::class.java)
        while (!destroyed) {
            try {
                val hot = power.currentThermalStatus >= PowerManager.THERMAL_STATUS_SEVERE
                val battery = registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
                val status = battery?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
                val charging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
                    status == BatteryManager.BATTERY_STATUS_FULL
                if (!hot && charging) thermalStopIssued = false
                if ((hot || !charging) && !thermalStopIssued && NativeCore.nativeMiningRunning()) {
                    thermalStopIssued = true
                    NativeCore.nativeStopMining()
                    recordStatus(if (hot) "Mining stopped: device overheating" else "Mining stopped: charger disconnected")
                    android.util.Log.w("QUINTUM-Node", "Mining stopped by background safety guard")
                }
            } catch (e: Exception) {
                android.util.Log.w("QUINTUM-Node", "Thermal guard status unavailable", e)
            }
            delay(3000)
        }
    }

    override fun onDestroy() {
        destroyed = true
        diagnosticEvent("destroyed")
        scope.cancel()
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
        Thread {
            ownership.stop(this@NodeService) { NativeCore.nativeStop() }
        }.start()
        super.onDestroy()
    }

    // Android 15 limits background dataSync foreground services. Stop promptly
    // when the platform budget expires rather than triggering an ANR/crash.
    override fun onTimeout(startId: Int, fgsType: Int) {
        diagnosticEvent("timeout")
        recordStatus("Android foreground data-sync time limit reached; reopen app to resume")
        android.util.Log.w("QUINTUM-Node", "Foreground timeout startId=$startId type=$fgsType")
        stopSelf()
    }

    override fun onTaskRemoved(rootIntent: Intent?) {
        diagnosticEvent("task_removed")
        super.onTaskRemoved(rootIntent)
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
            .setContentIntent(PendingIntent.getActivity(this, 0,
                Intent(this, MainActivity::class.java).apply {
                    flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
                }, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
            .setOngoing(true)
            .build()

    private companion object {
        val diagnosticExecutor = java.util.concurrent.Executors.newSingleThreadExecutor { runnable ->
            Thread(runnable, "QUINTUM-service-diagnostics").apply { isDaemon = true }
        }
        val ownership = NodeOwnership()
        const val CHANNEL_ID = "quintum-node"
        const val NOTIFICATION_ID = 1001
    }
}

