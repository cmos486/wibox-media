#include "fakes/fake_pjsip.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

pj_status_t fake_create_request_status;
pj_status_t fake_create_cancel_status;
pj_status_t fake_create_response_status;
pj_status_t fake_send_request_status;
pj_status_t fake_send_response_status;
pj_status_t fake_respond_status;
int fake_invite_sent;
int fake_ack_sent;
int fake_bye_sent;
int fake_cancel_sent;
int fake_response_sent;
int fake_stateless_response_count;
int fake_last_stateless_status;
int fake_add_ref_count;
int fake_dec_ref_count;

static pj_pool_t fake_pool;
static const pjsip_method invite_method = { FAKE_METHOD_INVITE, { "INVITE", 6 } };
static const pjsip_method ack_method = { FAKE_METHOD_ACK, { "ACK", 3 } };
static const pjsip_method bye_method = { FAKE_METHOD_BYE, { "BYE", 3 } };
static const pjsip_method cancel_method = { FAKE_METHOD_CANCEL, { "CANCEL", 6 } };
pj_pool_factory_policy pj_pool_factory_default_policy;

static char *duplicate_bytes(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) abort();
    if (length) memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void fake_pj_log(const char *source, const char *format, ...) {
    va_list arguments;
    (void)source;
    va_start(arguments, format);
    va_end(arguments);
}

static pjsip_tx_data *new_tx_data(int method_id) {
    pjsip_tx_data *data = calloc(1, sizeof(*data));
    if (!data) abort();
    data->msg = calloc(1, sizeof(*data->msg));
    if (!data->msg) abort();
    data->pool = &fake_pool;
    data->ref_count = 1;
    data->method_id = method_id;
    return data;
}

static void add_string_header(pjsip_msg *msg, int type, const pj_str_t *value) {
    if (type == PJSIP_H_CALL_ID) {
        pjsip_cid_hdr *header = calloc(1, sizeof(*header));
        header->hdr.type = type;
        pj_strdup(&fake_pool, &header->id, value);
        msg->headers[type] = (pjsip_hdr *)header;
    } else if (type == PJSIP_H_FROM) {
        pjsip_from_hdr *header = calloc(1, sizeof(*header));
        header->hdr.type = type;
        header->uri = value ? duplicate_bytes(value->ptr, (size_t)value->slen) : NULL;
        msg->headers[type] = (pjsip_hdr *)header;
    } else if (type == PJSIP_H_TO) {
        pjsip_to_hdr *header = calloc(1, sizeof(*header));
        header->hdr.type = type;
        header->uri = value ? duplicate_bytes(value->ptr, (size_t)value->slen) : NULL;
        msg->headers[type] = (pjsip_hdr *)header;
    }
}

void fake_pjsip_reset(void) {
    fake_create_request_status = PJ_SUCCESS;
    fake_create_cancel_status = PJ_SUCCESS;
    fake_create_response_status = PJ_SUCCESS;
    fake_send_request_status = PJ_SUCCESS;
    fake_send_response_status = PJ_SUCCESS;
    fake_respond_status = PJ_SUCCESS;
    fake_invite_sent = 0;
    fake_ack_sent = 0;
    fake_bye_sent = 0;
    fake_cancel_sent = 0;
    fake_response_sent = 0;
    fake_stateless_response_count = 0;
    fake_last_stateless_status = 0;
    fake_add_ref_count = 0;
    fake_dec_ref_count = 0;
}

int pj_strcmp(const pj_str_t *left, const pj_str_t *right) {
    int common;
    int comparison;
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    common = left->slen < right->slen ? left->slen : right->slen;
    comparison = common > 0 ? memcmp(left->ptr, right->ptr, (size_t)common) : 0;
    if (comparison) return comparison;
    return left->slen - right->slen;
}

void pj_strdup(pj_pool_t *pool, pj_str_t *destination, const pj_str_t *source) {
    (void)pool;
    destination->ptr = duplicate_bytes(source->ptr, (size_t)source->slen);
    destination->slen = source->slen;
}

