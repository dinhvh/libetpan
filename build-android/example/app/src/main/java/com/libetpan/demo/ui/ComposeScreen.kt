package com.libetpan.demo.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.libetpan.demo.SendUiState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ComposeScreen(
    fromEmail: String,
    sendState: SendUiState,
    onSend: (host: String, port: Int, to: String, subject: String, body: String) -> Unit,
    onBack: () -> Unit,
) {
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("465") }
    var to by remember { mutableStateOf("") }
    var subject by remember { mutableStateOf("") }
    var body by remember { mutableStateOf("") }
    val sending = sendState is SendUiState.Sending

    Column(Modifier.fillMaxSize()) {
        TopAppBar(
            title = { Text("Compose") },
            navigationIcon = { TextButton(onClick = onBack) { Text("Back") } },
        )
        Column(
            modifier = Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            when (sendState) {
                is SendUiState.Sent -> Surface(color = MaterialTheme.colorScheme.primaryContainer) {
                    Text(
                        "Message sent.",
                        modifier = Modifier.fillMaxWidth().padding(12.dp),
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                }
                is SendUiState.Error -> Surface(color = MaterialTheme.colorScheme.errorContainer) {
                    Text(
                        sendState.message,
                        modifier = Modifier.fillMaxWidth().padding(12.dp),
                        color = MaterialTheme.colorScheme.onErrorContainer,
                    )
                }
                else -> {}
            }

            OutlinedTextField(
                value = fromEmail, onValueChange = {}, label = { Text("From") },
                readOnly = true, enabled = false, modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = host, onValueChange = { host = it },
                label = { Text("SMTP host (e.g. smtp.gmail.com)") },
                singleLine = true, enabled = !sending, modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = port, onValueChange = { port = it.filter(Char::isDigit) },
                label = { Text("Port") }, singleLine = true, enabled = !sending,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = to, onValueChange = { to = it }, label = { Text("To") },
                singleLine = true, enabled = !sending, modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = subject, onValueChange = { subject = it }, label = { Text("Subject") },
                singleLine = true, enabled = !sending, modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = body, onValueChange = { body = it }, label = { Text("Body") },
                enabled = !sending, modifier = Modifier.fillMaxWidth().heightIn(min = 120.dp),
            )
            Button(
                onClick = { onSend(host.trim(), port.toIntOrNull() ?: 465, to.trim(), subject, body) },
                enabled = !sending && host.isNotBlank() && to.isNotBlank(),
                modifier = Modifier.fillMaxWidth(),
            ) { Text(if (sending) "Sending…" else "Send") }

            if (sending) {
                Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            }
        }
    }
}
