#include "tls.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>

#define CLM_TLS_MAX_RECORD ((size_t)16 * 1024 * 1024)

struct clm_tls_session {
    clm_id socket_id;
    clm_net_private_socket* socket;
    uint32_t timeout_ms;
    bool server;
    bool ready;
    CredHandle credentials;
    CtxtHandle context;
    bool credentials_valid;
    bool context_valid;
    HCERTSTORE certificate_store;
    PCCERT_CONTEXT certificate;
    SecPkgContext_StreamSizes stream_sizes;
    unsigned char* encrypted;
    size_t encrypted_length;
    size_t encrypted_capacity;
    unsigned char* plaintext;
    size_t plaintext_length;
    size_t plaintext_offset;
};

static clm_int tls_wait(clm_tls_session* session, unsigned int event)
{
    unsigned int observed = 0;
    clm_net_status status = clm_net_wait(session->socket_id, event,
        session->timeout_ms, &observed);
    if (status == CLM_NET_TIMEOUT)
        return CLM_TLS_TIMEOUT;
    if (status != CLM_NET_SUCCESS)
        return status == CLM_NET_ERROR_NOT_FOUND ? CLM_TLS_NOT_FOUND : CLM_TLS_SYSTEM_ERROR;
    return (observed & event) ? CLM_TLS_SUCCESS : CLM_TLS_SYSTEM_ERROR;
}

static clm_int tls_send_all(clm_tls_session* session, const void* data, size_t length)
{
    const unsigned char* bytes = data;
    size_t sent_total = 0;
    while (sent_total < length) {
        size_t sent = 0;
        clm_net_status status = clm_net_send(session->socket_id,
            bytes + sent_total, length - sent_total, &sent);
        if (status == CLM_NET_WOULD_BLOCK) {
            clm_int wait_status = tls_wait(session, CLM_NET_EVENT_WRITE);
            if (wait_status != CLM_TLS_SUCCESS)
                return wait_status;
        } else if (status != CLM_NET_SUCCESS) {
            return status == CLM_NET_ERROR_NOT_FOUND ? CLM_TLS_NOT_FOUND : CLM_TLS_SYSTEM_ERROR;
        } else if (sent == 0) {
            return CLM_TLS_SYSTEM_ERROR;
        } else {
            sent_total += sent;
        }
    }
    return CLM_TLS_SUCCESS;
}

static clm_int tls_reserve(unsigned char** data, size_t* capacity, size_t needed)
{
    if (needed <= *capacity)
        return CLM_TLS_SUCCESS;
    if (needed > CLM_TLS_MAX_RECORD)
        return CLM_TLS_HANDSHAKE_FAILED;
    size_t next = *capacity ? *capacity : 8192;
    while (next < needed) {
        if (next > CLM_TLS_MAX_RECORD / 2) {
            next = CLM_TLS_MAX_RECORD;
            break;
        }
        next *= 2;
    }
    unsigned char* resized = realloc(*data, next);
    if (!resized)
        return CLM_TLS_NO_MEMORY;
    *data = resized;
    *capacity = next;
    return CLM_TLS_SUCCESS;
}

static clm_int tls_receive_more(clm_tls_session* session)
{
    for (;;) {
        clm_int reserve_status = tls_reserve(&session->encrypted,
            &session->encrypted_capacity, session->encrypted_length + 4096);
        if (reserve_status != CLM_TLS_SUCCESS)
            return reserve_status;
        size_t received = 0;
        clm_net_status status = clm_net_receive(session->socket_id,
            session->encrypted + session->encrypted_length,
            session->encrypted_capacity - session->encrypted_length, &received);
        if (status == CLM_NET_WOULD_BLOCK) {
            clm_int wait_status = tls_wait(session, CLM_NET_EVENT_READ);
            if (wait_status != CLM_TLS_SUCCESS)
                return wait_status;
            continue;
        }
        if (status == CLM_NET_CLOSED)
            return CLM_TLS_TRUNCATED;
        if (status != CLM_NET_SUCCESS)
            return status == CLM_NET_ERROR_NOT_FOUND ? CLM_TLS_NOT_FOUND : CLM_TLS_SYSTEM_ERROR;
        if (!received)
            return CLM_TLS_TRUNCATED;
        session->encrypted_length += received;
        return CLM_TLS_SUCCESS;
    }
}