void pj_strdup2(pj_pool_t *pool, pj_str_t *destination, const char *source) {
    pj_str_t value = pj_str((char *)source);
    pj_strdup(pool, destination, &value);
}

char *pj_inet_ntoa(struct in_addr address) {
    static char buffer[INET_ADDRSTRLEN];
    return (char *)inet_ntop(AF_INET, &address, buffer, sizeof(buffer));
}

void *pjsip_msg_find_hdr(pjsip_msg *msg, int type, const void *start) {
    (void)start;
    if (!msg || type <= 0 || type >= PJSIP_H_COUNT) return NULL;
    return msg->headers[type];
}

int pjsip_uri_print(int context, const void *uri, char *buffer, size_t size) {
    size_t length;
    (void)context;
    if (!uri || !buffer || size == 0) return -1;
    length = strlen((const char *)uri);
    if (length >= size) return -1;
    memcpy(buffer, uri, length);
    return (int)length;
}

pjsip_contact_hdr *pjsip_contact_hdr_create(pj_pool_t *pool) {
    pjsip_contact_hdr *header;
    (void)pool;
    header = calloc(1, sizeof(*header));
    if (!header) abort();
    header->hdr.type = PJSIP_H_CONTACT;
    return header;
}

pjsip_generic_string_hdr *pjsip_generic_string_hdr_create(
    pj_pool_t *pool, const pj_str_t *name, const pj_str_t *value) {
    pjsip_generic_string_hdr *header = calloc(1, sizeof(*header));
    if (!header) abort();
    header->hdr.type = PJSIP_H_GENERIC;
    pj_strdup(pool, &header->name, name);
    pj_strdup(pool, &header->value, value);
    return header;
}

void *pjsip_parse_uri(pj_pool_t *pool, char *text, int length, unsigned int flags) {
    (void)pool;
    (void)flags;
    return text && length >= 0 ? duplicate_bytes(text, (size_t)length) : NULL;
}

void pjsip_msg_add_hdr(pjsip_msg *msg, pjsip_hdr *header) {
    if (msg && header && header->type > 0 && header->type < PJSIP_H_COUNT) {
        msg->headers[header->type] = header;
    }
}

pjsip_msg_body *pjsip_msg_body_create(pj_pool_t *pool,
                                      const pj_str_t *type,
                                      const pj_str_t *subtype,
                                      const pj_str_t *text) {
    pjsip_msg_body *body = calloc(1, sizeof(*body));
    (void)pool;
    (void)type;
    (void)subtype;
    if (!body) abort();
    body->data = duplicate_bytes(text->ptr, (size_t)text->slen);
    body->len = (unsigned int)text->slen;
    return body;
}

const pjsip_method *pjsip_get_invite_method(void) { return &invite_method; }
const pjsip_method *pjsip_get_ack_method(void) { return &ack_method; }
const pjsip_method *pjsip_get_bye_method(void) { return &bye_method; }
const pjsip_method *pjsip_get_cancel_method(void) { return &cancel_method; }

int pjsip_method_cmp(const pjsip_method *left, const pjsip_method *right) {
    if (!left || !right) return left == right ? 0 : (left ? 1 : -1);
    if (left->id && right->id) return left->id - right->id;
    return pj_strcmp(&left->name, &right->name);
}

