#ifndef SIMPLE_EEPROM_H
#define SIMPLE_EEPROM_H

#include <Arduino.h>

#if defined(ESP8266)
  #include <EEPROM.h>
#elif defined(ESP32)
  #include <EEPROM.h>
#else
  #error "This library only supports ESP8266 and ESP32"
#endif

class SimpleEEPROM {
  public:
    // Gọi trong setup()
    static void begin(size_t size = 512) {
      EEPROM.begin(size);
    }

    // ====== WRITE ANY DATA TYPE ======
    template <typename T>
    static void write(int address, const T &data) {
      const uint8_t* ptr = (const uint8_t*)(const void*)&data;
      for (size_t i = 0; i < sizeof(T); i++) {
        EEPROM.write(address + i, *ptr++);
      }
      EEPROM.commit();  // ESP bắt buộc
    }

    // ====== READ ANY DATA TYPE ======
    template <typename T>
    static void read(int address, T &data) {
      uint8_t* ptr = (uint8_t*)(void*)&data;
      for (size_t i = 0; i < sizeof(T); i++) {
        *ptr++ = EEPROM.read(address + i);
      }
    }

    // ====== WRITE STRING ======
    static void writeString(int address, const String &str, size_t maxLen) {
      for (size_t i = 0; i < maxLen; i++) {
        if (i < str.length())
          EEPROM.write(address + i, str[i]);
        else
          EEPROM.write(address + i, 0);
      }
      EEPROM.commit();
    }

    // ====== READ STRING ======
    static String readString(int address, size_t maxLen) {
      char data[maxLen + 1];
      for (size_t i = 0; i < maxLen; i++) {
        data[i] = EEPROM.read(address + i);
      }
      data[maxLen] = '\0';
      return String(data);
    }
};

#endif