static wchar_t* tls_utf8_to_wide(const char* text)
{
    if (!text)
        return NULL;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (count <= 0)
        return NULL;
    wchar_t* result = malloc((size_t)count * sizeof(*result));
    if (!result)
        return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, count) != count) {
        free(result);
        return NULL;
    }
    return result;
}

static clm_int tls_load_pfx(clm_tls_session* session, const clm_tls_options* options)
{
    wchar_t* path = tls_utf8_to_wide(options->certificate_file);
    wchar_t* password = tls_utf8_to_wide(options->certificate_password
        ? options->certificate_password : "");
    if (!path || !password) {
        free(path);
        free(password);
        return CLM_TLS_INVALID_ARGUMENT;
    }
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(path);
    if (file == INVALID_HANDLE_VALUE) {
        SecureZeroMemory(password, wcslen(password) * sizeof(*password));
        free(password);
        return CLM_TLS_SYSTEM_ERROR;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > CLM_TLS_MAX_RECORD) {
        CloseHandle(file);
        SecureZeroMemory(password, wcslen(password) * sizeof(*password));
        free(password);
        return CLM_TLS_INVALID_ARGUMENT;
    }
    BYTE* bytes = malloc((size_t)size.QuadPart);
    if (!bytes) {
        CloseHandle(file);
        SecureZeroMemory(password, wcslen(password) * sizeof(*password));
        free(password);
        return CLM_TLS_NO_MEMORY;
    }
    DWORD received = 0;
    BOOL read_ok = ReadFile(file, bytes, (DWORD)size.QuadPart, &received, NULL);
    CloseHandle(file);
    if (!read_ok || received != (DWORD)size.QuadPart) {
        SecureZeroMemory(bytes, (size_t)size.QuadPart);
        free(bytes);
        SecureZeroMemory(password, wcslen(password) * sizeof(*password));
        free(password);
        return CLM_TLS_SYSTEM_ERROR;
    }
    CRYPT_DATA_BLOB blob = {(DWORD)size.QuadPart, bytes};
    session->certificate_store = PFXImportCertStore(&blob, password,
        PKCS12_NO_PERSIST_KEY | CRYPT_USER_KEYSET);
    SecureZeroMemory(bytes, (size_t)size.QuadPart);
    free(bytes);
    SecureZeroMemory(password, wcslen(password) * sizeof(*password));
    free(password);
    if (!session->certificate_store)
        return CLM_TLS_CERTIFICATE_ERROR;

    PCCERT_CONTEXT current = NULL;
    while ((current = CertEnumCertificatesInStore(session->certificate_store, current)) != NULL) {
        DWORD property_size = 0;
        if (CertGetCertificateContextProperty(current, CERT_KEY_PROV_INFO_PROP_ID,
                NULL, &property_size)) {
            session->certificate = CertDuplicateCertificateContext(current);
            CertFreeCertificateContext(current);
            break;
        }
    }
    return session->certificate ? CLM_TLS_SUCCESS : CLM_TLS_CERTIFICATE_ERROR;
}

static clm_int tls_credentials_open(clm_tls_session* session,
                                    const clm_tls_options* options)
{
    SCHANNEL_CRED credentials = {0};
    credentials.dwVersion = SCHANNEL_CRED_VERSION;
    credentials.grbitEnabledProtocols = session->server
        ? SP_PROT_TLS1_2_SERVER : SP_PROT_TLS1_2_CLIENT;
    #if defined(SP_PROT_TLS1_3_SERVER) && defined(SP_PROT_TLS1_3_CLIENT)
    credentials.grbitEnabledProtocols |= session->server
        ? SP_PROT_TLS1_3_SERVER : SP_PROT_TLS1_3_CLIENT;
    #endif
    if (session->server) {
        clm_int result = tls_load_pfx(session, options);
        if (result != CLM_TLS_SUCCESS)
            return result;
        credentials.cCreds = 1;
        credentials.paCred = &session->certificate;
    } else {
        credentials.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
    }
    TimeStamp expiry;
    SECURITY_STATUS status = AcquireCredentialsHandleW(NULL, (SEC_WCHAR*)UNISP_NAME_W,
        session->server ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND,
        NULL, &credentials, NULL, NULL, &session->credentials, &expiry);
    if (status != SEC_E_OK)
        return CLM_TLS_SYSTEM_ERROR;
    session->credentials_valid = true;
    return CLM_TLS_SUCCESS;
}

