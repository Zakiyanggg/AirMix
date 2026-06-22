#include "BT_Player.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_check.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define BT_PLAYER_DEFAULT_SAMPLE_RATE_HZ 44100
#define BT_PLAYER_DMA_FRAME_COUNT        512
#define BT_PLAYER_RINGBUF_SIZE           (48 * 1024)
#define BT_PLAYER_I2S_WRITE_CHUNK        2048
#define BT_PLAYER_AUDIO_TASK_PRIO        23
#define BT_PLAYER_DRUM_VOICE_COUNT       4
#define BT_PLAYER_OUTPUT_FRAME_COUNT     (BT_PLAYER_I2S_WRITE_CHUNK / 4)

typedef enum {
    BT_PLAYER_EVT_CONN_STATE,
    BT_PLAYER_EVT_AUDIO_STATE,
    BT_PLAYER_EVT_AUDIO_CFG,
} bt_player_evt_id_t;

typedef struct {
    bt_player_evt_id_t id;
    esp_a2d_cb_param_t param;
} bt_player_evt_t;

typedef struct {
    BT_Player_Drum_t type;
    uint32_t frame_count;
    uint32_t position;
    float phase[6];
    float lp_state;
    float hp_state;
    float bp_lp_state;
    float bp_hp_state;
    float detune;
    bool active;
} bt_drum_voice_t;

static const char *TAG_BT_PLAYER = "BT_PLAYER";
static i2s_chan_handle_t i2s_tx_chan;
static RingbufHandle_t audio_ringbuf;
static SemaphoreHandle_t audio_start_sem;
static SemaphoreHandle_t i2s_write_mutex;
static QueueHandle_t bt_event_queue;
static TaskHandle_t audio_task_handle;
static TaskHandle_t bt_event_task_handle;
static bt_drum_voice_t s_drum_voices[BT_PLAYER_DRUM_VOICE_COUNT];
static int16_t s_output_buf[BT_PLAYER_I2S_WRITE_CHUNK / sizeof(int16_t)];
static volatile bool bt_ready;
static volatile bool bt_connected;
static volatile bool audio_started;
static volatile bool amp_enabled;
static volatile uint32_t packet_count;
static volatile uint32_t i2s_write_count;
static volatile uint32_t current_sample_rate_hz = BT_PLAYER_DEFAULT_SAMPLE_RATE_HZ;
static uint8_t s_avrc_tl = 0;
static bool s_avrc_connected;
static esp_avrc_rn_evt_cap_mask_t s_peer_rn_cap;
static char s_track_title[BT_PLAYER_TITLE_LEN];
static char s_track_artist[BT_PLAYER_ARTIST_LEN];
static bool s_metadata_valid;
static uint32_t s_noise_seed = 0x12345678;

static void bt_player_audio_task(void *arg);
static void bt_player_event_task(void *arg);
static esp_err_t bt_player_i2s_init(void);
static void bt_player_i2s_set_sample_rate(int sample_rate_hz, bool mono);
static void bt_player_handle_a2dp_event(const bt_player_evt_t *event);
static void bt_player_a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static void bt_player_a2dp_data_cb(const uint8_t *data, uint32_t len);
static void bt_player_avrc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
static void bt_player_avrc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param);
static void bt_player_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static esp_err_t bt_player_bluetooth_init(void);
static uint8_t bt_player_alloc_tl(void);
static void bt_player_clear_metadata(void);
static void bt_player_set_metadata(uint8_t attr_id, const char *text);
static void bt_player_request_metadata(void);
static void bt_player_register_track_notify(void);
static int16_t bt_player_clamp16(int32_t value);
static int16_t bt_player_noise_sample(void);
static float bt_player_soft_clip(float sample);
static float bt_player_drive(float sample, float gain);
static float bt_player_env_adsr(float t_norm, float attack, float decay);
static float bt_player_one_pole_lp(float input, float *state, float cutoff_hz, uint32_t sample_rate_hz);
static float bt_player_one_pole_hp(float input, float *state, float cutoff_hz, uint32_t sample_rate_hz);
static float bt_player_bandpass(float input, float *lp_state, float *hp_state,
                                float low_hz, float high_hz, uint32_t sample_rate_hz);
