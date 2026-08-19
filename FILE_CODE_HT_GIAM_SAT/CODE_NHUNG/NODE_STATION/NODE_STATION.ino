#include <Wire.h>
#include <SPI.h>
#include "MPU6050_offset.h"
#include "SimpleEEPROM.h"
#include <nRF24L01.h>
#include <RF24.h>
#include "Image.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <CRC32.h>
extern "C" { 
  #include "user_interface.h"
}
#define CE_PIN      D4    
#define CSN_PIN     D3  
#define POWER_PIN   D8
#define BAT_PIN     A0
#define EEPROM_SIZE 64

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET -  1

#define MPU_DATAOK  0x00
#define MPU_TIMEOUT 0x01
#define WAKETIME 3000
#define MAX_RETRY   3        // số lần retry phần mềm
#define RETRY_DELAY 30       // ms giữa mỗi lần retry

/*======================== !!!! Phần Quan trọng - Chỉnh sửa lại cho phù hợp !!!! =======================*/
#define ID    0xD1            // mỗi trạm sẽ có 1 ID riêng - Khi thay đổi ID của trạm Node, cần thay đổi ID trong code trạm master
#define SLEEPTIME 60          // Thời gian ngủ ---> đơn vị phút  [SLEEPTIME = n --> ngủ n phút (n <= 100)]
/*======================== =============================================================================*/

