#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <CRC32.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SimpleEEPROM.h"

#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#define CE_PIN D4    
#define CSN_PIN D8 
#define IRQ_PIN  D3

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET -  1

#define DISPLAY_WIFI_CONNECTING 0
#define DISPLAY_WIFI_SUCCESSFUL 1
#define DISPLAY_WIFI_FAILED 2

#define TIME_DELAY_DISPLAY_SCREEN_DATA 4000
#define DATA_ERROR 0x01
#define WIFI_TIMEOUT 10000  // 10s
#define WIFI_RECONNECT_INTERVAL  60000UL  // 60s
#define MAX_DATA           48         // Số mẫu dữ liệu tối đa lưu trữ cho mỗi trạm khi mất wifi

#define NUMBER_STATION  5             // Khai báo tổng số trạm
#define ID_STATON_1        0xD1       // Khai báo ID của trạm 1
#define ID_STATON_2        0xD2       // Khai báo ID của trạm 2
#define ID_STATON_3        0xD3       // Khai báo ID của trạm 3
#define ID_STATON_4        0xD4       // Khai báo ID của trạm 4
#define ID_STATON_5        0xD5       // Khai báo ID của trạm 5

const uint8_t ID_STATION[NUMBER_STATION] = {ID_STATON_1, ID_STATON_2, ID_STATON_3, ID_STATON_4, ID_STATON_5};

/*======================== !!!! Phần Quan trọng - Chỉnh sửa lại cho phù hợp !!!! =======================*/

/* ================= WIFI ================= */
const char* SSID     = "VIETTEL_Khanh";
const char* PASSWORD = "Khanh7@2k4";

/* ================= LINK Kết nối với ggsheet ================= */
String url = "https://script.google.com/macros/s/AKfycbz8OejMxvwfoRDuUM_P3r3jjU2FR_6hI6HCoiUzfBcPUW8HNhZoIpbuxaIX7s7p3BOo-Q/exec";
const char* ID = "AKfycbz8OejMxvwfoRDuUM_P3r3jjU2FR_6hI6HCoiUzfBcPUW8HNhZoIpbuxaIX7s7p3BOo-Q";

/*======================== =============================================================================*/

/* ================= GOOGLE SHEET ================= */
// String url = "https://script.google.com/macros/s/AKfycbwJrqnngnKG88w5Bm3UyLWmYOWtEw5FTn4W2f9K_2e6EMpKioGuYsK55ALUkB7_tEvD/exec";
// const char* ID = "AKfycbwJrqnngnKG88w5Bm3UyLWmYOWtEw5FTn4W2f9K_2e6EMpKioGuYsK55ALUkB7_tEvD";


