package com.libetpan.demo

import org.json.JSONObject

sealed interface SendResult {
    data object Success : SendResult
    data class Failure(val error: String) : SendResult
}

fun parseSendJson(json: String): SendResult = try {
    val obj = JSONObject(json)
    if (obj.optBoolean("ok", false)) {
        SendResult.Success
    } else {
        SendResult.Failure(obj.optString("error", "Unknown error"))
    }
} catch (e: Exception) {
    SendResult.Failure("Malformed response: ${e.message}")
}
