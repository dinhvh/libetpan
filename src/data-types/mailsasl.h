#ifndef MAILSASL_H

#define MAILSASL_H

#ifdef __cplusplus
extern"C"{
#endif

/* if Cyrus-SASL is used outside of libetpan */
void mailsasl_external_ref(void);

void mailsasl_ref(void);
void mailsasl_unref(void);

#ifdef __cplusplus
}
#endif


#endif