static clm_int tls_schannel_handshake(clm_tls_session* session, const char* server_name)
{
    wchar_t* target = server_name ? tls_utf8_to_wide(server_name) : NULL;
    if (!session->server && !target)
        return CLM_TLS_INVALID_ARGUMENT;
    ULONG flags = session->server
        ? ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY |
            ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM
        : ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
            ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    clm_int result = CLM_TLS_HANDSHAKE_FAILED;
    if (session->server) {
        result = tls_receive_more(session);
        if (result != CLM_TLS_SUCCESS) {
            free(target);
            return result;
        }
    }
    for (;;) {
        SecBuffer input_buffers[2] = {0};
        SecBufferDesc input = {SECBUFFER_VERSION, 2, input_buffers};
        if (session->encrypted_length) {
            input_buffers[0].BufferType = SECBUFFER_TOKEN;
            input_buffers[0].pvBuffer = session->encrypted;
            input_buffers[0].cbBuffer = (ULONG)session->encrypted_length;
            input_buffers[1].BufferType = SECBUFFER_EMPTY;
        }
        SecBuffer output_buffer = {0};
        output_buffer.BufferType = SECBUFFER_TOKEN;
        SecBufferDesc output = {SECBUFFER_VERSION, 1, &output_buffer};
        ULONG attributes = 0;
        TimeStamp expiry;
        SECURITY_STATUS status;
        if (session->server) {
            status = AcceptSecurityContext(&session->credentials,
                session->context_valid ? &session->context : NULL,
                session->encrypted_length ? &input : NULL, flags, SECURITY_NATIVE_DREP,
                &session->context, &output, &attributes, &expiry);
        } else {
            status = InitializeSecurityContextW(&session->credentials,
                session->context_valid ? &session->context : NULL, target, flags,
                0, SECURITY_NATIVE_DREP, session->encrypted_length ? &input : NULL,
                0, &session->context, &output, &attributes, &expiry);
        }
        if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
            if (CompleteAuthToken(&session->context, &output) != SEC_E_OK) {
                result = CLM_TLS_HANDSHAKE_FAILED;
                goto done;
            }
            status = status == SEC_I_COMPLETE_NEEDED
                ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
        }
        if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED ||
            status == SEC_E_INCOMPLETE_MESSAGE)
            session->context_valid = true;
        if (output_buffer.cbBuffer && output_buffer.pvBuffer) {
            result = tls_send_all(session, output_buffer.pvBuffer, output_buffer.cbBuffer);
            FreeContextBuffer(output_buffer.pvBuffer);
            if (result != CLM_TLS_SUCCESS)
                goto done;
        }
        size_t extra = 0;
        if (status != SEC_E_INCOMPLETE_MESSAGE) {
            for (size_t i = 0; i < 2; ++i)
                if (input_buffers[i].BufferType == SECBUFFER_EXTRA)
                    extra = input_buffers[i].cbBuffer;
            if (extra > session->encrypted_length) {
                result = CLM_TLS_HANDSHAKE_FAILED;
                goto done;
            }
            if (extra)
                memmove(session->encrypted,
                    session->encrypted + session->encrypted_length - extra, extra);
            session->encrypted_length = extra;
        }
        if (status == SEC_E_OK) {
            if (QueryContextAttributes(&session->context, SECPKG_ATTR_STREAM_SIZES,
                    &session->stream_sizes) != SEC_E_OK) {
                result = CLM_TLS_HANDSHAKE_FAILED;
                goto done;
            }
            session->ready = true;
            result = CLM_TLS_SUCCESS;
            goto done;
        }
        if (status != SEC_I_CONTINUE_NEEDED && status != SEC_I_COMPLETE_AND_CONTINUE &&
            status != SEC_E_INCOMPLETE_MESSAGE) {
            result = (status == SEC_E_WRONG_PRINCIPAL || status == SEC_E_UNTRUSTED_ROOT ||
                      status == SEC_E_CERT_UNKNOWN || status == SEC_E_CERT_EXPIRED)
                ? CLM_TLS_CERTIFICATE_ERROR : CLM_TLS_HANDSHAKE_FAILED;
            goto done;
        }
        if (status == SEC_E_INCOMPLETE_MESSAGE || extra == 0) {
            result = tls_receive_more(session);
            if (result != CLM_TLS_SUCCESS)
                goto done;
        }
    }
