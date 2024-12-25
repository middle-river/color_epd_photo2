// 7.3 inch Color E-Paper Photo Frame.
// 2024-12-01  T. Nakagawa

#include <LittleFS.h>
#include <Preferences.h>
#include <SimpleFTPServer.h>
#include <WiFi.h>
#include <cstring>
#include <driver/rtc_io.h>
#include <soc/rtc_cntl_reg.h>
#include "EPD.h"
#include "GIF.h"
#include "WiFiConfig.h"

extern "C" int rom_phy_get_vdd33();

constexpr int PIN_BUSY = 16;
constexpr int PIN_RST = 17;
constexpr int PIN_DC = 21;
constexpr int PIN_CS = 22;
constexpr int PIN_SW = 0;
constexpr uint32_t font[10] = {0x25555520, 0x26222270, 0x25124470, 0x25121520, 0x55571110, 0x74461520, 0x25465520, 0x71122220, 0x25525520, 0x25531520};  // 4x8 font data for digits 0-9.
constexpr float VOLTAGE_ADJUST = 3.30f / 3.60f;
constexpr float SHUTDOWN_VOLTAGE = 2.30f;
constexpr int PATH_LEN = 32;

Preferences preferences;
FtpServer ftp;
EPD epd(PIN_BUSY, PIN_RST, PIN_DC, PIN_CS);
RTC_DATA_ATTR char photo_path[PATH_LEN];
RTC_DATA_ATTR int low_battery_counter;

float getVoltage() {
  btStart();
  int v = 0;
  for (int i = 0; i < 20; i++) v += rom_phy_get_vdd33();
  btStop();
  v /= 20;
  const float vdd =  (0.0005045f * v + 0.3368f) * VOLTAGE_ADJUST;
  return vdd;
}

void transfer() {
  auto ftp_callback = [](FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace) {
    switch (ftpOperation) {
    case FTP_CONNECT:
      Serial.println("Client connected.");
      break;
    case FTP_DISCONNECT:
      Serial.println("Client disconnected.");
      break;
    default:
      break;
    }
  };
  auto ftp_transfer_callback = [](FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize) {
    switch (ftpOperation) {
    case FTP_UPLOAD_START:
      Serial.println("File upload started.");
      break;
    case FTP_TRANSFER_STOP:
      Serial.println("File transfer finished.");
      break;
    default:
      break;
    }
  };

  Serial.println("Entering the transfer mode.");
  preferences.begin("config", true);
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to " + preferences.getString("SSID"));
  WiFi.begin(preferences.getString("SSID").c_str(), preferences.getString("PASS").c_str());
  preferences.end();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  LittleFS.begin(true);
  ftp.setCallback(ftp_callback);
  ftp.setTransferCallback(ftp_transfer_callback);
  ftp.begin();	// FTP for anonymous connection.
  while (digitalRead(PIN_SW) == HIGH) ftp.handleFTP();
  while (digitalRead(PIN_SW) == LOW) delay(10);

  Serial.println("Disconnected.");
  ftp.end();
  LittleFS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

String find_path(const char *dir, const String& path_old, int delta) {
  if (delta == 0) return path_old;
  String path_head, path_tail, path_prev, path_next;
  int num = 0;
  File root = LittleFS.open(dir);
  if (root) {
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
      num++;
      const String path = file.name();
      file.close();
      if (path.length() > PATH_LEN - 1) continue;
      if (!path.endsWith(".gif")) continue;
      if (path_head == "" || path < path_head) path_head = path;
      if (path_tail == "" || path > path_tail) path_tail = path;
      if (path < path_old && (path_prev == "" || path > path_prev)) path_prev = path;
      if (path > path_old && (path_next == "" || path < path_next)) path_next = path;
    }
    root.close();
  }
  Serial.println("Number of files: " + String(num));
  if (path_prev == "") path_prev = path_tail;
  if (path_next == "") path_next = path_head;
  const String path_new = (delta > 0) ? path_next : path_prev;
  if (delta > 0) delta--; else delta++;
  return find_path(dir, path_new, delta);
}

