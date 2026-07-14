#include "audio_hw.h"
#include "adi_audio.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

enum fail_point {
    AUDIO_FAIL_NONE,
    AUDIO_FAIL_GPIO_OPEN,
    AUDIO_FAIL_GPIO_WRITE,
    AUDIO_FAIL_INIT,
    AUDIO_FAIL_AI_FORMAT,
    AUDIO_FAIL_AO_FORMAT,
    AUDIO_FAIL_AI_ATTR,
    AUDIO_FAIL_AO_ATTR,
    AUDIO_FAIL_GAIN,
    AUDIO_FAIL_VOLUME,
    AUDIO_FAIL_AI_ENABLE,
    AUDIO_FAIL_AO_ENABLE,
    AUDIO_FAIL_AEC_REGISTER,
    AUDIO_FAIL_AEC_ENABLE,
    AUDIO_FAIL_GET_FRAME,
    AUDIO_FAIL_SEND_FRAME
};

static enum fail_point fail_point;
static int init_count;
static int exit_count;
static int ai_disable_count;
static int ao_disable_count;
static int unregister_count;
static int gpio_write_count;
static char gpio_values[8];
static GADI_AUDIO_AioAttrT captured_attr;
static GADI_AUDIO_GainLevelEnumT captured_gain;
static GADI_AUDIO_VolumeLevelEnumT captured_volume;
static unsigned char input_frame[] = {1, 2, 3, 4};
static size_t sent_length;

int __real_open(const char *path, int flags, ...);
ssize_t __real_write(int fd, const void *buffer, size_t length);
int __real_close(int fd);

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list args;

    if (strstr(path, "/sys/class/gpio/gpio") == path) {
        if (fail_point == AUDIO_FAIL_GPIO_OPEN) {
            errno = ENOENT;
            return -1;
        }
        return 88;
    }
    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
        return __real_open(path, flags, mode);
    }
    return __real_open(path, flags);
}

ssize_t __wrap_write(int fd, const void *buffer, size_t length)
{
    if (fd != 88) return __real_write(fd, buffer, length);
    if (fail_point == AUDIO_FAIL_GPIO_WRITE) {
        errno = EIO;
        return -1;
    }
    if (length == 1 && gpio_write_count < (int)sizeof(gpio_values)) {
        gpio_values[gpio_write_count++] = *(const char *)buffer;
    }
    return (ssize_t)length;
}

int __wrap_close(int fd)
{
    if (fd == 88) return 0;
    return __real_close(fd);
}

