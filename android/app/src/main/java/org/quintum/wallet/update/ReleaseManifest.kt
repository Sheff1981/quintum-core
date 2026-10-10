package org.quintum.wallet.update

/**
 * Data-only description of a published Android release.
 *
 * Verification is deliberately separate: an untrusted network response must
 * never be treated as an authorized update merely because it parses.
 */
data class ReleaseManifest(
    val versionCode: Long,
    val versionName: String,
    val apkUrl: String,
    val apkSha256: String,
    val signature: String,
)
