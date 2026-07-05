#ifndef INTERCOM_H
#define INTERCOM_H

/**
 * Intercom device communication module
 * Handles commands to /dev/ttySGK1
 */

// Available intercom commands
typedef enum {
    INTERCOM_CMD_UNLOCK_DOOR,
    INTERCOM_CMD_START_CALL,
    INTERCOM_CMD_STOP_CALL,
    INTERCOM_CMD_ENABLE_PUSH_STATE,
    INTERCOM_CMD_DISABLE_PUSH_STATE,
    INTERCOM_CMD_F1_ON,
    INTERCOM_CMD_F1_OFF,
    INTERCOM_CMD_SOFIA_UART_SET_MODE,
    INTERCOM_CMD_SOFIA_UART_START,
    INTERCOM_CMD_SOFIA_UART_POST_INIT
} intercom_cmd_t;

/**
 * Initialize intercom module
 * @return 0 on success, -1 on failure
 */
int intercom_init(void);

/**
 * Send command to intercom device
 * @param cmd Command to send
 * @return 0 on success, -1 on failure
 */
int intercom_send_command(intercom_cmd_t cmd);

/**
 * Cleanup intercom module
 */
void intercom_cleanup(void);

#endif // INTERCOM_H