const unsigned char icon_signal [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x30, 
	0x00, 0x0c, 0x23, 0x00, 0xc4, 0x62, 0x3c, 0x46, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 
	0x66, 0x22, 0x3c, 0x44, 0x33, 0x18, 0xcc, 0x30, 0x18, 0x0c, 0x10, 0x18, 0x08, 0x00, 0x18, 0x00, 
	0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_nosignal [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x30, 0x00, 0x00, 0x18, 0x00, 0x08, 0x3c, 
	0x00, 0x0c, 0x26, 0x00, 0xc4, 0x63, 0x1c, 0x46, 0x67, 0x86, 0x66, 0x66, 0xc6, 0x66, 0x66, 0x60, 
	0x66, 0x22, 0x30, 0x44, 0x33, 0x18, 0xcc, 0x30, 0x1c, 0x0c, 0x10, 0x1e, 0x08, 0x00, 0x1b, 0x00, 
	0x00, 0x19, 0x80, 0x00, 0x18, 0xc0, 0x00, 0x18, 0x60, 0x00, 0x18, 0x30, 0x00, 0x18, 0x18, 0x00, 
	0x00, 0x0c, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
/*=== ESP8266  <=>  NRF24L01 ===
        D8 - IO15       CSN
        D7 - IO13       MOSI
        D6 - IO12       MISO
        D5 - IO14       SCK
        D4 - IO2        CE
        X  - IO13       IRQ
================================*/
RF24 radio(CE_PIN, CSN_PIN);

MPU6050 mpu(Wire);
SimpleEEPROM eeprom;
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastTimeDeepSleep;        //Thời gian hoạt động trước khi ngủ lại
const byte Address_Master[6] = "00001";

float x, y, z;
//  Cấu trúc dữ liệu gửi đi: [ID]  [Status] [Data xyz + battery]  [CRC] 
//                          1byte   1byte           22byte        4byte

float battery;

void setup() { 
  Serial.begin(9600);
  Serial.println("\nESP Thức dậy") ;

  systems_init();
  digitalWrite(POWER_PIN, HIGH);  // Bật nguồn cho NRF24, MPU6050 & Màn hình OLED

  Wire.begin();
  mpu.wake();
  mpu.begin();
  nrf24_init();

  battery = readBatteryVoltage();

  if(getDataMPU6050()){
    if(sendDataToMaster(MPU_DATAOK,x, y, z, battery)) display_data(true, x, y, z);
    else                                              display_data(false, x, y, z);
  }
  else{
    sendDataToMaster(MPU_TIMEOUT, x, y, z, battery);
  }
  Serial.println("Pin: " + String(battery));
  

  delay(WAKETIME);
  oled.ssd1306_command(SSD1306_DISPLAYOFF);
  mpu.sleep();           
  digitalWrite(CE_PIN, LOW);
  radio.powerDown(); 
  digitalWrite(POWER_PIN, LOW);

  Serial.println("ESP Ngủ ZZZ") ;
  ESP.deepSleep((uint64_t)SLEEPTIME * 60ULL * 1000000ULL, WAKE_RF_DISABLED);
  //ESP.deepSleep(10 * 1e6, WAKE_RF_DISABLED);     // Chỉ để test - ESP ngủ trong 10s
}

void loop() {
}

bool sendDataToMaster(uint8_t status, float x, float y, float z, float battery) {

  /* ---------- 1. Tạo DATA cố định 17 byte ---------- */
  // Format: ±XX.X,±YY.Y,±ZZ.Z  (17 ký tự)
  char dataStr[23]; // 22 + '\0' = 23
  sprintf(dataStr, "%+05.1f,%+05.1f,%+05.1f,%.2f", x, y, z, battery);

  /* ---------- 2. Payload không CRC ---------- */
  uint8_t payload[24];  
  payload[0] = ID;         // ID node
  payload[1] = status;     // STATUS

  memcpy(payload + 2, dataStr, 22);  // DATA

  /* ---------- 3. CRC32 ---------- */
  uint32_t crc = CRC32::calculate(payload, 24);

  /* ---------- 4. Gói gửi cuối ---------- */
  uint8_t txBuf[28];
  memcpy(txBuf, payload, 24);
  memcpy(txBuf + 24, &crc, 4);

/* ---------- DEBUG ---------- */
  // Serial.print("STATUS: ");
  // Serial.println(status, HEX);
  // Serial.print("DATA: ");
  // Serial.println(dataStr);

  // Serial.print("TX HEX: ");
  // for (int i = 0; i < 23; i++) {
  //   if (txBuf[i] < 16) Serial.print("0");
  //   Serial.print(txBuf[i], HEX);
  //   Serial.print(" ");
  // }
  // Serial.println();
//
  radio.stopListening();
  /* ---------- RETRY LOOP ---------- */
  for (uint8_t attempt = 1; attempt <= MAX_RETRY; attempt++) {
    bool ok = radio.write(txBuf, 28);
    if (ok) {
      Serial.print("Send OK (try ");
      Serial.print(attempt);
      Serial.println(")");

      // nếu có ACK payload
      if (radio.isAckPayloadAvailable()) {
        char reply[10] = "";
        radio.read(reply, sizeof(reply));
        Serial.print("Master reply: ");
        Serial.println(reply);
      }
      return true;
    }
    Serial.print("Retry ");
    Serial.print(attempt);
    Serial.println(" - no ACK");
    delay(RETRY_DELAY);
  }
  Serial.println("Send FAIL after retries ❌");
  return false;
}

bool getDataMPU6050(){
  OLED_init();
  oled.ssd1306_command(SSD1306_DISPLAYON);

  oled.setTextColor(1);
  oled.setTextWrap(false);
  oled.setCursor(8, 48);
  oled.print("Reading sensor data");

  oled.setCursor(32, 7);
  oled.print("Please wait");

  oled.drawRect(11, 25, 104, 10, 1);
  oled.display();

  mpu.calcGyroOffsets(1,1000,0,0);

  if(mpu.getGyroXoffset() == mpu.getGyroYoffset()){
    return 0;
  }
  mpu.update();
  x = mpu.getAngleX();
  y = mpu.getAngleY();
  z = mpu.getAngleZ();

  Serial.print("X: ") ;
  Serial.print(x);
  Serial.print("  Y: ") ;
  Serial.print(y);
  Serial.print("  Z: ") ;
  Serial.println(z);
  return 1;
}

void display_data(bool connect, float _x, float _y, float _z){

  oled.clearDisplay();

  // ===== VẼ KHUNG =====
  oled.drawLine(74, 0, 74, 63, 1);       // vạch chia màn hình
  oled.drawLine(36, 5, 36, 61, 1);       // trục dọc
  oled.drawLine(3, 32, 71, 32, 1);       // trục ngang

  // ===== HIỂN THỊ SỐ =====
  oled.setTextColor(1);
  oled.setTextWrap(false);
  oled.setCursor(83, 27);   oled.print("X:" + String(_x,1));
  oled.setCursor(83, 41);  oled.print("Y:" + String(_y,1));
  oled.setCursor(83, 55);  oled.print("Z:" + String(_z,1));

  oled.drawBitmap(88, 0, (connect) ? icon_signal : icon_nosignal, 24, 25, 1);

  // ===== TÍNH TỌA ĐỘ CHẤM TRÒN =====
  const int CENTER_X = 36;
  const int CENTER_Y = 32;
  const int MAX_X = 32;   // bán kính vùng di chuyển
  const int MAX_Y = 28;   // bán kính vùng di chuyển
  // Giới hạn góc trong khoảng -45 → 45
  _x = constrain(_x, -45, 45);
  _y = constrain(_y, -45, 45);

  int dotX = CENTER_X + (_x / 45.0) * MAX_X;
  int dotY = CENTER_Y - (_y / 45.0) * MAX_Y;  // đảo dấu vì trục Y màn hình

  // ===== VẼ CHẤM TRÒN VÀ ĐƯỜNG NỐI =====
  oled.drawCircle(dotX, dotY, 3, 1);          // chấm tròn theo góc
  oled.drawLine(CENTER_X, CENTER_Y, dotX, dotY, 1); // đường nối từ tâm

  oled.display();
}

void nrf24_init(){
  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!!"));
    delay(2000);
    ESP.restart();
    return;
  }

  radio.openWritingPipe(Address_Master);
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(120);
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.setRetries(5, 10);  //Delay time = delay x 250 µs = 1.25ms , retry 10 lần
  radio.setAutoAck(true);
  radio.stopListening();
} 

void systems_init(){
  pinMode(POWER_PIN, OUTPUT);
}

void OLED_init(){
  if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 hardware is not responding!!"));
    delay(2000);
    ESP.restart();
  }
  Serial.println("OLED Success");
  oled.clearDisplay();
}

void oled_printCenter(String text, int centerX, int y, uint8_t textSize) {
  oled.setTextSize(textSize);

  int16_t textWidth = text.length() * 6 * textSize;
  int16_t x = centerX - textWidth / 2;

  if (x < 0) x = 0;

  oled.setCursor(x, y);
  oled.print(text);
}

float readBatteryVoltage()
{
    const float R1 = 9500.0;   // ohm
    const float R2 = 9200.0;   // ohm
    const float ADC_MAX = 1023.0;
    const float ADC_REF = 3;

    int adc = analogRead(BAT_PIN);
    float v_adc = (adc / ADC_MAX) * ADC_REF;
    float v_bat = v_adc * (R1 + R2) / R2;
    v_bat -= 0.18;
    return v_bat;
}
