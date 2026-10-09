package org.quintum.wallet.core

import android.app.Application
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], application = Application::class)
class DiagnosticsPresentationTest {
    @Test fun missingAndNullMeasurementsStayUnavailable() {
        val snapshot = DiagnosticsPresentation.parse("""{"stage":"connect","remote_height":null,"local_height":0}""")
        assertEquals("connect", snapshot.stage)
        assertEquals("Unavailable", snapshot.value("remote_height"))
        assertEquals("Unavailable", snapshot.value("headers_verified"))
        assertEquals("0", snapshot.value("local_height"))
    }

    @Test fun eventHistoryRetainsOnlyLatestHundredAndHandlesPartialRecord() {
        val journal = (0..104).joinToString("\n") { """{"sequence":$it,"event":"connect"}""" } + "\npartial-record\n"
        val events = DiagnosticsPresentation.recentEvents(journal)
        assertEquals(100, events.size)
        assertTrue(events.first().contains("sequence: 6"))
        assertEquals("partial-record", events.last())
        assertTrue(events[98].contains("sequence: 104"))
    }

    @Test fun copiedReportSeparatesKnownAndActivePeersAndPreservesMeasuredError() {
        val snapshot = DiagnosticsSnapshot(mapOf("last_error_code" to "HEADER_INVALID", "headers_rejected" to "4"))
        val report = snapshot.report(false, null, 9)
        assertTrue(report.contains("Core running: false"))
        assertTrue(report.contains("Active peers: Unavailable"))
        assertTrue(report.contains("Known peers: 9"))
        assertTrue(report.contains("last_error_code: HEADER_INVALID"))
        assertTrue(report.contains("headers_rejected: 4"))
        assertFalse(report.contains("Active peers: 9"))
    }
}
