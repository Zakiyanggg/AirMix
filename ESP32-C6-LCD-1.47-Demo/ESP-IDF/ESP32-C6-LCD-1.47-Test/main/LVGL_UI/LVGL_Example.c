#include "LVGL_Example.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#define KEY_ACTIVE_LEVEL 0  // The key module outputs low level when pressed.
#define KEY_COUNT 4

#define KEY1_GPIO GPIO_NUM_0
#define KEY2_GPIO GPIO_NUM_1
#define KEY3_GPIO GPIO_NUM_2
#define KEY4_GPIO GPIO_NUM_3

#define MPU6050_SDA_GPIO GPIO_NUM_9
#define MPU6050_SCL_GPIO GPIO_NUM_18
#define MPU6050_INT_GPIO GPIO_NUM_19
#define MPU6050_I2C_ADDR 0x68  // AD0 to GND. Use 0x69 if AD0 is tied to 3V3.
#define MPU6050_I2C_FREQ_HZ 400000

#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define IMU_BAR_MIN -17000
#define IMU_BAR_MAX 17000

static const gpio_num_t key_gpios[KEY_COUNT] = {
  KEY1_GPIO,
  KEY2_GPIO,
  KEY3_GPIO,
  KEY4_GPIO,
};

static lv_obj_t * key_dots[KEY_COUNT];
static lv_obj_t * imu_status_label;
static lv_obj_t * imu_bars[2][3];
static lv_timer_t * ui_update_timer;

static lv_style_t style_text_muted;
static lv_style_t style_title;
static lv_style_t style_pressed;
static lv_style_t style_released;
static lv_style_t style_dot;
static lv_style_t style_dot_pressed;
static lv_style_t style_dot_released;

static i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t mpu6050_handle;
static bool mpu6050_ready;

static const lv_font_t * font_large;
static const lv_font_t * font_normal;

static void key_gpio_init(void);
static void mpu6050_init(void);
static esp_err_t mpu6050_write_reg(uint8_t reg, uint8_t value);
static esp_err_t mpu6050_read_regs(uint8_t start_reg, uint8_t * data, size_t len);
static void input_test_create(lv_obj_t * parent);
static void ui_update_timer_cb(lv_timer_t * t);
static int16_t read_i16_be(const uint8_t * data);
static void create_imu_bar_row(lv_obj_t * parent, const char * name, lv_coord_t y, lv_obj_t ** bars);
static void set_imu_bar_value(lv_obj_t * bar, int16_t value);
static lv_color_t imu_value_to_color(int16_t value);

void Lvgl_Example1(void)
{
  font_large = LV_FONT_DEFAULT;
  font_normal = LV_FONT_DEFAULT;

  lv_obj_clean(lv_scr_act());

  #if LV_FONT_MONTSERRAT_16
    font_large = &lv_font_montserrat_16;
  #else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
  #endif
  #if LV_FONT_MONTSERRAT_12
    font_normal = &lv_font_montserrat_12;
  #else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
  #endif

  key_gpio_init();
  mpu6050_init();

  lv_style_init(&style_text_muted);
  lv_style_set_text_opa(&style_text_muted, LV_OPA_90);

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title, font_normal);

  lv_style_init(&style_pressed);
  lv_style_set_text_color(&style_pressed, lv_palette_main(LV_PALETTE_GREEN));

  lv_style_init(&style_released);
  lv_style_set_text_color(&style_released, lv_palette_main(LV_PALETTE_GREY));

  lv_style_init(&style_dot);
  lv_style_set_radius(&style_dot, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&style_dot, 0);

  lv_style_init(&style_dot_pressed);
  lv_style_set_bg_color(&style_dot_pressed, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_bg_opa(&style_dot_pressed, LV_OPA_COVER);

  lv_style_init(&style_dot_released);
  lv_style_set_bg_color(&style_dot_released, lv_palette_main(LV_PALETTE_GREY));
  lv_style_set_bg_opa(&style_dot_released, LV_OPA_COVER);

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);
  lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

  input_test_create(lv_scr_act());

  ui_update_timer = lv_timer_create(ui_update_timer_cb, 100, NULL);
  ui_update_timer_cb(ui_update_timer);
}

