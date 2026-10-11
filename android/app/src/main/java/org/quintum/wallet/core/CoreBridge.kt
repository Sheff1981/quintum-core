package org.quintum.wallet.core

/**
 * Boundary between Android presentation code and QUINTUM Core.
 *
 * The production implementation will own the native node process and use only
 * loopback authenticated JSON-RPC. Keeping this interface small prevents UI
 * code from becoming a second implementation of consensus or wallet rules.
 */
interface CoreBridge {
    suspend fun start(): CoreStatus
    suspend fun stop()
    suspend fun status(): CoreStatus
}

data class CoreStatus(
    val running: Boolean,
    val rpcReady: Boolean,
    val blocks: Long? = null,
    val headers: Long? = null,
    val peers: Int? = null,
)
