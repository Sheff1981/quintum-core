package org.quintum.wallet.mining

import java.util.Locale

object MiningStatsFormatter {
    fun hashRate(value: Double?): String =
        value?.let { String.format(Locale.US, "%.1f H/s", it) } ?: "Unavailable"

    fun temperature(value: Double?): String =
        value?.let { String.format(Locale.US, "%.1f °C", it) } ?: "Unavailable"

    fun battery(value: Int?, charging: Boolean): String {
        val level = value?.let { "$it%" } ?: "Unavailable"
        return if (charging && value != null) "$level · Charging" else level
    }

    fun runtime(milliseconds: Long): String {
        val totalSeconds = (milliseconds / 1000L).coerceAtLeast(0L)
        val hours = totalSeconds / 3600L
        val minutes = (totalSeconds % 3600L) / 60L
        val seconds = totalSeconds % 60L
        return String.format(Locale.US, "%02d:%02d:%02d", hours, minutes, seconds)
    }
}
