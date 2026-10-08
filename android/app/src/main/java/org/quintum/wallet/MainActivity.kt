package org.quintum.wallet

import android.os.Bundle
import android.content.Intent
import androidx.core.content.ContextCompat
import org.quintum.wallet.core.NodeService
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import org.quintum.wallet.mining.MiningScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ContextCompat.startForegroundService(this, Intent(this, NodeService::class.java))
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) { MiningScreen() }
            }
        }
    }
}