package org.quintum.wallet.mining

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.SystemClock

data class DeviceMiningStats(
    val device: String,
    val abi: String,
    val cpuCores: Int,
    val miningThreads: Int,
    val hashRate: Double?,
    val temperatureCelsius: Double?,
    val batteryPercent: Int?,
    val charging: Boolean,
    val runtimeMillis: Long,
)

class DeviceMiningStatsReader(
    private val context: Context,
) {
    fun read(
        miningThreads: Int,
        hashRate: Double?,
        startedAtElapsedRealtime: Long?,
    ): DeviceMiningStats {
        val battery = context.registerReceiver(
            null,
            IntentFilter(Intent.ACTION_BATTERY_CHANGED),
        )

        val level = battery?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = battery?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        val batteryPercent = if (level >= 0 && scale > 0) {
            (level * 100 / scale).coerceIn(0, 100)
        } else {
            null
        }

        val status = battery?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val charging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
            status == BatteryManager.BATTERY_STATUS_FULL

        // Android has no portable API for a trustworthy CPU package temperature.
        // Battery temperature is available but must not be mislabeled as CPU temp.
        val batteryTenthsC = battery?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Int.MIN_VALUE)
        val temperature = batteryTenthsC
            ?.takeIf { it != Int.MIN_VALUE }
            ?.div(10.0)

        val runtime = startedAtElapsedRealtime?.let {
            (SystemClock.elapsedRealtime() - it).coerceAtLeast(0L)
        } ?: 0L

        return DeviceMiningStats(
            device = listOf(Build.MANUFACTURER, Build.MODEL)
                .filter { it.isNotBlank() }
                .joinToString(" "),
            abi = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown",
            cpuCores = Runtime.getRuntime().availableProcessors().coerceAtLeast(1),
            miningThreads = miningThreads.coerceAtLeast(0),
            hashRate = hashRate,
            temperatureCelsius = temperature,
            batteryPercent = batteryPercent,
            charging = charging,
            runtimeMillis = runtime,
        )
    }
}
