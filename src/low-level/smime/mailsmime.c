/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mailsmime.h"

#include <libetpan/clist.h>
#include <libetpan/mailimf.h>
#include <libetpan/mailmime_content.h>
#include <libetpan/mailmime_types_helper.h>
#include <libetpan/mailmime_write_mem.h>
#include <libetpan/mmapstring.h>

#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef USE_SMIME_OPENSSL
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#endif

#ifdef USE_SMIME_APPLE
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/CMSDecoder.h>
#include <Security/CMSEncoder.h>
#include <Security/SecCertificateOIDs.h>
#include <Security/SecImportExport.h>
#endif

struct mailsmime_cert_entry {
  char * email;
  char * filename;
#ifdef USE_SMIME_OPENSSL
  X509 * openssl_cert;
#endif
#ifdef USE_SMIME_APPLE
  SecCertificateRef apple_cert;
#endif
};

struct mailsmime_key_entry {
  char * email;
  char * cert_filename;
  char * key_filename;
  char * passphrase;
#ifdef USE_SMIME_OPENSSL
  X509 * openssl_cert;
  EVP_PKEY * openssl_key;
#endif
#ifdef USE_SMIME_APPLE
  SecCertificateRef apple_cert;
  SecIdentityRef apple_identity;
#endif
};

struct mailsmime {
  enum mailsmime_backend backend;
  clist * certs;
  clist * keys;
  mailsmime_passphrase_callback passphrase_callback;
  void * passphrase_context;
#ifdef USE_SMIME_OPENSSL
  X509_STORE * openssl_store;
#if OPENSSL_VERSION_MAJOR >= 3
  OSSL_PROVIDER * openssl_default_provider;
#endif
#endif
#ifdef USE_SMIME_APPLE
  CFMutableArrayRef apple_trusted_certs;
  SecKeychainRef apple_keychain;
  char * apple_keychain_path;
#endif
};

#ifdef USE_SMIME_APPLE
static const char apple_temp_keychain_password[] =
    "libetpan-smime-temporary";
#endif

struct mailsmime_certificate {
  enum mailsmime_backend backend;
#ifdef USE_SMIME_OPENSSL
  X509 * openssl_cert;
#endif
#ifdef USE_SMIME_APPLE
  SecCertificateRef apple_cert;
#endif
  char * email;
  char * name;
  char * subject;
  char * issuer;
  char * not_before;
  char * not_after;
  char * fingerprint_sha256;
};

struct mailsmime_result {
  int status;
  char * error;
  clist * signers;
  struct mailmime * signed_mime;
};

struct mailsmime_mime_owner {
  struct mailmime * mime;
  clist * buffers;
};

static clist * mailsmime_mime_owners = NULL;

static int str_case_equal(const char * a, const char * b)
{
  if ((a == NULL) || (b == NULL))
    return 0;
  return strcasecmp(a, b) == 0;
}

static void free_func(void * data, void * context)
{
  (void) context;
  free(data);
}

static void mime_owner_free(struct mailsmime_mime_owner * owner)
{
  if (owner == NULL)
    return;
  if (owner->buffers != NULL) {
    clist_foreach(owner->buffers, free_func, NULL);
    clist_free(owner->buffers);
  }
  free(owner);
}

static struct mailsmime_mime_owner * mime_owner_find(struct mailmime * mime)
{
  clistiter * cur;

  if (mailsmime_mime_owners == NULL)
    return NULL;

  for (cur = clist_begin(mailsmime_mime_owners); cur != NULL;
      cur = clist_next(cur)) {
    struct mailsmime_mime_owner * owner;

    owner = clist_content(cur);
    if (owner->mime == mime)
      return owner;
  }

  return NULL;
}

static int mime_register_owned_buffer(struct mailmime * mime, char * buffer)
{
  struct mailsmime_mime_owner * owner;

  if ((mime == NULL) || (buffer == NULL))
    return MAILSMIME_ERROR_INVAL;

  if (mailsmime_mime_owners == NULL) {
    mailsmime_mime_owners = clist_new();
    if (mailsmime_mime_owners == NULL)
      return MAILSMIME_ERROR_MEMORY;
  }

  owner = mime_owner_find(mime);
  if (owner == NULL) {
    owner = calloc(1, sizeof(* owner));
    if (owner == NULL)
      return MAILSMIME_ERROR_MEMORY;
    owner->mime = mime;
    owner->buffers = clist_new();
    if (owner->buffers == NULL) {
      mime_owner_free(owner);
      return MAILSMIME_ERROR_MEMORY;
    }
    if (clist_append(mailsmime_mime_owners, owner) < 0) {
      mime_owner_free(owner);
      return MAILSMIME_ERROR_MEMORY;
    }
  }

  if (clist_append(owner->buffers, buffer) < 0)
    return MAILSMIME_ERROR_MEMORY;

  return MAILSMIME_NO_ERROR;
}

static void mime_unregister_owned_buffers(struct mailmime * mime)
{
  clistiter * cur;

  if (mailsmime_mime_owners == NULL)
    return;

  for (cur = clist_begin(mailsmime_mime_owners); cur != NULL;
      cur = clist_next(cur)) {
    struct mailsmime_mime_owner * owner;

    owner = clist_content(cur);
    if (owner->mime == mime) {
      clist_delete(mailsmime_mime_owners, cur);
      mime_owner_free(owner);
      return;
    }
  }
}

static void mime_unregister_owned_buffers_recursive(struct mailmime * mime)
{
  clistiter * cur;

  if (mime == NULL)
    return;

  if (mime->mm_type == MAILMIME_MULTIPLE) {
    for (cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list); cur != NULL;
        cur = clist_next(cur))
      mime_unregister_owned_buffers_recursive(clist_content(cur));
  }
  else if ((mime->mm_type == MAILMIME_MESSAGE) &&
      (mime->mm_data.mm_message.mm_msg_mime != NULL)) {
    mime_unregister_owned_buffers_recursive(
        mime->mm_data.mm_message.mm_msg_mime);
  }

  mime_unregister_owned_buffers(mime);
}

static int content_is(struct mailmime_content * content,
    int discrete_type, int composite_type, const char * subtype)
{
  if ((content == NULL) || (content->ct_type == NULL) ||
      (content->ct_subtype == NULL))
    return 0;

  if (!str_case_equal(content->ct_subtype, subtype))
    return 0;

  if (content->ct_type->tp_type == MAILMIME_TYPE_DISCRETE_TYPE) {
    if (content->ct_type->tp_data.tp_discrete_type == NULL)
      return 0;
    return content->ct_type->tp_data.tp_discrete_type->dt_type == discrete_type;
  }

  if (content->ct_type->tp_type == MAILMIME_TYPE_COMPOSITE_TYPE) {
    if (content->ct_type->tp_data.tp_composite_type == NULL)
      return 0;
    return content->ct_type->tp_data.tp_composite_type->ct_type == composite_type;
  }

  return 0;
}

static int content_param_is(struct mailmime_content * content,
    const char * name, const char * value)
{
  char * param;

  if (content == NULL)
    return 0;

  param = mailmime_content_param_get(content, (char *) name);
  return str_case_equal(param, value);
}

static int content_is_pkcs7_mime(struct mailmime_content * content)
{
  if (!content_is(content, MAILMIME_DISCRETE_TYPE_APPLICATION, 0,
      "pkcs7-mime") &&
      !content_is(content, MAILMIME_DISCRETE_TYPE_APPLICATION, 0,
      "x-pkcs7-mime"))
    return 0;
  return 1;
}

static int content_is_pkcs7_signature(struct mailmime_content * content)
{
  if (content_is(content, MAILMIME_DISCRETE_TYPE_APPLICATION, 0,
      "pkcs7-signature"))
    return 1;
  if (content_is(content, MAILMIME_DISCRETE_TYPE_APPLICATION, 0,
      "x-pkcs7-signature"))
    return 1;
  return 0;
}

int mailsmime_is_signed(struct mailmime * mime)
{
  if ((mime == NULL) || (mime->mm_content_type == NULL))
    return 0;

  if (content_is_pkcs7_mime(mime->mm_content_type))
    return content_param_is(mime->mm_content_type, "smime-type", "signed-data");

  if (content_is(mime->mm_content_type, 0, MAILMIME_COMPOSITE_TYPE_MULTIPART,
      "signed")) {
    char * protocol;

    protocol = mailmime_content_param_get(mime->mm_content_type, "protocol");
    if ((protocol == NULL) || str_case_equal(protocol,
        "application/pkcs7-signature") ||
        str_case_equal(protocol, "application/x-pkcs7-signature"))
      return 1;
  }

  return content_is_pkcs7_signature(mime->mm_content_type);
}

int mailsmime_is_encrypted(struct mailmime * mime)
{
  if ((mime == NULL) || (mime->mm_content_type == NULL))
    return 0;

  if (!content_is_pkcs7_mime(mime->mm_content_type))
    return 0;

  if (content_param_is(mime->mm_content_type, "smime-type", "signed-data"))
    return 0;

  return 1;
}

int mailsmime_is_smime(struct mailmime * mime)
{
  return mailsmime_is_signed(mime) || mailsmime_is_encrypted(mime);
}

static void cert_entry_free(struct mailsmime_cert_entry * entry)
{
  if (entry == NULL)
    return;
  free(entry->email);
  free(entry->filename);
#ifdef USE_SMIME_OPENSSL
  if (entry->openssl_cert != NULL)
    X509_free(entry->openssl_cert);
#endif
#ifdef USE_SMIME_APPLE
  if (entry->apple_cert != NULL)
    CFRelease(entry->apple_cert);
#endif
  free(entry);
}

static void cert_entry_free_func(void * value, void * data)
{
  (void) data;
  cert_entry_free(value);
}

static void key_entry_free(struct mailsmime_key_entry * entry)
{
  if (entry == NULL)
    return;
  free(entry->email);
  free(entry->cert_filename);
  free(entry->key_filename);
  free(entry->passphrase);
#ifdef USE_SMIME_OPENSSL
  if (entry->openssl_cert != NULL)
    X509_free(entry->openssl_cert);
  if (entry->openssl_key != NULL)
    EVP_PKEY_free(entry->openssl_key);
#endif
#ifdef USE_SMIME_APPLE
  if (entry->apple_cert != NULL)
    CFRelease(entry->apple_cert);
  if (entry->apple_identity != NULL)
    CFRelease(entry->apple_identity);
#endif
  free(entry);
}

static void key_entry_free_func(void * value, void * data)
{
  (void) data;
  key_entry_free(value);
}

struct mailsmime * mailsmime_new(void)
{
  return mailsmime_new_with_backend(MAILSMIME_BACKEND_DEFAULT);
}

struct mailsmime * mailsmime_new_with_backend(enum mailsmime_backend backend)
{
  struct mailsmime * smime;

  if (backend == MAILSMIME_BACKEND_DEFAULT) {
#ifdef USE_SMIME_APPLE
    backend = MAILSMIME_BACKEND_APPLE;
#elif defined(USE_SMIME_OPENSSL)
    backend = MAILSMIME_BACKEND_OPENSSL;
#else
    return NULL;
#endif
  }
#ifndef USE_SMIME_APPLE
  if (backend == MAILSMIME_BACKEND_APPLE)
    return NULL;
#endif
#ifndef USE_SMIME_OPENSSL
  if (backend == MAILSMIME_BACKEND_OPENSSL)
    return NULL;
#endif
  if (backend != MAILSMIME_BACKEND_APPLE &&
      backend != MAILSMIME_BACKEND_OPENSSL)
    return NULL;

  smime = calloc(1, sizeof(* smime));
  if (smime == NULL)
    return NULL;
  smime->backend = backend;

