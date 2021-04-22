/**********************************************************************
  Filename    : Bluetooth Music By PCM5102
  Description : Streaming music via Bluetooth on one core while running 
  an LED strip on the other
  Auther      : Myric
  Modification: 2021/04/21
**********************************************************************/
#include "BluetoothSerial.h"
#include "driver/i2s.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "Freenove_WS2812_Lib_for_ESP32.h"
#include <Ticker.h>

#define LEDS_COUNT  8  // The number of led
#define LEDS_PIN    14  // define the pin connected to the Freenove 8 led strip
#define CHANNEL     0  // RMT module channel

#define CONFIG_I2S_LRCK_PIN 25
#define CONFIG_I2S_BCK_PIN  26
#define CONFIG_I2S_DATA_PIN 22
BluetoothSerial SerialBT;

// LED section ----
TaskHandle_t ledTaskHandle = NULL;
//Ticker ledTicker;
/** Flag if task should run */
//bool tasksEnabled = false;

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

int m_color[5][3] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {0, 0, 0} };
int delayval = 100;

int nowButtonState; // creating as global so not declared in standalone function

int relayPin = 13;          // the number of the relay pin
int buttonPin = 15;         // the number of the push button pin

int buttonState = HIGH;     // Record button state, and initial the state to high level
int relayState = LOW;       // Record relay state, and initial the state to low level
int lastButtonState = HIGH; // Record the button state of last detection
long lastChangeTime = 0;    // Record the time point for button state change
int runner = 0; //simple state checker

/**
 * triggerGoLED
 * 
 * called by Ticker ledTicker
 */
//void triggerGoLED() {
//  if (ledTaskHandle != NULL) {
//     xTaskResumeFromISR(ledTaskHandle);
//  }
//}

bool goPrettyColors() {

//  strip.setBrightness(6);

  while(1) {

//    if(!runner) {
//      strip.setBrightness(0);
//      vTaskSuspend(ledTaskHandle);
//    }
    
    for (int j = 0; j < 5; j++) {
      for (int i = 0; i < LEDS_COUNT; i++) {
        strip.setLedColorData(i, m_color[j][0], m_color[j][1], m_color[j][2]);// Set color data.
        strip.show();   // Send color data to LED, and display.
        delay(delayval);// Interval time of each LED.
      }
      delay(250);       // Interval time of each group of colors.
    }
  }
}

// END LED section ----

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32");
  Serial.println("Init success!");

//  LED Section ---

  strip.begin();
  strip.setBrightness(6);

  xTaskCreatePinnedToCore(
      ledTask,                       /* Function to implement the task */
      "ledTask ",                    /* Name of the task */
      4000,                           /* Stack size in words */
      NULL,                           /* Task input parameter */
      5,                              /* Priority of the task */
      &ledTaskHandle,                /* Task handle. */
      1);                             /* Core where the task should run */

  Serial.println("created task");

  if (ledTaskHandle == NULL) {
    Serial.println("Failed to start task");
//  } else {
//    // Trigger led to run every 10 seconds.
//    ledTicker.attach(10, triggerGoLED);
  }

  pinMode(buttonPin, INPUT_PULLUP);           // Set push button pin into input mode
  pinMode(relayPin, OUTPUT);                  // Set relay pin into output mode
  digitalWrite(relayPin, relayState);         // Set the initial state of relay into "off"
  
//  END LED Section ---

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  
  i2s_config_t i2s_config;
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  i2s_config.sample_rate = 44100;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_I2S_MSB;
  i2s_config.intr_alloc_flags = 0;
  i2s_config.dma_buf_count = 6;
  i2s_config.dma_buf_len = 60;
  i2s_config.tx_desc_auto_clear = true;
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  
  i2s_pin_config_t pin_config;
  pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
  pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
  pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
  pin_config.data_in_num = -1;
  i2s_set_pin(I2S_NUM_0, &pin_config);

  bt_app_task_start_up();
  
 /* initialize A2DP sink */
  esp_a2d_register_callback(&bt_app_a2d_cb);
  esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
  esp_a2d_sink_init();
  /* initialize AVRCP controller */
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
  /* set discoverable and connectable mode, wait to be connected */
  esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
  Serial.println("ok");
}

void loop(){

  nowButtonState = digitalRead(buttonPin);// Read current state of button pin
  // If button pin state has changed, record the time point
  if (nowButtonState != lastButtonState) {
    lastChangeTime = millis();
  }
  // If button state changes, and stays stable for a while, then it should have skipped the bounce area
  if (millis() - lastChangeTime > 10) {
    if (buttonState != nowButtonState) {    // Confirm button state has changed
      buttonState = nowButtonState;
      if (buttonState == LOW) {             // Low level indicates the button is pressed
        relayState = !relayState;           // Reverse relay state
        digitalWrite(relayPin, relayState); // Update relay state
        if(!runner) {
          runner = 1;
          if (ledTaskHandle != NULL) {
            vTaskResume(ledTaskHandle);
          }
        } else {
          strip.setBrightness(0);
          vTaskSuspend(ledTaskHandle);
          runner = 0;
        }
      }
    }
  }
  lastButtonState = nowButtonState; // Save the state of last button

//  if (!tasksEnabled) {
//    // Enable task that will read values from the DHT sensor
//    tasksEnabled = true;
//    if (ledTaskHandle != NULL) {
//      vTaskResume(ledTaskHandle);
//    }
//  }
  
}

void ledTask(void *pvParameters) {
  Serial.println("ledTask loop started");

  goPrettyColors();
  
//  while (1) // ledTask loop
//  {
//    if (tasksEnabled) {
//      // Run the thing
//      goPrettyColors();
//    }
//    // Got sleep again
//    vTaskSuspend(NULL);
//  }
}
