package org.quintum.wallet

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.delay
import org.quintum.wallet.core.NativeCore
import org.quintum.wallet.core.NodeService
import org.quintum.wallet.mining.DeviceMiningStatsCard
import org.quintum.wallet.mining.MiningController
import org.quintum.wallet.mining.MiningScreen

private enum class Page(val title: String, val subtitle: String) {
    Network("Network", "Peers and blockchain sync"),
    Wallet("Wallet", "Testnet wallet status"),
    Mining("Mining", "RandomX proof-of-work"),
    Device("Device", "CPU, battery and thermals")
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ContextCompat.startForegroundService(this, Intent(this, NodeService::class.java))
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) { QuintumHome() }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun QuintumHome() {
    var page by remember { mutableStateOf<Page?>(null) }
    val controller = remember { MiningController(androidx.compose.ui.platform.LocalContext.current.applicationContext) }
    var stats by remember { mutableStateOf(controller.snapshot()) }
    var nodeRunning by remember { mutableStateOf(false) }
    LaunchedEffect(Unit) {
        while (true) {
            nodeRunning = NativeCore.nativeRunning()
            stats = controller.snapshot()
            delay(1000)
        }
    }
    BackHandler(enabled = page != null) { page = null }
    Scaffold(topBar = {
        TopAppBar(
            title = { Text(page?.title ?: "QUINTUM") },
            navigationIcon = {
                if (page != null) TextButton(onClick = { page = null }) { Text("Back") }
            }
        )
    }) { padding ->
        Column(modifier = Modifier.fillMaxSize().padding(padding).padding(horizontal = 16.dp)) {
            if (page == null) {
                Text("QMU · RandomX Testnet", style = MaterialTheme.typography.titleMedium)
                Text(
                    if (nodeRunning) "Node running · ${stats.stats?.peers ?: 0} peers"
                    else "Connecting to QUINTUM network…",
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(Modifier.height(20.dp))
                LazyVerticalGrid(
                    columns = GridCells.Fixed(2),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    items(Page.entries) { item ->
                        ElevatedCard(
                            onClick = { page = item },
                            modifier = Modifier.fillMaxWidth().height(142.dp)
                        ) {
                            Column(modifier = Modifier.padding(16.dp)) {
                                Text(item.title, style = MaterialTheme.typography.titleLarge)
                                Spacer(Modifier.height(8.dp))
                                Text(item.subtitle, style = MaterialTheme.typography.bodySmall)
                            }
                        }
                    }
                }
            } else when (page) {
                Page.Network -> {
                    Text(if (nodeRunning) "Node running" else "Connecting…", style = MaterialTheme.typography.titleLarge)
                    Spacer(Modifier.height(16.dp))
                    Text("Peers: ${stats.stats?.peers ?: 0}")
                    Text("Block height: ${stats.stats?.blockHeight ?: "Unavailable"}")
                    Text("Difficulty: ${stats.stats?.networkDifficulty ?: "Unavailable"}")
                }
                Page.Wallet -> {
                    Text("Wallet not yet available in Android Testnet.", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(12.dp))
                    Text("No balance, receiving address or spending functionality is enabled in this build.")
                }
                Page.Mining -> MiningScreen()
                Page.Device -> {
                    stats.stats?.let { DeviceMiningStatsCard(it) }
                }
                null -> Unit
            }
        }
    }
}
