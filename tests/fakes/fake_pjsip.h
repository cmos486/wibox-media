#ifndef WIBOX_TEST_FAKE_PJSIP_CONTROL_H
#define WIBOX_TEST_FAKE_PJSIP_CONTROL_H

#include "pjsip.h"

enum {
    FAKE_METHOD_INVITE = 1,
    FAKE_METHOD_ACK = 2,
    FAKE_METHOD_BYE = 3,
    FAKE_METHOD_CANCEL = 4
};

extern pj_status_t fake_create_request_status;
extern pj_status_t fake_create_cancel_status;
extern pj_status_t fake_create_response_status;
extern pj_status_t fake_send_request_status;
extern pj_status_t fake_send_response_status;
extern pj_status_t fake_respond_status;
extern int fake_invite_sent;
extern int fake_ack_sent;
extern int fake_bye_sent;
extern int fake_cancel_sent;
extern int fake_response_sent;
extern int fake_stateless_response_count;
extern int fake_last_stateless_status;
extern int fake_add_ref_count;
extern int fake_dec_ref_count;

void fake_pjsip_reset(void);

#endif
