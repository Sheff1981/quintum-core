package org.quintum.wallet.mining

import org.junit.Assert.assertEquals
import org.junit.Test

class MiningStatsFormatterTest {
    @Test
    fun formatsHashRateAndMissingValue() {
        assertEquals("226.0 H/s", MiningStatsFormatter.hashRate(226.0))
        assertEquals("Unavailable", MiningStatsFormatter.hashRate(null))
    }

    @Test
    fun formatsBatteryChargingState() {
        assertEquals("78% · Charging", MiningStatsFormatter.battery(78, true))
        assertEquals("78%", MiningStatsFormatter.battery(78, false))
        assertEquals("Unavailable", MiningStatsFormatter.battery(null, false))
    }

    @Test
    fun formatsMiningRuntime() {
        assertEquals("03:17:00", MiningStatsFormatter.runtime(11_820_000L))
    }
}
