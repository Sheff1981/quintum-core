package org.quintum.wallet.core

import android.os.Process
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Must run in a separate instrumentation invocation after adb am force-stop. */
@RunWith(AndroidJUnit4::class)
class DiagnosticRestartTest {
    @Test fun backgroundJournalSurvivesActualProcessRestart() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val marker = File(context.filesDir, "instrumentation-background-event.txt")
        assertTrue("Run ServiceLifecycleTest before force-stop and this test", marker.isFile)
        assertNotEquals(File(context.filesDir, "instrumentation-process-id.txt").readText(), Process.myPid().toString())
        val expected = marker.readText()
        assertTrue(expected.contains("activity_background"))
        NativeCore.nativeInitializeDiagnostics(File(context.filesDir, "core").absolutePath)
        assertTrue("Native journal lost the original lifecycle event", NativeCore.nativeDiagnosticLog().lineSequence().any { it == expected })
        assertFalse("Reading persisted diagnostics must not start the core", NativeCore.nativeRunning())
        marker.delete()
        File(context.filesDir, "instrumentation-process-id.txt").delete()
    }
}
