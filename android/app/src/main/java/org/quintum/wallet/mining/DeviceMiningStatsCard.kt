package org.quintum.wallet.mining

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun DeviceMiningStatsCard(
    stats: DeviceMiningStats,
    modifier: Modifier = Modifier,
) {
    Card(modifier = modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text("Device Mining Stats", style = MaterialTheme.typography.titleLarge)
            Text(stats.device, style = MaterialTheme.typography.titleMedium)
            StatRow("Architecture", stats.abi)
            StatRow("CPU cores", stats.cpuCores.toString())
            StatRow("Mining threads", stats.miningThreads.toString())
            StatRow("Hashrate", MiningStatsFormatter.hashRate(stats.hashRate))
            StatRow("Device temperature", MiningStatsFormatter.temperature(stats.temperatureCelsius))
            StatRow("Battery", MiningStatsFormatter.battery(stats.batteryPercent, stats.charging))
            StatRow("Mining runtime", MiningStatsFormatter.runtime(stats.runtimeMillis))
            StatRow("Blocks found", stats.foundBlocks.toString())
            StatRow("Network difficulty", stats.networkDifficulty?.let { String.format(java.util.Locale.US, "%.2f", it) } ?: "Unavailable")
            StatRow("Block height", stats.blockHeight?.toString() ?: "Unavailable")
            StatRow("Peers", stats.peers.toString())
        }
    }
}

@Composable
private fun StatRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value)
    }
}