const unsigned char icon_receive_signal [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x48, 0x80, 
	0x00, 0x00, 0x4c, 0x60, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x10, 0xe0, 
	0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0x66, 0x00, 0x01, 0x81, 0x21, 0xc0, 0x02, 0x43, 0x20, 0x00, 
	0x02, 0x3d, 0xc0, 0x00, 0x00, 0x18, 0x80, 0x00, 0x04, 0x04, 0x80, 0x00, 0x00, 0x03, 0x80, 0x00, 
	0x02, 0x01, 0x80, 0x00, 0x02, 0x00, 0xc0, 0x00, 0x02, 0x00, 0x60, 0x00, 0x01, 0xa0, 0x10, 0x00, 
	0x01, 0x10, 0x10, 0x00, 0x01, 0x10, 0x60, 0x00, 0x01, 0xff, 0x80, 0x00, 0x01, 0x10, 0x00, 0x00, 
	0x01, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x03, 0xf8, 0x00, 0x00, 
	0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_wating_signal [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x00, 0x01, 0x00, 0x06, 
	0x30, 0x00, 0x00, 0x03, 0x80, 0x0c, 0x18, 0x00, 0x00, 0x07, 0xc0, 0x0c, 0x18, 0x00, 0x00, 0x0c, 
	0x60, 0x0c, 0x18, 0x00, 0x00, 0x0c, 0x38, 0x1c, 0x18, 0x00, 0x00, 0x08, 0x1c, 0xff, 0x30, 0x00, 
	0x00, 0x18, 0x0f, 0xc3, 0xe0, 0x00, 0x00, 0x18, 0x07, 0x81, 0x80, 0x00, 0x00, 0x18, 0x01, 0x81, 
	0x80, 0x00, 0x00, 0x10, 0x00, 0xe1, 0x00, 0x00, 0x00, 0x10, 0x00, 0x71, 0x00, 0x00, 0x00, 0x10, 
	0x00, 0x3b, 0x00, 0x00, 0x00, 0x10, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x18, 0x00, 0x07, 0x00, 0x00, 
	0x00, 0x18, 0x00, 0x03, 0x80, 0x00, 0x00, 0x18, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x0c, 0x00, 0x00, 
	0xe0, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x70, 0x00, 0x00, 0x06, 0xf0, 0x00, 0x18, 0x00, 0x00, 0x07, 
	0xfc, 0x00, 0x0c, 0x00, 0x00, 0x03, 0x0c, 0x00, 0x06, 0x00, 0x00, 0x03, 0x06, 0x00, 0x0e, 0x00, 
	0x00, 0x03, 0x06, 0x00, 0x1c, 0x00, 0x00, 0x03, 0x06, 0x00, 0x38, 0x00, 0x00, 0x03, 0x8e, 0x01, 
	0xe0, 0x00, 0x00, 0x03, 0xff, 0xff, 0x80, 0x00, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x02, 
	0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 
	0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x00, 
	0x00, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x03, 0x06, 0x00, 0x00, 0x00, 0x00, 0x1f, 
	0xff, 0x80, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 
	0x00, 0x60, 0x00, 0x20, 0x00, 0x00, 0x00, 0x60, 0x00, 0x20, 0x00, 0x00, 0x00, 0x60, 0x00, 0x20, 
	0x00, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x00, 0x00
};
const unsigned char icon_wifi [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x3c, 0x3c, 0x70, 0x0e, 0xc7, 0xe3, 0x1f, 0xf8, 0x18, 0x1c, 
	0x03, 0xc0, 0x07, 0xe0, 0x04, 0x20, 0x01, 0x80, 0x01, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_nowifi [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0f, 0xf0, 0x18, 0x28, 0x30, 0x4c, 0x07, 0xe0, 0x0c, 0xb0, 
	0x01, 0x00, 0x03, 0xc0, 0x06, 0x00, 0x04, 0x00, 0x0d, 0x80, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_upload [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0xe0, 0xe0, 0x01, 
	0x80, 0x20, 0x01, 0x00, 0x30, 0x03, 0x00, 0x10, 0x0e, 0x00, 0x18, 0x3c, 0x00, 0x1c, 0x60, 0x00, 
	0x06, 0x40, 0x18, 0x02, 0xc0, 0x3c, 0x03, 0xc0, 0x3c, 0x03, 0xc0, 0x7e, 0x03, 0xc0, 0x18, 0x03, 
	0x40, 0x18, 0x02, 0x60, 0x18, 0x06, 0x3f, 0xdb, 0xfc, 0x0f, 0xdb, 0xf0, 0x00, 0x18, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_noupload [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0xe0, 0xe0, 0x01, 
	0x80, 0x20, 0x01, 0x00, 0x30, 0x03, 0x00, 0x10, 0x0e, 0x00, 0x18, 0x3c, 0x00, 0x1c, 0x60, 0x00, 
	0x06, 0x40, 0x42, 0x02, 0xc0, 0x22, 0x03, 0xc0, 0x34, 0x03, 0xc0, 0x1c, 0x03, 0xc0, 0x18, 0x03, 
	0x40, 0x18, 0x02, 0x60, 0x24, 0x06, 0x3c, 0x66, 0x3c, 0x0c, 0x42, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct MonitorStation {
  uint8_t id;        // ID station (0xD1, 0xD2...)
  float x;
  float y;
  float z;
  float vbat;
};

//========================= Khai báo đối tượng ==================================
RF24 radio(CE_PIN, CSN_PIN);
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MonitorStation stations[NUMBER_STATION];
MonitorStation storeData[NUMBER_STATION][MAX_DATA]; // Mảng lưu trữ dữ liệu khi mất wifi - sẽ gửi lên ggsheet khi có wifi
uint8_t storeCount[NUMBER_STATION] = {0};   // số lượng data đã lưu của từng station
//========================= NRF14L01 ==================================
const byte address[6] = "00001";
volatile bool radioInterrupt = false;  // cờ báo có sự kiện từ NRF

uint8_t checkForDataErrors;
uint32_t idTXDevice;
int8_t stationIndex = -1;
uint32_t rxCRC, calcCRC;
const char reply[] = "OK";
uint8_t rxBuf[32] = {0};
char dataReceived[23] = {0};


bool internet = 0; // Biến trạng thái kết nối với internet [true : connect] [fals: no connect]
unsigned long lastTimeDisplayScreen = 0;
unsigned long lastTimeReconnectWifi = 0;
bool autoReconnect = false;
bool displayedlistening = false;
bool autoSaveData = false;

const char* host = "script.google.com";
const int httpsPort = 443;


void IRAM_ATTR handleRadioIRQ() {
  radioInterrupt = true;
}

void setup() {
  Serial.begin(9600);  
  GPIO_init();
  Stations_init();
  OLED_init();
  Wifi_init();
  nrf24_init();
  displayScreenListeningForData();
  Serial.println("============= Bắt đầu ================");
  
}

void loop() {

  if(radioInterrupt != true && autoReconnect == true && millis() - lastTimeReconnectWifi >= WIFI_RECONNECT_INTERVAL){
    Wifi_init();
    lastTimeReconnectWifi = millis();
  } 
    
  if(radioInterrupt != true && millis() - lastTimeDisplayScreen >= TIME_DELAY_DISPLAY_SCREEN_DATA && displayedlistening == true){
    displayScreenListeningForData();
    displayedlistening = false;
  }

  if (radioInterrupt) {
    radioInterrupt = false;
    bool tx_ok, tx_fail, rx_ready;
    radio.whatHappened(tx_ok, tx_fail, rx_ready);  // xóa cờ IRQ
    if (radio.available()) {
      int totalLen = radio.getDynamicPayloadSize();  //Lấy độ dài chuỗi (đếm từ 1)
      if (totalLen < 12 || totalLen > 32) return;

      memset(rxBuf, 0, sizeof(rxBuf));
      radio.read(rxBuf, totalLen);

//    // DEBUG HEX
      // Serial.print("RX HEX: ");
      // for (int i = 0; i < totalLen; i++) {
      //   if (rxBuf[i] < 16) Serial.print("0");
      //   Serial.print(rxBuf[i], HEX); Serial.print(" ");
      // }
      // Serial.println();
      
//    // ================ ID & STATUS =================
      
      checkForDataErrors = rxBuf[1];
      if(checkForDataErrors == DATA_ERROR){
        Serial.println("Lỗi đọc dữ từ MPU6050");
        return;
      }

      idTXDevice     = rxBuf[0];     // lấy ID từ thiết bị truyền tới
      stationIndex = -1;
      for (int i = 0; i < NUMBER_STATION; i++) {
        if (idTXDevice == stations[i].id) {
          stationIndex = i;
          break;
        }
      }
      if (stationIndex < 0) {
        Serial.println("NO STATION");
        return;
}
//    // ==================== CRC =====================
      memcpy(&rxCRC, rxBuf + 24, 4);
      calcCRC = CRC32::calculate(rxBuf, totalLen - 4);
      if (calcCRC != rxCRC) {
        Serial.println("CRC FAIL");
        return;
      }
//    // ================ Data Analysis =================
      memset(dataReceived, 0, sizeof(dataReceived));
      memcpy(dataReceived, rxBuf + 2, 22);
      sscanf(dataReceived, "%f,%f,%f,%f",&stations[stationIndex].x, &stations[stationIndex].y, &stations[stationIndex].z, &stations[stationIndex].vbat);
      Serial.print("ID: ");Serial.print(stations[stationIndex].id, HEX);
      Serial.print(" | X:" + String(stations[stationIndex].x) + " Y:" + String(stations[stationIndex].y) + " Z:" + String(stations[stationIndex].z));
      Serial.print("  | Pin: "); Serial.println(stations[stationIndex].vbat);
      radio.writeAckPayload(0, reply, sizeof(reply));   // Phản hồi Station gửi là đã nhận được gói tin

//    // ====== display and upload data to Google Sheet =========
        String nameTXdevice = "Station " + String(stationIndex + 1);
        bool upload_status;
        if(WiFi.status() == WL_CONNECTED){
          internet = true;
          upload_status = uploadToGoogleSheet(stationIndex, stations[stationIndex]);
        } 
        else {
          Serial.println("Không có internet -> lưu dữ liệu");
          saveDataOffline(stationIndex, stations[stationIndex]);
          internet = false;
          upload_status = false;
          if(!autoReconnect){
            autoReconnect = true;
            lastTimeReconnectWifi = millis();
          }
        }
        displayScreenData(nameTXdevice, stations[stationIndex], upload_status);
        displayedlistening = true;
        lastTimeDisplayScreen = millis();
        return;
    }
  }
}


void saveDataOffline(uint8_t index, const MonitorStation &st){

  if(storeCount[index] >= MAX_DATA){
    Serial.println("FULL BUFFER -> overwrite oldest");
    storeCount[index] = 0;
  }

  storeData[index][storeCount[index]] = st;

  Serial.print("Save offline Station ");
  Serial.print(index + 1);
  Serial.print(" -> slot ");
  Serial.println(storeCount[index]);

  storeCount[index]++;
}

void resendStoredData(){

  Serial.println("========== RESEND STORED DATA ==========");

  for(uint8_t i = 0; i < NUMBER_STATION; i++){

    if(storeCount[i] == 0) continue;

    Serial.print("Resend Station ");
    Serial.print(i + 1);
    Serial.print(" | total: ");
    Serial.println(storeCount[i]);

    for(uint8_t j = 0; j < storeCount[i]; j++){

      bool ok = uploadToGoogleSheet(i, storeData[i][j]);

      if(ok){
        Serial.print("Upload success slot ");
        Serial.println(j);
      }
      else{
        Serial.print("Upload failed slot ");
        Serial.println(j);
        return;
      }

      delay(300);
    }

    storeCount[i] = 0;
  }

  Serial.println("========== RESEND DONE ==========");
}

bool uploadToGoogleSheet(uint8_t index, const MonitorStation &st) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Bỏ kiểm tra SSL để tiết kiệm RAM

  Serial.println("==========");
  Serial.print("Connecting to ");
  Serial.println(host);

  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    return false;
  }

  // ----- Tạo URL bằng snprintf (không dùng String ghép nhiều) -----
  char url[220];

  snprintf(url, sizeof(url),
           "/macros/s/%s/exec?station=%d&x=%.2f&y=%.2f&z=%.2f&vbat=%.2f",
           ID,
           index + 1,
           st.x,
           st.y,
           st.z,
           st.vbat);

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // ----- Gửi HTTP GET -----
  client.print("GET ");
  client.print(url);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("User-Agent: ESP8266");
  client.println("Connection: close");
  client.println();

  Serial.println("Request sent");

  // ----- Đợi phản hồi -----
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readStringUntil('\n');

  Serial.print("Reply: ");
  Serial.println(response);

  Serial.println("Closing connection");
  Serial.println("==========\n");

  client.stop();

  return true;
}



void displayScreenData(String name, const MonitorStation &st, bool upload_status){

  Serial.println("Hiển thị dữ liệu");
  oled.clearDisplay();
  oled.setTextColor(1);
  oled.setTextWrap(false);
  oled.setCursor(38, 1);
  oled.print(name);

  oled.drawBitmap(0, 20, icon_receive_signal, 32, 32, 1);
  oled.drawBitmap(32, 28, (upload_status) ? icon_upload : icon_noupload, 24, 24, 1);


  //oled.setCursor(8, 53);  oled.print("ID:0x"); oled.print(id, HEX);
  oled.setCursor(75, 19); oled.print("X:" + String(st.x,1));
  oled.setCursor(75, 38); oled.print("Y:" + String(st.y,1));
  oled.setCursor(75, 54); oled.print("Z:" + String(st.z,1));

  oled.drawLine(61, 12, 61, 63, 1);
  oled.drawLine(0, 11, 127, 11, 1);

  oled.display();

}

void displayScreenListeningForData(){
  Serial.println("Hiển thị chờ ");
  oled.clearDisplay();
  oled.setTextColor(1);
  oled.setTextWrap(false);
  oled.setCursor(52, 30);
  oled.print("Listening");
  oled.setCursor(75, 44);
  oled.print("for data");

  oled.drawBitmap(4, 10, icon_wating_signal, 45, 45, 1);
  oled.drawBitmap(111, 0, (internet) ? icon_wifi : icon_nowifi, 16, 16, 1);
  oled.display();
}

void displayWifiConnecting(uint8_t check) {
  oled.clearDisplay();
  oled.setTextColor(1);
  oled.setTextWrap(false);
  if( check == DISPLAY_WIFI_CONNECTING){
    oled_printCenter("Connecting to Wi-Fi", 64, 16, 1);
    oled_printCenter("o o o o o", 64, 33, 1);
  }
  else if (check == DISPLAY_WIFI_SUCCESSFUL) {
    oled_printCenter("Wifi: " + WiFi.SSID(), 64, 18, 1);
    oled_printCenter("Successful", 64, 38, 1);
  }
  else if (check == DISPLAY_WIFI_FAILED){
    oled_printCenter("Connecting to Wi-Fi", 64, 16, 1);
    oled_printCenter("Failed", 64, 33, 1);
  }
  delay(2000);
  oled.display(); 
}

void Stations_init() {
  for (int i = 0; i < NUMBER_STATION; i++) {
    stations[i].id = ID_STATION[i];   // STATION[] là mảng ID cũ của bạn
    stations[i].x = 0;
    stations[i].y = 0;
    stations[i].z = 0;
  }
}

void Wifi_init() {
  displayWifiConnecting(DISPLAY_WIFI_CONNECTING);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  (!autoReconnect) ? Serial.print("Đang kết nối WiFi") : Serial.print("Đang thử kết nối lại WiFi");

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    Serial.print(".");
    delay(400);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nĐã kết nối WiFi!");
    internet = true;
    autoReconnect = false;
    displayWifiConnecting(DISPLAY_WIFI_SUCCESSFUL);
    resendStoredData();
  }
  else {
    Serial.println("\nKết nối WiFi thất bại!");
    displayWifiConnecting(DISPLAY_WIFI_FAILED);
    autoReconnect = true;
    internet = false;
    lastTimeReconnectWifi = millis();
  }
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

void nrf24_init(){
  if (!radio.begin()) {
    Serial.println(F("Radio hardware is not responding!!"));
    delay(2000);
    ESP.restart();
    return;
  }
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(120);
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.startListening();
}

void GPIO_init(){
  pinMode(IRQ_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), handleRadioIRQ, FALLING);
}

void oled_printCenter(String text, int centerX, int y, uint8_t textSize) {
  oled.setTextSize(textSize);

  int16_t textWidth = text.length() * 6 * textSize;
  int16_t x = centerX - textWidth / 2;

  if (x < 0) x = 0;

  oled.setCursor(x, y);
  oled.print(text);
  oled.display();
}