  smime->certs = clist_new();
  smime->keys = clist_new();
  if ((smime->certs == NULL) || (smime->keys == NULL))
    goto err;

#ifdef USE_SMIME_OPENSSL
  if (backend == MAILSMIME_BACKEND_OPENSSL) {
#if OPENSSL_VERSION_MAJOR >= 3
    smime->openssl_default_provider = OSSL_PROVIDER_load(NULL, "default");
    if (smime->openssl_default_provider == NULL)
      goto err;
#endif
    smime->openssl_store = X509_STORE_new();
    if (smime->openssl_store == NULL)
      goto err;
#ifdef X509_V_FLAG_PARTIAL_CHAIN
    X509_STORE_set_flags(smime->openssl_store, X509_V_FLAG_PARTIAL_CHAIN);
#endif
  }
#endif
#ifdef USE_SMIME_APPLE
  if (backend == MAILSMIME_BACKEND_APPLE) {
    char path[] = "/tmp/libetpan-smime-keychain-XXXXXX";
    int fd;

    fd = mkstemp(path);
    if (fd < 0)
      goto err;
    close(fd);
    unlink(path);
    if (SecKeychainCreate(path,
        (UInt32) strlen(apple_temp_keychain_password),
        apple_temp_keychain_password, false, NULL,
        &smime->apple_keychain) != errSecSuccess)
      goto err;
    smime->apple_keychain_path = strdup(path);
    if (smime->apple_keychain_path == NULL)
      goto err;
    smime->apple_trusted_certs = CFArrayCreateMutable(NULL, 0,
        &kCFTypeArrayCallBacks);
    if (smime->apple_trusted_certs == NULL)
      goto err;
  }
#endif

  return smime;

 err:
  mailsmime_free(smime);
  return NULL;
}

void mailsmime_free(struct mailsmime * smime)
{
  if (smime == NULL)
    return;

  if (smime->certs != NULL) {
    clist_foreach(smime->certs, cert_entry_free_func, NULL);
    clist_free(smime->certs);
  }
  if (smime->keys != NULL) {
    clist_foreach(smime->keys, key_entry_free_func, NULL);
    clist_free(smime->keys);
  }
#ifdef USE_SMIME_OPENSSL
  if (smime->openssl_store != NULL)
    X509_STORE_free(smime->openssl_store);
#if OPENSSL_VERSION_MAJOR >= 3
  if (smime->openssl_default_provider != NULL)
    OSSL_PROVIDER_unload(smime->openssl_default_provider);
#endif
#endif
#ifdef USE_SMIME_APPLE
  if (smime->apple_trusted_certs != NULL)
    CFRelease(smime->apple_trusted_certs);
  if (smime->apple_keychain != NULL) {
    SecKeychainDelete(smime->apple_keychain);
    CFRelease(smime->apple_keychain);
  }
  if (smime->apple_keychain_path != NULL) {
    unlink(smime->apple_keychain_path);
    free(smime->apple_keychain_path);
  }
#endif
  free(smime);
}

int mailsmime_set_passphrase_callback(struct mailsmime * smime,
    mailsmime_passphrase_callback callback,
    void * context)
{
  if (smime == NULL)
    return MAILSMIME_ERROR_INVAL;

  smime->passphrase_callback = callback;
  smime->passphrase_context = context;
  return MAILSMIME_NO_ERROR;
}

#ifdef USE_SMIME_OPENSSL
static X509 * load_cert_file(const char * filename)
{
  BIO * bio;
  X509 * cert;

  bio = BIO_new_file(filename, "rb");
  if (bio == NULL)
    return NULL;

  cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  BIO_free(bio);

  return cert;
}

static int passwd_cb(char * buf, int size, int rwflag, void * userdata)
{
  const char * passphrase;
  size_t len;

  (void) rwflag;

  passphrase = userdata;
  if (passphrase == NULL)
    return 0;

  len = strlen(passphrase);
  if (len > (size_t) size)
    len = (size_t) size;
  memcpy(buf, passphrase, len);
  return (int) len;
}

static EVP_PKEY * load_key_file(const char * filename, const char * passphrase)
{
  BIO * bio;
  EVP_PKEY * key;

  bio = BIO_new_file(filename, "rb");
  if (bio == NULL)
    return NULL;

  key = PEM_read_bio_PrivateKey(bio, NULL, passwd_cb, (void *) passphrase);
  BIO_free(bio);

  return key;
}
#endif

#ifdef USE_SMIME_APPLE
static pthread_mutex_t apple_keychain_search_mutex = PTHREAD_MUTEX_INITIALIZER;

static int apple_push_keychain_search(SecKeychainRef keychain,
    CFArrayRef * previous, SecKeychainRef * previous_default)
{
  CFArrayRef current;
  CFMutableArrayRef updated;

  current = NULL;
  pthread_mutex_lock(&apple_keychain_search_mutex);
  if (SecKeychainUnlock(keychain,
      (UInt32) strlen(apple_temp_keychain_password),
      apple_temp_keychain_password, true) != errSecSuccess)
    goto err;
  if (SecKeychainCopySearchList(&current) != errSecSuccess)
    goto err;
  updated = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
  if (updated == NULL)
    goto err;
  CFArrayAppendValue(updated, keychain);
  if (SecKeychainSetSearchList(updated) != errSecSuccess) {
    CFRelease(updated);
    goto err;
  }
  * previous_default = NULL;
  if (SecKeychainCopyDefault(previous_default) != errSecSuccess ||
      SecKeychainSetDefault(keychain) != errSecSuccess) {
    SecKeychainSetSearchList(current);
    CFRelease(updated);
    goto err;
  }
  CFRelease(updated);
  * previous = current;
  return MAILSMIME_NO_ERROR;

 err:
  if ((previous_default != NULL) && (* previous_default != NULL)) {
    CFRelease(* previous_default);
    * previous_default = NULL;
  }
  if (current != NULL)
    CFRelease(current);
  pthread_mutex_unlock(&apple_keychain_search_mutex);
  return MAILSMIME_ERROR_PRIVATE_KEY;
}

static void apple_pop_keychain_search(CFArrayRef previous,
    SecKeychainRef previous_default)
{
  if (previous_default != NULL) {
    SecKeychainSetDefault(previous_default);
    CFRelease(previous_default);
  }
  if (previous != NULL) {
    SecKeychainSetSearchList(previous);
    CFRelease(previous);
  }
  pthread_mutex_unlock(&apple_keychain_search_mutex);
}

static int read_file_to_mem(const char * filename, char ** result,
    size_t * result_len)
{
  FILE * f;
  struct stat stat_info;
  char * content;
  size_t read_len;

  if ((filename == NULL) || (result == NULL) || (result_len == NULL))
    return MAILSMIME_ERROR_INVAL;

  if (stat(filename, &stat_info) < 0)
    return MAILSMIME_ERROR_PARSE;
  if (stat_info.st_size < 0)
    return MAILSMIME_ERROR_PARSE;

  content = malloc((size_t) stat_info.st_size + 1);
  if (content == NULL)
    return MAILSMIME_ERROR_MEMORY;

  f = fopen(filename, "rb");
  if (f == NULL) {
    free(content);
    return MAILSMIME_ERROR_PARSE;
  }

  read_len = fread(content, 1, (size_t) stat_info.st_size, f);
  fclose(f);
  if (read_len != (size_t) stat_info.st_size) {
    free(content);
    return MAILSMIME_ERROR_PARSE;
  }

  content[(size_t) stat_info.st_size] = '\0';
  * result = content;
  * result_len = (size_t) stat_info.st_size;
  return MAILSMIME_NO_ERROR;
}

static CFDataRef cfdata_from_file(const char * filename)
{
  char * data;
  size_t data_len;
  CFDataRef cfdata;
  int r;

  r = read_file_to_mem(filename, &data, &data_len);
  if (r != MAILSMIME_NO_ERROR)
    return NULL;

  cfdata = CFDataCreate(NULL, (const UInt8 *) data, (CFIndex) data_len);
  free(data);
  return cfdata;
}

static SecCertificateRef apple_load_cert_file(const char * filename)
{
  CFDataRef data;
  SecCertificateRef cert;

  data = cfdata_from_file(filename);
  if (data == NULL)
    return NULL;

  cert = SecCertificateCreateWithData(NULL, data);
  if (cert == NULL) {
    SecExternalFormat format;
    SecExternalItemType item_type;
    SecItemImportExportKeyParameters params;
    CFArrayRef items;

    format = kSecFormatUnknown;
    item_type = kSecItemTypeCertificate;
    memset(&params, 0, sizeof(params));
    params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
    items = NULL;
    if (SecItemImport(data, NULL, &format, &item_type, 0, &params, NULL,
        &items) == errSecSuccess && items != NULL &&
        CFArrayGetCount(items) > 0) {
      CFTypeRef item;

      item = CFArrayGetValueAtIndex(items, 0);
      if (item != NULL && CFGetTypeID(item) == SecCertificateGetTypeID())
        cert = (SecCertificateRef) CFRetain(item);
    }
    if (items != NULL)
      CFRelease(items);
  }
  CFRelease(data);
  return cert;
}

