package org.quintum.wallet.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class NodeLaunchContractTest {
    @Test fun requestsEphemeralLoopbackRpcWithoutNetworkOverrides() {
        val args = NodeLaunchContract(
            dataDir = "/data/user/0/org.quintum.wallet/files/core",
            executablePath = "/data/app/libquintumd.so",
        ).arguments()

        assertEquals(
            listOf("--datadir", "/data/user/0/org.quintum.wallet/files/core", "--rpc", "--rpc-port", "0"),
            args,
        )
        assertFalse(args.any { it.contains("magic", ignoreCase = true) })
        assertFalse(args.any { it.contains("genesis", ignoreCase = true) })
    }
}
