package org.quintum.wallet.update

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class UpdatePolicyTest {
    @Test fun acceptsOnlyNewerVersionCodes() {
        assertTrue(UpdatePolicy.isUpgrade(7, 8))
        assertFalse(UpdatePolicy.isUpgrade(7, 7))
        assertFalse(UpdatePolicy.isUpgrade(7, 6))
    }

    @Test fun acceptsCanonicalLowercaseSha256Only() {
        assertTrue(UpdatePolicy.isValidSha256("a".repeat(64)))
        assertFalse(UpdatePolicy.isValidSha256("A".repeat(64)))
        assertFalse(UpdatePolicy.isValidSha256("a".repeat(63)))
        assertFalse(UpdatePolicy.isValidSha256("g".repeat(64)))
    }
}
