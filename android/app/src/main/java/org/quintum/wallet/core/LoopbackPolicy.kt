package org.quintum.wallet.core

import java.net.InetAddress

object LoopbackPolicy {
    fun isAllowedHost(host: String): Boolean = try {
        InetAddress.getByName(host).isLoopbackAddress
    } catch (_: Exception) {
        false
    }
}
