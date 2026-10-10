package org.quintum.wallet.update

object UpdatePolicy {
    fun isUpgrade(currentVersionCode: Long, candidateVersionCode: Long): Boolean =
        candidateVersionCode > currentVersionCode

    fun isValidSha256(value: String): Boolean =
        value.length == 64 && value.all { it in '0'..'9' || it in 'a'..'f' }
}