static float bt_player_hat_metal(bt_drum_voice_t *voice, uint32_t sample_rate_hz, float base_hz, float decay);
static bool bt_player_any_drum_active(void);
static void bt_player_mix_drums(int16_t *pcm, uint32_t frame_count);
static uint32_t bt_player_drum_length(BT_Player_Drum_t drum, uint32_t sample_rate_hz);
static int16_t bt_player_render_drum_sample(bt_drum_voice_t *voice, uint32_t sample_rate_hz);
static bt_drum_voice_t *bt_player_voice_for_drum(BT_Player_Drum_t drum);
static void bt_player_start_drum_voice(bt_drum_voice_t *voice, BT_Player_Drum_t drum, uint32_t sample_rate_hz);

static uint8_t bt_player_alloc_tl(void)
{
    s_avrc_tl = (s_avrc_tl + 1) % (ESP_AVRC_TRANS_LABEL_MAX + 1);
    return s_avrc_tl;
}

static void bt_player_clear_metadata(void)
{
    s_metadata_valid = false;
    s_track_title[0] = '\0';
    s_track_artist[0] = '\0';
}

static void bt_player_set_metadata(uint8_t attr_id, const char *text)
{
    if (!text || text[0] == '\0') {
        return;
    }

    if (attr_id == ESP_AVRC_MD_ATTR_TITLE) {
        strncpy(s_track_title, text, sizeof(s_track_title) - 1);
        s_track_title[sizeof(s_track_title) - 1] = '\0';
        s_metadata_valid = true;
        ESP_LOGI(TAG_BT_PLAYER, "Track title: %s", s_track_title);
    } else if (attr_id == ESP_AVRC_MD_ATTR_ARTIST) {
        strncpy(s_track_artist, text, sizeof(s_track_artist) - 1);
        s_track_artist[sizeof(s_track_artist) - 1] = '\0';
        s_metadata_valid = true;
        ESP_LOGI(TAG_BT_PLAYER, "Track artist: %s", s_track_artist);
    }
}

static void bt_player_request_metadata(void)
{
    if (!s_avrc_connected) {
        return;
    }

    uint8_t attr_mask = ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST;
    esp_avrc_ct_send_metadata_cmd(bt_player_alloc_tl(), attr_mask);
}

static void bt_player_register_track_notify(void)
{
    if (!s_avrc_connected) {
        return;
    }

    if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &s_peer_rn_cap, ESP_AVRC_RN_TRACK_CHANGE)) {
        esp_avrc_ct_send_register_notification_cmd(bt_player_alloc_tl(), ESP_AVRC_RN_TRACK_CHANGE, 0);
    }
}

static int16_t bt_player_clamp16(int32_t value)
{
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return (int16_t)value;
}

static int16_t bt_player_noise_sample(void)
{
    s_noise_seed = s_noise_seed * 1664525UL + 1013904223UL;
    return (int16_t)((int32_t)(s_noise_seed >> 16) - 32768);
}

static float bt_player_soft_clip(float sample)
{
    if (sample > 1.0f) {
        return 1.0f;
    }
    if (sample < -1.0f) {
        return -1.0f;
    }
    return sample - (sample * sample * sample) / 3.0f;
}

static float bt_player_drive(float sample, float gain)
{
    return bt_player_soft_clip(sample * gain) / gain;
}

static float bt_player_env_adsr(float t_norm, float attack, float decay)
{
    if (t_norm < attack) {
        return t_norm / attack;
    }
    float release_t = (t_norm - attack) / (1.0f - attack);
    return expf(-decay * release_t);
}

static float bt_player_one_pole_lp(float input, float *state, float cutoff_hz, uint32_t sample_rate_hz)
{
    float coef = 2.0f * (float)M_PI * cutoff_hz /
                 (2.0f * (float)M_PI * cutoff_hz + (float)sample_rate_hz);
    *state += coef * (input - *state);
    return *state;
}

static float bt_player_one_pole_hp(float input, float *state, float cutoff_hz, uint32_t sample_rate_hz)
{
    float lp = bt_player_one_pole_lp(input, state, cutoff_hz, sample_rate_hz);
    return input - lp;
}