pj_status_t pj_init(void) { return PJ_SUCCESS; }
void pj_shutdown(void) {}
void pj_log_set_level(int level) { (void)level; }
void pj_caching_pool_init(pj_caching_pool *pool,
                          const pj_pool_factory_policy *policy,
                          int size) {
    (void)policy;
    (void)size;
    memset(pool, 0, sizeof(*pool));
}
void pj_caching_pool_destroy(pj_caching_pool *pool) { (void)pool; }
pj_pool_t *pj_pool_create(pj_pool_factory *factory, const char *name,
                          size_t initial_size, size_t increment,
                          void *callback) {
    pj_pool_t *result = calloc(1, sizeof(*result));
    (void)factory;
    (void)name;
    (void)initial_size;
    (void)increment;
    (void)callback;
    return result;
}
void pj_pool_release(pj_pool_t *pool) { free(pool); }
pj_status_t pj_mutex_create_simple(pj_pool_t *pool, const char *name,
                                   pj_mutex_t **mutex) {
    (void)pool;
    (void)name;
    *mutex = calloc(1, sizeof(**mutex));
    if (!*mutex) return PJ_EINVAL;
    return pthread_mutex_init(&(*mutex)->mutex, NULL) == 0 ? PJ_SUCCESS : PJ_EINVAL;
}
void pj_mutex_lock(pj_mutex_t *mutex) { pthread_mutex_lock(&mutex->mutex); }
void pj_mutex_unlock(pj_mutex_t *mutex) { pthread_mutex_unlock(&mutex->mutex); }
pj_status_t pj_thread_create(pj_pool_t *pool, const char *name,
                             pj_thread_proc *procedure, void *arg,
                             size_t stack_size, unsigned int flags,
                             pj_thread_t **thread) {
    (void)pool;
    (void)name;
    (void)procedure;
    (void)arg;
    (void)stack_size;
    (void)flags;
    *thread = calloc(1, sizeof(**thread));
    return *thread ? PJ_SUCCESS : PJ_EINVAL;
}
pj_status_t pj_thread_join(pj_thread_t *thread) { return thread ? PJ_SUCCESS : PJ_EINVAL; }
pj_status_t pj_thread_destroy(pj_thread_t *thread) { free(thread); return PJ_SUCCESS; }
pj_bool_t pj_thread_is_registered(void) { return PJ_TRUE; }
pj_status_t pj_thread_register(const char *name, pj_thread_desc descriptor,
                               pj_thread_t **thread) {
    static pj_thread_t registered;
    (void)name;
    (void)descriptor;
    *thread = &registered;
    return PJ_SUCCESS;
}
void pj_bzero(void *data, size_t size) { memset(data, 0, size); }
pj_uint16_t pj_htons(pj_uint16_t value) { return htons(value); }
pj_uint16_t pj_ntohs(pj_uint16_t value) { return ntohs(value); }

pj_status_t pjsip_endpt_create(pj_pool_factory *factory, const char *name,
                               pjsip_endpoint **endpoint) {
    (void)factory;
    (void)name;
    *endpoint = calloc(1, sizeof(**endpoint));
    return *endpoint ? PJ_SUCCESS : PJ_EINVAL;
}
void pjsip_endpt_destroy(pjsip_endpoint *endpoint) { free(endpoint); }
pj_status_t pjsip_endpt_register_module(pjsip_endpoint *endpoint,
                                        pjsip_module *module) {
    (void)endpoint;
    (void)module;
    return PJ_SUCCESS;
}
pj_status_t pjsip_endpt_handle_events(pjsip_endpoint *endpoint,
                                      const pj_time_val *timeout) {
    (void)endpoint;
    (void)timeout;
    return PJ_SUCCESS;
}
pj_status_t pjsip_udp_transport_start(pjsip_endpoint *endpoint,
                                      const pj_sockaddr_in *address,
                                      const void *published_address,
                                      unsigned int async_count,
                                      pjsip_transport **transport) {
    static pjsip_transport fake_transport;
    (void)endpoint;
    (void)address;
    (void)published_address;
    (void)async_count;
    *transport = &fake_transport;
    return PJ_SUCCESS;
}

