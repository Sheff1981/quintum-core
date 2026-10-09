package org.quintum.wallet.diagnostics

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.quintum.wallet.core.DiagnosticsSnapshot

/** The containing page supplies the single scroll owner. No JNI calls during composition. */
@Composable
fun DiagnosticsScreen(snapshot: DiagnosticsSnapshot, coreRunning: Boolean, activePeers: Long?, knownPeers: Long,
    events: List<String>, actionStatus: String = "", currentError: String = "", onCopy: () -> Unit, onExport: () -> Unit, onClear: () -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Local diagnostics", style = MaterialTheme.typography.titleLarge)
        Text("Reports stay on this device. Export saves a log to a location you choose.")
        Button(onClick = onCopy) { Text("Copy report") }
        Button(onClick = onExport) { Text("Export log") }
        OutlinedButton(onClick = onClear) { Text("Clear diagnostics") }
        if (actionStatus.isNotBlank()) Text(actionStatus)
        Text("Core: ${if (coreRunning) "Running" else "Stopped"} · Active peers: ${activePeers ?: "Unavailable"} · Known peers: $knownPeers")
        Text("Current core / service error: ${currentError.ifBlank { "None reported" }}")
        Text("RandomX operation: " + when (snapshot.value("randomx_active")) { "1" -> "Initializing cache"; "2" -> "Verifying hash"; "0" -> "Idle"; else -> "Unavailable" })
        val fields = listOf(
            "Stage" to "stage", "Stage elapsed (ms)" to "stage_elapsed_ms", "Connection duration (ms)" to "connection_duration_ms", "Peer endpoint" to "peer_endpoint",
            "Protocol version" to "protocol_version", "Remote height" to "remote_height", "Local height" to "local_height",
            "Connection attempts" to "attempt_count", "Retries" to "retry_count", "Last error code" to "last_error_code",
            "Last error description" to "last_error_description", "Headers received" to "headers_received",
            "Headers verified" to "headers_verified", "Headers rejected" to "headers_rejected", "Headers not yet validated" to "headers_unvalidated", "Header batch size" to "header_batch_size",
            "Header verification (ms)" to "headers_verify_ms", "RandomX calls" to "randomx_calls",
            "RandomX verification (ms)" to "randomx_verify_ms", "RandomX cache (ms)" to "randomx_cache_ms", "Current RandomX operation (ms)" to "randomx_operation_elapsed_ms",
            "Blocks requested" to "blocks_requested", "Blocks received" to "blocks_received", "Blocks accepted" to "blocks_accepted",
            "Blocks rejected" to "blocks_rejected", "Block verification (ms)" to "block_verify_ms",
            "Last network progress (ms)" to "last_network_progress_ms", "Last successful sync (UTC)" to "last_sync_utc",
            "Throughput (blocks/s)" to "throughput_blocks_per_second", "Log error" to "log_error")
        for ((label, key) in fields) {
            Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(12.dp)) {
                Text(label, style = MaterialTheme.typography.labelLarge)
                Text(snapshot.value(key))
            } }
        }
        Text("Recent events (up to 100)", style = MaterialTheme.typography.titleMedium)
        if (events.isEmpty()) Text("No recorded events")
        for (event in events) Text(event, style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(16.dp))
    }
}
