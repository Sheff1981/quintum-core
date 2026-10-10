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
    private val powerManager =
        context.getSystemService(Context.POWER_SERVICE) as PowerManager

    suspend fun start(payoutAddress: String, threads: Int): Int =
        withContext(Dispatchers.IO) {
            NativeCore.nativeStartMining(payoutAddress.trim(), threads)
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
        val batteryTemperatureTenths = runCatching {
            context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
                ?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Int.MIN_VALUE)
        }.getOrNull()
        val batteryTooHot = batteryTemperatureTenths != null &&
            batteryTemperatureTenths != Int.MIN_VALUE && batteryTemperatureTenths >= 450

        val miningRunning = NativeCore.nativeMiningRunning()
        if (!miningRunning) thermalStopRequested = false
        if (miningRunning && (ThermalGuard.shouldStop(thermal) || batteryTooHot) && !thermalStopRequested) {
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