done:
    free(target);
    return result;
}

static clm_int tls_schannel_read(clm_tls_session* session, void* output,
                                 size_t capacity, size_t* out_received)
{
    if (session->plaintext_offset < session->plaintext_length) {
        size_t available = session->plaintext_length - session->plaintext_offset;
        size_t copied = available < capacity ? available : capacity;
        memcpy(output, session->plaintext + session->plaintext_offset, copied);
        session->plaintext_offset += copied;
        *out_received = copied;
        return CLM_TLS_SUCCESS;
    }
    session->plaintext_length = session->plaintext_offset = 0;
    for (;;) {
        if (!session->encrypted_length) {
            clm_int received = tls_receive_more(session);
            if (received != CLM_TLS_SUCCESS)
                return received;
        }
        SecBuffer buffers[4] = {0};
        buffers[0].BufferType = SECBUFFER_DATA;
        buffers[0].pvBuffer = session->encrypted;
        buffers[0].cbBuffer = (ULONG)session->encrypted_length;
        SecBufferDesc desc = {SECBUFFER_VERSION, 4, buffers};
        SECURITY_STATUS status = DecryptMessage(&session->context, &desc, 0, NULL);
        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            clm_int received = tls_receive_more(session);
            if (received != CLM_TLS_SUCCESS)
                return received;
            continue;
        }
        if (status == SEC_I_CONTEXT_EXPIRED)
            return CLM_TLS_CLOSED;
        if (status == SEC_I_RENEGOTIATE)
            return CLM_TLS_UNSUPPORTED;
        if (status != SEC_E_OK)
            return CLM_TLS_HANDSHAKE_FAILED;
        size_t extra = 0;
        for (size_t i = 0; i < 4; ++i) {
            if (buffers[i].BufferType == SECBUFFER_DATA && buffers[i].cbBuffer) {
                unsigned char* plain = realloc(session->plaintext, buffers[i].cbBuffer);
                if (!plain)
                    return CLM_TLS_NO_MEMORY;
                session->plaintext = plain;
                memcpy(plain, buffers[i].pvBuffer, buffers[i].cbBuffer);
                session->plaintext_length = buffers[i].cbBuffer;
            } else if (buffers[i].BufferType == SECBUFFER_EXTRA) {
                extra = buffers[i].cbBuffer;
            }
        }
        if (extra > session->encrypted_length)
            return CLM_TLS_HANDSHAKE_FAILED;
        if (extra)
            memmove(session->encrypted,
                session->encrypted + session->encrypted_length - extra, extra);
        session->encrypted_length = extra;
        if (session->plaintext_length) {
            size_t copied = session->plaintext_length < capacity ? session->plaintext_length : capacity;
            memcpy(output, session->plaintext, copied);
            session->plaintext_offset = copied;
            *out_received = copied;
            return CLM_TLS_SUCCESS;
        }
    }
}

