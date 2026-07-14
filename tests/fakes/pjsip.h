#ifndef WIBOX_TEST_FAKE_PJSIP_H
#define WIBOX_TEST_FAKE_PJSIP_H

#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

typedef int pj_status_t;
typedef int pj_bool_t;
typedef uint32_t pj_uint32_t;
typedef uint16_t pj_uint16_t;

#define PJ_SUCCESS 0
#define PJ_EINVAL 7001
#define PJ_EBUSY 7002
#define PJ_TRUE 1
#define PJ_FALSE 0
void fake_pj_log(const char *source, const char *format, ...);
#define PJ_LOG(level, args) do { (void)(level); fake_pj_log args; } while (0)

typedef struct pj_pool_t { int unused; } pj_pool_t;
typedef struct pjsip_endpoint { int unused; } pjsip_endpoint;
typedef struct pjsip_transport { int unused; } pjsip_transport;

typedef struct {
    char *ptr;
    int slen;
} pj_str_t;

static inline pj_str_t pj_str(char *value) {
    pj_str_t result;
    result.ptr = value;
    result.slen = value ? (int)strlen(value) : 0;
    return result;
}

int pj_strcmp(const pj_str_t *left, const pj_str_t *right);
void pj_strdup(pj_pool_t *pool, pj_str_t *destination, const pj_str_t *source);
void pj_strdup2(pj_pool_t *pool, pj_str_t *destination, const char *source);
char *pj_inet_ntoa(struct in_addr address);

enum {
    PJSIP_H_CALL_ID = 1,
    PJSIP_H_FROM = 2,
    PJSIP_H_TO = 3,
    PJSIP_H_CONTACT = 4,
    PJSIP_H_CSEQ = 5,
    PJSIP_H_GENERIC = 6,
    PJSIP_H_COUNT = 8
};

#define PJSIP_URI_IN_FROMTO_HDR 1
#define PJSIP_URI_IN_CONTACT_HDR 2

typedef struct pjsip_hdr {
    int type;
} pjsip_hdr;

typedef struct {
    pjsip_hdr hdr;
    pj_str_t id;
} pjsip_cid_hdr;

typedef struct {
    pjsip_hdr hdr;
    pj_str_t tag;
    void *uri;
} pjsip_from_hdr;

typedef struct {
    pjsip_hdr hdr;
    pj_str_t tag;
    void *uri;
} pjsip_to_hdr;

typedef struct {
    pjsip_hdr hdr;
    void *uri;
} pjsip_contact_hdr;

typedef struct {
    pjsip_hdr hdr;
    pj_uint32_t cseq;
} pjsip_cseq_hdr;

typedef struct {
    pjsip_hdr hdr;
    pj_str_t name;
    pj_str_t value;
} pjsip_generic_string_hdr;

typedef struct pjsip_msg_body {
    void *data;
    unsigned int len;
} pjsip_msg_body;

typedef struct pjsip_method {
    int id;
    pj_str_t name;
} pjsip_method;

typedef struct pjsip_msg {
    struct {
        struct { int code; } status;
        struct { pjsip_method method; } req;
    } line;
    pjsip_msg_body *body;
    pjsip_hdr *headers[PJSIP_H_COUNT];
} pjsip_msg;

typedef struct pjsip_tx_data {
    pjsip_msg *msg;
    pj_pool_t *pool;
    int ref_count;
    int method_id;
} pjsip_tx_data;

typedef struct pjsip_rx_data {
    struct { pjsip_msg *msg; } msg_info;
    struct {
        union { struct sockaddr_in ipv4; } src_addr;
    } pkt_info;
} pjsip_rx_data;

typedef struct pjsip_module {
    void *prev;
    void *next;
    pj_str_t name;
    int id;
    int priority;
    void *load;
    void *start;
    void *stop;
    void *unload;
    pj_bool_t (*on_rx_request)(pjsip_rx_data *data);
    pj_bool_t (*on_rx_response)(pjsip_rx_data *data);
    void *on_tx_request;
    void *on_tx_response;
    void *on_transaction_state;
} pjsip_module;

#define PJSIP_MOD_PRIORITY_APPLICATION 10
#define PJ_AF_INET AF_INET
#define PJ_THREAD_DEFAULT_STACK_SIZE 0

typedef struct pj_mutex_t { pthread_mutex_t mutex; } pj_mutex_t;
typedef struct pj_thread_t { int unused; } pj_thread_t;
typedef long pj_thread_desc[64];
typedef void *pj_thread_proc(void *arg);
typedef struct { long sec; long msec; } pj_time_val;
typedef struct { int unused; } pj_pool_factory;
typedef struct { pj_pool_factory factory; } pj_caching_pool;
typedef struct { int unused; } pj_pool_factory_policy;

extern pj_pool_factory_policy pj_pool_factory_default_policy;

