package com.libetpan.demo.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.libetpan.demo.Message

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InboxScreen(exists: Int, messages: List<Message>, onCompose: () -> Unit, onBack: () -> Unit) {
    Column(Modifier.fillMaxSize()) {
        TopAppBar(
            title = { Text("INBOX ($exists messages)") },
            navigationIcon = { TextButton(onClick = onBack) { Text("Back") } },
            actions = { TextButton(onClick = onCompose) { Text("Compose") } },
        )
        if (messages.isEmpty()) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("No messages")
            }
        } else {
            LazyColumn(Modifier.fillMaxSize()) {
                items(messages) { m ->
                    Column(Modifier.fillMaxWidth().padding(16.dp)) {
                        Text(m.from, fontWeight = FontWeight.Bold, maxLines = 1)
                        Text(m.subject, maxLines = 2)
                        Text(m.date, style = MaterialTheme.typography.bodySmall)
                    }
                    HorizontalDivider()
                }
            }
        }
    }
}
