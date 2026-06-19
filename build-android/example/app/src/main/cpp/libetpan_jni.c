#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libetpan/libetpan.h>

/* ---- growable string buffer for JSON assembly ---- */
struct sbuf { char *p; size_t len, cap; };

/* On allocation failure b->p is left NULL; all writers below become no-ops, so
 * the caller can keep appending safely and just check b->p before using it. */
static void sb_init(struct sbuf *b) {
    b->cap = 256; b->len = 0; b->p = malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void sb_ensure(struct sbuf *b, size_t extra) {
    if (b->p == NULL) return;
    if (b->len + extra + 1 > b->cap) {
        size_t newcap = b->cap;
        while (b->len + extra + 1 > newcap) newcap *= 2;
        char *np = realloc(b->p, newcap);
        if (np == NULL) { free(b->p); b->p = NULL; return; }
        b->p = np; b->cap = newcap;
    }
}
static void sb_putc(struct sbuf *b, char c) {
    sb_ensure(b, 1); if (b->p == NULL) return;
    b->p[b->len++] = c; b->p[b->len] = '\0';
}
static void sb_puts(struct sbuf *b, const char *s) {
    size_t n = strlen(s); sb_ensure(b, n); if (b->p == NULL) return;
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}

/* append s as a quoted, escaped JSON string */
static void sb_json_str(struct sbuf *b, const char *s) {
    sb_putc(b, '"');
    if (s) {
        for (; *s; s++) {
            unsigned char c = (unsigned char) *s;
            switch (c) {
                case '"':  sb_puts(b, "\\\""); break;
                case '\\': sb_puts(b, "\\\\"); break;
                case '\n': sb_puts(b, "\\n"); break;
                case '\r': sb_puts(b, "\\r"); break;
                case '\t': sb_puts(b, "\\t"); break;
                default:
                    if (c < 0x20) { char u[8]; snprintf(u, sizeof(u), "\\u%04x", c); sb_puts(b, u); }
                    else sb_putc(b, (char) c);
            }
        }
    }
    sb_putc(b, '"');
}

static jstring err_json(JNIEnv *env, const char *msg) {
    struct sbuf b; sb_init(&b);
    sb_puts(&b, "{\"ok\":false,\"error\":");
    sb_json_str(&b, msg);
    sb_putc(&b, '}');
    jstring r = (*env)->NewStringUTF(env, b.p);
    free(b.p);
    return r;
}

/* error JSON that also carries the libetpan return code, to make a failure
 * against a real server diagnosable (e.g. "login failed (libetpan code 5)"). */
static jstring err_json_code(JNIEnv *env, const char *msg, int code) {
    char buf[160];
    snprintf(buf, sizeof(buf), "%s (libetpan code %d)", msg, code);
    return err_json(env, buf);
}

/* locate the ENVELOPE inside a fetched message attribute */
static struct mailimap_envelope * find_envelope(struct mailimap_msg_att *att) {
    clistiter *it;
    for (it = clist_begin(att->att_list); it != NULL; it = clist_next(it)) {
        struct mailimap_msg_att_item *item = clist_content(it);
        if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC &&
            item->att_data.att_static != NULL &&
            item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_ENVELOPE) {
            return item->att_data.att_static->att_data.att_env;
        }
    }
    return NULL;
}

/* append the first From address (display name, else mailbox@host) as JSON string */
static void append_from(struct sbuf *b, struct mailimap_envelope *env) {
    const char *out = NULL;
    char composed[512];
    if (env && env->env_from && env->env_from->frm_list &&
        clist_count(env->env_from->frm_list) > 0) {
        struct mailimap_address *addr = clist_content(clist_begin(env->env_from->frm_list));
        if (addr->ad_personal_name && addr->ad_personal_name[0]) {
            out = addr->ad_personal_name;
        } else if (addr->ad_mailbox_name && addr->ad_host_name) {
            snprintf(composed, sizeof(composed), "%s@%s", addr->ad_mailbox_name, addr->ad_host_name);
            out = composed;
        } else if (addr->ad_mailbox_name) {
            out = addr->ad_mailbox_name;
        }
    }
    sb_json_str(b, out ? out : "(unknown)");
}

