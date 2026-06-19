package com.libetpan.demo

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import com.libetpan.demo.ui.ComposeScreen
import com.libetpan.demo.ui.InboxScreen
import com.libetpan.demo.ui.LoginScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    AppRoot()
                }
            }
        }
    }
}

@Composable
fun AppRoot(
    vm: ImapViewModel = viewModel(),
    smtpVm: SmtpViewModel = viewModel(),
) {
    val state by vm.state.collectAsState()
    var showCompose by rememberSaveable { mutableStateOf(false) }

    when (val s = state) {
        is UiState.Loaded -> {
            val account = vm.account
            if (showCompose && account != null) {
                val sendState by smtpVm.state.collectAsState()
                ComposeScreen(
                    fromEmail = account.email,
                    sendState = sendState,
                    onSend = { host, port, to, subject, body ->
                        smtpVm.send(host, port, account.email, account.password, to, subject, body)
                    },
                    onBack = {
                        smtpVm.reset()
                        showCompose = false
                    },
                )
            } else {
                InboxScreen(
                    exists = s.exists,
                    messages = s.messages,
                    onCompose = { showCompose = true },
                    onBack = vm::reset,
                )
            }
        }
        else -> LoginScreen(state = s, onConnect = { h, p, e, pw -> vm.connect(h, p, e, pw) })
    }
}
