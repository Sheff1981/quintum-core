package org.quintum.wallet.diagnostics

import android.content.ContentResolver
import android.net.Uri

/** Writes only to the destination explicitly selected by the user. Call on Dispatchers.IO. */
fun writeDiagnosticsExport(resolver: ContentResolver, uri: Uri, bytes: ByteArray) {
    val output = resolver.openOutputStream(uri, "wt") ?: error("Output unavailable")
    output.use { it.write(bytes) }
}