GADI_ERR gadi_audio_init(void)
{
    init_count++;
    return fail_point == AUDIO_FAIL_INIT ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_exit(void)
{
    exit_count++;
    return GADI_OK;
}

GADI_S32 gadi_audio_ai_get_fd(void) { return 10; }
GADI_S32 gadi_audio_ao_get_fd(void) { return 11; }

GADI_ERR gadi_audio_set_sample_format(GADI_S32 fd, GADI_AUDIO_SampleFormatEnumT format)
{
    (void)format;
    if (fd == 10 && fail_point == AUDIO_FAIL_AI_FORMAT) return -1;
    if (fd == 11 && fail_point == AUDIO_FAIL_AO_FORMAT) return -1;
    return GADI_OK;
}

GADI_ERR gadi_audio_ai_set_attr(GADI_AUDIO_AioAttrT *attribute)
{
    captured_attr = *attribute;
    return fail_point == AUDIO_FAIL_AI_ATTR ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ao_set_attr(GADI_AUDIO_AioAttrT *attribute)
{
    captured_attr = *attribute;
    return fail_point == AUDIO_FAIL_AO_ATTR ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ai_set_gain(GADI_AUDIO_GainLevelEnumT *gain)
{
    captured_gain = *gain;
    return fail_point == AUDIO_FAIL_GAIN ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ao_set_volume(GADI_AUDIO_VolumeLevelEnumT *volume)
{
    captured_volume = *volume;
    return fail_point == AUDIO_FAIL_VOLUME ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ai_enable(void)
{
    return fail_point == AUDIO_FAIL_AI_ENABLE ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ao_enable(void)
{
    return fail_point == AUDIO_FAIL_AO_ENABLE ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ai_disable(void) { ai_disable_count++; return GADI_OK; }
GADI_ERR gadi_audio_ao_disable(void) { ao_disable_count++; return GADI_OK; }

int ap_aec_register(int *handle)
{
    if (fail_point == AUDIO_FAIL_AEC_REGISTER) return -1;
    *handle = 9;
    return GADI_OK;
}

int ap_aec_unregister(int handle)
{
    CHECK(handle == 9);
    unregister_count++;
    return GADI_OK;
}

GADI_ERR gadi_audio_ai_aec_enable(void)
{
    return fail_point == AUDIO_FAIL_AEC_ENABLE ? -1 : GADI_OK;
}

GADI_ERR gadi_audio_ai_aec_disable(void) { return GADI_OK; }

GADI_ERR gadi_audio_ai_get_frame_aec(GADI_AUDIO_AioFrameT *frame,
                                      GADI_AEC_AioFrameT *aec_frame,
                                      GADI_BOOL block)
{
    (void)aec_frame;
    (void)block;
    if (fail_point == AUDIO_FAIL_GET_FRAME) return -1;
    memset(frame, 0, sizeof(*frame));
    frame->virAddr = input_frame;
    frame->len = sizeof(input_frame);
    return GADI_OK;
}

GADI_ERR gadi_audio_ao_send_frame_aec(GADI_AUDIO_AioFrameT *frame, GADI_BOOL block)
{
    (void)block;
    sent_length = frame->len;
    return fail_point == AUDIO_FAIL_SEND_FRAME ? -1 : GADI_OK;
}

static void reset_mock(void)
{
    fail_point = AUDIO_FAIL_NONE;
    init_count = 0;
    exit_count = 0;
    ai_disable_count = 0;
    ao_disable_count = 0;
    unregister_count = 0;
    gpio_write_count = 0;
    memset(gpio_values, 0, sizeof(gpio_values));
    memset(&captured_attr, 0, sizeof(captured_attr));
    captured_gain = GLEVEL_MAX;
    captured_volume = VLEVEL_0;
    sent_length = 0;
}

static int expect_start_failure(enum fail_point point)
{
    reset_mock();
    fail_point = point;
    CHECK(audio_hw_start(18, 160, 35, 50) == -1);
    audio_hw_stop();
    return 0;
}

int main(void)
{
    static const enum fail_point failures[] = {
        AUDIO_FAIL_GPIO_OPEN, AUDIO_FAIL_GPIO_WRITE, AUDIO_FAIL_INIT,
        AUDIO_FAIL_AI_FORMAT, AUDIO_FAIL_AO_FORMAT, AUDIO_FAIL_AI_ATTR,
        AUDIO_FAIL_AO_ATTR, AUDIO_FAIL_GAIN, AUDIO_FAIL_VOLUME,
        AUDIO_FAIL_AI_ENABLE, AUDIO_FAIL_AO_ENABLE
    };
    unsigned char buffer[8] = {0};
    size_t i;

    CHECK(audio_hw_get_frame(buffer, sizeof(buffer)) == -1);
    CHECK(audio_hw_send_frame(buffer, sizeof(buffer)) == -1);
    for (i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        CHECK(expect_start_failure(failures[i]) == 0);
    }

    reset_mock();
    fail_point = AUDIO_FAIL_AEC_REGISTER;
    CHECK(audio_hw_start(18, 160, 35, 50) == 0);
    audio_hw_stop();
    CHECK(unregister_count == 0);

    reset_mock();
    fail_point = AUDIO_FAIL_AEC_ENABLE;
    CHECK(audio_hw_start(18, 160, 35, 50) == 0);
    audio_hw_stop();
    CHECK(unregister_count == 1);

    reset_mock();
    CHECK(audio_hw_start(0, 0, -20, 200) == 0);
    CHECK(audio_hw_start(23, 320, 50, 50) == 0);
    CHECK(init_count == 1);
    CHECK(audio_hw_frame_size() == 160);
    CHECK(captured_attr.frameSamples == 160);
    CHECK(captured_attr.sampleRate == GADI_AUDIO_SAMPLE_RATE_8000);
    CHECK(captured_gain == GLEVEL_0);
    CHECK(captured_volume == VLEVEL_12);
    CHECK(gpio_values[0] == '0');
    CHECK(audio_hw_get_frame(NULL, sizeof(buffer)) == -1);
    CHECK(audio_hw_get_frame(buffer, 0) == -1);
    CHECK(audio_hw_get_frame(buffer, 2) == 2);
    CHECK(buffer[0] == 1 && buffer[1] == 2);
    fail_point = AUDIO_FAIL_GET_FRAME;
    CHECK(audio_hw_get_frame(buffer, sizeof(buffer)) == -1);
    fail_point = AUDIO_FAIL_NONE;
    CHECK(audio_hw_send_frame(NULL, 1) == -1);
    CHECK(audio_hw_send_frame(buffer, 0) == -1);
    CHECK(audio_hw_send_frame(buffer, 2) == 0);
    CHECK(sent_length == 2);
    fail_point = AUDIO_FAIL_SEND_FRAME;
    CHECK(audio_hw_send_frame(buffer, 2) == -1);
    fail_point = AUDIO_FAIL_NONE;
    audio_hw_stop();
    audio_hw_stop();
    CHECK(ai_disable_count >= 1 && ao_disable_count >= 1);
    CHECK(exit_count == 1);
    CHECK(gpio_values[gpio_write_count - 1] == '1');
    printf("RESULT audio_hw_mock PASS failures=%zu\n",
           sizeof(failures) / sizeof(failures[0]));
    return 0;
}
