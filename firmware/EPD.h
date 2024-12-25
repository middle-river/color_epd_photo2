// E-ink rendering library for 7.3 inch 7-color e-Paper.
// 2024-12-01  T. Nakagawa

#ifndef EPD_H_
#define EPD_H_

#include <SPI.h>

class EPD {
public:
  static constexpr const int WIDTH = 800;
  static constexpr const int HEIGHT = 480;

  enum Color {
    BLACK  = 0b000,
    WHITE  = 0b001,
    GREEN  = 0b010,
    BLUE   = 0b011,
    RED    = 0b100,
    YELLOW = 0b101,
    ORANGE = 0b110,
  };

  EPD(int busy_pin, int rst_pin, int dc_pin, int cs_pin) : busy_pin_(busy_pin), rst_pin_(rst_pin), dc_pin_(dc_pin), cs_pin_(cs_pin) {
  }

  ~EPD() {
  }

  void begin() {
    pinMode(busy_pin_, INPUT); 
    pinMode(rst_pin_, OUTPUT);
    pinMode(dc_pin_, OUTPUT);
    pinMode(cs_pin_, OUTPUT);
    digitalWrite(rst_pin_, HIGH);
    digitalWrite(cs_pin_, HIGH);
    SPI.begin();
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    // HW reset.
    digitalWrite(rst_pin_, LOW);
    delay(2);
    digitalWrite(rst_pin_, HIGH);
    delay(20);
    wait();
    delay(30);

    command(0xaa, 6, (const uint8_t []){0x49, 0x55, 0x20, 0x08, 0x09, 0x18});  //CMDH.
    command(0x01, 6, (const uint8_t []){0x3f, 0x00, 0x32, 0x2a, 0x0e, 0x2a});  // Power Setting (PWR)?
    command(0x00, 2, (const uint8_t []){0x5f, 0x69});  // Panel Setting (PSR)?
    command(0x03, 4, (const uint8_t []){0x00, 0x54, 0x00, 0x44});  // Power Off Sequence Setting (PFS)?
    command(0x05, 4, (const uint8_t []){0x40, 0x1f, 0x1f, 0x2c});  // Power ON Measure (PMES)?
    command(0x06, 4, (const uint8_t []){0x6f, 0x1f, 0x1f, 0x22});  // Booster Soft Start (BTST)?
    command(0x08, 4, (const uint8_t []){0x6f, 0x1f, 0x1f, 0x22});
    command(0x13, 2, (const uint8_t []){0x00, 0x04});  // IPC.
    command(0x30, 1, (const uint8_t []){0x3c});  // PLL Control (PLL)?
    command(0x41, 1, (const uint8_t []){0x00});  // Temperature Sensor (TSE).
    command(0x50, 1, (const uint8_t []){0x3f});  // VCOM and DATA Interval Setting (CDI)?
    command(0x60, 2, (const uint8_t []){0x02, 0x00});  // TCON Setting (TCON)?
    command(0x61, 4, (const uint8_t []){0x03, 0x20, 0x01, 0xe0});  // Resolution Setting (TRES)?
    command(0x82, 1, (const uint8_t []){0x1e});  // VCOM_DC Setting (VDCS)?
    command(0x84, 1, (const uint8_t []){0x00});  // Data Start Transmission (DTM)?
    command(0x86, 1, (const uint8_t []){0x00});  // AGID.
    command(0xe3, 1, (const uint8_t []){0x2f});  // Power Saving (PWS)?
    command(0xe0, 1, (const uint8_t []){0x00});  // Cascade Setting (CCSET).
    command(0xe6, 1, (const uint8_t []){0x00});  // Force Temperature (TSSET).
    command(0x10, 0, nullptr);  // Data Start Transmission (DTM)?
  }

  void write(uint8_t *data, int size) {
    digitalWrite(dc_pin_, HIGH);
    digitalWrite(cs_pin_, LOW);
    SPI.transfer(data, size);
    digitalWrite(cs_pin_, HIGH);
  }

  void end() {
    command(0x04, 0, nullptr);  // Power ON.
    wait();
    command(0x12, 1, (const uint8_t []){0x00});  // Display Refresh (DRF).
    wait();
    command(0x02, 1, (const uint8_t []){0x00});  // Power OFF (POF).
    wait();

    command(0x07, 1, (const uint8_t []){0xa5});  // Deep Sleep Command (DSLP).

    SPI.endTransaction();
    SPI.end();
    digitalWrite(rst_pin_, LOW);
    digitalWrite(dc_pin_, LOW);
    digitalWrite(cs_pin_, LOW);
    pinMode(rst_pin_, INPUT);
    pinMode(dc_pin_, INPUT);
    pinMode(cs_pin_, INPUT);
  }

private:
  int busy_pin_;
  int rst_pin_;
  int dc_pin_;
  int cs_pin_;

  void command(uint8_t cmd, int size, const uint8_t *data) {
    digitalWrite(dc_pin_, LOW);
    digitalWrite(cs_pin_, LOW);
    SPI.transfer(cmd);
    digitalWrite(cs_pin_, HIGH);

    for (int i = 0; i < size; i++) {
      digitalWrite(dc_pin_, HIGH);
      digitalWrite(cs_pin_, LOW);
      SPI.transfer(data[i]);
      digitalWrite(cs_pin_, HIGH);
    }
  }

  void wait() {
    for (int i = 0; i < 50000; i++) {
      delay(10);
      if (digitalRead(busy_pin_) == HIGH) break;
    }
  }
};

#endif