void Lvgl_Example1_close(void)
{
  lv_anim_del(NULL, NULL);

  if(ui_update_timer) {
    lv_timer_del(ui_update_timer);
    ui_update_timer = NULL;
  }

  lv_obj_clean(lv_scr_act());

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_pressed);
  lv_style_reset(&style_released);
  lv_style_reset(&style_dot);
  lv_style_reset(&style_dot_pressed);
  lv_style_reset(&style_dot_released);
}

static void key_gpio_init(void)
{
  uint64_t pin_mask = 0;

  for (int i = 0; i < KEY_COUNT; i++) {
    pin_mask |= 1ULL << key_gpios[i];
  }

  gpio_config_t io_conf = {
    .pin_bit_mask = pin_mask,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static void mpu6050_init(void)
{
  gpio_config_t int_conf = {
    .pin_bit_mask = 1ULL << MPU6050_INT_GPIO,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&int_conf));

  i2c_master_bus_config_t bus_config = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = MPU6050_SDA_GPIO,
    .scl_io_num = MPU6050_SCL_GPIO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };

  esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
  if (ret != ESP_OK) {
    mpu6050_ready = false;
    return;
  }

  i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = MPU6050_I2C_ADDR,
    .scl_speed_hz = MPU6050_I2C_FREQ_HZ,
  };

  ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &mpu6050_handle);
  if (ret != ESP_OK) {
    mpu6050_ready = false;
    return;
  }

  ret = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00);
  mpu6050_ready = (ret == ESP_OK);
}

static esp_err_t mpu6050_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t data[] = {reg, value};
  return i2c_master_transmit(mpu6050_handle, data, sizeof(data), 100);
}

static esp_err_t mpu6050_read_regs(uint8_t start_reg, uint8_t * data, size_t len)
{
  return i2c_master_transmit_receive(mpu6050_handle, &start_reg, 1, data, len, 100);
}

