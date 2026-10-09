package org.quintum.wallet.core

/**
 * Minimal JNI ownership boundary for the native QUINTUM node.
 *
 * Consensus, wallet, P2P and RandomX remain in C++. Kotlin controls lifecycle
 * only and never reimplements validation rules.
 */
object NativeCore {
    init {
        System.loadLibrary("quintum_android")
    }

    external fun nativeInitializeDiagnostics(dataDirectory: String)
    external fun nativeDiagnostics(): String
    external fun nativeDiagnosticLog(): String
    external fun nativeClearDiagnosticLog(): Boolean
    external fun nativeServiceEvent(name: String)

    external fun nativeStart(dataDirectory: String): Int
    external fun nativeStop()
    external fun nativeRunning(): Boolean

    external fun nativeStartMining(payoutAddress: String, threads: Int): Int
    external fun nativeStopMining()
    external fun nativeMiningRunning(): Boolean
    external fun nativeMiningAttempts(): Long
    external fun nativeMiningFoundBlocks(): Long
    external fun nativeMiningThreads(): Int
    external fun nativeMiningRuntimeMillis(): Long
    external fun nativeMiningHashRate(): Double

    external fun nativeBlockHeight(): Long
    external fun nativePeerCount(): Long
    external fun nativeKnownAddressCount(): Long
    external fun nativeP2pDiagnostic(): Int
    external fun nativePeerDetails(): String
    external fun nativeConnectElapsedMs(): Long
    external fun nativeLastConnectError(): Int
    external fun nativeConnectAttempts(): Long
    external fun nativeDifficulty(): Double
}

