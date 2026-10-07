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
}
