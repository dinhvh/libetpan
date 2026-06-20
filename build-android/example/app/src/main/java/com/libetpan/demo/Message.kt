package com.libetpan.demo

import org.json.JSONObject

data class Message(
    val from: String,
    val subject: String,
    val date: String,
)

sealed interface InboxResult {
    data class Success(val exists: Int, val messages: List<Message>) : InboxResult
    data class Failure(val error: String) : InboxResult
}

fun parseInboxJson(json: String): InboxResult = try {
    val obj = JSONObject(json)
    if (!obj.optBoolean("ok", false)) {
        InboxResult.Failure(obj.optString("error", "Unknown error"))
    } else {
        val arr = obj.optJSONArray("messages")
        val messages = buildList {
            if (arr != null) {
                for (i in 0 until arr.length()) {
                    val m = arr.getJSONObject(i)
                    add(
                        Message(
                            from = m.optString("from", ""),
                            subject = m.optString("subject", ""),
                            date = m.optString("date", ""),
                        )
                    )
                }
            }
        }
        InboxResult.Success(obj.optInt("exists", messages.size), messages)
    }
} catch (e: Exception) {
    InboxResult.Failure("Malformed response: ${e.message}")
}
