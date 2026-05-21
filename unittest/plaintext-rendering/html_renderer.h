#ifndef PLAINTEXT_RENDERING_HTML_RENDERER_H
#define PLAINTEXT_RENDERING_HTML_RENDERER_H

struct mailmime;

char * plaintext_rendering_render_message_html(struct mailmime * mime,
    const char * default_charset);

#endif