pj_status_t pjsip_endpt_create_request(pjsip_endpoint *endpoint,
                                       const pjsip_method *method,
                                       const pj_str_t *target,
                                       const pj_str_t *from,
                                       const pj_str_t *to,
                                       const void *contact,
                                       const pj_str_t *call_id,
                                       int cseq,
                                       const void *body,
                                       pjsip_tx_data **result) {
    pjsip_tx_data *data;
    pjsip_cseq_hdr *cseq_header;
    (void)endpoint;
    (void)target;
    (void)contact;
    (void)body;
    if (fake_create_request_status != PJ_SUCCESS) return fake_create_request_status;
    data = new_tx_data(method ? method->id : 0);
    add_string_header(data->msg, PJSIP_H_CALL_ID, call_id);
    add_string_header(data->msg, PJSIP_H_FROM, from);
    add_string_header(data->msg, PJSIP_H_TO, to);
    cseq_header = calloc(1, sizeof(*cseq_header));
    if (!cseq_header) abort();
    cseq_header->hdr.type = PJSIP_H_CSEQ;
    cseq_header->cseq = cseq < 0 ? 42U : (pj_uint32_t)cseq;
    data->msg->headers[PJSIP_H_CSEQ] = (pjsip_hdr *)cseq_header;
    *result = data;
    return PJ_SUCCESS;
}

pj_status_t pjsip_endpt_create_cancel(pjsip_endpoint *endpoint,
                                      pjsip_tx_data *invite,
                                      pjsip_tx_data **result) {
    (void)endpoint;
    (void)invite;
    if (fake_create_cancel_status != PJ_SUCCESS) return fake_create_cancel_status;
    *result = new_tx_data(FAKE_METHOD_CANCEL);
    return PJ_SUCCESS;
}

pj_status_t pjsip_endpt_create_response(pjsip_endpoint *endpoint,
                                        pjsip_rx_data *request,
                                        int status_code,
                                        const pj_str_t *reason,
                                        pjsip_tx_data **result) {
    pjsip_tx_data *data;
    pjsip_to_hdr *to_header;
    (void)endpoint;
    (void)request;
    (void)reason;
    if (fake_create_response_status != PJ_SUCCESS) return fake_create_response_status;
    data = new_tx_data(0);
    data->msg->line.status.code = status_code;
    to_header = calloc(1, sizeof(*to_header));
    if (!to_header) abort();
    to_header->hdr.type = PJSIP_H_TO;
    data->msg->headers[PJSIP_H_TO] = (pjsip_hdr *)to_header;
    *result = data;
    return PJ_SUCCESS;
}

pj_status_t pjsip_endpt_send_request_stateless(pjsip_endpoint *endpoint,
                                                pjsip_tx_data *data,
                                                void *token,
                                                void *callback) {
    (void)endpoint;
    (void)token;
    (void)callback;
    if (data) {
        if (data->method_id == FAKE_METHOD_INVITE) fake_invite_sent++;
        if (data->method_id == FAKE_METHOD_ACK) fake_ack_sent++;
        if (data->method_id == FAKE_METHOD_BYE) fake_bye_sent++;
        if (data->method_id == FAKE_METHOD_CANCEL) fake_cancel_sent++;
    }
    return fake_send_request_status;
}

pj_status_t pjsip_endpt_send_response2(pjsip_endpoint *endpoint,
                                       pjsip_rx_data *request,
                                       pjsip_tx_data *response,
                                       void *token,
                                       void *callback) {
    (void)endpoint;
    (void)request;
    (void)response;
    (void)token;
    (void)callback;
    fake_response_sent++;
    return fake_send_response_status;
}

pj_status_t pjsip_endpt_respond_stateless(pjsip_endpoint *endpoint,
                                           pjsip_rx_data *request,
                                           int status_code,
                                           const pj_str_t *reason,
                                           const void *headers,
                                           const void *body) {
    (void)endpoint;
    (void)request;
    (void)reason;
    (void)headers;
    (void)body;
    fake_stateless_response_count++;
    fake_last_stateless_status = status_code;
    return fake_respond_status;
}

void pjsip_tx_data_add_ref(pjsip_tx_data *data) {
    if (data) data->ref_count++;
    fake_add_ref_count++;
}

void pjsip_tx_data_dec_ref(pjsip_tx_data *data) {
    if (data && data->ref_count > 0) data->ref_count--;
    fake_dec_ref_count++;
}
