package org.quintum.wallet.core

import org.json.JSONObject

/** Read-only presentation of native measurements. Missing values remain unavailable. */
data class DiagnosticsSnapshot(val values: Map<String, String> = emptyMap()) {
    fun value(key: String): String = values[key]?.takeIf { it.isNotBlank() } ?: "Unavailable"
    val stage: String get() = value("stage")
    fun report(coreRunning: Boolean, activePeers: Long?, knownPeers: Long): String = buildString {
        appendLine("QUINTUM local diagnostics")
        appendLine("UTC: ${java.time.Instant.now()}")
        appendLine("Source revision: ${org.quintum.wallet.BuildConfig.SOURCE_REVISION}")
        appendLine("Core running: $coreRunning")
        appendLine("Active peers: ${activePeers ?: "Unavailable"}")
        appendLine("Known peers: $knownPeers")
        for ((key, value) in values) appendLine("$key: $value")
    }
}

object DiagnosticsPresentation {
    fun parse(json: String): DiagnosticsSnapshot {
        val objectValue = JSONObject(json)
        return DiagnosticsSnapshot(objectValue.keys().asSequence().associateWith { key ->
            if (objectValue.isNull(key)) "Unavailable" else objectValue.get(key).toString()
        })
    }
    fun recentEvents(jsonl: String): List<String> = jsonl.lineSequence().filter { it.isNotBlank() }
        .takeLastLines(100).map { line ->
            runCatching {
                val event = JSONObject(line)
                event.keys().asSequence().joinToString(" · ") { key -> "$key: ${event.opt(key)}" }
            }.getOrDefault(line)
        }
    private fun Sequence<String>.takeLastLines(count: Int): List<String> {
        val recent = ArrayDeque<String>()
        for (line in this) { if (recent.size == count) recent.removeFirst(); recent.addLast(line) }
        return recent.toList()
    }
}
