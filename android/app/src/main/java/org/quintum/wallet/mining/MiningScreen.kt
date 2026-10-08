package org.quintum.wallet.mining

import android.content.Context
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import org.quintum.wallet.core.NativeCore
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.launch

@Composable
fun MiningScreen() {
    val context = LocalContext.current
    val controller = remember { MiningController(context.applicationContext) }
    val scope = rememberCoroutineScope()
    val payoutPreferences = remember(context) { context.getSharedPreferences("mining_payout", Context.MODE_PRIVATE) }
    var payout by rememberSaveable { mutableStateOf(payoutPreferences.getString("address", "") ?: "") }
    var threadsText by rememberSaveable { mutableStateOf((Runtime.getRuntime().availableProcessors() / 2).coerceAtLeast(1).toString()) }
    var state by remember { mutableStateOf(MiningUiState()) }
    var error by remember { mutableStateOf<String?>(null) }
    var nodeRunning by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        while (true) {
            val result = withContext(Dispatchers.IO) {
                runCatching { Pair(controller.snapshot(), NativeCore.nativeRunning()) }
            }
            result.onSuccess { (snapshot, running) ->
                state = snapshot
                nodeRunning = running
            }
            result.onFailure { error = "Unable to read node status." }
            delay(1000)
        }
    }

    Column(modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Mining", style = MaterialTheme.typography.headlineMedium)
        Text("RandomX Testnet · real Proof-of-Work")
        Text(if (nodeRunning) "Node: running" else "Node: connecting…")
        Text("Peers: ${state.stats?.peers ?: 0} · Block height: ${state.stats?.blockHeight ?: 0}")
        if (nodeRunning && (state.stats?.peers ?: 0L) == 0L) {
            Text("No connected peers. Blocks mined while disconnected may be replaced when the node synchronizes.", color = MaterialTheme.colorScheme.error)
        }
        OutlinedTextField(value = payout, onValueChange = { payout = it.trim() }, enabled = !state.running, label = { Text("QMU payout address") }, supportingText = { Text("Public address only. You may mine to a wallet on another device.") }, singleLine = true)
        TextButton(onClick = { payout = ""; payoutPreferences.edit().remove("address").apply() }, enabled = !state.running && payout.isNotEmpty()) { Text("Clear saved payout address") }
        OutlinedTextField(value = threadsText, onValueChange = { threadsText = it.filter(Char::isDigit) }, enabled = !state.running, label = { Text("Mining threads") }, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), singleLine = true)
        Button(onClick = {
            error = null
            scope.launch {
                if (state.running) controller.stop() else {
                    val threads = threadsText.toIntOrNull()?.coerceAtLeast(1) ?: 1
                    error = when (controller.start(payout, threads)) {
                        0 -> {
                            payoutPreferences.edit().putString("address", payout.trim()).apply()
                            null
                        }
                        1 -> "Payout address is required."
                        2 -> "QUINTUM node is not running."
                        3 -> "Mining is already running."
                        4 -> "Invalid RandomX Testnet QMU address."
                        else -> "Unable to start mining."
                    }
                }
                state = withContext(Dispatchers.IO) { controller.snapshot() }
            }
        }) { Text(if (state.running) "Stop mining" else "Start mining") }
        Text("Thermal status: " + state.thermalStatus)
        if (state.stoppedForThermalSafety) Text("Mining stopped for thermal safety.", color = MaterialTheme.colorScheme.error)
        error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
        state.stats?.let { DeviceMiningStatsCard(it) }
    }
}