static clm_int tls_schannel_write(clm_tls_session* session, const void* input,
                                  size_t length, size_t* out_written)
{
    size_t count = length < session->stream_sizes.cbMaximumMessage
        ? length : session->stream_sizes.cbMaximumMessage;
    size_t total = session->stream_sizes.cbHeader + count + session->stream_sizes.cbTrailer;
    unsigned char* record = malloc(total);
    if (!record)
        return CLM_TLS_NO_MEMORY;
    SecBuffer buffers[4] = {0};
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[0].pvBuffer = record;
    buffers[0].cbBuffer = session->stream_sizes.cbHeader;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[1].pvBuffer = record + session->stream_sizes.cbHeader;
    buffers[1].cbBuffer = (ULONG)count;
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[2].pvBuffer = record + session->stream_sizes.cbHeader + count;
    buffers[2].cbBuffer = session->stream_sizes.cbTrailer;
    SecBufferDesc desc = {SECBUFFER_VERSION, 4, buffers};
    if (count)
        memcpy(buffers[1].pvBuffer, input, count);
    SECURITY_STATUS status = EncryptMessage(&session->context, 0, &desc, 0);
    clm_int result = status == SEC_E_OK ? tls_send_all(session, record, total) : CLM_TLS_SYSTEM_ERROR;
    free(record);
    if (result == CLM_TLS_SUCCESS)
        *out_written = count;
    return result;
}

static void tls_schannel_dispose(clm_tls_session* session)
{
    if (session->context_valid) {
        SecBuffer token = {sizeof(DWORD), SECBUFFER_TOKEN, NULL};
        DWORD shutdown = SCHANNEL_SHUTDOWN;
        token.pvBuffer = &shutdown;
        SecBufferDesc desc = {SECBUFFER_VERSION, 1, &token};
        if (ApplyControlToken(&session->context, &desc) == SEC_E_OK) {
            SecBuffer output_buffer = {0};
            output_buffer.BufferType = SECBUFFER_TOKEN;
            SecBufferDesc output = {SECBUFFER_VERSION, 1, &output_buffer};
            ULONG attributes = 0;
            TimeStamp expiry;
            SECURITY_STATUS status = session->server
                ? AcceptSecurityContext(&session->credentials, &session->context, NULL,
                    ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM, SECURITY_NATIVE_DREP,
                    &session->context, &output, &attributes, &expiry)
                : InitializeSecurityContextW(&session->credentials, &session->context,
                    NULL, ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM, 0,
                    SECURITY_NATIVE_DREP, NULL, 0, &session->context,
                    &output, &attributes, &expiry);
            if ((status == SEC_E_OK || status == SEC_I_CONTEXT_EXPIRED) &&
                output_buffer.pvBuffer && output_buffer.cbBuffer)
                (void)tls_send_all(session, output_buffer.pvBuffer, output_buffer.cbBuffer);
            if (output_buffer.pvBuffer)
                FreeContextBuffer(output_buffer.pvBuffer);
        }
        DeleteSecurityContext(&session->context);
    }
    if (session->credentials_valid)
        FreeCredentialsHandle(&session->credentials);
    if (session->certificate)
        CertFreeCertificateContext(session->certificate);
    if (session->certificate_store)
        CertCloseStore(session->certificate_store, 0);
    free(session->encrypted);
    free(session->plaintext);
}

#else
#include <errno.h>
#include <sys/socket.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

typedef struct { int descriptor; } clm_tls_bio_data;
static once_flag clm_tls_bio_once = ONCE_FLAG_INIT;
static BIO_METHOD* clm_tls_bio_method;

struct clm_tls_session {
    clm_id socket_id;
    clm_net_private_socket* socket;
    uint32_t timeout_ms;
    SSL_CTX* context;
    SSL* ssl;
    bool server;
    bool ready;
};

static clm_int tls_wait(clm_tls_session* session, unsigned int event)
{
    unsigned int observed = 0;
    clm_net_status status = clm_net_wait(session->socket_id, event,
        session->timeout_ms, &observed);
    if (status == CLM_NET_TIMEOUT)
        return CLM_TLS_TIMEOUT;
    if (status != CLM_NET_SUCCESS)
        return status == CLM_NET_ERROR_NOT_FOUND ? CLM_TLS_NOT_FOUND : CLM_TLS_SYSTEM_ERROR;
    return (observed & event) ? CLM_TLS_SUCCESS : CLM_TLS_SYSTEM_ERROR;
}