static float bt_player_bandpass(float input, float *lp_state, float *hp_state,
                                float low_hz, float high_hz, uint32_t sample_rate_hz)
{
    float hp = bt_player_one_pole_hp(input, hp_state, low_hz, sample_rate_hz);
    return bt_player_one_pole_lp(hp, lp_state, high_hz, sample_rate_hz);
}

static float bt_player_hat_metal(bt_drum_voice_t *voice, uint32_t sample_rate_hz, float base_hz, float decay)
{
    static const float partials[6] = {1.00f, 1.47f, 2.09f, 2.71f, 3.58f, 4.83f};
    static const float weights[6] = {0.30f, 0.24f, 0.20f, 0.14f, 0.08f, 0.05f};
    float t = (float)voice->position / (float)voice->frame_count;
    float ring = expf(-decay * t);
    float sum = 0.0f;

    for (int i = 0; i < 6; i++) {
        float freq = base_hz * partials[i] * voice->detune;
        voice->phase[i] += 2.0f * (float)M_PI * freq / (float)sample_rate_hz;
        sum += sinf(voice->phase[i]) * weights[i];
    }

    float bright = bt_player_one_pole_hp(sum, &voice->bp_lp_state, 1800.0f, sample_rate_hz);
    return bt_player_soft_clip(bright * ring * 1.35f);
}

static bool bt_player_any_drum_active(void)
{
    for (int i = 0; i < BT_PLAYER_DRUM_VOICE_COUNT; i++) {
        if (s_drum_voices[i].active) {
            return true;
        }
    }
    return false;
}

static void bt_player_mix_drums(int16_t *pcm, uint32_t frame_count)
{
    uint32_t sample_rate_hz = current_sample_rate_hz;
    if (sample_rate_hz == 0) {
        sample_rate_hz = BT_PLAYER_DEFAULT_SAMPLE_RATE_HZ;
    }

    for (int voice_idx = 0; voice_idx < BT_PLAYER_DRUM_VOICE_COUNT; voice_idx++) {
        bt_drum_voice_t *voice = &s_drum_voices[voice_idx];
        if (!voice->active) {
            continue;
        }

        for (uint32_t i = 0; i < frame_count; i++) {
            if (voice->position >= voice->frame_count) {
                voice->active = false;
                break;
            }

            int16_t drum = bt_player_render_drum_sample(voice, sample_rate_hz);
            int32_t mixed_l = (int32_t)pcm[i * 2] + drum;
            int32_t mixed_r = (int32_t)pcm[i * 2 + 1] + drum;
            pcm[i * 2] = bt_player_clamp16(mixed_l);
            pcm[i * 2 + 1] = bt_player_clamp16(mixed_r);
            voice->position++;
        }
    }
}

static uint32_t bt_player_drum_length(BT_Player_Drum_t drum, uint32_t sample_rate_hz)
{
    switch (drum) {
    case BT_PLAYER_DRUM_KICK:
        return (sample_rate_hz * 14) / 100;
    case BT_PLAYER_DRUM_SNARE:
        return (sample_rate_hz * 22) / 100;
    case BT_PLAYER_DRUM_OPEN_HAT:
        return (sample_rate_hz * 28) / 100;
    case BT_PLAYER_DRUM_CLOSED_HAT:
        return (sample_rate_hz * 6) / 100;
    default:
        return sample_rate_hz / 20;
    }
}