JNIEXPORT jstring JNICALL
Java_com_libetpan_demo_ImapClient_fetchInbox(JNIEnv *env, jobject thiz,
        jstring jhost, jint port, jstring jemail, jstring jpassword, jint limit) {

    const char *host     = (*env)->GetStringUTFChars(env, jhost, NULL);
    const char *email    = (*env)->GetStringUTFChars(env, jemail, NULL);
    const char *password = (*env)->GetStringUTFChars(env, jpassword, NULL);

    jstring result = NULL;
    struct sbuf b; sb_init(&b); /* always freed in cleanup */
    mailimap *imap = NULL;      /* NULL until connected; cleanup guards on it */
    int r;

    /* GetStringUTFChars returns NULL under OOM (and raises an exception).
     * ReleaseStringUTFChars(env, j, NULL) in cleanup is a safe no-op. */
    if (host == NULL || email == NULL || password == NULL) {
        result = err_json(env, "out of memory");
        goto cleanup;
    }

    imap = mailimap_new(0, NULL);
    if (imap == NULL) { result = err_json(env, "out of memory"); goto cleanup; }

    /* NOTE: mailimap_ssl_connect establishes implicit TLS but does NOT verify
     * the server certificate against a trust store. This demo is therefore not
     * MITM-safe; a production client must wire up certificate verification. */
    r = mailimap_ssl_connect(imap, host, (uint16_t) port);
    if (r != MAILIMAP_NO_ERROR && r != MAILIMAP_NO_ERROR_AUTHENTICATED &&
        r != MAILIMAP_NO_ERROR_NON_AUTHENTICATED) {
        result = err_json_code(env, "TLS connect failed", r);
        goto cleanup;
    }

    r = mailimap_login(imap, email, password);
    if (r != MAILIMAP_NO_ERROR) { result = err_json_code(env, "login failed", r); goto cleanup; }

    r = mailimap_select(imap, "INBOX");
    if (r != MAILIMAP_NO_ERROR) { result = err_json_code(env, "SELECT INBOX failed", r); goto cleanup; }

    uint32_t exists = 0;
    if (imap->imap_selection_info != NULL) exists = imap->imap_selection_info->sel_exists;

    {
        char head[96];
        snprintf(head, sizeof(head), "{\"ok\":true,\"exists\":%u,\"messages\":[", exists);
        sb_puts(&b, head);
    }

    if (exists > 0) {
        uint32_t want = (uint32_t) (limit > 0 ? limit : 20);
        uint32_t from = (exists > want) ? (exists - want + 1) : 1;

        struct mailimap_set *set = mailimap_set_new_interval(from, exists);
        struct mailimap_fetch_type *ft = mailimap_fetch_type_new_fetch_att_list_empty();
        if (set == NULL || ft == NULL) {
            if (set) mailimap_set_free(set);
            if (ft) mailimap_fetch_type_free(ft);
            result = err_json(env, "out of memory");
            goto cleanup;
        }
        mailimap_fetch_type_new_fetch_att_list_add(ft, mailimap_fetch_att_new_envelope());

        clist *fetch_result = NULL;
        r = mailimap_fetch(imap, set, ft, &fetch_result);

        if (r == MAILIMAP_NO_ERROR && fetch_result != NULL) {
            clistiter *it; int first = 1;
            for (it = clist_begin(fetch_result); it != NULL; it = clist_next(it)) {
                struct mailimap_msg_att *att = clist_content(it);
                struct mailimap_envelope *envp = find_envelope(att);
                if (!first) sb_putc(&b, ',');
                first = 0;
                sb_puts(&b, "{\"from\":");
                append_from(&b, envp);
                sb_puts(&b, ",\"subject\":");
                sb_json_str(&b, (envp && envp->env_subject) ? envp->env_subject : "(no subject)");
                sb_puts(&b, ",\"date\":");
                sb_json_str(&b, (envp && envp->env_date) ? envp->env_date : "");
                sb_putc(&b, '}');
            }
            mailimap_fetch_list_free(fetch_result);
            mailimap_set_free(set);
            mailimap_fetch_type_free(ft);
        } else {
            mailimap_set_free(set);
            mailimap_fetch_type_free(ft);
            result = err_json_code(env, "FETCH failed", r);
            goto cleanup;
        }
    }

    sb_puts(&b, "]}");
    result = (b.p != NULL) ? (*env)->NewStringUTF(env, b.p)
                           : err_json(env, "out of memory");

cleanup:
    free(b.p);
    if (imap) {
        /* Do NOT call mailimap_logout() here: it ends in mailstream_close(),
         * which NULL-derefs when the session never connected (imap_stream == NULL,
         * e.g. mailimap_ssl_connect failed). mailimap_free() already logs out
         * internally, but only when imap_stream != NULL — so it is the safe way
         * to tear down on every path. */
        mailimap_free(imap);
    }
    (*env)->ReleaseStringUTFChars(env, jhost, host);
    (*env)->ReleaseStringUTFChars(env, jemail, email);
    (*env)->ReleaseStringUTFChars(env, jpassword, password);
    return result;
}