static int tls_bio_create(BIO* bio)
{
    BIO_set_init(bio, 1);
    BIO_set_data(bio, NULL);
    return 1;
}

static int tls_bio_destroy(BIO* bio)
{
    if (!bio)
        return 0;
    free(BIO_get_data(bio));
    BIO_set_data(bio, NULL);
    BIO_set_init(bio, 0);
    return 1;
}

static int tls_bio_read(BIO* bio, char* output, int length)
{
    BIO_clear_retry_flags(bio);
    clm_tls_bio_data* data = BIO_get_data(bio);
    if (!data || !output || length <= 0)
        return 0;
    ssize_t received;
    do {
        received = recv(data->descriptor, output, (size_t)length, 0);
    } while (received < 0 && errno == EINTR);
    if (received > 0)
        return (int)received;
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        BIO_set_retry_read(bio);
    return received == 0 ? 0 : -1;
}

static int tls_bio_write(BIO* bio, const char* input, int length)
{
    BIO_clear_retry_flags(bio);
    clm_tls_bio_data* data = BIO_get_data(bio);
    if (!data || !input || length <= 0)
        return 0;
    int flags = 0;
    #ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
    #endif
    ssize_t sent;
    do {
        sent = send(data->descriptor, input, (size_t)length, flags);
    } while (sent < 0 && errno == EINTR);
    if (sent > 0)
        return (int)sent;
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        BIO_set_retry_write(bio);
    return sent == 0 ? 0 : -1;
}

static long tls_bio_control(BIO* bio, int command, long number, void* pointer)
{
    (void)number;
    clm_tls_bio_data* data = BIO_get_data(bio);
    if (command == BIO_CTRL_FLUSH || command == BIO_CTRL_DUP)
        return 1;
    if (command == BIO_CTRL_GET_CLOSE)
        return 0;
    if (command == BIO_CTRL_SET_CLOSE)
        return 1;
    if (command == BIO_C_GET_FD && data) {
        if (pointer)
            *(int*)pointer = data->descriptor;
        return data->descriptor;
    }
    return 0;
}

static void tls_bio_method_init(void)
{
    clm_tls_bio_method = BIO_meth_new(BIO_TYPE_SOURCE_SINK | BIO_get_new_index(),
        "clm registered socket");
    if (!clm_tls_bio_method)
        return;
    BIO_meth_set_create(clm_tls_bio_method, tls_bio_create);
    BIO_meth_set_destroy(clm_tls_bio_method, tls_bio_destroy);
    BIO_meth_set_read(clm_tls_bio_method, tls_bio_read);
    BIO_meth_set_write(clm_tls_bio_method, tls_bio_write);
    BIO_meth_set_ctrl(clm_tls_bio_method, tls_bio_control);
}

static BIO* tls_socket_bio(int descriptor)
{
    call_once(&clm_tls_bio_once, tls_bio_method_init);
    if (!clm_tls_bio_method)
        return NULL;
    BIO* bio = BIO_new(clm_tls_bio_method);
    clm_tls_bio_data* data = malloc(sizeof(*data));
    if (!bio || !data) {
        BIO_free(bio);
        free(data);
        return NULL;
    }
    data->descriptor = descriptor;
    BIO_set_data(bio, data);
    return bio;
}

static clm_int tls_ssl_result(clm_tls_session* session, int result)
{
    int error = SSL_get_error(session->ssl, result);
    if (error == SSL_ERROR_WANT_READ)
        return tls_wait(session, CLM_NET_EVENT_READ);
    if (error == SSL_ERROR_WANT_WRITE)
        return tls_wait(session, CLM_NET_EVENT_WRITE);
    if (error == SSL_ERROR_ZERO_RETURN)
        return CLM_TLS_CLOSED;
    if (error == SSL_ERROR_SYSCALL && result == 0)
        return CLM_TLS_TRUNCATED;
    if (error == SSL_ERROR_SSL) {
        if (!session->server && SSL_get_verify_result(session->ssl) != X509_V_OK)
            return CLM_TLS_CERTIFICATE_ERROR;
        return CLM_TLS_HANDSHAKE_FAILED;
    }
    return CLM_TLS_SYSTEM_ERROR;
}