static int apple_import_items_from_file(const char * filename,
    const char * passphrase, SecKeychainRef keychain, CFArrayRef * result)
{
  CFDataRef data;
  CFArrayRef items;
  SecExternalFormat format;
  SecExternalItemType item_type;
  SecItemImportExportKeyParameters params;
  CFStringRef cf_passphrase;
  SecAccessRef access;
  OSStatus status;

  if ((filename == NULL) || (result == NULL))
    return MAILSMIME_ERROR_INVAL;

  data = cfdata_from_file(filename);
  if (data == NULL)
    return MAILSMIME_ERROR_PARSE;
  access = NULL;
  {
    SecTrustedApplicationRef application;
    CFArrayRef applications;

    application = NULL;
    applications = NULL;
    if (SecTrustedApplicationCreateFromPath(NULL, &application) ==
        errSecSuccess) {
      const void * values[] = { application };

      applications = CFArrayCreate(NULL, values, 1,
          &kCFTypeArrayCallBacks);
      if (applications != NULL)
        SecAccessCreate(CFSTR("libetpan S/MIME temporary identity"),
            applications, &access);
    }
    if (applications != NULL)
      CFRelease(applications);
    if (application != NULL)
      CFRelease(application);
  }

  memset(&params, 0, sizeof(params));
  params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
  params.accessRef = access;
  cf_passphrase = NULL;
  if (passphrase != NULL) {
    cf_passphrase = CFStringCreateWithCString(NULL, passphrase,
        kCFStringEncodingUTF8);
    params.passphrase = cf_passphrase;
  }

  format = kSecFormatUnknown;
  item_type = kSecItemTypeUnknown;
  items = NULL;
  status = SecItemImport(data, NULL, &format, &item_type, 0, &params, keychain,
      &items);

  /* SecItemImport honors importKeychain directly.  Prefer it to
     SecPKCS12Import so loading a context does not expose its identity through
     the user's default keychain. */
  if ((status != errSecSuccess) || (items == NULL)) {
    CFMutableDictionaryRef options;
    CFStringRef password;

    if (items != NULL) {
      CFRelease(items);
      items = NULL;
    }
    options = CFDictionaryCreateMutable(NULL, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    password = CFStringCreateWithCString(NULL,
        passphrase != NULL ? passphrase : "", kCFStringEncodingUTF8);
    if ((options != NULL) && (password != NULL)) {
      CFDictionarySetValue(options, kSecImportExportPassphrase, password);
      CFDictionarySetValue(options, kSecImportExportKeychain, keychain);
      if (access != NULL)
        CFDictionarySetValue(options, kSecImportExportAccess, access);
      status = SecPKCS12Import(data, options, &items);
    }
    if (password != NULL)
      CFRelease(password);
    if (options != NULL)
      CFRelease(options);
  }

  if (cf_passphrase != NULL)
    CFRelease(cf_passphrase);
  if (access != NULL)
    CFRelease(access);
  CFRelease(data);

  if (status != errSecSuccess)
    return MAILSMIME_ERROR_CERT;
  if (items == NULL)
    return MAILSMIME_ERROR_CERT;

  * result = items;
  return MAILSMIME_NO_ERROR;
}

static SecIdentityRef apple_identity_from_items(CFArrayRef items,
    SecCertificateRef cert, SecKeychainRef keychain)
{
  CFIndex count;
  CFIndex i;

  if (items == NULL)
    return NULL;

  count = CFArrayGetCount(items);
  for (i = 0; i < count; i ++) {
    CFTypeRef item;
    SecIdentityRef identity;

    item = CFArrayGetValueAtIndex(items, i);
    identity = NULL;
    if ((item != NULL) && (CFGetTypeID(item) == SecIdentityGetTypeID()))
      identity = (SecIdentityRef) item;
    if ((item != NULL) && (CFGetTypeID(item) == CFDictionaryGetTypeID())) {
      identity = (SecIdentityRef) CFDictionaryGetValue(
          (CFDictionaryRef) item, kSecImportItemIdentity);
    }
    if (identity != NULL)
      return (SecIdentityRef) CFRetain(identity);
  }

  if (cert != NULL) {
    SecIdentityRef identity;

    identity = NULL;
    if (SecIdentityCreateWithCertificate(keychain, cert, &identity) ==
        errSecSuccess)
      return identity;
  }

  return NULL;
}

static char * apple_cfstring_to_cstring(CFStringRef string)
{
  CFIndex length;
  CFIndex max_size;
  char * result;

  if (string == NULL)
    return NULL;

  length = CFStringGetLength(string);
  max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
  result = malloc((size_t) max_size + 1);
  if (result == NULL)
    return NULL;

  if (!CFStringGetCString(string, result, max_size + 1,
      kCFStringEncodingUTF8)) {
    free(result);
    return NULL;
  }

  return result;
}

static char * apple_cert_summary(SecCertificateRef cert)
{
  CFStringRef summary;
  char * result;

  if (cert == NULL)
    return NULL;

  summary = SecCertificateCopySubjectSummary(cert);
  result = apple_cfstring_to_cstring(summary);
  if (summary != NULL)
    CFRelease(summary);
  return result;
}
#endif

int mailsmime_add_trusted_cert_file(struct mailsmime * smime,
    const char * filename)
{
  if ((smime == NULL) || (filename == NULL))
    return MAILSMIME_ERROR_INVAL;
#ifdef USE_SMIME_OPENSSL
  if (smime->backend == MAILSMIME_BACKEND_OPENSSL) {
    X509 * cert;

    cert = load_cert_file(filename);
    if (cert == NULL)
      return MAILSMIME_ERROR_CERT;

    if (X509_STORE_add_cert(smime->openssl_store, cert) != 1) {
      unsigned long err;

      err = ERR_peek_last_error();
      if (ERR_GET_REASON(err) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
        X509_free(cert);
        return MAILSMIME_ERROR_CERT;
      }
      ERR_clear_error();
    }
    X509_free(cert);
    return MAILSMIME_NO_ERROR;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (smime->backend == MAILSMIME_BACKEND_APPLE) {
    SecCertificateRef cert;

    cert = apple_load_cert_file(filename);
    if (cert == NULL)
      return MAILSMIME_ERROR_CERT;
    CFArrayAppendValue(smime->apple_trusted_certs, cert);
    CFRelease(cert);
    return MAILSMIME_NO_ERROR;
  }
#endif
  return MAILSMIME_ERROR_CRYPTO;
}

int mailsmime_add_cert_file(struct mailsmime * smime,
    const char * email,
    const char * filename)
{
  struct mailsmime_cert_entry * entry;

  if ((smime == NULL) || (email == NULL) || (filename == NULL))
    return MAILSMIME_ERROR_INVAL;

  entry = calloc(1, sizeof(* entry));
  if (entry == NULL)
    return MAILSMIME_ERROR_MEMORY;

  entry->email = strdup(email);
  entry->filename = strdup(filename);
  if ((entry->email == NULL) || (entry->filename == NULL))
    goto err;

#ifdef USE_SMIME_OPENSSL
  if (smime->backend == MAILSMIME_BACKEND_OPENSSL) {
    entry->openssl_cert = load_cert_file(filename);
    if (entry->openssl_cert == NULL)
      goto cert_err;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (smime->backend == MAILSMIME_BACKEND_APPLE) {
    entry->apple_cert = apple_load_cert_file(filename);
    if (entry->apple_cert == NULL)
      goto cert_err;
  }
#endif

  if (clist_append(smime->certs, entry) < 0)
    goto err;

  return MAILSMIME_NO_ERROR;

#if defined(USE_SMIME_OPENSSL) || defined(USE_SMIME_APPLE)
 cert_err:
  cert_entry_free(entry);
  return MAILSMIME_ERROR_CERT;
#endif
 err:
  cert_entry_free(entry);
  return MAILSMIME_ERROR_MEMORY;
}

int mailsmime_set_private_key_file(struct mailsmime * smime,
    const char * email,
    const char * cert_filename,
    const char * key_filename,
    const char * passphrase)
{
  struct mailsmime_key_entry * entry;

  if ((smime == NULL) || (email == NULL) || (cert_filename == NULL) ||
      (key_filename == NULL))
    return MAILSMIME_ERROR_INVAL;

  entry = calloc(1, sizeof(* entry));
  if (entry == NULL)
    return MAILSMIME_ERROR_MEMORY;

  entry->email = strdup(email);
  entry->cert_filename = strdup(cert_filename);
  entry->key_filename = strdup(key_filename);
  if (passphrase != NULL)
    entry->passphrase = strdup(passphrase);

  if ((entry->email == NULL) || (entry->cert_filename == NULL) ||
      (entry->key_filename == NULL) ||
      ((passphrase != NULL) && (entry->passphrase == NULL)))
    goto err;

#ifdef USE_SMIME_OPENSSL
  if (smime->backend == MAILSMIME_BACKEND_OPENSSL) {
    const char * key_passphrase;

    entry->openssl_cert = load_cert_file(cert_filename);
    if (entry->openssl_cert == NULL)
      goto cert_err;

    key_passphrase = entry->passphrase;
    if ((key_passphrase == NULL) && (smime->passphrase_callback != NULL))
      key_passphrase = smime->passphrase_callback(email,
          smime->passphrase_context);

    entry->openssl_key = load_key_file(key_filename, key_passphrase);
    if (entry->openssl_key == NULL)
      goto key_err;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (smime->backend == MAILSMIME_BACKEND_APPLE) {
    const char * key_passphrase;
    CFArrayRef imported_items;
    CFArrayRef previous_keychains;
    SecKeychainRef previous_default_keychain;

    key_passphrase = entry->passphrase;
    if ((key_passphrase == NULL) && (smime->passphrase_callback != NULL))
      key_passphrase = smime->passphrase_callback(email,
          smime->passphrase_context);

    entry->apple_cert = apple_load_cert_file(cert_filename);
    if (entry->apple_cert == NULL)
      goto cert_err;

    imported_items = NULL;
    previous_keychains = NULL;
    previous_default_keychain = NULL;
    if (apple_push_keychain_search(smime->apple_keychain, &previous_keychains,
        &previous_default_keychain) != MAILSMIME_NO_ERROR)
      goto key_err;
    if (apple_import_items_from_file(key_filename, key_passphrase,
        smime->apple_keychain,
        &imported_items) != MAILSMIME_NO_ERROR) {
      apple_pop_keychain_search(previous_keychains,
          previous_default_keychain);
      goto key_err;
    }

    entry->apple_identity = apple_identity_from_items(imported_items,
        entry->apple_cert, smime->apple_keychain);
    CFRelease(imported_items);
    apple_pop_keychain_search(previous_keychains, previous_default_keychain);
    if (entry->apple_identity == NULL)
      goto key_err;
  }
#endif

  if (clist_append(smime->keys, entry) < 0)
    goto err;

  return MAILSMIME_NO_ERROR;

#if defined(USE_SMIME_OPENSSL) || defined(USE_SMIME_APPLE)
 key_err:
  key_entry_free(entry);
  return MAILSMIME_ERROR_PRIVATE_KEY;
 cert_err:
  key_entry_free(entry);
  return MAILSMIME_ERROR_CERT;
#endif
 err:
  key_entry_free(entry);
  return MAILSMIME_ERROR_MEMORY;
}

#if defined(USE_SMIME_OPENSSL) || defined(USE_SMIME_APPLE)
static struct mailsmime_cert_entry * find_cert(struct mailsmime * smime,
    const char * email)
{
  clistiter * cur;

  for (cur = clist_begin(smime->certs); cur != NULL; cur = clist_next(cur)) {
    struct mailsmime_cert_entry * entry;

    entry = clist_content(cur);
    if (str_case_equal(entry->email, email))
      return entry;
  }

  return NULL;
}

static struct mailsmime_key_entry * find_key(struct mailsmime * smime,
    const char * email)
{
  clistiter * cur;

  for (cur = clist_begin(smime->keys); cur != NULL; cur = clist_next(cur)) {
    struct mailsmime_key_entry * entry;

    entry = clist_content(cur);
    if (str_case_equal(entry->email, email))
      return entry;
  }

  return NULL;
}

static int mmap_to_alloc(MMAPString * mmapstr, char ** result,
    size_t * result_len)
{
  char * str;

  str = malloc(mmapstr->len + 1);
  if (str == NULL)
    return MAILSMIME_ERROR_MEMORY;

  memcpy(str, mmapstr->str, mmapstr->len);
  str[mmapstr->len] = '\0';
  * result = str;
  * result_len = mmapstr->len;
  return MAILSMIME_NO_ERROR;
}

static int write_mime_to_mem(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  MMAPString * mmapstr;
  struct mailmime fake_parent;
  struct mailmime * saved_parent;
  int saved_parent_type;
  int col;
  int r;

  if ((mime == NULL) || (result == NULL) || (result_len == NULL))
    return MAILSMIME_ERROR_INVAL;

  mmapstr = mmap_string_new("");
  if (mmapstr == NULL)
    return MAILSMIME_ERROR_MEMORY;

  saved_parent = mime->mm_parent;
  saved_parent_type = mime->mm_parent_type;
  if ((mime->mm_type != MAILMIME_MESSAGE) && (mime->mm_parent == NULL)) {
    memset(&fake_parent, 0, sizeof(fake_parent));
    mime->mm_parent = &fake_parent;
    mime->mm_parent_type = MAILMIME_MESSAGE;
  }

  col = 0;
  r = mailmime_write_mem(mmapstr, &col, mime);
  mime->mm_parent = saved_parent;
  mime->mm_parent_type = saved_parent_type;
  if (r != MAILIMF_NO_ERROR) {
    mmap_string_free(mmapstr);
    return MAILSMIME_ERROR_PARSE;
  }

  r = mmap_to_alloc(mmapstr, result, result_len);
  mmap_string_free(mmapstr);
  return r;
}

static int canonical_mime_to_mem(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  if ((mime == NULL) || (result == NULL) || (result_len == NULL))
    return MAILSMIME_ERROR_INVAL;

  if ((mime->mm_mime_start != NULL) && (mime->mm_length != 0)) {
    char * data;

    data = malloc(mime->mm_length + 1);
    if (data == NULL)
      return MAILSMIME_ERROR_MEMORY;

    memcpy(data, mime->mm_mime_start, mime->mm_length);
    data[mime->mm_length] = '\0';
    * result = data;
    * result_len = mime->mm_length;
    return MAILSMIME_NO_ERROR;
  }

  return write_mime_to_mem(mime, result, result_len);
}

static int data_to_mem(struct mailmime_data * data, char ** result,
    size_t * result_len)
{
  size_t index;
  int r;

  if (data == NULL)
    return MAILSMIME_ERROR_INVAL;

  if (data->dt_type == MAILMIME_DATA_FILE) {
    FILE * f;
    struct stat stat_info;
    char * content;
    size_t read_len;

    if (stat(data->dt_data.dt_filename, &stat_info) < 0)
      return MAILSMIME_ERROR_PARSE;
    if (stat_info.st_size < 0)
      return MAILSMIME_ERROR_PARSE;

    content = malloc((size_t) stat_info.st_size + 1);
    if (content == NULL)
      return MAILSMIME_ERROR_MEMORY;

    f = fopen(data->dt_data.dt_filename, "rb");
    if (f == NULL) {
      free(content);
      return MAILSMIME_ERROR_PARSE;
    }

    read_len = fread(content, 1, (size_t) stat_info.st_size, f);
    fclose(f);
    if (read_len != (size_t) stat_info.st_size) {
      free(content);
      return MAILSMIME_ERROR_PARSE;
    }

    content[(size_t) stat_info.st_size] = '\0';
    if (!data->dt_encoded) {
      * result = content;
      * result_len = (size_t) stat_info.st_size;
      return MAILSMIME_NO_ERROR;
    }

    index = 0;
    {
      char * decoded;
      size_t decoded_len;

      r = mailmime_part_parse(content, (size_t) stat_info.st_size, &index,
          data->dt_encoding, &decoded, &decoded_len);
      if (r == MAILIMF_NO_ERROR) {
        char * copy;

        copy = malloc(decoded_len + 1);
        if (copy == NULL) {
          mailmime_decoded_part_free(decoded);
          free(content);
          return MAILSMIME_ERROR_MEMORY;
        }
        memcpy(copy, decoded, decoded_len);
        copy[decoded_len] = '\0';
        mailmime_decoded_part_free(decoded);
        * result = copy;
        * result_len = decoded_len;
      }
    }
    free(content);
    if (r != MAILIMF_NO_ERROR)
      return MAILSMIME_ERROR_PARSE;
    return MAILSMIME_NO_ERROR;
  }

  if (data->dt_type != MAILMIME_DATA_TEXT)
    return MAILSMIME_ERROR_INVAL;

  if (!data->dt_encoded) {
    char * copy;

    copy = malloc(data->dt_data.dt_text.dt_length + 1);
    if (copy == NULL)
      return MAILSMIME_ERROR_MEMORY;
    memcpy(copy, data->dt_data.dt_text.dt_data, data->dt_data.dt_text.dt_length);
    copy[data->dt_data.dt_text.dt_length] = '\0';
    * result = copy;
    * result_len = data->dt_data.dt_text.dt_length;
    return MAILSMIME_NO_ERROR;
  }

  index = 0;
  {
    char * decoded;
    size_t decoded_len;

    r = mailmime_part_parse(data->dt_data.dt_text.dt_data,
        data->dt_data.dt_text.dt_length, &index, data->dt_encoding, &decoded,
        &decoded_len);
    if (r != MAILIMF_NO_ERROR)
      return MAILSMIME_ERROR_PARSE;

    * result = malloc(decoded_len + 1);
    if (* result == NULL) {
      mailmime_decoded_part_free(decoded);
      return MAILSMIME_ERROR_MEMORY;
    }
    memcpy(* result, decoded, decoded_len);
    (* result)[decoded_len] = '\0';
    * result_len = decoded_len;
    mailmime_decoded_part_free(decoded);
  }

  return MAILSMIME_NO_ERROR;
}

static int smime_body_to_mem(struct mailmime * mime, char ** result,
    size_t * result_len)
{
  if ((mime == NULL) || (mime->mm_type != MAILMIME_SINGLE))
    return MAILSMIME_ERROR_INVAL;

  return data_to_mem(mime->mm_data.mm_single, result, result_len);
}

#ifdef USE_SMIME_OPENSSL
static BIO * bio_from_mem(const char * data, size_t len)
{
  if (len > (size_t) INT_MAX)
    return NULL;
  return BIO_new_mem_buf(data, (int) len);
}

static BIO * bio_from_text_mem(const char * data)
{
  return BIO_new_mem_buf(data, -1);
}

static int bio_to_alloc(BIO * bio, char ** result, size_t * result_len)
{
  BUF_MEM * mem;
  char * copy;

  if (BIO_flush(bio) != 1)
    return MAILSMIME_ERROR_CRYPTO;

  BIO_get_mem_ptr(bio, &mem);
  if (mem == NULL)
    return MAILSMIME_ERROR_CRYPTO;

  copy = malloc(mem->length + 1);
  if (copy == NULL)
    return MAILSMIME_ERROR_MEMORY;

  memcpy(copy, mem->data, mem->length);
  copy[mem->length] = '\0';
  * result = copy;
  * result_len = mem->length;
  return MAILSMIME_NO_ERROR;
}

static int cms_to_der(CMS_ContentInfo * cms, char ** result,
    size_t * result_len)
{
  BIO * out;
  int r;

  out = BIO_new(BIO_s_mem());
  if (out == NULL)
    return MAILSMIME_ERROR_MEMORY;

  if (i2d_CMS_bio(out, cms) != 1) {
    BIO_free(out);
    return MAILSMIME_ERROR_CRYPTO;
  }

  r = bio_to_alloc(out, result, result_len);
  BIO_free(out);
  return r;
}

static CMS_ContentInfo * cms_from_der(const char * data, size_t len)
{
  BIO * in;
  CMS_ContentInfo * cms;

  in = bio_from_mem(data, len);
  if (in == NULL)
    return NULL;

  cms = d2i_CMS_bio(in, NULL);
  BIO_free(in);
  return cms;
}
#endif

static int add_param(struct mailmime_content * content, const char * name,
    const char * value)
{
  struct mailmime_parameter * param;

  param = mailmime_param_new_with_data((char *) name, (char *) value);
  if (param == NULL)
    return MAILSMIME_ERROR_MEMORY;

  if (clist_append(content->ct_parameters, param) < 0) {
    mailmime_parameter_free(param);
    return MAILSMIME_ERROR_MEMORY;
  }

  return MAILSMIME_NO_ERROR;
}

static struct mailmime_fields * fields_with_encoding_and_disposition(
    int encoding_type, int disposition_type, const char * filename)
{
  struct mailmime_mechanism * encoding;
  struct mailmime_disposition * disposition;
  struct mailmime_fields * fields;

  encoding = mailmime_mechanism_new(encoding_type, NULL);
  if (encoding == NULL)
    return NULL;

  disposition = mailmime_disposition_new_filename(disposition_type,
      filename != NULL ? strdup(filename) : NULL);
  if (disposition == NULL)
    goto free_encoding;

  fields = mailmime_fields_new_with_data(encoding, NULL, NULL, disposition,
      NULL);
  if (fields == NULL)
    goto free_disposition;

  return fields;

 free_disposition:
  mailmime_disposition_free(disposition);
 free_encoding:
  mailmime_encoding_free(encoding);
  return NULL;
}

static int make_single_body(const char * content_type,
    struct mailmime_fields * fields,
    const char * body,
    size_t body_len,
    struct mailmime ** result)
{
  struct mailmime * mime;
  struct mailmime_data * data;
  char * body_copy;
  int r;

  r = mailmime_new_with_content(content_type, fields, &mime);
  if (r != MAILIMF_NO_ERROR)
    return MAILSMIME_ERROR_MEMORY;

  body_copy = malloc(body_len + 1);
  if (body_copy == NULL) {
    mailmime_free(mime);
    return MAILSMIME_ERROR_MEMORY;
  }
  memcpy(body_copy, body, body_len);
  body_copy[body_len] = '\0';

  data = mailmime_data_new_data(MAILMIME_MECHANISM_BASE64, 0, body_copy,
      body_len);
  if (data == NULL) {
    free(body_copy);
    mailmime_free(mime);
    return MAILSMIME_ERROR_MEMORY;
  }

  mime->mm_data.mm_single = data;
  mime->mm_body = data;
  r = mime_register_owned_buffer(mime, body_copy);
  if (r != MAILSMIME_NO_ERROR) {
    mailmime_free(mime);
    free(body_copy);
    return r;
  }
  * result = mime;
  return MAILSMIME_NO_ERROR;
}

static int make_pkcs7_mime(const char * smime_type, const char * der,
    size_t der_len, struct mailmime ** result)
{
  struct mailmime_fields * fields;
  struct mailmime * mime;
  int r;

  fields = fields_with_encoding_and_disposition(MAILMIME_MECHANISM_BASE64,
      MAILMIME_DISPOSITION_TYPE_ATTACHMENT, "smime.p7m");
  if (fields == NULL)
    return MAILSMIME_ERROR_MEMORY;

  r = make_single_body("application/pkcs7-mime", fields, der, der_len, &mime);
  if (r != MAILSMIME_NO_ERROR) {
    mailmime_fields_free(fields);
    return r;
  }

  r = add_param(mime->mm_content_type, "smime-type", smime_type);
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(mime);
    return r;
  }

  r = add_param(mime->mm_content_type, "name", "smime.p7m");
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(mime);
    return r;
  }

  * result = mime;
  return MAILSMIME_NO_ERROR;
}

static int make_signature_part(const char * der, size_t der_len,
    struct mailmime ** result)
{
  struct mailmime_fields * fields;
  struct mailmime * mime;
  int r;

  fields = fields_with_encoding_and_disposition(MAILMIME_MECHANISM_BASE64,
      MAILMIME_DISPOSITION_TYPE_ATTACHMENT, "smime.p7s");
  if (fields == NULL)
    return MAILSMIME_ERROR_MEMORY;

  r = make_single_body("application/pkcs7-signature", fields, der, der_len,
      &mime);
  if (r != MAILSMIME_NO_ERROR) {
    mailmime_fields_free(fields);
    return r;
  }

  r = add_param(mime->mm_content_type, "name", "smime.p7s");
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(mime);
    return r;
  }

  * result = mime;
  return MAILSMIME_NO_ERROR;
}

static int copy_mime(struct mailmime * mime, struct mailmime ** result)
{
  char * data;
  size_t len;
  size_t index;
  struct mailmime * parsed;
  int r;

  r = write_mime_to_mem(mime, &data, &len);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  index = 0;
  parsed = NULL;
  r = mailmime_parse(data, len, &index, &parsed);
  if (r != MAILIMF_NO_ERROR) {
    free(data);
    return MAILSMIME_ERROR_PARSE;
  }

  if ((parsed->mm_type == MAILMIME_MESSAGE) &&
      (parsed->mm_data.mm_message.mm_msg_mime != NULL)) {
    * result = parsed->mm_data.mm_message.mm_msg_mime;
    parsed->mm_data.mm_message.mm_msg_mime = NULL;
    mailmime_free(parsed);
  }
  else {
    * result = parsed;
  }

  r = mime_register_owned_buffer(* result, data);
  if (r != MAILSMIME_NO_ERROR) {
    mailmime_free(* result);
    free(data);
    * result = NULL;
    return r;
  }

  return MAILSMIME_NO_ERROR;
}

static int make_multipart_signed(struct mailmime * signed_part,
    const char * sig_der, size_t sig_der_len, struct mailmime ** result)
{
  struct mailmime * multipart;
  struct mailmime * signature;
  int r;

  r = mailmime_new_with_content("multipart/signed", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR)
    return MAILSMIME_ERROR_MEMORY;

  r = add_param(multipart->mm_content_type, "protocol",
      "application/pkcs7-signature");
  if (r != MAILSMIME_NO_ERROR)
    goto free_multipart;

  r = add_param(multipart->mm_content_type, "micalg", "sha-256");
  if (r != MAILSMIME_NO_ERROR)
    goto free_multipart;

  r = make_signature_part(sig_der, sig_der_len, &signature);
  if (r != MAILSMIME_NO_ERROR)
    goto free_signed_part;

  r = mailmime_smart_add_part(multipart, signed_part);
  if (r != MAILIMF_NO_ERROR)
    goto free_signature;

  r = mailmime_smart_add_part(multipart, signature);
  if (r != MAILIMF_NO_ERROR) {
    mailsmime_mime_free(signature);
    goto free_multipart;
  }

  * result = multipart;
  return MAILSMIME_NO_ERROR;

 free_signature:
  mailsmime_mime_free(signature);
 free_signed_part:
  mailsmime_mime_free(signed_part);
 free_multipart:
  mailsmime_mime_free(multipart);
  return MAILSMIME_ERROR_MEMORY;
}

static struct mailmime * multipart_first_part(struct mailmime * mime)
{
  clistiter * cur;

  if ((mime == NULL) || (mime->mm_type != MAILMIME_MULTIPLE))
    return NULL;

  cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
  if (cur == NULL)
    return NULL;

  return clist_content(cur);
}

#ifdef USE_SMIME_OPENSSL
static char * x509_name_to_string(X509_NAME * name)
{
  BIO * bio;
  char * result;
  size_t len;

  if (name == NULL)
    return NULL;

  bio = BIO_new(BIO_s_mem());
  if (bio == NULL)
    return NULL;

  X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);
  if (bio_to_alloc(bio, &result, &len) != MAILSMIME_NO_ERROR)
    result = NULL;
  BIO_free(bio);
  return result;
}

