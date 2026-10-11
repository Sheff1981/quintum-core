package org.quintum.wallet

import android.app.Application
import android.os.Process
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.time.Instant

/** Records uncaught Java/Kotlin exceptions locally; never uploads reports. */
class QuintumApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            try {
                val writer = StringWriter()
                throwable.printStackTrace(PrintWriter(writer))
                val report = buildString {
                    appendLine("QUINTUM Android crash report")
                    appendLine("UTC: ${Instant.now()}")
                    appendLine("Thread: ${thread.name}")
                    appendLine("Android: ${android.os.Build.VERSION.RELEASE} (API ${android.os.Build.VERSION.SDK_INT})")
                    appendLine("Device: ${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}")
                    appendLine(writer.toString().take(24000))
                }
                val directory = File(filesDir, "diagnostics")
                if (directory.exists() || directory.mkdirs()) {
                    val target = File(directory, "last-java-crash.txt")
                    val temp = File(directory, "last-java-crash.tmp")
                    temp.writeText(report)
                    if (!temp.renameTo(target)) {
                        target.writeText(report)
                        temp.delete()
                    }
                }
            } catch (_: Throwable) {
                // Never mask the original crash.
            } finally {
                if (previous != null) previous.uncaughtException(thread, throwable)
                else Process.killProcess(Process.myPid())
            }
        }
    }
}
