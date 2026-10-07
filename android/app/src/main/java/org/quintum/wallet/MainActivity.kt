package org.quintum.wallet

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.quintum.wallet.mining.DeviceMiningStats
import org.quintum.wallet.mining.DeviceMiningStatsCard
import org.quintum.wallet.core.NativeCore

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { QuintumApp() }
    }
}

@Composable
private fun QuintumApp() {
    MaterialTheme {
        Surface(modifier = Modifier.fillMaxSize()) {
            Column(
                modifier = Modifier.padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text("QUINTUM", style = MaterialTheme.typography.headlineLarge)
                Text("Android Core")
                Text("Native node connection: not started")
                DeviceMiningStatsCard(
                    DeviceMiningStats(
                        device = "Device telemetry pending",
                        abi = android.os.Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown",
                        cpuCores = Runtime.getRuntime().availableProcessors(),
                        miningThreads = NativeCore.nativeMiningThreads(),
                        hashRate = NativeCore.nativeMiningHashRate().takeIf { it > 0.0 },
                        temperatureCelsius = null,
                        batteryPercent = null,
                        charging = false,
                        runtimeMillis = NativeCore.nativeMiningRuntimeMillis(),
                        foundBlocks = NativeCore.nativeMiningFoundBlocks(),
                        networkDifficulty = NativeCore.nativeDifficulty().takeIf { it >= 0.0 },
                        blockHeight = NativeCore.nativeBlockHeight().takeIf { it >= 0L },
                        peers = NativeCore.nativePeerCount(),
                    ),
                )
            }
        }
    }
}