static char * x509_subject_email(X509 * cert)
{
  GENERAL_NAMES * names;
  int count;
  int i;
  char * result;

  names = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
  if (names != NULL) {
    count = sk_GENERAL_NAME_num(names);
    for (i = 0; i < count; i ++) {
      GENERAL_NAME * name;

      name = sk_GENERAL_NAME_value(names, i);
      if (name->type == GEN_EMAIL) {
        ASN1_IA5STRING * str;
        int length;

        str = name->d.rfc822Name;
        length = ASN1_STRING_length(str);
        result = malloc((size_t) length + 1);
        if (result != NULL) {
          memcpy(result, ASN1_STRING_get0_data(str), (size_t) length);
          result[length] = '\0';
        }
        GENERAL_NAMES_free(names);
        return result;
      }
    }
    GENERAL_NAMES_free(names);
  }

  {
    X509_NAME * subject;
    int index;

    subject = X509_get_subject_name(cert);
    index = X509_NAME_get_index_by_NID(subject, NID_pkcs9_emailAddress, -1);
    if (index >= 0) {
      X509_NAME_ENTRY * entry;
      ASN1_STRING * str;

      entry = X509_NAME_get_entry(subject, index);
      str = X509_NAME_ENTRY_get_data(entry);
      result = malloc((size_t) ASN1_STRING_length(str) + 1);
      if (result == NULL)
        return NULL;
      memcpy(result, ASN1_STRING_get0_data(str),
          (size_t) ASN1_STRING_length(str));
      result[ASN1_STRING_length(str)] = '\0';
      return result;
    }
  }

  return NULL;
}

