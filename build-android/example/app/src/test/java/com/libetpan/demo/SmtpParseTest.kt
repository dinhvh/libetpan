package com.libetpan.demo

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SmtpParseTest {

    @Test
    fun parsesSuccess() {
        assertTrue(parseSendJson("""{"ok":true}""") is SendResult.Success)
    }

    @Test
    fun mapsErrorPayloadToFailure() {
        val r = parseSendJson("""{"ok":false,"error":"auth failed (libetpan code 5)"}""")
        assertTrue(r is SendResult.Failure)
        assertEquals("auth failed (libetpan code 5)", (r as SendResult.Failure).error)
    }

    @Test
    fun mapsMalformedJsonToFailure() {
        assertTrue(parseSendJson("not json") is SendResult.Failure)
    }
}