static int16_t bt_player_render_drum_sample(bt_drum_voice_t *voice, uint32_t sample_rate_hz)
{
    if (voice->frame_count == 0) {
        return 0;
    }

    float t = (float)voice->position / (float)voice->frame_count;
    float t_sec = (float)voice->position / (float)sample_rate_hz;
    float noise = (float)bt_player_noise_sample() / 32768.0f;

    switch (voice->type) {
    case BT_PLAYER_DRUM_KICK: {
        float pitch = 38.0f + 195.0f * expf(-42.0f * t);
        voice->phase[0] += 2.0f * (float)M_PI * pitch / (float)sample_rate_hz;
        float body = bt_player_drive(sinf(voice->phase[0]), 2.4f);

        voice->phase[1] += 2.0f * (float)M_PI * (pitch * 0.5f) / (float)sample_rate_hz;
        float sub = sinf(voice->phase[1]) * expf(-2.8f * t) * 0.42f;

        float amp = bt_player_env_adsr(t, 0.004f, 3.6f);
        float click = 0.0f;
        if (t_sec < 0.004f) {
            float click_noise = bt_player_one_pole_hp(noise, &voice->lp_state, 1200.0f, sample_rate_hz);
            click = click_noise * expf(-900.0f * t_sec) * 0.75f;
        }

        float mixed = bt_player_soft_clip((body * 0.88f + sub + click) * amp);
        return (int16_t)(mixed * 27000.0f);
    }
    case BT_PLAYER_DRUM_SNARE: {
        voice->phase[0] += 2.0f * (float)M_PI * 195.0f / (float)sample_rate_hz;
        voice->phase[1] += 2.0f * (float)M_PI * 365.0f / (float)sample_rate_hz;
        float tone = sinf(voice->phase[0]) * expf(-24.0f * t) * 0.34f;
        tone += sinf(voice->phase[1]) * expf(-31.0f * t) * 0.18f;

        float body = bt_player_bandpass(noise, &voice->lp_state, &voice->hp_state,
                                        140.0f, 360.0f, sample_rate_hz) * expf(-9.0f * t);
        float snap = bt_player_bandpass(noise, &voice->bp_lp_state, &voice->bp_hp_state,
                                        2800.0f, 9000.0f, sample_rate_hz) * expf(-18.0f * t);

        float amp = bt_player_env_adsr(t, 0.003f, 4.8f);
        float mixed = bt_player_soft_clip((tone + body * 0.95f + snap * 0.82f) * amp);
        return (int16_t)(mixed * 23500.0f);
    }
    case BT_PLAYER_DRUM_OPEN_HAT: {
        float metal = bt_player_hat_metal(voice, sample_rate_hz, 285.0f, 2.4f);
        float sizzle = bt_player_bandpass(noise, &voice->lp_state, &voice->hp_state,
                                          5000.0f, 14000.0f, sample_rate_hz);
        float amp = bt_player_env_adsr(t, 0.006f, 2.6f);
        float mixed = bt_player_soft_clip((metal * 0.78f + sizzle * 0.42f) * amp);
        return (int16_t)(mixed * 16500.0f);
    }
    case BT_PLAYER_DRUM_CLOSED_HAT: {
        float metal = bt_player_hat_metal(voice, sample_rate_hz, 430.0f, 11.0f);
        float sizzle = bt_player_bandpass(noise, &voice->lp_state, &voice->hp_state,
                                          6500.0f, 16000.0f, sample_rate_hz);
        float amp = bt_player_env_adsr(t, 0.002f, 9.5f);
        float mixed = bt_player_soft_clip((metal * 0.70f + sizzle * 0.48f) * amp);
        return (int16_t)(mixed * 17500.0f);
    }
    default:
        return 0;
    }
}

static void bt_player_start_drum_voice(bt_drum_voice_t *voice, BT_Player_Drum_t drum, uint32_t sample_rate_hz)
{
    voice->type = drum;
    voice->frame_count = bt_player_drum_length(drum, sample_rate_hz);
    voice->position = 0;
    memset(voice->phase, 0, sizeof(voice->phase));
    voice->lp_state = 0.0f;
    voice->hp_state = 0.0f;
    voice->bp_lp_state = 0.0f;
    voice->bp_hp_state = 0.0f;
    voice->detune = 0.985f + (float)((s_noise_seed >> 8) & 0x3F) / 1024.0f;
    voice->active = true;
}

static bt_drum_voice_t *bt_player_voice_for_drum(BT_Player_Drum_t drum)
{
    return &s_drum_voices[(uint8_t)drum];
}

