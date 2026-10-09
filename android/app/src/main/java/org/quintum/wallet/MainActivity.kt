package org.quintum.wallet

import android.content.Intent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.net.InetSocketAddress
import java.net.Socket
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
private fun p2pDiagnosticMessage(code: Int): String {
    val peerErrors = listOf(
        "none", "socket runtime failed", "DNS resolve failed", "socket creation failed",
        "bind failed", "listen failed", "accept failed", "TCP connect failed",
        "connection timed out", "send failed", "receive failed",
        "wire framing or network magic mismatch", "malformed version",
        "unsupported protocol version", "self connection",
        "unexpected P2P message", "malformed ping", "encrypted transport failed",
        "proxy negotiation failed", "proxy rejected"
    )
    return when {
        code == -1 -> "Native core unavailable"
        code == 0 -> "No connection attempt recorded"
        code == 10 -> "Selecting a peer / connecting"
        code == 4000 -> "P2P handshake and peer setup completed"
        code == 4001 -> "Handshake completed; negotiating chainwork"
        code == 4002 -> "Handshake completed; initial blockchain synchronization"
        code == 4003 -> "Blockchain synchronization completed; exchanging peer addresses"
        code == 4004 -> "Exchanging mempool after blockchain synchronization"
        code == 3001 || code == 3002 -> "Handshake succeeded, but sync / peer setup failed (code $code)"
        code in 2000..2399 -> {
            val discovery = (code - 2000) / 100
            val peer = (code - 2000) % 100
            val phase = when (discovery) {
                1 -> "No eligible peer (retry backoff)"
                2 -> "Outbound connection / handshake failed"
                3 -> "Peer database save failed"
                else -> "Discovery failed"
            }
            val detail = peerErrors.getOrNull(peer) ?: "error $peer"
            "$phase: $detail (code $code)"
        }
        code in 1000..1099 -> {
            val peer = code - 1000
            "Reconnection failed: ${peerErrors.getOrNull(peer) ?: "error $peer"} (code $code)"
        }
        else -> "P2P diagnostic code: $code"
    }
}


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
    var knownAddresses by remember { mutableLongStateOf(0L) }
    var p2pDiagnostic by remember { mutableIntStateOf(0) }
    var peerDetails by remember { mutableStateOf("No active P2P peer") }
    var connectElapsedMs by remember { mutableLongStateOf(0L) }
    var lastConnectError by remember { mutableIntStateOf(0) }
    var connectAttempts by remember { mutableLongStateOf(0L) }
    var lastStartupMs by remember { mutableLongStateOf(0L) }
    var showDiagnostics by remember { mutableStateOf(false) }
    var showCrashReport by remember { mutableStateOf(false) }
    val crashReport = remember { runCatching { context.filesDir.resolve("diagnostics/last-java-crash.txt").takeIf { it.isFile }?.readText() }.getOrNull() }
    var tcpProbeResult by remember { mutableStateOf("Not tested") }
    var tcpProbeRunning by remember { mutableStateOf(false) }
    var diagnosticUpdatedAt by remember { mutableLongStateOf(0L) }
    val diagnosticScope = rememberCoroutineScope()
    var startRequested by remember { mutableStateOf(false) }
    var nodeError by remember { mutableStateOf("") }
    var startupSeconds by remember { mutableIntStateOf(0) }
    LaunchedEffect(Unit) {
        while (true) {
            val result = withContext(Dispatchers.IO) {
                runCatching { Triple(Triple(NativeCore.nativeRunning(), controller.snapshot(), NativeCore.nativeKnownAddressCount()), NativeCore.nativeP2pDiagnostic(), NativeCore.nativePeerDetails()) }
            }
            result.onSuccess { (snapshotTriple, diagnostic, details) ->
                    peerDetails = details
                    connectElapsedMs = NativeCore.nativeConnectElapsedMs()
                    lastConnectError = NativeCore.nativeLastConnectError()
                    connectAttempts = NativeCore.nativeConnectAttempts()
                    val (running, snapshot, known) = snapshotTriple
                    p2pDiagnostic = diagnostic
                    diagnosticUpdatedAt = System.currentTimeMillis()
                knownAddresses = known
                lastStartupMs = context.getSharedPreferences("node_status", android.content.Context.MODE_PRIVATE)
                    .getLong("last_start_duration_ms", 0L)
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
                    if (nodeError.isNotBlank() || startupSeconds >= 240) {
                        if (startupSeconds >= 240 && nodeError.isBlank()) nodeError = "Node startup exceeded 4 minutes. Check device logs."
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
        Column(modifier = Modifier.fillMaxSize().padding(padding).padding(horizontal = 18.dp).verticalScroll(rememberScrollState())) {
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
                            if (nodeRunning) "QUINTUM Core is active" else if (startRequested) "Initializing local blockchain (network connection follows)" else "Start the node when ready",
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
                    modifier = Modifier.fillMaxWidth().height(360.dp),
                    userScrollEnabled = false,
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
                    DetailCard("Connection", if (nodeRunning) "Core running" else if (startRequested) "Starting core (${startupSeconds}s)…" else "Node stopped")
                    if (nodeError.isNotBlank()) Text(nodeError, color = MaterialTheme.colorScheme.error)
                    if (crashReport != null) {
                        Spacer(Modifier.height(12.dp))
                        Text("Previous app crash recorded", color = MaterialTheme.colorScheme.error)
                        TextButton(onClick = { showCrashReport = !showCrashReport }) { Text(if (showCrashReport) "Hide crash report" else "View last crash report") }
                        if (showCrashReport) {
                            Button(onClick = {
                                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                                clipboard.setPrimaryClip(ClipData.newPlainText("QUINTUM crash report", crashReport))
                            }) { Text("Copy crash report") }
                            Text(crashReport.take(4000), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                    Spacer(Modifier.height(12.dp))
                    DetailCard("Network status", when {
                        !nodeRunning -> "Node is not running"
                        (stats.stats?.peers ?: 0L) > 0L -> "Connected to QUINTUM Testnet"
                        else -> "Searching for network peers"
                    })
                    Spacer(Modifier.height(12.dp))
                    DetailCard("Block height", "${stats.stats?.blockHeight ?: "Unavailable"}")
                    Spacer(Modifier.height(12.dp))
                    TextButton(onClick = { showDiagnostics = !showDiagnostics }) {
                        Text(if (showDiagnostics) "Hide technical details" else "Show technical details")
                    }
                    if (showDiagnostics) {
                        Text("QUINTUM P2P diagnostic report", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                        Text("Read-only local node diagnostics. TCP reachability is not a P2P handshake.", color = Muted)
                        Spacer(Modifier.height(8.dp))
                        val report = buildString {
                            appendLine("QUINTUM Android / RandomX Testnet")
                            appendLine("Timestamp UTC: ${java.time.Instant.ofEpochMilli(diagnosticUpdatedAt)}")
                            appendLine("Core running: $nodeRunning")
                            appendLine("P2P code: $p2pDiagnostic")
                            appendLine("Pending P2P attempt ms: $connectElapsedMs")
                            appendLine("P2P attempts: $connectAttempts")
                            appendLine("Last P2P failure code: $lastConnectError")
                            appendLine("P2P status: ${p2pDiagnosticMessage(p2pDiagnostic)}")
                            appendLine("Remote peer details:")
                            appendLine(peerDetails)
                            appendLine("Known addresses: $knownAddresses")
                            appendLine("Connected peers: ${stats.stats?.peers ?: 0}")
                            appendLine("Local block height: ${stats.stats?.blockHeight ?: "unavailable"}")
                            appendLine("TCP probe: $tcpProbeResult")
                            appendLine("Startup duration ms: $lastStartupMs")
                            appendLine("Core error: ${nodeError.ifBlank { "none" }}")
                        }
                        Button(onClick = {
                            val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                            clipboard.setPrimaryClip(ClipData.newPlainText("QUINTUM diagnostics", report))
                        }) { Text("Copy diagnostic report") }
                        Spacer(Modifier.height(12.dp))
                        DetailCard("P2P connection diagnostic", p2pDiagnosticMessage(p2pDiagnostic))
                        DetailCard("Connected node (verified handshake)", peerDetails)
                        Spacer(Modifier.height(12.dp))
                        Text("VPS transport diagnostic", style = MaterialTheme.typography.titleSmall)
                        Text("Tests TCP reachability only; does not verify the QUINTUM P2P handshake.", color = Muted)
                        Text("Result: $tcpProbeResult", color = if (tcpProbeResult.startsWith("TCP failed")) MaterialTheme.colorScheme.error else Navy)
                        Spacer(Modifier.height(8.dp))
                        Button(onClick = {
                            tcpProbeRunning = true
                            tcpProbeResult = "Testing..."
                            diagnosticScope.launch {
                                tcpProbeResult = withContext(Dispatchers.IO) {
                                    val start = android.os.SystemClock.elapsedRealtime()
                                    try {
                                        Socket().use { socket ->
                                            socket.connect(InetSocketAddress("212.193.15.139", 39444), 5000)
                                        }
                                        "TCP connected in ${android.os.SystemClock.elapsedRealtime() - start} ms; handshake not tested"
                                    } catch (e: Exception) {
                                        "TCP failed: ${e.javaClass.simpleName}: ${e.message ?: "No details"}"
                                    }
                                }
                                tcpProbeRunning = false
                            }
                        }, enabled = !tcpProbeRunning) {
                            Text(if (tcpProbeRunning) "Testing VPS..." else "Test VPS connection")
                        }

                        Spacer(Modifier.height(12.dp))
                        DetailCard("Connected peers", "${stats.stats?.peers ?: 0}")
                        Spacer(Modifier.height(12.dp))
                        DetailCard("Known peer addresses", "$knownAddresses")
                        Spacer(Modifier.height(12.dp))
                        Spacer(Modifier.height(12.dp))
                        if (lastStartupMs > 0L) {
                            Spacer(Modifier.height(12.dp))
                            DetailCard("Last core startup", "${lastStartupMs / 1000L} s")
                        }
                        if (nodeRunning && knownAddresses == 0L) {
                            Text("No bootstrap addresses found. Peer discovery needs attention.", color = MaterialTheme.colorScheme.error)
                        }
                        Spacer(Modifier.height(12.dp))
                        DetailCard("Network difficulty", "${stats.stats?.networkDifficulty ?: "Unavailable"}")
                    }
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
