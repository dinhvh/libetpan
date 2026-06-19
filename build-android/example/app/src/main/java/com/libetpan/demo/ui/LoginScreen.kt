package com.libetpan.demo.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.libetpan.demo.UiState

@Composable
fun LoginScreen(
    state: UiState,
    onConnect: (host: String, port: Int, email: String, password: String) -> Unit,
) {
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("993") }
    var email by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    val loading = state is UiState.Loading

    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("libetpan IMAP demo", style = MaterialTheme.typography.headlineSmall)

        if (state is UiState.Error) {
            Surface(color = MaterialTheme.colorScheme.errorContainer) {
                Text(
                    state.message,
                    modifier = Modifier.fillMaxWidth().padding(12.dp),
                    color = MaterialTheme.colorScheme.onErrorContainer,
                )
            }
        }

        OutlinedTextField(
            value = host, onValueChange = { host = it },
            label = { Text("IMAP host (e.g. imap.gmail.com)") },
            singleLine = true, enabled = !loading, modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = port, onValueChange = { port = it.filter(Char::isDigit) },
            label = { Text("Port") }, singleLine = true, enabled = !loading,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = email, onValueChange = { email = it },
            label = { Text("Email / username") }, singleLine = true, enabled = !loading,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = password, onValueChange = { password = it },
            label = { Text("Password") }, singleLine = true, enabled = !loading,
            visualTransformation = PasswordVisualTransformation(),
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { onConnect(host.trim(), port.toIntOrNull() ?: 993, email.trim(), password) },
            enabled = !loading && host.isNotBlank() && email.isNotBlank(),
            modifier = Modifier.fillMaxWidth(),
        ) { Text(if (loading) "Connecting…" else "Connect") }

        if (loading) {
            Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                CircularProgressIndicator()
            }
        }
    }
}