static void bt_player_audio_task(void *arg)
{
    (void)arg;

    while (1) {
        if (!audio_started && !bt_player_any_drum_active()) {
            xSemaphoreTake(audio_start_sem, portMAX_DELAY);
            continue;
        }

        if (audio_started) {
            size_t item_size = 0;
            uint8_t *data = (uint8_t *)xRingbufferReceiveUpTo(
                audio_ringbuf,
                &item_size,
                pdMS_TO_TICKS(50),
                BT_PLAYER_I2S_WRITE_CHUNK);

            if (data != NULL && item_size >= 4) {
                uint32_t frame_count = (uint32_t)(item_size / 4);
                memcpy(s_output_buf, data, frame_count * 4);
                bt_player_mix_drums(s_output_buf, frame_count);

                gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 1);
                amp_enabled = true;

                size_t bytes_written = 0;
                esp_err_t ret = i2s_channel_write(
                    i2s_tx_chan,
                    s_output_buf,
                    frame_count * 4,
                    &bytes_written,
                    portMAX_DELAY);

                if (ret == ESP_OK && bytes_written == frame_count * 4) {
                    i2s_write_count++;
                }
                vRingbufferReturnItem(audio_ringbuf, data);
                continue;
            }

            if (data != NULL) {
                vRingbufferReturnItem(audio_ringbuf, data);
            }
            vTaskDelay(1);
            continue;
        }

        if (bt_player_any_drum_active()) {
            uint32_t frame_count = BT_PLAYER_OUTPUT_FRAME_COUNT;
            memset(s_output_buf, 0, frame_count * 4);
            bt_player_mix_drums(s_output_buf, frame_count);

            gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 1);
            amp_enabled = true;

            size_t bytes_written = 0;
            i2s_channel_write(
                i2s_tx_chan,
                s_output_buf,
                frame_count * 4,
                &bytes_written,
                portMAX_DELAY);
            continue;
        }

        gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 0);
        amp_enabled = false;
    }
}

