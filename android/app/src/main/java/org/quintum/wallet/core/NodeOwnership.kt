package org.quintum.wallet.core

/** Serializes process-wide native lifetime across replaced Service instances. */
internal class NodeOwnership {
    private var owner: Any? = null

    @Synchronized
    fun start(token: Any, isDestroyed: () -> Boolean, action: () -> Int): Int {
        if (isDestroyed()) return -1
        val result = action()
        if (result == 0 || result == 2) owner = token
        return result
    }

    @Synchronized
    fun stop(token: Any, action: () -> Unit) {
        if (owner !== token) return
        try {
            action()
        } finally {
            owner = null
        }
    }
}
