package org.quintum.wallet.core

import android.app.NotificationManager
import android.content.Intent
import android.os.Process
import android.os.SystemClock
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.quintum.wallet.MainActivity

/** Runs against the real packaged JNI library; never substitute a fake CoreBridge. */
@RunWith(AndroidJUnit4::class)
class ServiceLifecycleTest {
    @Test fun foregroundServiceSurvivesBackgroundAndMiningStop() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val scenario = ActivityScenario.launch(MainActivity::class.java)
        try {
            ContextCompat.startForegroundService(context, Intent(context, NodeService::class.java))
            await("Native core did not start", 90_000) {
                org.json.JSONObject(NativeCore.nativeDiagnostics()).optLong("core_running") == 1L && context.getSharedPreferences("node_status", 0).contains("last_start_duration_ms")
            }
            val startDuration = context.getSharedPreferences("node_status", 0).getLong("last_start_duration_ms", -1)
            scenario.moveToState(Lifecycle.State.CREATED)
            await("Activity background event was not persisted") {
                NativeCore.nativeDiagnosticLog().lineSequence().any { it.contains("activity_background") }
            }
            val backgroundEvent = NativeCore.nativeDiagnosticLog().lineSequence().last { it.contains("activity_background") }
            File(context.filesDir, "instrumentation-background-event.txt").writeText(backgroundEvent)
            File(context.filesDir, "instrumentation-process-id.txt").writeText(Process.myPid().toString())
            val deadline = SystemClock.elapsedRealtime() + 5_000
            while (SystemClock.elapsedRealtime() < deadline) {
                assertTrue("Core stopped while activity was backgrounded", NativeCore.nativeRunning())
                SystemClock.sleep(250)
            }
            assertTrue(context.getSystemService(NotificationManager::class.java).activeNotifications.any { it.id == 1001 })
            scenario.moveToState(Lifecycle.State.RESUMED)
            assertTrue(NativeCore.nativeRunning())
            assertEquals("Foregrounding must not start another core", startDuration,
                context.getSharedPreferences("node_status", 0).getLong("last_start_duration_ms", -1))
            NativeCore.nativeStopMining()
            assertFalse(NativeCore.nativeMiningRunning())
            assertTrue("Stopping mining must leave the core alive", NativeCore.nativeRunning())
        } finally {
            // Keep the service running until the workflow force-stops the process for the restart test.
            scenario.close()
        }
    }

    private fun await(message: String, timeout: Long = 15_000, condition: () -> Boolean) {
        val deadline = SystemClock.elapsedRealtime() + timeout
        while (SystemClock.elapsedRealtime() < deadline) {
            if (condition()) return
            SystemClock.sleep(250)
        }
        fail(message)
    }
}
