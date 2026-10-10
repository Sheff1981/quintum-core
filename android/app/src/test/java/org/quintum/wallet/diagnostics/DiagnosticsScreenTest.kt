package org.quintum.wallet.diagnostics

import android.app.Application
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.unit.dp
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.quintum.wallet.core.DiagnosticsSnapshot
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

/** Measures real Compose layout, including the unbounded constraints of the parent scroll. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], application = Application::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class DiagnosticsScreenTest {
    @get:Rule val compose = createComposeRule()

    @Test fun longReportFitsParentScrollAndLastEventIsReachable() {
        compose.setContent {
            MaterialTheme {
                Column(Modifier.height(320.dp).verticalScroll(rememberScrollState())) {
                    DiagnosticsScreen(
                        DiagnosticsSnapshot(mapOf("stage" to "verify_headers", "headers_rejected" to "7")),
                        true, 2, 5, List(100) { "Recorded diagnostic event $it" },
                        onCopy = {}, onExport = {}, onClear = {},
                    )
                }
            }
        }
        compose.onNodeWithText("Local diagnostics").assertIsDisplayed()
        compose.onNodeWithText("Recorded diagnostic event 99").performScrollTo().assertIsDisplayed()
        compose.onNodeWithText("Headers rejected").performScrollTo().assertIsDisplayed()
        compose.onNodeWithText("7").assertIsDisplayed()
    }

    @Test fun reportActionsRequireExplicitUserClicks() {
        var copies = 0
        var exports = 0
        var clears = 0
        compose.setContent {
            MaterialTheme {
                Column(Modifier.height(480.dp).verticalScroll(rememberScrollState())) {
                    DiagnosticsScreen(DiagnosticsSnapshot(), false, null, 0, emptyList(),
                        onCopy = { copies++ }, onExport = { exports++ }, onClear = { clears++ })
                }
            }
        }
        compose.runOnIdle {
            assertEquals(0, copies)
            assertEquals(0, exports)
            assertEquals(0, clears)
        }
        compose.onNodeWithText("Copy report").performScrollTo().performClick()
        compose.onNodeWithText("Export log").performScrollTo().performClick()
        compose.onNodeWithText("Clear diagnostics").performScrollTo().performClick()
        compose.runOnIdle {
            assertEquals(1, copies)
            assertEquals(1, exports)
            assertEquals(1, clears)
        }
    }
}
