package org.quintum.wallet.mining

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
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.unit.dp
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [33], application = Application::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
class MiningScreenLayoutTest {
    @get:Rule val compose = createComposeRule()

    /** Adding a nested verticalScroll/LazyColumn reproduces the original infinite-height failure. */
    @Test fun miningContentMeasuresInsideParentScrollAndBottomIsReachable() {
        compose.setContent {
            MaterialTheme {
                Column(Modifier.height(320.dp).verticalScroll(rememberScrollState())) {
                    MiningScreenContent(
                        state = MiningUiState(thermalStatus = "Normal"),
                        nodeRunning = true, payout = "", threadsText = "2", error = null,
                        onPayoutChange = {}, onThreadsChange = {}, onClearPayout = {}, onToggleMining = {},
                    )
                }
            }
        }
        compose.onNodeWithText("Mining").assertIsDisplayed()
        compose.onNodeWithText("Thermal status: Normal").performScrollTo().assertIsDisplayed()
        compose.onNodeWithText("Start mining").performScrollTo().assertIsDisplayed()
    }
}