static clm_int tls_openssl_handshake(clm_tls_session* session)
{
    for (;;) {
        int result = session->server ? SSL_accept(session->ssl) : SSL_connect(session->ssl);
        if (result == 1) {
            if (!session->server && SSL_get_verify_result(session->ssl) != X509_V_OK)
                return CLM_TLS_CERTIFICATE_ERROR;
            session->ready = true;
            return CLM_TLS_SUCCESS;
        }
        clm_int status = tls_ssl_result(session, result);
        if (status != CLM_TLS_SUCCESS)
            return status;
    }
}

static void tls_openssl_dispose(clm_tls_session* session)
{
    if (session->ssl) {
        (void)SSL_shutdown(session->ssl);
        SSL_free(session->ssl);
    }
    SSL_CTX_free(session->context);
}
#endif

static uint32_t tls_timeout(const clm_tls_options* options)
{
    return options && options->timeout_ms ? options->timeout_ms : CLM_TLS_DEFAULT_TIMEOUT_MS;
}

static clm_int tls_validate_options(const clm_tls_options* options)
{
    uint32_t timeout_ms = tls_timeout(options);
    return timeout_ms == CLM_NET_WAIT_FOREVER || timeout_ms <= INT_MAX
        ? CLM_TLS_SUCCESS : CLM_TLS_INVALID_ARGUMENT;
}

static void tls_dispose_resources(clm_tls_session* session)
{
#ifdef _WIN32
    tls_schannel_dispose(session);
#else
    tls_openssl_dispose(session);
#endif
    if (session->socket)
        clm_net_private_release(session->socket);
    free(session);
}

static clm_int tls_open(clm_id socket_id, const char* server_name,
                        const clm_tls_options* options, bool server,
                        clm_tls_session** out_session)
{
    if (!out_session || (!server && (!server_name || !*server_name)) ||
        (server && (!options || !options->certificate_file || !*options->certificate_file)))
        return CLM_TLS_INVALID_ARGUMENT;
    clm_int option_status = tls_validate_options(options);
    if (option_status != CLM_TLS_SUCCESS)
        return option_status;
#ifndef _WIN32
    if (server && (!options->private_key_file || !*options->private_key_file))
        return CLM_TLS_INVALID_ARGUMENT;
#endif
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (!socket)
        return CLM_TLS_NOT_FOUND;
    if (socket->transport != CLM_NET_TCP) {
        clm_net_private_release(socket);
        return CLM_TLS_INVALID_ARGUMENT;
    }
#if !defined(_WIN32) && defined(SO_NOSIGPIPE)
    int no_sigpipe = 1;
    if (setsockopt(socket->fd, SOL_SOCKET, SO_NOSIGPIPE,
            &no_sigpipe, sizeof(no_sigpipe)) != 0) {
        clm_net_private_release(socket);
        return CLM_TLS_SYSTEM_ERROR;
    }
#endif
    clm_tls_session* session = calloc(1, sizeof(*session));
    if (!session) {
        clm_net_private_release(socket);
        return CLM_TLS_NO_MEMORY;
    }
    session->socket_id = socket_id;
    session->socket = socket;
    session->timeout_ms = tls_timeout(options);
    session->server = server;
    clm_int result = CLM_TLS_SYSTEM_ERROR;
#ifdef _WIN32
    result = tls_credentials_open(session, options);
    if (result == CLM_TLS_SUCCESS)
        result = tls_schannel_handshake(session, server_name);
#else
    session->context = SSL_CTX_new(TLS_method());
    if (!session->context) {
        result = CLM_TLS_NO_MEMORY;
        goto failed;
    }
    if (!SSL_CTX_set_min_proto_version(session->context, TLS1_2_VERSION))
        goto failed;
    if (server) {
        if (SSL_CTX_use_certificate_chain_file(session->context, options->certificate_file) != 1)
            goto failed;
        if (options->certificate_password)
            SSL_CTX_set_default_passwd_cb_userdata(session->context,
                (void*)options->certificate_password);
        if (SSL_CTX_use_PrivateKey_file(session->context, options->private_key_file,
                SSL_FILETYPE_PEM) != 1 || SSL_CTX_check_private_key(session->context) != 1)
            goto failed;
    } else {
        SSL_CTX_set_verify(session->context, SSL_VERIFY_PEER, NULL);
        if (SSL_CTX_set_default_verify_paths(session->context) != 1)
            goto failed;
    }
    session->ssl = SSL_new(session->context);
    if (!session->ssl) {
        result = CLM_TLS_NO_MEMORY;
        goto failed;
    }
    BIO* bio = tls_socket_bio(socket->fd);
    if (!bio) {
        result = CLM_TLS_NO_MEMORY;
        goto failed;
    }
    SSL_set_bio(session->ssl, bio, bio);
    if (server) {
        SSL_set_accept_state(session->ssl);
    } else {
        struct in_addr ipv4;
        struct in6_addr ipv6;
        bool is_ip = inet_pton(AF_INET, server_name, &ipv4) == 1 ||
            inet_pton(AF_INET6, server_name, &ipv6) == 1;
        if (!is_ip && (SSL_set_tlsext_host_name(session->ssl, server_name) != 1 ||
                       SSL_set1_host(session->ssl, server_name) != 1))
            goto failed;
        if (is_ip && X509_VERIFY_PARAM_set1_ip_asc(
                SSL_get0_param(session->ssl), server_name) != 1)
            goto failed;
        SSL_set_connect_state(session->ssl);
    }
    result = tls_openssl_handshake(session);
#endif
    if (result != CLM_TLS_SUCCESS)
        goto failed;
    *out_session = session;
    return CLM_TLS_SUCCESS;

failed:
    tls_dispose_resources(session);
    return result;
}

