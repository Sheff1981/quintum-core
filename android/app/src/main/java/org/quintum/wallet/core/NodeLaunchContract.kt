package org.quintum.wallet.core

/**
 * Immutable Android launch contract for the bundled QUINTUM daemon.
 *
 * The daemon itself remains authoritative for network parameters. Android only
 * supplies runtime paths and requests loopback RPC. No consensus parameter is
 * duplicated here.
 */
data class NodeLaunchContract(
    val dataDir: String,
    val executablePath: String,
    val extraArguments: List<String> = emptyList(),
) {
    fun arguments(): List<String> = buildList {
        add("--datadir")
        add(dataDir)
        add("--rpc")
        add("--rpc-port")
        add("0")
        addAll(extraArguments)
    }
}
