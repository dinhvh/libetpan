package com.libetpan.demo

object SmtpClient {
    init {
        System.loadLibrary("etpanjni")
    }

    /** Returns a JSON string (see parseSendJson). Must be called off the main thread. */
    external fun sendMail(
        host: String,
        port: Int,
        email: String,
        password: String,
        to: String,
        subject: String,
        body: String,
    ): String
}