clm_int clm_tls_client_open(clm_id socket_id, const char* server_name,
                           const clm_tls_options* options,
                           clm_tls_session** out_session)
{
    return tls_open(socket_id, server_name, options, false, out_session);
}

clm_int clm_tls_server_open(clm_id socket_id, const clm_tls_options* options,
                           clm_tls_session** out_session)
{
    return tls_open(socket_id, NULL, options, true, out_session);
}

clm_int clm_tls_read(clm_tls_session* session, void* buffer, size_t capacity,
                     size_t* out_received)
{
    if (!session || !session->ready || !buffer || !capacity || !out_received)
        return CLM_TLS_INVALID_ARGUMENT;
#ifdef _WIN32
    return tls_schannel_read(session, buffer, capacity, out_received);
#else
    int count = capacity > INT_MAX ? INT_MAX : (int)capacity;
    for (;;) {
        int received = SSL_read(session->ssl, buffer, count);
        if (received > 0) {
            *out_received = (size_t)received;
            return CLM_TLS_SUCCESS;
        }
        clm_int status = tls_ssl_result(session, received);
        if (status != CLM_TLS_SUCCESS)
            return status;
    }
#endif
}

clm_int clm_tls_write(clm_tls_session* session, const void* buffer, size_t length,
                      size_t* out_written)
{
    if (!session || !session->ready || (!buffer && length) || !length || !out_written)
        return CLM_TLS_INVALID_ARGUMENT;
#ifdef _WIN32
    return tls_schannel_write(session, buffer, length, out_written);
#else
    int count = length > INT_MAX ? INT_MAX : (int)length;
    for (;;) {
        int written = SSL_write(session->ssl, buffer, count);
        if (written > 0) {
            *out_written = (size_t)written;
            return CLM_TLS_SUCCESS;
        }
        clm_int status = tls_ssl_result(session, written);
        if (status != CLM_TLS_SUCCESS)
            return status;
    }
#endif
}

clm_int clm_tls_close(clm_tls_session* session)
{
    if (!session)
        return CLM_TLS_INVALID_ARGUMENT;
    tls_dispose_resources(session);
    return CLM_TLS_SUCCESS;
}