static char * x509_common_name(X509 * cert)
{
  X509_NAME * subject;
  int index;
  X509_NAME_ENTRY * entry;
  ASN1_STRING * str;
  char * result;

  subject = X509_get_subject_name(cert);
  index = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
  if (index < 0)
    return NULL;

  entry = X509_NAME_get_entry(subject, index);
  str = X509_NAME_ENTRY_get_data(entry);
  result = malloc((size_t) ASN1_STRING_length(str) + 1);
  if (result == NULL)
    return NULL;
  memcpy(result, ASN1_STRING_get0_data(str), (size_t) ASN1_STRING_length(str));
  result[ASN1_STRING_length(str)] = '\0';
  return result;
}

static char * x509_fingerprint_sha256(X509 * cert)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len;
  char * result;
  unsigned int i;

  if (X509_digest(cert, EVP_sha256(), md, &len) != 1)
    return NULL;

  result = malloc((size_t) len * 3 + 1);
  if (result == NULL)
    return NULL;

  for (i = 0; i < len; i ++)
    snprintf(result + i * 3, (size_t) (len - i) * 3 + 1, "%02X%s",
        md[i], i + 1 == len ? "" : ":");

  return result;
}

static char * x509_time_to_string(const ASN1_TIME * time)
{
  BIO * out;
  char * result;
  size_t result_len;
  int r;

  if (time == NULL)
    return NULL;

  out = BIO_new(BIO_s_mem());
  if (out == NULL)
    return NULL;

  if (ASN1_TIME_print(out, time) != 1) {
    BIO_free(out);
    return NULL;
  }

  r = bio_to_alloc(out, &result, &result_len);
  (void) result_len;
  BIO_free(out);
  return r == MAILSMIME_NO_ERROR ? result : NULL;
}

static struct mailsmime_certificate * certificate_new_from_x509(X509 * x509)
{
  struct mailsmime_certificate * cert;

  cert = calloc(1, sizeof(* cert));
  if (cert == NULL)
    return NULL;

  cert->backend = MAILSMIME_BACKEND_OPENSSL;
  cert->openssl_cert = X509_dup(x509);
  if (cert->openssl_cert == NULL)
    goto err;

  cert->email = x509_subject_email(x509);
  cert->name = x509_common_name(x509);
  if ((cert->name == NULL) && (cert->email != NULL))
    cert->name = strdup(cert->email);
  cert->subject = x509_name_to_string(X509_get_subject_name(x509));
  cert->issuer = x509_name_to_string(X509_get_issuer_name(x509));
  cert->not_before = x509_time_to_string(X509_get_notBefore(x509));
  cert->not_after = x509_time_to_string(X509_get_notAfter(x509));
  cert->fingerprint_sha256 = x509_fingerprint_sha256(x509);

  return cert;

 err:
  mailsmime_certificate_free(cert);
  return NULL;
}
#endif

#ifdef USE_SMIME_APPLE
static char * apple_cert_fingerprint_sha256(SecCertificateRef cert)
{
  CFDataRef data;
  unsigned char md[CC_SHA256_DIGEST_LENGTH];
  char * result;
  CFIndex len;
  unsigned int i;

  if (cert == NULL)
    return NULL;

  data = SecCertificateCopyData(cert);
  if (data == NULL)
    return NULL;

  CC_SHA256(CFDataGetBytePtr(data), (CC_LONG) CFDataGetLength(data), md);
  CFRelease(data);

  len = CC_SHA256_DIGEST_LENGTH;
  result = malloc((size_t) len * 3 + 1);
  if (result == NULL)
    return NULL;

  for (i = 0; i < (unsigned int) len; i ++)
    snprintf(result + i * 3, (size_t) (len - i) * 3 + 1, "%02X%s",
        md[i], i + 1 == (unsigned int) len ? "" : ":");

  return result;
}

static char * apple_cert_property_description(SecCertificateRef cert,
    CFStringRef oid)
{
  const void * values[] = { oid };
  CFArrayRef keys;
  CFDictionaryRef properties;
  CFDictionaryRef property;
  CFTypeRef value;
  CFStringRef description;
  char * result;

  keys = CFArrayCreate(NULL, values, 1, &kCFTypeArrayCallBacks);
  if (keys == NULL)
    return NULL;
  properties = SecCertificateCopyValues(cert, keys, NULL);
  CFRelease(keys);
  if (properties == NULL)
    return NULL;

  property = (CFDictionaryRef) CFDictionaryGetValue(properties, oid);
  value = property != NULL ? CFDictionaryGetValue(property,
      kSecPropertyKeyValue) : NULL;
  description = value != NULL ? CFCopyDescription(value) : NULL;
  result = apple_cfstring_to_cstring(description);
  if (description != NULL)
    CFRelease(description);
  CFRelease(properties);
  return result;
}

static struct mailsmime_certificate * certificate_new_from_sec_certificate(
    SecCertificateRef sec_cert)
{
  struct mailsmime_certificate * cert;
  char * summary;

  cert = calloc(1, sizeof(* cert));
  if (cert == NULL)
    return NULL;

  cert->backend = MAILSMIME_BACKEND_APPLE;
  cert->apple_cert = (SecCertificateRef) CFRetain(sec_cert);
  {
    CFArrayRef emails;

    emails = NULL;
    if (SecCertificateCopyEmailAddresses(sec_cert, &emails) == errSecSuccess &&
        emails != NULL && CFArrayGetCount(emails) > 0) {
      CFStringRef email;

      email = (CFStringRef) CFArrayGetValueAtIndex(emails, 0);
      cert->email = apple_cfstring_to_cstring(email);
    }
    if (emails != NULL)
      CFRelease(emails);
  }
  summary = apple_cert_summary(sec_cert);
  cert->name = summary != NULL ? strdup(summary) : NULL;
  cert->subject = summary;
  cert->issuer = NULL;
  cert->not_before = apple_cert_property_description(sec_cert,
      kSecOIDX509V1ValidityNotBefore);
  cert->not_after = apple_cert_property_description(sec_cert,
      kSecOIDX509V1ValidityNotAfter);
  cert->fingerprint_sha256 = apple_cert_fingerprint_sha256(sec_cert);

  return cert;
}
#endif

static void result_set_error(struct mailsmime_result * result,
    const char * error)
{
  free(result->error);
  result->error = error != NULL ? strdup(error) : NULL;
}

static struct mailsmime_result * result_new(void)
{
  struct mailsmime_result * result;

  result = calloc(1, sizeof(* result));
  if (result == NULL)
    return NULL;
  result->status = MAILSMIME_VERIFY_ERROR;
  result->signers = clist_new();
  if (result->signers == NULL) {
    free(result);
    return NULL;
  }
  return result;
}

#ifdef USE_SMIME_OPENSSL
static int result_add_signers(struct mailsmime_result * result,
    CMS_ContentInfo * cms)
{
  STACK_OF(X509) * signers;
  int count;
  int i;

  signers = CMS_get0_signers(cms);
  if (signers == NULL)
    return MAILSMIME_NO_ERROR;

  count = sk_X509_num(signers);
  for (i = 0; i < count; i ++) {
    struct mailsmime_certificate * cert;
    X509 * x509;

    x509 = sk_X509_value(signers, i);
    cert = certificate_new_from_x509(x509);
    if (cert == NULL) {
      sk_X509_free(signers);
      return MAILSMIME_ERROR_MEMORY;
    }

    if (clist_append(result->signers, cert) < 0) {
      mailsmime_certificate_free(cert);
      sk_X509_free(signers);
      return MAILSMIME_ERROR_MEMORY;
    }
  }

  sk_X509_free(signers);
  return MAILSMIME_NO_ERROR;
}

static int verify_error_to_status(int verify_error)
{
  switch (verify_error) {
  case X509_V_ERR_CERT_HAS_EXPIRED:
  case X509_V_ERR_CRL_HAS_EXPIRED:
    return MAILSMIME_VERIFY_EXPIRED;

  case X509_V_ERR_CERT_REVOKED:
    return MAILSMIME_VERIFY_REVOKED;

  case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
  case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
  case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
  case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
  case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
  case X509_V_ERR_CERT_UNTRUSTED:
    return MAILSMIME_VERIFY_UNTRUSTED;

  default:
    return MAILSMIME_VERIFY_INVALID;
  }
}