static void bt_player_event_task(void *arg)
{
    (void)arg;

    bt_player_evt_t event;
    while (1) {
        if (xQueueReceive(bt_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            bt_player_handle_a2dp_event(&event);
        }
    }
}

esp_err_t BT_Player_Init(void)
{
    ESP_LOGI(TAG_BT_PLAYER, "Starting A2DP sink: device=%s", BT_PLAYER_DEVICE_NAME);
    ESP_LOGI(TAG_BT_PLAYER, "MAX98357A pins: BCLK=%d LRC/WS=%d DIN=%d SD/EN=%d",
             BT_PLAYER_I2S_BCLK_GPIO,
             BT_PLAYER_I2S_LRC_GPIO,
             BT_PLAYER_I2S_DIN_GPIO,
             BT_PLAYER_AMP_SD_GPIO);

    ESP_RETURN_ON_ERROR(bt_player_bluetooth_init(), TAG_BT_PLAYER, "Bluetooth init failed");
    ESP_RETURN_ON_ERROR(bt_player_i2s_init(), TAG_BT_PLAYER, "I2S init failed");

    ESP_LOGI(TAG_BT_PLAYER, "Ready. Pair phone with \"%s\" and play music.", BT_PLAYER_DEVICE_NAME);
    return ESP_OK;
}

void BT_Player_GetStatus(BT_Player_Status_t *status)
{
    if (status == NULL) {
        return;
    }

    size_t buffered = 0;
    if (audio_ringbuf != NULL) {
        vRingbufferGetInfo(audio_ringbuf, NULL, NULL, NULL, NULL, &buffered);
    }

    status->bt_ready = bt_ready;
    status->connected = bt_connected;
    status->audio_started = audio_started;
    status->amp_enabled = amp_enabled;
    status->packet_count = packet_count;
    status->i2s_write_count = i2s_write_count;
    status->sample_rate_hz = current_sample_rate_hz;
    status->buffered_bytes = (uint32_t)buffered;
    status->metadata_valid = s_metadata_valid;
    strncpy(status->title, s_track_title, sizeof(status->title) - 1);
    status->title[sizeof(status->title) - 1] = '\0';
    strncpy(status->artist, s_track_artist, sizeof(status->artist) - 1);
    status->artist[sizeof(status->artist) - 1] = '\0';
}

void BT_Player_TriggerDrum(BT_Player_Drum_t drum)
{
    if (drum >= BT_PLAYER_DRUM_CLOSED_HAT + 1) {
        return;
    }

    uint32_t sample_rate_hz = current_sample_rate_hz;
    if (sample_rate_hz == 0) {
        sample_rate_hz = BT_PLAYER_DEFAULT_SAMPLE_RATE_HZ;
    }

    bt_drum_voice_t *voice = bt_player_voice_for_drum(drum);
    bt_player_start_drum_voice(voice, drum, sample_rate_hz);

    switch (drum) {
    case BT_PLAYER_DRUM_SNARE:
        ESP_LOGI(TAG_BT_PLAYER, "Trigger drum: snare");
        break;
    case BT_PLAYER_DRUM_KICK:
        ESP_LOGI(TAG_BT_PLAYER, "Trigger drum: kick");
        break;
    case BT_PLAYER_DRUM_OPEN_HAT:
        ESP_LOGI(TAG_BT_PLAYER, "Trigger drum: open hat");
        break;
    case BT_PLAYER_DRUM_CLOSED_HAT:
        ESP_LOGI(TAG_BT_PLAYER, "Trigger drum: closed hat");
        break;
    default:
        voice->active = false;
        return;
    }

    xSemaphoreGive(audio_start_sem);
}

static esp_err_t bt_player_i2s_init(void)
{
    gpio_config_t amp_sd_config = {
        .pin_bit_mask = 1ULL << BT_PLAYER_AMP_SD_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&amp_sd_config), TAG_BT_PLAYER, "amp gpio config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 0), TAG_BT_PLAYER, "amp mute failed");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = BT_PLAYER_DMA_FRAME_COUNT;
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BT_PLAYER, "Failed to create I2S TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG_BT_PLAYER, "I2S TX channel created");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BT_PLAYER_DEFAULT_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BT_PLAYER_I2S_BCLK_GPIO,
            .ws = BT_PLAYER_I2S_LRC_GPIO,
            .dout = BT_PLAYER_I2S_DIN_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg), TAG_BT_PLAYER, "I2S std init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_tx_chan), TAG_BT_PLAYER, "I2S enable failed");

    audio_ringbuf = xRingbufferCreate(BT_PLAYER_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    audio_start_sem = xSemaphoreCreateBinary();
    i2s_write_mutex = xSemaphoreCreateMutex();
    bt_event_queue = xQueueCreate(8, sizeof(bt_player_evt_t));
    if (audio_ringbuf == NULL || audio_start_sem == NULL || i2s_write_mutex == NULL || bt_event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ret = xTaskCreatePinnedToCore(
        bt_player_audio_task,
        "bt_i2s_audio",
        6144,
        NULL,
        BT_PLAYER_AUDIO_TASK_PRIO,
        &audio_task_handle,
        1);
    if (task_ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    task_ret = xTaskCreate(
        bt_player_event_task,
        "bt_evt",
        4096,
        NULL,
        7,
        &bt_event_task_handle);
    if (task_ret != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void bt_player_i2s_set_sample_rate(int sample_rate_hz, bool mono)
{
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_hz);
    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT,
        mono ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO);

    xSemaphoreTake(i2s_write_mutex, portMAX_DELAY);
    ESP_ERROR_CHECK(i2s_channel_disable(i2s_tx_chan));
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(i2s_tx_chan, &clk_cfg));
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(i2s_tx_chan, &slot_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_chan));
    xSemaphoreGive(i2s_write_mutex);
    current_sample_rate_hz = (uint32_t)sample_rate_hz;
    ESP_LOGI(TAG_BT_PLAYER, "I2S configured: sample_rate=%d channels=%s", sample_rate_hz, mono ? "mono" : "stereo");
}

static void bt_player_handle_a2dp_event(const bt_player_evt_t *event)
{
    const esp_a2d_cb_param_t *param = &event->param;

    switch (event->id) {
    case BT_PLAYER_EVT_CONN_STATE:
        ESP_LOGI(TAG_BT_PLAYER, "A2DP connection state=%d", param->conn_stat.state);
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        break;

    case BT_PLAYER_EVT_AUDIO_STATE:
        ESP_LOGI(TAG_BT_PLAYER, "A2DP audio state=%d", param->audio_stat.state);
        if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
            bt_player_request_metadata();
        }
        break;

    case BT_PLAYER_EVT_AUDIO_CFG: {
        int sample_rate = 16000;
        bool mono = false;
        const esp_a2d_mcc_t *mcc = &param->audio_cfg.mcc;

        if (mcc->type == ESP_A2D_MCT_SBC) {
            if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_48K) {
                sample_rate = 48000;
            } else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_44K) {
                sample_rate = 44100;
            } else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_32K) {
                sample_rate = 32000;
            }
            mono = (mcc->cie.sbc_info.ch_mode & ESP_A2D_SBC_CIE_CH_MODE_MONO) != 0;
            bt_player_i2s_set_sample_rate(sample_rate, mono);
        }
        break;
    }

    default:
        break;
    }
}