static void input_test_create(lv_obj_t * parent)
{
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(parent, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_t * panel = lv_obj_create(parent);
  lv_obj_set_grid_cell(panel, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_style_pad_all(panel, 4, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * key_bar = lv_obj_create(panel);
  lv_obj_remove_style_all(key_bar);
  lv_obj_set_pos(key_bar, 0, 0);
  lv_obj_set_size(key_bar, LV_PCT(100), 22);

  for (int i = 0; i < KEY_COUNT; i++) {
    lv_obj_t * name = lv_label_create(key_bar);
    lv_label_set_text_fmt(name, "PB%d", i + 1);
    lv_obj_set_pos(name, i * 42, 2);

    key_dots[i] = lv_obj_create(key_bar);
    lv_obj_add_style(key_dots[i], &style_dot, 0);
    lv_obj_add_style(key_dots[i], &style_dot_released, 0);
    lv_obj_set_size(key_dots[i], 10, 10);
    lv_obj_set_pos(key_dots[i], i * 42 + 28, 5);
  }

  imu_status_label = lv_label_create(key_bar);
  lv_obj_add_style(imu_status_label, &style_text_muted, 0);
  lv_label_set_text(imu_status_label, "Gyro NOT OK");
  lv_obj_align(imu_status_label, LV_ALIGN_TOP_RIGHT, 0, 2);

  create_imu_bar_row(panel, "A", 56, imu_bars[0]);
  create_imu_bar_row(panel, "G", 94, imu_bars[1]);
}

static void create_imu_bar_row(lv_obj_t * parent, const char * name, lv_coord_t y, lv_obj_t ** bars)
{
  lv_obj_t * row_obj = lv_obj_create(parent);
  lv_obj_remove_style_all(row_obj);
  lv_obj_set_pos(row_obj, 0, y);
  lv_obj_set_size(row_obj, LV_PCT(100), 26);

  static lv_coord_t col_dsc[] = {
    12,
    10, LV_GRID_FR(1),
    10, LV_GRID_FR(1),
    10, LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(row_obj, col_dsc, row_dsc);

  lv_obj_t * row_name = lv_label_create(row_obj);
  lv_label_set_text(row_name, name);
  lv_obj_add_style(row_name, &style_text_muted, 0);
  lv_obj_set_grid_cell(row_name, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  const char * axes[] = {"X", "Y", "Z"};
  for (int i = 0; i < 3; i++) {
    lv_obj_t * axis = lv_label_create(row_obj);
    lv_label_set_text(axis, axes[i]);
    lv_obj_set_grid_cell(axis, LV_GRID_ALIGN_START, 1 + i * 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    bars[i] = lv_bar_create(row_obj);
    lv_bar_set_range(bars[i], IMU_BAR_MIN, IMU_BAR_MAX);
    lv_bar_set_value(bars[i], 0, LV_ANIM_OFF);
    lv_obj_set_size(bars[i], LV_PCT(100), 7);
    lv_obj_set_style_bg_color(bars[i], lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bars[i], LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_radius(bars[i], 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bars[i], 4, LV_PART_INDICATOR);
    set_imu_bar_value(bars[i], 0);
    lv_obj_set_grid_cell(bars[i], LV_GRID_ALIGN_STRETCH, 2 + i * 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  }
}

static void ui_update_timer_cb(lv_timer_t * t)
{
  LV_UNUSED(t);

  for (int i = 0; i < KEY_COUNT; i++) {
    int level = gpio_get_level(key_gpios[i]);
    bool pressed = (level == KEY_ACTIVE_LEVEL);

    lv_obj_remove_style(key_dots[i], &style_dot_pressed, 0);
    lv_obj_remove_style(key_dots[i], &style_dot_released, 0);
    lv_obj_add_style(key_dots[i], pressed ? &style_dot_pressed : &style_dot_released, 0);
  }

  if (!mpu6050_ready) {
    lv_label_set_text(imu_status_label, "Gyro NOT OK");
    for (int axis = 0; axis < 3; axis++) {
      set_imu_bar_value(imu_bars[0][axis], 0);
      set_imu_bar_value(imu_bars[1][axis], 0);
    }
    return;
  }

  uint8_t data[14] = {0};
  esp_err_t ret = mpu6050_read_regs(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));
  if (ret != ESP_OK) {
    lv_label_set_text(imu_status_label, "Gyro NOT OK");
    return;
  }

  int16_t ax = read_i16_be(&data[0]);
  int16_t ay = read_i16_be(&data[2]);
  int16_t az = read_i16_be(&data[4]);
  int16_t gx = read_i16_be(&data[8]);
  int16_t gy = read_i16_be(&data[10]);
  int16_t gz = read_i16_be(&data[12]);

  lv_label_set_text(imu_status_label, "Gyro OK");
  set_imu_bar_value(imu_bars[0][0], ax);
  set_imu_bar_value(imu_bars[0][1], ay);
  set_imu_bar_value(imu_bars[0][2], az);
  set_imu_bar_value(imu_bars[1][0], gx);
  set_imu_bar_value(imu_bars[1][1], gy);
  set_imu_bar_value(imu_bars[1][2], gz);
}

static int16_t read_i16_be(const uint8_t * data)
{
  return (int16_t)((data[0] << 8) | data[1]);
}

static void set_imu_bar_value(lv_obj_t * bar, int16_t value)
{
  if (value < IMU_BAR_MIN) {
    value = IMU_BAR_MIN;
  } else if (value > IMU_BAR_MAX) {
    value = IMU_BAR_MAX;
  }

  lv_bar_set_value(bar, value, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, imu_value_to_color(value), LV_PART_INDICATOR);
  lv_obj_invalidate(bar);
}

static lv_color_t imu_value_to_color(int16_t value)
{
  int32_t clamped = value;
  if (clamped < IMU_BAR_MIN) {
    clamped = IMU_BAR_MIN;
  } else if (clamped > IMU_BAR_MAX) {
    clamped = IMU_BAR_MAX;
  }

  uint8_t red = (uint8_t)(((clamped - IMU_BAR_MIN) * 255) / (IMU_BAR_MAX - IMU_BAR_MIN));
  uint8_t green = 255 - red;
  return lv_color_make(red, green, 0);
}
