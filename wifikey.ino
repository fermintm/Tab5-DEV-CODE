/*\
 *
 * Thanks Tobozo for m5unified-rtc.hpp ;)
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <lvgl.h>
#include <Preferences.h>
#include <Wire.h>
#include "m5unified-rtc.hpp"

// ============================= DEFINICIONES =============================
#define EXAMPLE_LCD_H_RES 720
#define EXAMPLE_LCD_V_RES 1280
#define LVGL_LCD_BUF_SIZE (EXAMPLE_LCD_H_RES * 40)

#define SDIO2_CLK GPIO_NUM_12
#define SDIO2_CMD GPIO_NUM_13
#define SDIO2_D0 GPIO_NUM_11
#define SDIO2_D1 GPIO_NUM_10
#define SDIO2_D2 GPIO_NUM_9
#define SDIO2_D3 GPIO_NUM_8
#define SDIO2_RST GPIO_NUM_15

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

lv_obj_t *ssid_input, *pass_input, *keyboard, *status_label;
lv_obj_t *main_screen, *wifi_screen, *config_screen;
lv_obj_t *btn_wifi, *btn_config, *btn_back_wifi, *btn_back_config;
lv_obj_t *bat_label, *slider_label;
lv_obj_t *btn_carga, *label_carga;

// Labels para fecha y hora
lv_obj_t *rtc_date_label;
lv_obj_t *rtc_time_label;

String ssid = "";
String password = "";

Preferences preferences;

// ============================= CONTROL DE CARGA =============================
enum EstadoCarga { ACTIVADA, DESACTIVADA };
EstadoCarga estadoCarga = ACTIVADA;

// ============================= RTC =============================
m5::RTC8130_Class Tab5Rtc;
bool rtc_ok = false;

// ============================= LVGL DISPLAY HANDLER =============================
void lv_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  if (!esp_ptr_external_ram(color_p)) {
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  } else {
    M5.Display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  }
  lv_disp_flush_ready(disp);
}

static void lv_indev_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  lgfx::touch_point_t tp[3];
  if (M5.Display.getTouchRaw(tp, 3)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = tp[0].x;
    data->point.y = tp[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ============================= WIFI CALLBACKS =============================
void connect_wifi_cb(lv_event_t *e) {
  ssid = lv_textarea_get_text(ssid_input);
  password = lv_textarea_get_text(pass_input);

  lv_label_set_text_fmt(status_label, "Conectando a %s...", ssid.c_str());

  WiFi.begin(ssid.c_str(), password.c_str());

  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.end();
}

// ============================= CALLBACKS DE NAVEGACIÓN =============================
void goto_wifi_cb(lv_event_t *e) {
  lv_scr_load(wifi_screen);
}
void goto_config_cb(lv_event_t *e) {
  lv_scr_load(config_screen);
}
void back_to_main_from_wifi(lv_event_t *e) {
  WiFi.disconnect(true, false);
  lv_label_set_text(status_label, "Estado: desconectado");
  lv_scr_load(main_screen);
}
void back_to_main_from_config(lv_event_t *e) {
  lv_scr_load(main_screen);
}

// ============================= CALLBACKS CONFIG =============================
void slider_event_cb(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  int value = lv_slider_get_value(slider);
  M5.Display.setBrightness(value);
  lv_label_set_text_fmt(slider_label, "Brillo: %d", value);
}

// ============================= CONTROL DE CARGA CALLBACK =============================
void cambiar_estado_cb(lv_event_t *e) {
  if (estadoCarga == ACTIVADA) {
    estadoCarga = DESACTIVADA;
    lv_label_set_text(label_carga, "DESACTIVADA");
    lv_obj_set_style_bg_color(btn_carga, lv_color_hex(0x00FF00), LV_PART_MAIN); // Verde
    lv_obj_set_style_text_color(label_carga, lv_color_black(), LV_PART_MAIN);
  } else {
    estadoCarga = ACTIVADA;
    lv_label_set_text(label_carga, "ACTIVADA");
    lv_obj_set_style_bg_color(btn_carga, lv_color_hex(0xFFA500), LV_PART_MAIN); // Naranja
    lv_obj_set_style_text_color(label_carga, lv_color_black(), LV_PART_MAIN);
  }

  Serial.print("Estado de carga: ");
  Serial.println(estadoCarga);
}

// ============================= PANTALLAS =============================
void create_main_screen() {
  main_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x111111), LV_PART_MAIN);

  btn_wifi = lv_btn_create(main_screen);
  lv_obj_set_size(btn_wifi, 200, 80);
  lv_obj_align(btn_wifi, LV_ALIGN_TOP_LEFT, 20, 20);
  lv_obj_set_style_bg_color(btn_wifi, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_t *label_wifi = lv_label_create(btn_wifi);
  lv_label_set_text(label_wifi, "WIFI");
  lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_center(label_wifi);
  lv_obj_add_event_cb(btn_wifi, goto_wifi_cb, LV_EVENT_CLICKED, NULL);

  btn_config = lv_btn_create(main_screen);
  lv_obj_set_size(btn_config, 200, 80);
  lv_obj_align(btn_config, LV_ALIGN_TOP_RIGHT, -20, 20);
  lv_obj_set_style_bg_color(btn_config, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_t *label_config = lv_label_create(btn_config);
  lv_label_set_text(label_config, "CONFIG");
  lv_obj_set_style_text_font(label_config, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_center(label_config);
  lv_obj_add_event_cb(btn_config, goto_config_cb, LV_EVENT_CLICKED, NULL);

  // Labels RTC
  rtc_date_label = lv_label_create(main_screen);
  lv_label_set_text(rtc_date_label, "00/00/0000");
  lv_obj_set_style_text_font(rtc_date_label, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_style_text_color(rtc_date_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(rtc_date_label, LV_ALIGN_CENTER, 0, -20);

  rtc_time_label = lv_label_create(main_screen);
  lv_label_set_text(rtc_time_label, "00:00:00");
  lv_obj_set_style_text_font(rtc_time_label, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_style_text_color(rtc_time_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(rtc_time_label, LV_ALIGN_CENTER, 0, 20);
}

void create_wifi_screen() {
  wifi_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(wifi_screen, lv_color_black(), LV_PART_MAIN);

  status_label = lv_label_create(wifi_screen);
  lv_label_set_text(status_label, "Estado: no conectado");
  lv_obj_set_style_text_color(status_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 10, 10);

  btn_back_wifi = lv_btn_create(wifi_screen);
  lv_obj_set_size(btn_back_wifi, 200, 60);
  lv_obj_align(btn_back_wifi, LV_ALIGN_TOP_LEFT, 20, 80);
  lv_obj_set_style_bg_color(btn_back_wifi, lv_color_hex(0x555555), LV_PART_MAIN);
  lv_obj_t *label_back_wifi = lv_label_create(btn_back_wifi);
  lv_label_set_text(label_back_wifi, "Volver");
  lv_obj_set_style_text_font(label_back_wifi, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_center(label_back_wifi);
  lv_obj_add_event_cb(btn_back_wifi, back_to_main_from_wifi, LV_EVENT_CLICKED, NULL);

  lv_obj_t *container = lv_obj_create(wifi_screen);
  lv_obj_set_size(container, 700, 300);
  lv_obj_align(container, LV_ALIGN_CENTER, 0, -80);
  lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);

  ssid_input = lv_textarea_create(container);
  lv_obj_set_width(ssid_input, 680);
  lv_obj_set_style_text_font(ssid_input, &lv_font_montserrat_30, LV_PART_MAIN);
  lv_textarea_set_placeholder_text(ssid_input, "SSID");
  lv_obj_align(ssid_input, LV_ALIGN_TOP_MID, 0, -20);
  lv_textarea_set_one_line(ssid_input, true);
  lv_obj_set_style_bg_color(ssid_input, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_color(ssid_input, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_color(ssid_input, lv_color_white(), LV_PART_MAIN);

  pass_input = lv_textarea_create(container);
  lv_obj_set_width(pass_input, 680);
  lv_obj_set_style_text_font(pass_input, &lv_font_montserrat_30, LV_PART_MAIN);
  lv_textarea_set_placeholder_text(pass_input, "PASSWORD");
  lv_obj_align(pass_input, LV_ALIGN_TOP_MID, 0, 50);
  lv_textarea_set_one_line(pass_input, true);
  lv_textarea_set_password_mode(pass_input, true);
  lv_obj_set_style_bg_color(pass_input, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_color(pass_input, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_color(pass_input, lv_color_white(), LV_PART_MAIN);

  lv_obj_t *btn_connect = lv_btn_create(container);
  lv_obj_set_size(btn_connect, 300, 60);
  lv_obj_align(btn_connect, LV_ALIGN_TOP_MID, 0, 125);
  lv_obj_set_style_bg_color(btn_connect, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_text_color(btn_connect, lv_color_white(), LV_PART_MAIN);
  lv_obj_t *label_btn = lv_label_create(btn_connect);
  lv_label_set_text(label_btn, "Conectar WiFi");
  lv_obj_set_style_text_font(label_btn, &lv_font_montserrat_30, LV_PART_MAIN);
  lv_obj_center(label_btn);
  lv_obj_add_event_cb(btn_connect, connect_wifi_cb, LV_EVENT_CLICKED, NULL);

  keyboard = lv_keyboard_create(wifi_screen);
  lv_obj_set_size(keyboard, 1100, 300);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboard, ssid_input);

  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x222222), LV_PART_MAIN);
  lv_obj_set_style_border_width(keyboard, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x444444), LV_PART_ITEMS);
  lv_obj_set_style_text_color(keyboard, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_30, LV_PART_ITEMS);
  lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(keyboard, lv_color_hex(0x666666), LV_PART_ITEMS);

  lv_obj_add_event_cb(ssid_input, [](lv_event_t *e) {
    lv_keyboard_set_textarea(keyboard, ssid_input);
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(pass_input, [](lv_event_t *e) {
    lv_keyboard_set_textarea(keyboard, pass_input);
  }, LV_EVENT_CLICKED, NULL);
}

// ============================= CONFIG SCREEN =============================
void create_config_screen() {
  config_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(config_screen, lv_color_hex(0x111111), LV_PART_MAIN);

  btn_back_config = lv_btn_create(config_screen);
  lv_obj_set_size(btn_back_config, 200, 60);
  lv_obj_align(btn_back_config, LV_ALIGN_TOP_LEFT, 20, 20);
  lv_obj_set_style_bg_color(btn_back_config, lv_color_hex(0x555555), LV_PART_MAIN);
  lv_obj_t *label_back_config = lv_label_create(btn_back_config);
  lv_label_set_text(label_back_config, "Volver");
  lv_obj_set_style_text_font(label_back_config, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_center(label_back_config);
  lv_obj_add_event_cb(btn_back_config, back_to_main_from_config, LV_EVENT_CLICKED, NULL);

  lv_obj_t *slider = lv_slider_create(config_screen);
  lv_obj_set_size(slider, 400, 40);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 100);
  lv_slider_set_range(slider, 0, 255);
  lv_slider_set_value(slider, 200, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x00FFFF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x00FFFF), LV_PART_KNOB);

  slider_label = lv_label_create(config_screen);
  lv_label_set_text_fmt(slider_label, "Brillo: %d", lv_slider_get_value(slider));
  lv_obj_align(slider_label, LV_ALIGN_TOP_MID, 0, 150);
  lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  bat_label = lv_label_create(config_screen);
  lv_label_set_text(bat_label, "Cargando info...");
  lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_align(bat_label, LV_ALIGN_CENTER, 0, 40);

  // =================== BOTÓN DE CARGA ===================
  btn_carga = lv_btn_create(config_screen);
  lv_obj_set_size(btn_carga, 300, 80);
  lv_obj_align(btn_carga, LV_ALIGN_CENTER, 0, 140);
  lv_obj_set_style_bg_color(btn_carga, lv_color_hex(0xFFA500), LV_PART_MAIN); // Naranja inicial
  label_carga = lv_label_create(btn_carga);
  lv_label_set_text(label_carga, "ACTIVADA");
  lv_obj_set_style_text_font(label_carga, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_carga, lv_color_black(), LV_PART_MAIN);
  lv_obj_center(label_carga);
  lv_obj_add_event_cb(btn_carga, cambiar_estado_cb, LV_EVENT_CLICKED, NULL);
}

// ============================= RTC =============================
void initRTC() {
  if (Tab5Rtc.begin()) {
    m5::rtc_datetime_t dt;
    if (Tab5Rtc.getDateTime(&dt)) {
      auto tm = dt.get_tm();
      Serial.printf("Got RTC Time: %04d/%02d/%02d %02d:%02d:%02d\n",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);
      Tab5Rtc.setSystemTimeFromRtc();
      rtc_ok = true;
    } else {
      Serial.println("Failed to get RTC Time");
    }
  } else {
    Serial.println("Tab5 RTC fail");
  }
}

// ============================= FORMATEO FECHA/HORA =============================
void update_rtc_label() {
  if (rtc_ok) {
    m5::rtc_datetime_t dt;
    if (Tab5Rtc.getDateTime(&dt)) {
      auto tm = dt.get_tm();
      char date_buf[16], time_buf[16];
      sprintf(date_buf, "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
      sprintf(time_buf, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
      lv_label_set_text(rtc_date_label, date_buf);
      lv_label_set_text(rtc_time_label, time_buf);
    }
  }
}

// ============================= INICIALIZACIÓN UI =============================
void ui_init() {
  create_main_screen();
  create_wifi_screen();
  create_config_screen();
  lv_scr_load(main_screen);
}

// ============================= SETUP / LOOP =============================
void setup() {
  auto cfg = M5.config();
  cfg.external_rtc = false;
  M5.begin(cfg);
  Serial.begin(115200);

  lv_init();

  buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf1 || !buf2) while (true);

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_LCD_BUF_SIZE);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = lv_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.sw_rotate = 1;
  disp_drv.rotated = LV_DISP_ROT_90;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_indev_read;
  lv_indev_drv_register(&indev_drv);

  initRTC();
  ui_init();
}

void loop() {
  M5.update();
  lv_timer_handler();

  // Actualiza reloj RTC en pantalla principal
  update_rtc_label();

  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 1000) {
    lastStatusUpdate = millis();

    // Estado WiFi
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      lv_label_set_text_fmt(status_label, "Conectado: %s", ip.toString().c_str());
    } else {
      lv_label_set_text(status_label, "Estado: no conectado");
    }

    // Información batería
    bool bat_ischarging = M5.Power.isCharging();
    int bat_vol = M5.Power.getBatteryVoltage();
    int bat_level = M5.Power.getBatteryLevel();

    lv_color_t bat_color;
    if (bat_level >= 80) bat_color = lv_color_make(0, 255, 0);
    else if (bat_level >= 50) bat_color = lv_color_make(255, 255, 0);
    else bat_color = lv_color_make(255, 0, 0);

    lv_obj_set_style_text_color(bat_label, bat_color, LV_PART_MAIN);
    lv_label_set_text_fmt(bat_label, "Bat Charging: %s\nBat Voltage: %dmv\nBat Level: %d%%",
                          bat_ischarging ? "Yes" : "No", bat_vol, bat_level);

    // Control de carga
    if (estadoCarga == ACTIVADA) {
      M5.Power.setBatteryCharge(true);
    } else {
      M5.Power.setBatteryCharge(false);
    }
  }

  delay(5);
}
