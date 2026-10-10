package org.quintum.wallet.mining

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.PowerManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.quintum.wallet.core.NativeCore

class MiningController(
    private val context: Context,
) {
    private val statsReader = DeviceMiningStatsReader(context)
    private val thermalStopScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    @Volatile private var thermalStopRequested = false
    private companion object {
        @Volatile var cooldownActive = false
    }
    private val powerManager =
        context.getSystemService(Context.POWER_SERVICE) as PowerManager

    private fun batteryTooHot(): Boolean = runCatching {
        val tenths = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
            ?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Int.MIN_VALUE)
        tenths != null && tenths != Int.MIN_VALUE && tenths >= 450
    }.getOrDefault(false)

    private fun cooldownBlocked(thermal: Int): Boolean {
        val temp = runCatching {
            context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
                ?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Int.MIN_VALUE)
        }.getOrNull()
        synchronized(MiningController::class.java) {
            if (ThermalGuard.shouldStop(thermal) ||
                (temp != null && temp != Int.MIN_VALUE && temp >= 450)) {
                cooldownActive = true
            } else if (cooldownActive && temp != null && temp != Int.MIN_VALUE &&
                temp <= 400 && (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q ||
                    thermal <= PowerManager.THERMAL_STATUS_LIGHT)) {
                cooldownActive = false
            }
            return cooldownActive
        }
    }

    private fun batteryTooLow(): Boolean = runCatching {
        val battery = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val level = battery?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = battery?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        val status = battery?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val charging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
            status == BatteryManager.BATTERY_STATUS_FULL
        !charging && level >= 0 && scale > 0 && level.toLong() * 100 < scale.toLong() * 15
    }.getOrDefault(false)

    private fun unsafeToMine(): Boolean =
        (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q &&
            ThermalGuard.shouldStop(powerManager.currentThermalStatus)) || batteryTooHot() || batteryTooLow()

    suspend fun start(payoutAddress: String, threads: Int): Int =
        withContext(Dispatchers.IO) {
            // Prevent starting a hot device, not only stopping it on the next UI poll.
            if (unsafeToMine()) 5 else NativeCore.nativeStartMining(payoutAddress.trim(), threads)
        }

    suspend fun stop() = withContext(Dispatchers.IO) {
        NativeCore.nativeStopMining()
    }

    fun snapshot(): MiningUiState {
        val thermal = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            powerManager.currentThermalStatus
        } else {
            -1
        }

        // Battery temperature is reported in tenths of a degree Celsius.
        // Some devices omit it; never treat an unavailable reading as safe/unsafe.
        val batteryTooHot = batteryTooHot()

        val miningRunning = NativeCore.nativeMiningRunning()
        if (!miningRunning) thermalStopRequested = false
        if (miningRunning && (ThermalGuard.shouldStop(thermal) || batteryTooHot || batteryTooLow()) && !thermalStopRequested) {
            thermalStopRequested = true
            // JNI stop joins the mining thread; never block the Compose/UI thread.
            thermalStopScope.launch { NativeCore.nativeStopMining() }
        }

        val stats = statsReader.read(
            miningThreads = NativeCore.nativeMiningThreads(),
            hashRate = NativeCore.nativeMiningHashRate().takeIf { it > 0.0 },
            startedAtElapsedRealtime = null,
        ).copy(
            runtimeMillis = NativeCore.nativeMiningRuntimeMillis(),
            foundBlocks = NativeCore.nativeMiningFoundBlocks(),
            networkDifficulty = NativeCore.nativeDifficulty().takeIf { it >= 0.0 },
            blockHeight = NativeCore.nativeBlockHeight().takeIf { it >= 0L },
            peers = NativeCore.nativePeerCount(),
        )

        return MiningUiState(
            running = NativeCore.nativeMiningRunning(),
            stats = stats,
            thermalStatus = ThermalGuard.label(thermal),
            stoppedForThermalSafety = ThermalGuard.shouldStop(thermal) || batteryTooHot,
        )
    }
}

data class MiningUiState(
    val running: Boolean = false,
    val stats: DeviceMiningStats? = null,
    val thermalStatus: String = "Unavailable",
    val stoppedForThermalSafety: Boolean = false,
)
