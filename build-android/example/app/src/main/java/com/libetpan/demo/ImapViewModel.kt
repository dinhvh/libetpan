package com.libetpan.demo

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

sealed interface UiState {
    data object Idle : UiState
    data object Loading : UiState
    data class Loaded(val exists: Int, val messages: List<Message>) : UiState
    data class Error(val message: String) : UiState
}

/** Credentials from the last successful login, reused by the SMTP compose flow. */
data class Account(val email: String, val password: String)

class ImapViewModel : ViewModel() {

    private val _state = MutableStateFlow<UiState>(UiState.Idle)
    val state: StateFlow<UiState> = _state.asStateFlow()

    var account: Account? = null
        private set

    fun connect(host: String, port: Int, email: String, password: String) {
        _state.value = UiState.Loading
        viewModelScope.launch {
            val result = withContext(Dispatchers.IO) {
                try {
                    parseInboxJson(ImapClient.fetchInbox(host, port, email, password, 20))
                } catch (t: Throwable) {
                    InboxResult.Failure(t.message ?: "Native call failed")
                }
            }
            _state.value = when (result) {
                is InboxResult.Success -> {
                    account = Account(email, password)
                    UiState.Loaded(result.exists, result.messages)
                }
                is InboxResult.Failure -> UiState.Error(result.error)
            }
        }
    }

    fun reset() {
        account = null
        _state.value = UiState.Idle
    }
}
