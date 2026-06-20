package com.libetpan.demo

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MessageParseTest {

    @Test
    fun parsesSuccessPayload() {
        val json = """
            {"ok":true,"exists":2,"messages":[
              {"from":"Alice","subject":"Hi","date":"Mon, 1 Jan 2024"},
              {"from":"bob@x.com","subject":"(no subject)","date":""}
            ]}
        """.trimIndent()

        val r = parseInboxJson(json)
        assertTrue(r is InboxResult.Success)
        r as InboxResult.Success
        assertEquals(2, r.exists)
        assertEquals(2, r.messages.size)
        assertEquals("Alice", r.messages[0].from)
        assertEquals("Hi", r.messages[0].subject)
        assertEquals("bob@x.com", r.messages[1].from)
    }

    @Test
    fun parsesEmptyInbox() {
        val r = parseInboxJson("""{"ok":true,"exists":0,"messages":[]}""")
        assertTrue(r is InboxResult.Success)
        assertEquals(0, (r as InboxResult.Success).messages.size)
    }

    @Test
    fun mapsErrorPayloadToFailure() {
        val r = parseInboxJson("""{"ok":false,"error":"login failed"}""")
        assertTrue(r is InboxResult.Failure)
        assertEquals("login failed", (r as InboxResult.Failure).error)
    }

    @Test
    fun mapsMalformedJsonToFailure() {
        val r = parseInboxJson("not json")
        assertTrue(r is InboxResult.Failure)
    }
}