pj_status_t pj_init(void);
void pj_shutdown(void);
void pj_log_set_level(int level);
void pj_caching_pool_init(pj_caching_pool *pool,
                          const pj_pool_factory_policy *policy,
                          int size);
void pj_caching_pool_destroy(pj_caching_pool *pool);
pj_pool_t *pj_pool_create(pj_pool_factory *factory, const char *name,
                          size_t initial_size, size_t increment,
                          void *callback);
void pj_pool_release(pj_pool_t *pool);
pj_status_t pj_mutex_create_simple(pj_pool_t *pool, const char *name,
                                   pj_mutex_t **mutex);
void pj_mutex_lock(pj_mutex_t *mutex);
void pj_mutex_unlock(pj_mutex_t *mutex);
pj_status_t pj_thread_create(pj_pool_t *pool, const char *name,
                             pj_thread_proc *procedure, void *arg,
                             size_t stack_size, unsigned int flags,
                             pj_thread_t **thread);
pj_status_t pj_thread_join(pj_thread_t *thread);
pj_status_t pj_thread_destroy(pj_thread_t *thread);
pj_bool_t pj_thread_is_registered(void);
pj_status_t pj_thread_register(const char *name, pj_thread_desc descriptor,
                               pj_thread_t **thread);
void pj_bzero(void *data, size_t size);
pj_uint16_t pj_htons(pj_uint16_t value);
pj_uint16_t pj_ntohs(pj_uint16_t value);

typedef struct sockaddr_in pj_sockaddr_in;

void *pjsip_msg_find_hdr(pjsip_msg *msg, int type, const void *start);
int pjsip_uri_print(int context, const void *uri, char *buffer, size_t size);
pjsip_contact_hdr *pjsip_contact_hdr_create(pj_pool_t *pool);
pjsip_generic_string_hdr *pjsip_generic_string_hdr_create(
    pj_pool_t *pool, const pj_str_t *name, const pj_str_t *value);
void *pjsip_parse_uri(pj_pool_t *pool, char *text, int length, unsigned int flags);
void pjsip_msg_add_hdr(pjsip_msg *msg, pjsip_hdr *header);
pjsip_msg_body *pjsip_msg_body_create(pj_pool_t *pool,
                                      const pj_str_t *type,
                                      const pj_str_t *subtype,
                                      const pj_str_t *text);

const pjsip_method *pjsip_get_invite_method(void);
const pjsip_method *pjsip_get_ack_method(void);
const pjsip_method *pjsip_get_bye_method(void);
const pjsip_method *pjsip_get_cancel_method(void);
int pjsip_method_cmp(const pjsip_method *left, const pjsip_method *right);

pj_status_t pjsip_endpt_create(pj_pool_factory *factory, const char *name,
                               pjsip_endpoint **endpoint);
void pjsip_endpt_destroy(pjsip_endpoint *endpoint);
pj_status_t pjsip_endpt_register_module(pjsip_endpoint *endpoint,
                                        pjsip_module *module);
pj_status_t pjsip_endpt_handle_events(pjsip_endpoint *endpoint,
                                      const pj_time_val *timeout);
pj_status_t pjsip_udp_transport_start(pjsip_endpoint *endpoint,
                                      const pj_sockaddr_in *address,
                                      const void *published_address,
                                      unsigned int async_count,
                                      pjsip_transport **transport);

pj_status_t pjsip_endpt_create_request(pjsip_endpoint *endpoint,
                                       const pjsip_method *method,
                                       const pj_str_t *target,
                                       const pj_str_t *from,
                                       const pj_str_t *to,
                                       const void *contact,
                                       const pj_str_t *call_id,
                                       int cseq,
                                       const void *body,
                                       pjsip_tx_data **result);
pj_status_t pjsip_endpt_create_cancel(pjsip_endpoint *endpoint,
                                      pjsip_tx_data *invite,
                                      pjsip_tx_data **result);
pj_status_t pjsip_endpt_create_response(pjsip_endpoint *endpoint,
                                        pjsip_rx_data *request,
                                        int status_code,
                                        const pj_str_t *reason,
                                        pjsip_tx_data **result);
pj_status_t pjsip_endpt_send_request_stateless(pjsip_endpoint *endpoint,
                                                pjsip_tx_data *data,
                                                void *token,
                                                void *callback);
pj_status_t pjsip_endpt_send_response2(pjsip_endpoint *endpoint,
                                       pjsip_rx_data *request,
                                       pjsip_tx_data *response,
                                       void *token,
                                       void *callback);
pj_status_t pjsip_endpt_respond_stateless(pjsip_endpoint *endpoint,
                                           pjsip_rx_data *request,
                                           int status_code,
                                           const pj_str_t *reason,
                                           const void *headers,
                                           const void *body);
void pjsip_tx_data_add_ref(pjsip_tx_data *data);
void pjsip_tx_data_dec_ref(pjsip_tx_data *data);

#endif