static void result_set_certificate_verify_error(struct mailsmime * smime,
    struct mailsmime_result * result, CMS_ContentInfo * cms,
    const char * default_error)
{
  STACK_OF(X509) * signers;
  int i;

  signers = CMS_get0_signers(cms);
  if (signers == NULL) {
    result->status = MAILSMIME_VERIFY_INVALID;
    result_set_error(result, default_error);
    return;
  }

  for (i = 0; i < sk_X509_num(signers); i ++) {
    X509 * cert;
    X509_STORE_CTX * store_ctx;
    int verify_result;

    cert = sk_X509_value(signers, i);
    store_ctx = X509_STORE_CTX_new();
    if (store_ctx == NULL)
      continue;

    if (X509_STORE_CTX_init(store_ctx, smime->openssl_store, cert, signers) != 1) {
      X509_STORE_CTX_free(store_ctx);
      continue;
    }

    verify_result = X509_verify_cert(store_ctx);
    if (verify_result != 1) {
      int verify_error;

      verify_error = X509_STORE_CTX_get_error(store_ctx);
      result->status = verify_error_to_status(verify_error);
      result_set_error(result, X509_verify_cert_error_string(verify_error));
      X509_STORE_CTX_free(store_ctx);
      sk_X509_free(signers);
      return;
    }

    X509_STORE_CTX_free(store_ctx);
  }

  sk_X509_free(signers);
  result->status = MAILSMIME_VERIFY_INVALID;
  result_set_error(result, default_error);
}

static void result_set_certificate_verify_status(struct mailsmime * smime,
    struct mailsmime_result * result, CMS_ContentInfo * cms)
{
  STACK_OF(X509) * signers;
  int i;

  signers = CMS_get0_signers(cms);
  if (signers == NULL) {
    result->status = MAILSMIME_VERIFY_INVALID;
    result_set_error(result, "S/MIME signer certificate missing");
    return;
  }

  for (i = 0; i < sk_X509_num(signers); i ++) {
    X509 * cert;
    X509_STORE_CTX * store_ctx;

    cert = sk_X509_value(signers, i);
    store_ctx = X509_STORE_CTX_new();
    if (store_ctx == NULL) {
      sk_X509_free(signers);
      result->status = MAILSMIME_VERIFY_ERROR;
      result_set_error(result, "S/MIME certificate verification failed");
      return;
    }

    if (X509_STORE_CTX_init(store_ctx, smime->openssl_store, cert, signers) != 1) {
      X509_STORE_CTX_free(store_ctx);
      sk_X509_free(signers);
      result->status = MAILSMIME_VERIFY_ERROR;
      result_set_error(result, "S/MIME certificate verification failed");
      return;
    }

    if (X509_verify_cert(store_ctx) != 1) {
      int verify_error;

      verify_error = X509_STORE_CTX_get_error(store_ctx);
      result->status = verify_error_to_status(verify_error);
      result_set_error(result, X509_verify_cert_error_string(verify_error));
      X509_STORE_CTX_free(store_ctx);
      sk_X509_free(signers);
      return;
    }

    X509_STORE_CTX_free(store_ctx);
  }

  sk_X509_free(signers);
  result->status = MAILSMIME_VERIFY_VALID;
}
#endif

static int parse_mime_from_mem(const char * data, size_t len,
    struct mailmime ** result)
{
  size_t index;
  char * data_copy;
  int r;

  data_copy = malloc(len + 1);
  if (data_copy == NULL)
    return MAILSMIME_ERROR_MEMORY;
  memcpy(data_copy, data, len);
  data_copy[len] = '\0';

  index = 0;
  r = mailmime_parse(data_copy, len, &index, result);
  if (r != MAILIMF_NO_ERROR) {
    free(data_copy);
    return MAILSMIME_ERROR_PARSE;
  }

  r = mime_register_owned_buffer(* result, data_copy);
  if (r != MAILSMIME_NO_ERROR) {
    mailmime_free(* result);
    free(data_copy);
    * result = NULL;
    return r;
  }
  return MAILSMIME_NO_ERROR;
}

#ifdef USE_SMIME_OPENSSL
static int openssl_verify_smime_entity(struct mailsmime * smime,
    struct mailmime * mime, struct mailsmime_result * result)
{
  char * data;
  size_t data_len;
  BIO * in;
  BIO * dcont;
  BIO * out;
  CMS_ContentInfo * cms;
  int verify_flags;
  int r;

  dcont = NULL;
  r = canonical_mime_to_mem(mime, &data, &data_len);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  in = bio_from_text_mem(data);
  if (in == NULL) {
    free(data);
    return MAILSMIME_ERROR_MEMORY;
  }

  cms = SMIME_read_CMS(in, &dcont);
  BIO_free(in);
  free(data);
  if (cms == NULL) {
    BIO_free(dcont);
    return MAILSMIME_ERROR_PARSE;
  }

  out = BIO_new(BIO_s_mem());
  if (out == NULL) {
    CMS_ContentInfo_free(cms);
    BIO_free(dcont);
    return MAILSMIME_ERROR_MEMORY;
  }

  r = MAILSMIME_NO_ERROR;
  verify_flags = CMS_BINARY;
#ifdef CMS_NO_SIGNER_CERT_VERIFY
  verify_flags |= CMS_NO_SIGNER_CERT_VERIFY;
#endif
  if (CMS_verify(cms, NULL, smime->openssl_store, dcont, out, verify_flags) == 1) {
    result_set_certificate_verify_status(smime, result, cms);
    if (content_is(mime->mm_content_type, 0, MAILMIME_COMPOSITE_TYPE_MULTIPART,
        "signed")) {
      struct mailmime * signed_part;
      int copy_r;

      signed_part = multipart_first_part(mime);
      copy_r = copy_mime(signed_part, &result->signed_mime);
      (void) copy_r;
    }
    else {
      char * content;
      size_t content_len;

      r = bio_to_alloc(out, &content, &content_len);
      if (r == MAILSMIME_NO_ERROR) {
        r = parse_mime_from_mem(content, content_len, &result->signed_mime);
        free(content);
      }
    }
  }
  else {
    result_set_certificate_verify_error(smime, result, cms,
        "S/MIME signature verification failed");
    ERR_clear_error();
  }

  if (r == MAILSMIME_NO_ERROR)
    r = result_add_signers(result, cms);

  BIO_free(out);
  BIO_free(dcont);
  CMS_ContentInfo_free(cms);
  return r;
}
#endif

#ifdef USE_SMIME_APPLE
static int cfdata_to_alloc(CFDataRef data, char ** result,
    size_t * result_len)
{
  CFIndex len;
  char * copy;

  if ((data == NULL) || (result == NULL) || (result_len == NULL))
    return MAILSMIME_ERROR_INVAL;

  len = CFDataGetLength(data);
  copy = malloc((size_t) len + 1);
  if (copy == NULL)
    return MAILSMIME_ERROR_MEMORY;

  memcpy(copy, CFDataGetBytePtr(data), (size_t) len);
  copy[len] = '\0';
  * result = copy;
  * result_len = (size_t) len;
  return MAILSMIME_NO_ERROR;
}

static int apple_result_add_signers(struct mailsmime_result * result,
    CMSDecoderRef decoder)
{
  size_t count;
  size_t i;
  OSStatus status;

  status = CMSDecoderGetNumSigners(decoder, &count);
  if (status != errSecSuccess)
    return MAILSMIME_NO_ERROR;

  for (i = 0; i < count; i ++) {
    SecCertificateRef sec_cert;
    struct mailsmime_certificate * cert;

    sec_cert = NULL;
    status = CMSDecoderCopySignerCert(decoder, i, &sec_cert);
    if ((status != errSecSuccess) || (sec_cert == NULL))
      continue;

    cert = certificate_new_from_sec_certificate(sec_cert);
    CFRelease(sec_cert);
    if (cert == NULL)
      return MAILSMIME_ERROR_MEMORY;

    if (clist_append(result->signers, cert) < 0) {
      mailsmime_certificate_free(cert);
      return MAILSMIME_ERROR_MEMORY;
    }
  }

  return MAILSMIME_NO_ERROR;
}

static int apple_trust_status(struct mailsmime * smime, SecTrustRef trust)
{
  SecTrustResultType trust_result;
  OSStatus status;

  if (trust == NULL)
    return MAILSMIME_VERIFY_INVALID;

  if ((smime->apple_trusted_certs != NULL) &&
      (CFArrayGetCount(smime->apple_trusted_certs) > 0)) {
    status = SecTrustSetAnchorCertificates(trust, smime->apple_trusted_certs);
    if (status != errSecSuccess)
      return MAILSMIME_VERIFY_INVALID;
  }

  trust_result = kSecTrustResultInvalid;
  status = SecTrustEvaluate(trust, &trust_result);
  if (status != errSecSuccess)
    return MAILSMIME_VERIFY_INVALID;

  if ((trust_result == kSecTrustResultProceed) ||
      (trust_result == kSecTrustResultUnspecified))
    return MAILSMIME_VERIFY_VALID;

  return MAILSMIME_VERIFY_UNTRUSTED;
}

static int apple_verify_decoder(struct mailsmime * smime,
    CMSDecoderRef decoder, struct mailmime * mime,
    struct mailsmime_result * result)
{
  CMSSignerStatus signer_status;
  SecPolicyRef policy;
  SecTrustRef trust;
  OSStatus verify_status;
  OSStatus status;
  int r;

  policy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, NULL);
  if (policy == NULL)
    policy = SecPolicyCreateBasicX509();
  if (policy == NULL)
    return MAILSMIME_ERROR_MEMORY;

  signer_status = kCMSSignerInvalidSignature;
  trust = NULL;
  verify_status = errSecSuccess;
  status = CMSDecoderCopySignerStatus(decoder, 0, policy, false,
      &signer_status, &trust, &verify_status);
  CFRelease(policy);
  if (status != errSecSuccess) {
    if (trust != NULL)
      CFRelease(trust);
    return MAILSMIME_ERROR_VERIFY;
  }

  if (signer_status == kCMSSignerValid) {
    int trust_status;

    trust_status = apple_trust_status(smime, trust);
    result->status = trust_status;
    if (trust_status == MAILSMIME_VERIFY_VALID) {
      if (content_is(mime->mm_content_type, 0,
          MAILMIME_COMPOSITE_TYPE_MULTIPART, "signed")) {
        struct mailmime * signed_part;
        int copy_r;

        signed_part = multipart_first_part(mime);
        copy_r = copy_mime(signed_part, &result->signed_mime);
        (void) copy_r;
      }
      else {
        CFDataRef content;

        content = NULL;
        if (CMSDecoderCopyContent(decoder, &content) == errSecSuccess) {
          char * content_data;
          size_t content_len;

          content_data = NULL;
          content_len = 0;
          r = cfdata_to_alloc(content, &content_data, &content_len);
          CFRelease(content);
          if (r == MAILSMIME_NO_ERROR) {
            r = parse_mime_from_mem(content_data, content_len,
                &result->signed_mime);
            free(content_data);
            if (r != MAILSMIME_NO_ERROR) {
              if (trust != NULL)
                CFRelease(trust);
              return r;
            }
          }
        }
      }
    }
  }
  else {
    result->status = MAILSMIME_VERIFY_INVALID;
    result_set_error(result, "S/MIME signature verification failed");
  }

  if (trust != NULL)
    CFRelease(trust);

  r = apple_result_add_signers(result, decoder);
  return r;
}

static int apple_decoder_update(CMSDecoderRef decoder, const char * data,
    size_t data_len)
{
  if (data_len > (size_t) LONG_MAX)
    return MAILSMIME_ERROR_INVAL;
  if (CMSDecoderUpdateMessage(decoder, data, data_len) != errSecSuccess)
    return MAILSMIME_ERROR_PARSE;
  return MAILSMIME_NO_ERROR;
}

static int apple_verify_smime_entity(struct mailsmime * smime,
    struct mailmime * mime, struct mailsmime_result * result)
{
  CMSDecoderRef decoder;
  CFDataRef detached_content;
  char * cms_data;
  size_t cms_len;
  int r;

