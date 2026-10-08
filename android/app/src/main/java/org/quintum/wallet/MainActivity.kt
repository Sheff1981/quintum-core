package org.quintum.wallet

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.delay
import org.quintum.wallet.core.NativeCore
import org.quintum.wallet.core.NodeService
import org.quintum.wallet.mining.DeviceMiningStatsCard
import org.quintum.wallet.mining.MiningController
import org.quintum.wallet.mining.MiningScreen

private val Navy = Color(0xFF101C38)
private val Blue = Color(0xFF2563EB)
private val Canvas = Color(0xFFF4F7FC)
private val Muted = Color(0xFF64748B)
private val Green = Color(0xFF12805C)

private enum class Page(val title: String, val subtitle: String, val symbol: String) {
    Network("Network", "Peers & synchronization", "◎"),
    Wallet("Wallet", "Addresses & payments", "◈"),
    Mining("Mining", "RandomX · CPU mining", "✦"),
    Device("Device", "Battery & thermal safety", "▣")
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(
                colorScheme = lightColorScheme(primary = Blue, background = Canvas, surface = Color.White)
            ) {
                Surface(modifier = Modifier.fillMaxSize(), color = Canvas) { QuintumHome() }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun QuintumHome() {
    var page by remember { mutableStateOf<Page?>(null) }
    val context = LocalContext.current
    val controller = remember { MiningController(context.applicationContext) }
    var stats by remember { mutableStateOf(org.quintum.wallet.mining.MiningUiState()) }
    var nodeRunning by remember { mutableStateOf(false) }
    var startRequested by remember { mutableStateOf(false) }
    var nodeError by remember { mutableStateOf("") }
    var startupSeconds by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) {
        while (true) {
            val result = withContext(Dispatchers.IO) {
                runCatching { Pair(NativeCore.nativeRunning(), controller.snapshot()) }
            }
            result.onSuccess { (running, snapshot) ->
                nodeRunning = running
                stats = snapshot
                if (running) {
                    startRequested = false
                    startupSeconds = 0
                    nodeError = ""
                } else {
                    nodeError = context.getSharedPreferences("node_status", android.content.Context.MODE_PRIVATE)
                        .getString("last_error", "") ?: ""
                    if (startRequested) startupSeconds++
                    if (nodeError.isNotBlank() || startupSeconds >= 30) {
                        if (startupSeconds >= 30 && nodeError.isBlank()) nodeError = "Node startup timed out. Check device logs."
                        startRequested = false
                        startupSeconds = 0
                    }
                }
            }
            result.onFailure { android.util.Log.e("QUINTUM-UI", "Status polling failed", it) }
            delay(1000)
        }
    }
    BackHandler(enabled = page != null) { page = null }
    Scaffold(containerColor = Canvas, topBar = {
        TopAppBar(
            title = {
                Column {
                    Text(if (page == null) "QUINTUM" else page!!.title, color = Navy, fontWeight = FontWeight.Bold)
                    if (page == null) Text("QMU  /  RANDOMX TESTNET", style = MaterialTheme.typography.labelSmall, color = Muted)
                }
            },
            navigationIcon = {
                if (page != null) TextButton(onClick = { page = null }) { Text("‹ Back") }
            },
            colors = TopAppBarDefaults.topAppBarColors(containerColor = Canvas)
        )
    }) { padding ->
        Column(modifier = Modifier.fillMaxSize().padding(padding).padding(horizontal = 18.dp)) {
            if (page == null) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(22.dp),
                    colors = CardDefaults.cardColors(containerColor = Navy)
                ) {
                    Column(modifier = Modifier.padding(22.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                        Text("NETWORK STATUS", style = MaterialTheme.typography.labelMedium, color = Color(0xFFAFC4F4))
                        Text(
                            if (nodeRunning) "Node running" else if (startRequested) "Connecting…" else "Node stopped",
                            style = MaterialTheme.typography.headlineSmall,
                            color = Color.White,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            if (nodeRunning) "QUINTUM Core is active" else if (startRequested) "Starting QUINTUM Core" else "Start the node when ready",
                            color = Color(0xFFD3E0FA),
                            style = MaterialTheme.typography.bodySmall
                        )
                        if (!nodeRunning) {
                            Button(onClick = {
                                startRequested = true
                                startupSeconds = 0
                                nodeError = ""
                                context.getSharedPreferences("node_status", android.content.Context.MODE_PRIVATE)
                                    .edit().putString("last_error", "").apply()
                                try {
                                    ContextCompat.startForegroundService(context, Intent(context, NodeService::class.java))
                                } catch (e: RuntimeException) {
                                    nodeError = "Android cannot start node service: ${e.javaClass.simpleName}"
                                    startRequested = false
                                }
                            }, enabled = !startRequested) { Text(if (startRequested) "Starting…" else "Start node") }
                        }
                        if (nodeError.isNotBlank()) Text(nodeError, color = Color(0xFFFFC9C9))
                        HorizontalDivider(color = Color(0xFF34466B))
                        Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                            StatusMetric("PEERS", "${stats.stats?.peers ?: 0}")
                            StatusMetric("HEIGHT", "${stats.stats?.blockHeight ?: "—"}")
                            StatusMetric("MINING", if (stats.running) "ACTIVE" else "OFF")
                        }
                    }
                }
                Spacer(Modifier.height(24.dp))
                Text("Your workspace", style = MaterialTheme.typography.titleLarge, color = Navy, fontWeight = FontWeight.Bold)
                Spacer(Modifier.height(12.dp))
                LazyVerticalGrid(
                    columns = GridCells.Fixed(2),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                    contentPadding = PaddingValues(bottom = 20.dp)
                ) {
                    items(Page.entries) { item ->
                        ElevatedCard(
                            onClick = { page = item },
                            modifier = Modifier.fillMaxWidth().height(158.dp),
                            shape = RoundedCornerShape(20.dp),
                            colors = CardDefaults.elevatedCardColors(containerColor = Color.White),
                            elevation = CardDefaults.elevatedCardElevation(defaultElevation = 1.dp)
                        ) {
                            Column(modifier = Modifier.fillMaxSize().padding(17.dp), verticalArrangement = Arrangement.SpaceBetween) {
                                Text(item.symbol, style = MaterialTheme.typography.headlineMedium, color = Blue)
                                Column {
                                    Text(item.title, style = MaterialTheme.typography.titleMedium, color = Navy, fontWeight = FontWeight.Bold)
                                    Spacer(Modifier.height(4.dp))
                                    Text(item.subtitle, style = MaterialTheme.typography.bodySmall, color = Muted)
                                }
                            }
                        }
                    }
                }
            } else when (page) {
                Page.Network -> {
                    DetailCard("Connection", if (nodeRunning) "Core running" else if (startRequested) "Connecting…" else "Node stopped")
                    if (nodeError.isNotBlank()) Text(nodeError, color = MaterialTheme.colorScheme.error)
                    Spacer(Modifier.height(12.dp))
                    DetailCard("Connected peers", "${stats.stats?.peers ?: 0}")
                    Spacer(Modifier.height(12.dp))
                    DetailCard("Block height", "${stats.stats?.blockHeight ?: "Unavailable"}")
                    Spacer(Modifier.height(12.dp))
                    DetailCard("Network difficulty", "${stats.stats?.networkDifficulty ?: "Unavailable"}")
                }
                Page.Wallet -> {
                    DetailCard("Wallet", "Not yet enabled in Android Testnet")
                    Spacer(Modifier.height(12.dp))
                    Text("Receiving, sending and balance will appear here after secure QUINTUM wallet integration. No simulated funds.", color = Muted)
                }
                Page.Mining -> MiningScreen()
                Page.Device -> stats.stats?.let { DeviceMiningStatsCard(it) }
                null -> Unit
            }
        }
    }
}

@Composable
private fun StatusMetric(label: String, value: String) {
    Column {
        Text(label, color = Color(0xFFAFC4F4), style = MaterialTheme.typography.labelSmall)
        Spacer(Modifier.height(4.dp))
        Text(value, color = Color.White, fontWeight = FontWeight.Bold, style = MaterialTheme.typography.titleMedium)
    }
}

@Composable
private fun DetailCard(label: String, value: String) {
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = Color.White)
    ) {
        Column(modifier = Modifier.padding(20.dp)) {
            Text(label, color = Muted, style = MaterialTheme.typography.labelLarge)
            Spacer(Modifier.height(8.dp))
            Text(value, color = Navy, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
        }
    }
}