static void bt_player_a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    bt_player_evt_t queued_event = {0};
    bool should_queue = false;

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            bt_connected = true;
        } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            bt_connected = false;
            audio_started = false;
            vRingbufferReset(audio_ringbuf);
            gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 0);
            amp_enabled = false;
            bt_player_clear_metadata();
        }
        queued_event.id = BT_PLAYER_EVT_CONN_STATE;
        should_queue = true;
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
            vRingbufferReset(audio_ringbuf);
            audio_started = true;
            packet_count = 0;
            i2s_write_count = 0;
            gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 1);
            amp_enabled = true;
            xSemaphoreGive(audio_start_sem);
        } else {
            audio_started = false;
            if (!bt_player_any_drum_active()) {
                gpio_set_level(BT_PLAYER_AMP_SD_GPIO, 0);
                amp_enabled = false;
            }
        }
        queued_event.id = BT_PLAYER_EVT_AUDIO_STATE;
        should_queue = true;
        break;

    case ESP_A2D_AUDIO_CFG_EVT:
        queued_event.id = BT_PLAYER_EVT_AUDIO_CFG;
        should_queue = true;
        break;

    default:
        ESP_LOGI(TAG_BT_PLAYER, "A2DP event=%d", event);
        break;
    }

    if (should_queue && bt_event_queue != NULL) {
        queued_event.param = *param;
        if (xQueueSend(bt_event_queue, &queued_event, 0) != pdTRUE) {
            ESP_LOGW(TAG_BT_PLAYER, "BT event queue full, drop event=%d", event);
        }
    }
}

static void bt_player_a2dp_data_cb(const uint8_t *data, uint32_t len)
{
    if (!audio_started || data == NULL || len == 0) {
        return;
    }

    packet_count++;
    if (xRingbufferSend(audio_ringbuf, (void *)data, len, 0) != pdTRUE) {
        if (packet_count % 100 == 0) {
            ESP_LOGW(TAG_BT_PLAYER, "audio ringbuffer full, drop packet len=%lu", (unsigned long)len);
        }
        return;
    }
}

static void bt_player_avrc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
        s_avrc_connected = param->conn_stat.connected;
        ESP_LOGI(TAG_BT_PLAYER, "AVRCP CT connected=%d", param->conn_stat.connected);
        if (param->conn_stat.connected) {
            esp_avrc_ct_send_get_rn_capabilities_cmd(bt_player_alloc_tl());
            bt_player_request_metadata();
        } else {
            bt_player_clear_metadata();
        }
        break;
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
        s_peer_rn_cap.bits = param->get_rn_caps_rsp.evt_set.bits;
        bt_player_register_track_notify();
        bt_player_request_metadata();
        break;
    case ESP_AVRC_CT_METADATA_RSP_EVT:
        if (param->meta_rsp.attr_text != NULL && param->meta_rsp.attr_length > 0) {
            char text[64];
            size_t copy_len = param->meta_rsp.attr_length;
            if (copy_len >= sizeof(text)) {
                copy_len = sizeof(text) - 1;
            }
            memcpy(text, param->meta_rsp.attr_text, copy_len);
            text[copy_len] = '\0';
            bt_player_set_metadata(param->meta_rsp.attr_id, text);
        }
        break;
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        ESP_LOGI(TAG_BT_PLAYER, "AVRCP notify event=%d", param->change_ntf.event_id);
        if (param->change_ntf.event_id == ESP_AVRC_RN_TRACK_CHANGE) {
            bt_player_register_track_notify();
            bt_player_request_metadata();
        }
        break;
    default:
        ESP_LOGI(TAG_BT_PLAYER, "AVRCP CT event=%d", event);
        break;
    }
}

