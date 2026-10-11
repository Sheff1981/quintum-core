package org.quintum.wallet.core

import org.junit.Assert.assertEquals
import org.junit.Test

class NodeOwnershipTest {
    @Test fun oldServiceCannotStopNodeAdoptedByReplacement() {
        val ownership = NodeOwnership()
        val oldService = Any()
        val replacement = Any()
        var stops = 0
        assertEquals(0, ownership.start(oldService, { false }) { 0 })
        assertEquals(2, ownership.start(replacement, { false }) { 2 })
        ownership.stop(oldService) { stops++ }
        assertEquals(0, stops)
        ownership.stop(replacement) { stops++ }
        ownership.stop(replacement) { stops++ }
        assertEquals(1, stops)
    }

    @Test fun destroyedServiceCannotStartNativeCore() {
        val ownership = NodeOwnership()
        var starts = 0
        assertEquals(-1, ownership.start(Any(), { true }) { starts++; 0 })
        assertEquals(0, starts)
    }

    @Test fun failedReplacementDoesNotTakeOwnership() {
        val ownership = NodeOwnership()
        val owner = Any()
        var stops = 0
        ownership.start(owner, { false }) { 0 }
        ownership.start(Any(), { false }) { 3 }
        ownership.stop(owner) { stops++ }
        assertEquals(1, stops)
    }
}