  if (content_is(mime->mm_content_type, 0, MAILMIME_COMPOSITE_TYPE_MULTIPART,
      "signed")) {
    struct mailmime * signed_part;
    struct mailmime * sig_part;
    char * signed_data;
    size_t signed_len;

    signed_part = multipart_first_part(mime);
    sig_part = NULL;
    if ((mime != NULL) && (mime->mm_type == MAILMIME_MULTIPLE)) {
      clistiter * cur;

      cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
      if (cur != NULL)
        cur = clist_next(cur);
      if (cur != NULL)
        sig_part = clist_content(cur);
    }
    if ((signed_part == NULL) || (sig_part == NULL))
      return MAILSMIME_ERROR_PARSE;

    r = canonical_mime_to_mem(signed_part, &signed_data, &signed_len);
    if (r != MAILSMIME_NO_ERROR)
      return r;
    r = smime_body_to_mem(sig_part, &cms_data, &cms_len);
    if (r != MAILSMIME_NO_ERROR) {
      free(signed_data);
      return r;
    }

    detached_content = CFDataCreate(NULL, (const UInt8 *) signed_data,
        (CFIndex) signed_len);
    free(signed_data);
    if (detached_content == NULL) {
      free(cms_data);
      return MAILSMIME_ERROR_MEMORY;
    }
  }
  else {
    detached_content = NULL;
    r = smime_body_to_mem(mime, &cms_data, &cms_len);
    if (r != MAILSMIME_NO_ERROR)
      return r;
  }

  decoder = NULL;
  if (CMSDecoderCreate(&decoder) != errSecSuccess) {
    if (detached_content != NULL)
      CFRelease(detached_content);
    free(cms_data);
    return MAILSMIME_ERROR_MEMORY;
  }

  if (detached_content != NULL) {
    if (CMSDecoderSetDetachedContent(decoder, detached_content) !=
        errSecSuccess) {
      CFRelease(detached_content);
      CFRelease(decoder);
      free(cms_data);
      return MAILSMIME_ERROR_PARSE;
    }
    CFRelease(detached_content);
  }

  r = apple_decoder_update(decoder, cms_data, cms_len);
  free(cms_data);
  if (r == MAILSMIME_NO_ERROR) {
    if (CMSDecoderFinalizeMessage(decoder) != errSecSuccess)
      r = MAILSMIME_ERROR_PARSE;
  }
  if (r == MAILSMIME_NO_ERROR)
    r = apple_verify_decoder(smime, decoder, mime, result);

  CFRelease(decoder);
  return r;
}
#endif
#endif

int mailsmime_sign(struct mailsmime * smime,
    struct mailmime * mime,
    const char * signer_email,
    struct mailmime ** result)
{
  if ((smime == NULL) || (mime == NULL) || (signer_email == NULL) ||
      (result == NULL))
    return MAILSMIME_ERROR_INVAL;
#ifdef USE_SMIME_OPENSSL
  if (smime->backend == MAILSMIME_BACKEND_OPENSSL) {
  struct mailsmime_key_entry * key;
  struct mailmime * signed_part;
  char * data;
  size_t data_len;
  char * der;
  size_t der_len;
  BIO * in;
  CMS_ContentInfo * cms;
  int r;

  key = find_key(smime, signer_email);
  if (key == NULL)
    return MAILSMIME_ERROR_PRIVATE_KEY;

  r = copy_mime(mime, &signed_part);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  r = canonical_mime_to_mem(signed_part, &data, &data_len);
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(signed_part);
    return r;
  }

  in = bio_from_mem(data, data_len);
  if (in == NULL) {
    mailsmime_mime_free(signed_part);
    free(data);
    return MAILSMIME_ERROR_MEMORY;
  }

  cms = CMS_sign(key->openssl_cert, key->openssl_key, NULL, in,
      CMS_BINARY | CMS_DETACHED);
  BIO_free(in);
  if (cms == NULL) {
    free(data);
    mailsmime_mime_free(signed_part);
    return MAILSMIME_ERROR_CRYPTO;
  }

  r = cms_to_der(cms, &der, &der_len);
  free(data);
  CMS_ContentInfo_free(cms);
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(signed_part);
    return r;
  }

  r = make_multipart_signed(signed_part, der, der_len, result);
  free(der);
  return r;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (smime->backend == MAILSMIME_BACKEND_APPLE) {
  struct mailsmime_key_entry * key;
  struct mailmime * signed_part;
  CMSEncoderRef encoder;
  CFDataRef encoded;
  char * data;
  size_t data_len;
  char * der;
  size_t der_len;
  int r;

  key = find_key(smime, signer_email);
  if ((key == NULL) || (key->apple_identity == NULL))
    return MAILSMIME_ERROR_PRIVATE_KEY;

  r = copy_mime(mime, &signed_part);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  r = canonical_mime_to_mem(signed_part, &data, &data_len);
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(signed_part);
    return r;
  }

  encoder = NULL;
  encoded = NULL;
  if (CMSEncoderCreate(&encoder) != errSecSuccess) {
    mailsmime_mime_free(signed_part);
    free(data);
    return MAILSMIME_ERROR_MEMORY;
  }

  if ((CMSEncoderSetHasDetachedContent(encoder, true) != errSecSuccess) ||
      (CMSEncoderAddSigners(encoder, key->apple_identity) != errSecSuccess) ||
      ((key->apple_cert != NULL) &&
      (CMSEncoderAddSupportingCerts(encoder, key->apple_cert) != errSecSuccess)) ||
      (CMSEncoderUpdateContent(encoder, data, data_len) != errSecSuccess) ||
      (CMSEncoderCopyEncodedContent(encoder, &encoded) != errSecSuccess)) {
    CFRelease(encoder);
    mailsmime_mime_free(signed_part);
    free(data);
    return MAILSMIME_ERROR_CRYPTO;
  }
  free(data);

  r = cfdata_to_alloc(encoded, &der, &der_len);
  CFRelease(encoded);
  CFRelease(encoder);
  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_mime_free(signed_part);
    return r;
  }

  r = make_multipart_signed(signed_part, der, der_len, result);
  free(der);
  return r;
  }
#endif
  return MAILSMIME_ERROR_CRYPTO;
}

int mailsmime_verify(struct mailsmime * smime,
    struct mailmime * mime,
    struct mailsmime_result ** result)
{
#if defined(USE_SMIME_OPENSSL) || defined(USE_SMIME_APPLE)
  struct mailsmime_result * verify_result;
  int r;

  if ((smime == NULL) || (mime == NULL) || (result == NULL))
    return MAILSMIME_ERROR_INVAL;

  verify_result = result_new();
  if (verify_result == NULL)
    return MAILSMIME_ERROR_MEMORY;

  if (!(content_is(mime->mm_content_type, 0,
      MAILMIME_COMPOSITE_TYPE_MULTIPART, "signed") ||
      content_is_pkcs7_mime(mime->mm_content_type)))
    r = MAILSMIME_ERROR_INVAL;
  else {
    r = MAILSMIME_ERROR_CRYPTO;
#ifdef USE_SMIME_OPENSSL
    if (smime->backend == MAILSMIME_BACKEND_OPENSSL)
      r = openssl_verify_smime_entity(smime, mime, verify_result);
#endif
#ifdef USE_SMIME_APPLE
    if (smime->backend == MAILSMIME_BACKEND_APPLE)
      r = apple_verify_smime_entity(smime, mime, verify_result);
#endif
  }

  if (r != MAILSMIME_NO_ERROR) {
    mailsmime_result_free(verify_result);
    return r;
  }

  * result = verify_result;
  return MAILSMIME_NO_ERROR;
#else
  (void) smime;
  (void) mime;
  (void) result;
  return MAILSMIME_ERROR_CRYPTO;
#endif
}