static void bt_player_avrc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
    switch (event) {
    case ESP_AVRC_TG_CONNECTION_STATE_EVT:
        ESP_LOGI(TAG_BT_PLAYER, "AVRCP TG connected=%d", param->conn_stat.connected);
        break;
    case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
        ESP_LOGI(TAG_BT_PLAYER, "AVRCP passthrough key=%d state=%d",
                 param->psth_cmd.key_code,
                 param->psth_cmd.key_state);
        break;
    default:
        ESP_LOGI(TAG_BT_PLAYER, "AVRCP TG event=%d", event);
        break;
    }
}

static void bt_player_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        ESP_LOGI(TAG_BT_PLAYER, "Pairing %s: %s",
                 param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS ? "success" : "failed",
                 param->auth_cmpl.device_name);
        break;
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG_BT_PLAYER, "BT scan mode changed: mode=%d", param->mode_chg.mode);
        break;
    default:
        ESP_LOGI(TAG_BT_PLAYER, "GAP event=%d", event);
        break;
    }
}

static esp_err_t bt_player_bluetooth_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG_BT_PLAYER, "NVS erase failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG_BT_PLAYER, "NVS init failed");

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(ret, TAG_BT_PLAYER, "release BLE memory failed");
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&bt_cfg), TAG_BT_PLAYER, "BT controller init failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), TAG_BT_PLAYER, "BT controller enable failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG_BT_PLAYER, "Bluedroid init failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG_BT_PLAYER, "Bluedroid enable failed");

    esp_bt_gap_register_callback(bt_player_gap_cb);
    esp_bt_gap_set_device_name(BT_PLAYER_DEVICE_NAME);
    esp_bt_cod_t cod = {
        .major = ESP_BT_COD_MAJOR_DEV_AV,
        .minor = 0,
        .service = ESP_BT_COD_SRVC_AUDIO,
    };
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    ESP_RETURN_ON_ERROR(esp_avrc_ct_register_callback(bt_player_avrc_ct_cb), TAG_BT_PLAYER, "AVRCP CT cb failed");
    ESP_RETURN_ON_ERROR(esp_avrc_ct_init(), TAG_BT_PLAYER, "AVRCP CT init failed");
    ESP_RETURN_ON_ERROR(esp_avrc_tg_register_callback(bt_player_avrc_tg_cb), TAG_BT_PLAYER, "AVRCP TG cb failed");
    ESP_RETURN_ON_ERROR(esp_avrc_tg_init(), TAG_BT_PLAYER, "AVRCP TG init failed");
    ESP_RETURN_ON_ERROR(esp_a2d_register_callback(bt_player_a2dp_cb), TAG_BT_PLAYER, "A2DP cb failed");
    ESP_RETURN_ON_ERROR(esp_a2d_sink_init(), TAG_BT_PLAYER, "A2DP sink init failed");
    ESP_RETURN_ON_ERROR(esp_a2d_sink_register_data_callback(bt_player_a2dp_data_cb), TAG_BT_PLAYER, "A2DP data cb failed");
    ESP_RETURN_ON_ERROR(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE),
                        TAG_BT_PLAYER,
                        "BT scan mode failed");
    bt_ready = true;

    ESP_LOGI(TAG_BT_PLAYER, "BT address: %02x:%02x:%02x:%02x:%02x:%02x",
             esp_bt_dev_get_address()[0],
             esp_bt_dev_get_address()[1],
             esp_bt_dev_get_address()[2],
             esp_bt_dev_get_address()[3],
             esp_bt_dev_get_address()[4],
             esp_bt_dev_get_address()[5]);

    return ESP_OK;
}
