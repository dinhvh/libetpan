package com.libetpan.demo

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

sealed interface SendUiState {
    data object Idle : SendUiState
    data object Sending : SendUiState
    data object Sent : SendUiState
    data class Error(val message: String) : SendUiState
}

class SmtpViewModel : ViewModel() {

    private val _state = MutableStateFlow<SendUiState>(SendUiState.Idle)
    val state: StateFlow<SendUiState> = _state.asStateFlow()

    fun send(
        host: String,
        port: Int,
        email: String,
        password: String,
        to: String,
        subject: String,
        body: String,
    ) {
        _state.value = SendUiState.Sending
        viewModelScope.launch {
            val result = withContext(Dispatchers.IO) {
                try {
                    parseSendJson(SmtpClient.sendMail(host, port, email, password, to, subject, body))
                } catch (t: Throwable) {
                    SendResult.Failure(t.message ?: "Native call failed")
                }
            }
            _state.value = when (result) {
                is SendResult.Success -> SendUiState.Sent
                is SendResult.Failure -> SendUiState.Error(result.error)
            }
        }
    }

    fun reset() {
        _state.value = SendUiState.Idle
    }
}