void display(int delta, float voltage) {
  LittleFS.begin();

  // Generate the bitmap for battery monitor.
  uint8_t bitmap[16 * 16 / 2];  // 16x16 pixels.
  const int digit0 = (int)(voltage * 10.0) % 10;
  const int digit1 = (int)(voltage * 100.0 + 0.5) % 10;
  uint32_t font0 = font[digit0];
  uint32_t font1 = (voltage >= 3.0f) ? 0x00000000 : font[digit1];  // Only one digit is shown when the voltage is high.
  for (int i = 0; i < 16; i++) {  // 4x8 dot font is rendered as 8x16 dot font.
    const int shift = 4 * (7 - i / 2);
    uint32_t bit = 0b1000;
    for (int j = 0; j < 4; j++) {
      bitmap[8 * i + 0 + j] = ((font0 >> shift) & bit) ? (EPD::Color::BLACK << 4 | EPD::Color::BLACK) : (EPD::Color::WHITE << 4 | EPD::Color::WHITE);
      bitmap[8 * i + 4 + j] = ((font1 >> shift) & bit) ? (EPD::Color::BLACK << 4 | EPD::Color::BLACK) : (EPD::Color::WHITE << 4 | EPD::Color::WHITE);
      bit >>= 1;
    }
  }
  int line = 0;
  auto callback = [bitmap, &line](uint8_t *data, int size) {  // Superimpose function.
    if (line < 16) std::memcpy(data + EPD::WIDTH / 2 - 8, bitmap + 8 * line, 8);
    epd.write(data, size);
    line++;
  };

  // Find the file to view.
  String path = String(photo_path);
  Serial.println("Previous photo: " + path);
  path = find_path("/", path, delta);
  path.toCharArray(photo_path, PATH_LEN);
  Serial.println("Drawing the photo: " + path);

  // Draw the picture.
  if (path != "") {
    epd.begin();
    File file = LittleFS.open(("/" + path).c_str(), "r");
    const int status = GIF::read(&file, EPD::WIDTH, EPD::HEIGHT, callback);
    file.close();
    epd.end();
    Serial.println("Status=" + String(status));
  }

  LittleFS.end();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brown-out detection.
  pinMode(PIN_SW, INPUT);
  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("7.3 inch Color E-Paper Photo Frame");

  // Check the battery voltage.
  const float voltage = getVoltage();
  Serial.println("Battery voltage: " + String(voltage));
  if (voltage < SHUTDOWN_VOLTAGE) {
    low_battery_counter++;
    if (low_battery_counter >= 10) {
      Serial.println("Shutting down due to low battery voltage.");
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
      esp_deep_sleep_start();
    }
  }
  low_battery_counter = 0;

  // Determine the mode.
  enum Mode {NONE, CONFIG, TRANSFER, FORWARD, BACKWARD} mode = NONE;
  const esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_TIMER) {
    mode = FORWARD;
  } else if (reason == ESP_SLEEP_WAKEUP_EXT0) {
    mode = FORWARD;
    while (digitalRead(PIN_SW) == LOW) {
      delay(10);
      if (millis() >= 2000) {
        mode = BACKWARD;
        break;
      }
    }
  } else {
    while (digitalRead(PIN_SW) == HIGH && millis() <= 3000) delay(10);
    if (digitalRead(PIN_SW) == LOW) {
      mode = TRANSFER;
      const uint32_t millis0 = millis();
      while (digitalRead(PIN_SW) == LOW) {
        delay(10);
        if (millis() - millis0 > 2000) {
          mode = CONFIG;
          break;
        }
      }
    }
  }
  Serial.println("Mode=" + String(mode));

  if (mode == CONFIG) {
    WiFiConfig::configure("config");
  } else if (mode == TRANSFER) {
    transfer();
  } else if (mode == FORWARD || mode == BACKWARD) {
    display((mode == FORWARD) ? 1 : -1, voltage);
  }

  // Deep sleep.
  preferences.begin("config", true);
  unsigned long long sleep = preferences.getString("SLEEP", "86400").toInt();
  preferences.end();
  Serial.println("Sleeping: " + String((unsigned long)sleep) + "sec.");
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_SW, LOW);
  esp_sleep_enable_timer_wakeup(sleep * 1000000);
  esp_deep_sleep_start();
}

void loop() {
}