int mailsmime_encrypt(struct mailsmime * smime,
    struct mailmime * mime,
    const char ** recipient_emails,
    unsigned int recipient_count,
    struct mailmime ** result)
{
  if ((smime == NULL) || (mime == NULL) || (recipient_emails == NULL) ||
      (recipient_count == 0) || (result == NULL))
    return MAILSMIME_ERROR_INVAL;
#ifdef USE_SMIME_OPENSSL
  if (smime->backend == MAILSMIME_BACKEND_OPENSSL) {
  STACK_OF(X509) * certs;
  char * data;
  size_t data_len;
  char * der;
  size_t der_len;
  BIO * in;
  CMS_ContentInfo * cms;
  unsigned int i;
  int r;

  certs = sk_X509_new_null();
  if (certs == NULL)
    return MAILSMIME_ERROR_MEMORY;

  for (i = 0; i < recipient_count; i ++) {
    struct mailsmime_cert_entry * entry;

    entry = find_cert(smime, recipient_emails[i]);
    if (entry == NULL) {
      sk_X509_free(certs);
      return MAILSMIME_ERROR_CERT;
    }
    if (sk_X509_push(certs, entry->openssl_cert) == 0) {
      sk_X509_free(certs);
      return MAILSMIME_ERROR_MEMORY;
    }
  }

  r = write_mime_to_mem(mime, &data, &data_len);
  if (r != MAILSMIME_NO_ERROR) {
    sk_X509_free(certs);
    return r;
  }

  in = bio_from_mem(data, data_len);
  if (in == NULL) {
    free(data);
    sk_X509_free(certs);
    return MAILSMIME_ERROR_MEMORY;
  }

  cms = CMS_encrypt(certs, in, EVP_aes_256_cbc(), CMS_BINARY);
  BIO_free(in);
  sk_X509_free(certs);
  if (cms == NULL) {
    free(data);
    return MAILSMIME_ERROR_CRYPTO;
  }

  r = cms_to_der(cms, &der, &der_len);
  free(data);
  CMS_ContentInfo_free(cms);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  r = make_pkcs7_mime("enveloped-data", der, der_len, result);
  free(der);
  return r;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (smime->backend == MAILSMIME_BACKEND_APPLE) {
  CMSEncoderRef encoder;
  CFMutableArrayRef certs;
  CFDataRef encoded;
  char * data;
  size_t data_len;
  char * der;
  size_t der_len;
  unsigned int i;
  int r;

  certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
  if (certs == NULL)
    return MAILSMIME_ERROR_MEMORY;

  for (i = 0; i < recipient_count; i ++) {
    struct mailsmime_cert_entry * entry;

    entry = find_cert(smime, recipient_emails[i]);
    if ((entry == NULL) || (entry->apple_cert == NULL)) {
      CFRelease(certs);
      return MAILSMIME_ERROR_CERT;
    }
    CFArrayAppendValue(certs, entry->apple_cert);
  }

  r = write_mime_to_mem(mime, &data, &data_len);
  if (r != MAILSMIME_NO_ERROR) {
    CFRelease(certs);
    return r;
  }

  encoder = NULL;
  encoded = NULL;
  if (CMSEncoderCreate(&encoder) != errSecSuccess) {
    CFRelease(certs);
    free(data);
    return MAILSMIME_ERROR_MEMORY;
  }

  if ((CMSEncoderAddRecipients(encoder, certs) != errSecSuccess) ||
      (CMSEncoderUpdateContent(encoder, data, data_len) != errSecSuccess) ||
      (CMSEncoderCopyEncodedContent(encoder, &encoded) != errSecSuccess)) {
    CFRelease(encoder);
    CFRelease(certs);
    free(data);
    return MAILSMIME_ERROR_CRYPTO;
  }
  free(data);
  CFRelease(certs);

  r = cfdata_to_alloc(encoded, &der, &der_len);
  CFRelease(encoded);
  CFRelease(encoder);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  r = make_pkcs7_mime("enveloped-data", der, der_len, result);
  free(der);
  return r;
  }
#endif
  return MAILSMIME_ERROR_CRYPTO;
}

int mailsmime_decrypt(struct mailsmime * smime,
    struct mailmime * mime,
    struct mailmime ** result)
{
  if ((smime == NULL) || (mime == NULL) || (result == NULL))
    return MAILSMIME_ERROR_INVAL;
#ifdef USE_SMIME_OPENSSL
  if (smime->backend == MAILSMIME_BACKEND_OPENSSL) {
  char * der;
  size_t der_len;
  CMS_ContentInfo * cms;
  clistiter * cur;
  int r;

  r = smime_body_to_mem(mime, &der, &der_len);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  cms = cms_from_der(der, der_len);
  free(der);
  if (cms == NULL)
    return MAILSMIME_ERROR_PARSE;

  for (cur = clist_begin(smime->keys); cur != NULL; cur = clist_next(cur)) {
    struct mailsmime_key_entry * entry;
    BIO * out;

    entry = clist_content(cur);
    out = BIO_new(BIO_s_mem());
    if (out == NULL) {
      CMS_ContentInfo_free(cms);
      return MAILSMIME_ERROR_MEMORY;
    }

    if (CMS_decrypt(cms, entry->openssl_key, entry->openssl_cert, NULL, out,
        CMS_BINARY) == 1) {
      char * content;
      size_t content_len;

      r = bio_to_alloc(out, &content, &content_len);
      BIO_free(out);
      CMS_ContentInfo_free(cms);
      if (r != MAILSMIME_NO_ERROR)
        return r;
      r = parse_mime_from_mem(content, content_len, result);
      free(content);
      return r;
    }
    BIO_free(out);
    ERR_clear_error();
  }

  CMS_ContentInfo_free(cms);
  return MAILSMIME_ERROR_DECRYPT;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (smime->backend == MAILSMIME_BACKEND_APPLE) {
  char * der;
  size_t der_len;
  CMSDecoderRef decoder;
  CFArrayRef previous_keychains;
  SecKeychainRef previous_default_keychain;
  CFDataRef content;
  char * content_data;
  size_t content_len;
  int r;

  r = smime_body_to_mem(mime, &der, &der_len);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  previous_keychains = NULL;
  previous_default_keychain = NULL;
  if (apple_push_keychain_search(smime->apple_keychain, &previous_keychains,
      &previous_default_keychain) != MAILSMIME_NO_ERROR) {
    free(der);
    return MAILSMIME_ERROR_DECRYPT;
  }
  decoder = NULL;
  if (CMSDecoderCreate(&decoder) != errSecSuccess) {
    apple_pop_keychain_search(previous_keychains, previous_default_keychain);
    free(der);
    return MAILSMIME_ERROR_MEMORY;
  }

  r = apple_decoder_update(decoder, der, der_len);
  free(der);
  if (r != MAILSMIME_NO_ERROR)
    r = MAILSMIME_ERROR_DECRYPT;
  if (r == MAILSMIME_NO_ERROR) {
    if (CMSDecoderFinalizeMessage(decoder) != errSecSuccess)
      r = MAILSMIME_ERROR_DECRYPT;
  }
  if (r != MAILSMIME_NO_ERROR) {
    apple_pop_keychain_search(previous_keychains, previous_default_keychain);
    CFRelease(decoder);
    return r;
  }

  content = NULL;
  if ((CMSDecoderCopyContent(decoder, &content) != errSecSuccess) ||
      (content == NULL)) {
    apple_pop_keychain_search(previous_keychains, previous_default_keychain);
    CFRelease(decoder);
    return MAILSMIME_ERROR_DECRYPT;
  }
  apple_pop_keychain_search(previous_keychains, previous_default_keychain);
  CFRelease(decoder);

  r = cfdata_to_alloc(content, &content_data, &content_len);
  CFRelease(content);
  if (r != MAILSMIME_NO_ERROR)
    return r;

  r = parse_mime_from_mem(content_data, content_len, result);
  free(content_data);
  if ((r == MAILSMIME_NO_ERROR) && mailsmime_is_encrypted(* result)) {
    mailsmime_mime_free(* result);
    * result = NULL;
    return MAILSMIME_ERROR_DECRYPT;
  }
  return r;
  }
#endif
  return MAILSMIME_ERROR_CRYPTO;
}

void mailsmime_mime_free(struct mailmime * mime)
{
  if (mime == NULL)
    return;
  mime_unregister_owned_buffers_recursive(mime);
  mailmime_free(mime);
}

int mailsmime_result_status(struct mailsmime_result * result)
{
  if (result == NULL)
    return MAILSMIME_VERIFY_ERROR;
  return result->status;
}

const char * mailsmime_result_error(struct mailsmime_result * result)
{
  if (result == NULL)
    return NULL;
  return result->error;
}

unsigned int mailsmime_result_signer_count(struct mailsmime_result * result)
{
  if ((result == NULL) || (result->signers == NULL))
    return 0;
  return (unsigned int) clist_count(result->signers);
}

int mailsmime_result_signed_by_address(struct mailsmime_result * result,
    const char * email)
{
  clistiter * cur;

  if ((result == NULL) || (email == NULL))
    return 0;

  for (cur = clist_begin(result->signers); cur != NULL; cur = clist_next(cur)) {
    struct mailsmime_certificate * cert;

    cert = clist_content(cur);
    if (str_case_equal(cert->email, email))
      return 1;
  }

  return 0;
}

int mailsmime_result_signed_by_from(struct mailsmime_result * result,
    struct mailimf_fields * fields)
{
  clistiter * cur;

  if ((result == NULL) || (fields == NULL) || (fields->fld_list == NULL))
    return 0;

  for (cur = clist_begin(fields->fld_list); cur != NULL;
      cur = clist_next(cur)) {
    struct mailimf_field * field;
    struct mailimf_from * from;
    clistiter * mb_cur;

    field = clist_content(cur);
    if ((field == NULL) || (field->fld_type != MAILIMF_FIELD_FROM))
      continue;

    from = field->fld_data.fld_from;
    if ((from == NULL) || (from->frm_mb_list == NULL) ||
        (from->frm_mb_list->mb_list == NULL))
      continue;

    for (mb_cur = clist_begin(from->frm_mb_list->mb_list); mb_cur != NULL;
        mb_cur = clist_next(mb_cur)) {
      struct mailimf_mailbox * mailbox;

      mailbox = clist_content(mb_cur);
      if ((mailbox != NULL) &&
          mailsmime_result_signed_by_address(result, mailbox->mb_addr_spec))
        return 1;
    }
  }

  return 0;
}

struct mailmime * mailsmime_result_get_signed_mime(
    struct mailsmime_result * result)
{
  if (result == NULL)
    return NULL;
  return result->signed_mime;
}

static void certificate_free_func(void * value, void * data)
{
  (void) data;
  mailsmime_certificate_free(value);
}

int mailsmime_result_get_signer(struct mailsmime_result * result,
    unsigned int index,
    struct mailsmime_certificate ** cert)
{
  clistiter * cur;
  unsigned int i;

  if ((result == NULL) || (cert == NULL))
    return MAILSMIME_ERROR_INVAL;

  cur = clist_begin(result->signers);
  for (i = 0; (i < index) && (cur != NULL); i ++)
    cur = clist_next(cur);

  if (cur == NULL)
    return MAILSMIME_ERROR_INVAL;

  {
    struct mailsmime_certificate * source;

    source = clist_content(cur);
    * cert = NULL;
#ifdef USE_SMIME_OPENSSL
    if (source->backend == MAILSMIME_BACKEND_OPENSSL)
      * cert = certificate_new_from_x509(source->openssl_cert);
#endif
#ifdef USE_SMIME_APPLE
    if (source->backend == MAILSMIME_BACKEND_APPLE)
      * cert = certificate_new_from_sec_certificate(source->apple_cert);
#endif
  }
  if (* cert == NULL)
    return MAILSMIME_ERROR_MEMORY;
#if !defined(USE_SMIME_OPENSSL) && !defined(USE_SMIME_APPLE)
  * cert = NULL;
  return MAILSMIME_ERROR_CRYPTO;
#endif

  return MAILSMIME_NO_ERROR;
}

void mailsmime_result_free(struct mailsmime_result * result)
{
  if (result == NULL)
    return;
  free(result->error);
  if (result->signers != NULL) {
    clist_foreach(result->signers, certificate_free_func, NULL);
    clist_free(result->signers);
  }
  if (result->signed_mime != NULL)
    mailsmime_mime_free(result->signed_mime);
  free(result);
}

const char * mailsmime_certificate_email(struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->email;
}

const char * mailsmime_certificate_name(struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->name;
}

const char * mailsmime_certificate_subject(struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->subject;
}

const char * mailsmime_certificate_issuer(struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->issuer;
}

const char * mailsmime_certificate_not_before(
    struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->not_before;
}

const char * mailsmime_certificate_not_after(
    struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->not_after;
}

const char * mailsmime_certificate_fingerprint_sha256(
    struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return NULL;
  return cert->fingerprint_sha256;
}

int mailsmime_certificate_export_pem(struct mailsmime_certificate * cert,
    char ** pem,
    size_t * pem_len)
{
  if ((cert == NULL) || (pem == NULL) || (pem_len == NULL))
    return MAILSMIME_ERROR_INVAL;
#ifdef USE_SMIME_OPENSSL
  if (cert->backend == MAILSMIME_BACKEND_OPENSSL) {
  BIO * out;
  int r;

  out = BIO_new(BIO_s_mem());
  if (out == NULL)
    return MAILSMIME_ERROR_MEMORY;

  if (PEM_write_bio_X509(out, cert->openssl_cert) != 1) {
    BIO_free(out);
    return MAILSMIME_ERROR_CRYPTO;
  }

  r = bio_to_alloc(out, pem, pem_len);
  BIO_free(out);
  return r;
  }
#endif
#ifdef USE_SMIME_APPLE
  if (cert->backend == MAILSMIME_BACKEND_APPLE) {
  CFDataRef der;
  static const char begin[] = "-----BEGIN CERTIFICATE-----\n";
  static const char end[] = "-----END CERTIFICATE-----\n";
  MMAPString * encoded;
  int col;
  int r;

  der = SecCertificateCopyData(cert->apple_cert);
  if (der == NULL)
    return MAILSMIME_ERROR_CRYPTO;

  encoded = mmap_string_new("");
  if (encoded == NULL) {
    CFRelease(der);
    return MAILSMIME_ERROR_MEMORY;
  }

  if (mmap_string_append_len(encoded, begin, sizeof(begin) - 1) == NULL) {
    mmap_string_free(encoded);
    CFRelease(der);
    return MAILSMIME_ERROR_MEMORY;
  }
  col = 0;
  r = mailmime_base64_write_mem(encoded, &col,
      (const char *) CFDataGetBytePtr(der), (size_t) CFDataGetLength(der));
  CFRelease(der);
  if (r != MAILIMF_NO_ERROR) {
    mmap_string_free(encoded);
    return MAILSMIME_ERROR_CRYPTO;
  }
  if (mmap_string_append_len(encoded, end, sizeof(end) - 1) == NULL) {
    mmap_string_free(encoded);
    return MAILSMIME_ERROR_MEMORY;
  }

  r = mmap_to_alloc(encoded, pem, pem_len);
  mmap_string_free(encoded);
  return r;
  }
#endif
  return MAILSMIME_ERROR_CRYPTO;
}

void mailsmime_certificate_free(struct mailsmime_certificate * cert)
{
  if (cert == NULL)
    return;
#ifdef USE_SMIME_OPENSSL
  if (cert->openssl_cert != NULL)
    X509_free(cert->openssl_cert);
#endif
#ifdef USE_SMIME_APPLE
  if (cert->apple_cert != NULL)
    CFRelease(cert->apple_cert);
#endif
  free(cert->email);
  free(cert->name);
  free(cert->subject);
  free(cert->issuer);
  free(cert->not_before);
  free(cert->not_after);
  free(cert->fingerprint_sha256);
  free(cert);
}
