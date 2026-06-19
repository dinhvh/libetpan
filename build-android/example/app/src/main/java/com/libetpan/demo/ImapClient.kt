package com.libetpan.demo

object ImapClient {
    init {
        System.loadLibrary("etpanjni")
    }

    /** Returns a JSON string (see parseInboxJson). Must be called off the main thread. */
    external fun fetchInbox(
        host: String,
        port: Int,
        email: String,
        password: String,
        limit: Int,
    ): String
}