/* Append a header value with CR/LF replaced by spaces, to prevent header
 * injection through user-supplied From/To/Subject fields. */
static void sb_put_header_value(struct sbuf *b, const char *s) {
    if (s == NULL) return;
    for (; *s; s++) {
        sb_putc(b, (*s == '\r' || *s == '\n') ? ' ' : *s);
    }
}

JNIEXPORT jstring JNICALL
Java_com_libetpan_demo_SmtpClient_sendMail(JNIEnv *env, jobject thiz,
        jstring jhost, jint port, jstring jemail, jstring jpassword,
        jstring jto, jstring jsubject, jstring jbody) {

    const char *host     = (*env)->GetStringUTFChars(env, jhost, NULL);
    const char *email    = (*env)->GetStringUTFChars(env, jemail, NULL);
    const char *password = (*env)->GetStringUTFChars(env, jpassword, NULL);
    const char *to       = (*env)->GetStringUTFChars(env, jto, NULL);
    const char *subject  = (*env)->GetStringUTFChars(env, jsubject, NULL);
    const char *body     = (*env)->GetStringUTFChars(env, jbody, NULL);

    jstring result = NULL;
    struct sbuf msg; sb_init(&msg); /* always freed in cleanup */
    mailsmtp *smtp = NULL;
    int r;

    if (!host || !email || !password || !to || !subject || !body) {
        result = err_json(env, "out of memory");
        goto cleanup;
    }

    smtp = mailsmtp_new(0, NULL);
    if (smtp == NULL) { result = err_json(env, "out of memory"); goto cleanup; }

    /* NOTE: like mailimap_ssl_connect, this does NOT verify the server
     * certificate — not MITM-safe; a production client must add verification. */
    r = mailsmtp_ssl_connect(smtp, host, (uint16_t) port);
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "TLS connect failed", r); goto cleanup; }

    r = mailesmtp_ehlo(smtp);
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "EHLO failed", r); goto cleanup; }

    r = mailsmtp_auth(smtp, email, password);
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "auth failed", r); goto cleanup; }

    r = mailesmtp_mail(smtp, email, 1, "libetpan-android-demo");
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "MAIL FROM failed", r); goto cleanup; }

    r = mailesmtp_rcpt(smtp, to,
                       MAILSMTP_DSN_NOTIFY_FAILURE | MAILSMTP_DSN_NOTIFY_DELAY, NULL);
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "RCPT TO failed", r); goto cleanup; }

    r = mailsmtp_data(smtp);
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "DATA failed", r); goto cleanup; }

    /* Build the RFC 822 message (CRLF line endings). */
    {
        char date[64];
        time_t now = time(NULL);
        struct tm tm_utc;
        date[0] = '\0';
        if (gmtime_r(&now, &tm_utc) != NULL) {
            strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S +0000", &tm_utc);
        }

        if (date[0] != '\0') { sb_puts(&msg, "Date: "); sb_puts(&msg, date); sb_puts(&msg, "\r\n"); }
        sb_puts(&msg, "From: ");    sb_put_header_value(&msg, email);   sb_puts(&msg, "\r\n");
        sb_puts(&msg, "To: ");      sb_put_header_value(&msg, to);      sb_puts(&msg, "\r\n");
        sb_puts(&msg, "Subject: "); sb_put_header_value(&msg, subject); sb_puts(&msg, "\r\n");
        sb_puts(&msg, "MIME-Version: 1.0\r\n");
        sb_puts(&msg, "Content-Type: text/plain; charset=utf-8\r\n");
        sb_puts(&msg, "\r\n");
        sb_puts(&msg, body);
        sb_puts(&msg, "\r\n");
    }
    if (msg.p == NULL) { result = err_json(env, "out of memory"); goto cleanup; }

    r = mailsmtp_data_message(smtp, msg.p, msg.len);
    if (r != MAILSMTP_NO_ERROR) { result = err_json_code(env, "send failed", r); goto cleanup; }

    result = (*env)->NewStringUTF(env, "{\"ok\":true}");

cleanup:
    free(msg.p);
    if (smtp != NULL) {
        mailsmtp_quit(smtp);   /* harmless if the session never fully established */
        mailsmtp_free(smtp);
    }
    (*env)->ReleaseStringUTFChars(env, jhost, host);
    (*env)->ReleaseStringUTFChars(env, jemail, email);
    (*env)->ReleaseStringUTFChars(env, jpassword, password);
    (*env)->ReleaseStringUTFChars(env, jto, to);
    (*env)->ReleaseStringUTFChars(env, jsubject, subject);
    (*env)->ReleaseStringUTFChars(env, jbody, body);
    return result;
}
