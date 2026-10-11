package org.quintum.wallet.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LoopbackPolicyTest {
    @Test fun acceptsIpv4Loopback() {
        assertTrue(LoopbackPolicy.isAllowedHost("127.0.0.1"))
    }

    @Test fun acceptsIpv6Loopback() {
        assertTrue(LoopbackPolicy.isAllowedHost("::1"))
    }

    @Test fun rejectsExternalAddress() {
        assertFalse(LoopbackPolicy.isAllowedHost("8.8.8.8"))
    }

    @Test fun rejectsInvalidHost() {
        assertFalse(LoopbackPolicy.isAllowedHost("not a host.invalid"))
    }
}
