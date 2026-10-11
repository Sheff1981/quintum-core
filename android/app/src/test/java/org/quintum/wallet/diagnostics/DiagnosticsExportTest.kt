package org.quintum.wallet.diagnostics

import android.app.Application
import android.net.Uri
import androidx.test.core.app.ApplicationProvider
import java.io.File
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], application = Application::class)
class DiagnosticsExportTest {
    @Test fun unavailableDestinationPropagatesFailure() {
        val context = ApplicationProvider.getApplicationContext<Application>()
        val missingDirectory = File(context.cacheDir, "missing-export-parent-${System.nanoTime()}")
        assertThrows(java.io.FileNotFoundException::class.java) {
            writeDiagnosticsExport(context.contentResolver, Uri.fromFile(File(missingDirectory, "report.jsonl")), byteArrayOf(1))
        }
    }

    @Test fun writesExactUtf8JournalToChosenDestinationAndTruncatesPreviousContent() {
        val context = ApplicationProvider.getApplicationContext<Application>()
        val chosen = File.createTempFile("chosen-diagnostics-", ".jsonl", context.cacheDir)
        val untouched = File.createTempFile("other-diagnostics-", ".jsonl", context.cacheDir)
        try {
            chosen.writeText("previous content that must be truncated".repeat(10))
            untouched.writeText("keep this file")
            val journal = "{\"event\":\"peer_error\",\"description\":\"réponse invalide\"}\n".toByteArray(Charsets.UTF_8)
            writeDiagnosticsExport(context.contentResolver, Uri.fromFile(chosen), journal)
            assertArrayEquals(journal, chosen.readBytes())
            assertArrayEquals("keep this file".toByteArray(), untouched.readBytes())
        } finally {
            chosen.delete()
            untouched.delete()
        }
    }
}
