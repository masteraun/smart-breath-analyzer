#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* ---------- WiFi & Server Settings ---------- */
const char* ssid     = "@PCCPL-WIFI"; 
const char* password = "";
const char* serverName = "http://172.16.12.103:5022/api/status"; 

/* ---------- Pin Definitions ---------- */
#define MQ2_PIN    33
#define MQ3_PIN    32
#define MQ7_PIN    35
#define MQ8_PIN    34
#define MQ135_PIN  36
#define DHTPIN      27
#define DHTTYPE     DHT22
#define relay1_PIN 26
#define relay2_PIN 25

// LCD I2C (Address 0x27, ขา SDA=21, SCL=22)
LiquidCrystal_I2C lcd(0x27, 16, 2); 
DHT dht(DHTPIN, DHTTYPE);

/* ---------- MQ Parameters ---------- */
struct MQCurve { float a; float b; };
MQCurve MQ2_C={613.05,-2.022}, MQ3_C={0.45,-1.53}, MQ7_C={99.04,-1.518}, MQ8_C={1.19,-2.10}, MQ135_C={110.47,-2.862};

float MQ2_Ratio_CleanAir=9.83, MQ3_Ratio_CleanAir=60.0, MQ7_Ratio_CleanAir=27.0, MQ8_Ratio_CleanAir=70.0, MQ135_Ratio_CleanAir=3.6;
float Ro_MQ2=1.0, Ro_MQ3=1.0, Ro_MQ7=1.0, Ro_MQ8=1.0, Ro_MQ135=1.0; 
float f_mq2=0, f_mq3=0, f_mq7=0, f_mq8=0, f_mq135=0;
float alpha = 0.2; 

unsigned long lastMsg=0, lastLcdUpdate=0;
int lcdPage = 0;
bool isMeasuringOnWeb = false; // รับสถานะมาจาก Server
bool lastMeasuringStatus = false;
bool showingResult = false;
String lastRiskAssessment = "";

/* ---------- Helper Functions ---------- */
float calculateResistance(int raw_adc) {
  if (raw_adc <= 0) return 100.0;
  if (raw_adc >= 4095) return 0.1;
  return (float)1.0 * (4095.0 - (float)raw_adc) / (float)raw_adc;
}

float calculatePPM_New(float rs_ro, MQCurve curve) {
  if (rs_ro <= 0) return 0.0;
  float ppm = curve.a * pow(rs_ro, curve.b);
  return (isnan(ppm) || isinf(ppm)) ? 0.0 : ppm;
}

// อนิเมชันจุดไข่ปลาวิ่ง 0-1-2-3-2-1-0
void updateCalibAnimation(int step) {
  lcd.setCursor(12, 0);
  if(step == 0) lcd.print("    ");
  else if(step == 1) lcd.print(".   ");
  else if(step == 2) lcd.print("..  ");
  else if(step == 3) lcd.print("... ");
}

void calibrateSensors() {
  Serial.println("\n--- [CALIBRATION PHASE] ---");
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Calibrating");
  
  digitalWrite(relay1_PIN, HIGH); 
  digitalWrite(relay2_PIN, HIGH); 
  
  float sumRs2=0, sumRs3=0, sumRs7=0, sumRs8=0, sumRs135=0;
  long totalSamples = 0;
  unsigned long startTime = millis();
  int animStep = 0;
  int direction = 1;

  while (millis() - startTime < 60000) {
    sumRs2 += calculateResistance(analogRead(MQ2_PIN));
    sumRs3 += calculateResistance(analogRead(MQ3_PIN));
    sumRs7 += calculateResistance(analogRead(MQ7_PIN));
    sumRs8 += calculateResistance(analogRead(MQ8_PIN));
    sumRs135 += calculateResistance(analogRead(MQ135_PIN));
    totalSamples++;

    if (totalSamples % 5 == 0) {
      updateCalibAnimation(animStep);
      animStep += direction;
      if (animStep >= 3) direction = -1;
      if (animStep <= 0) direction = 1;
    }
    delay(100); 
  }

  Ro_MQ2 = (sumRs2/totalSamples)/MQ2_Ratio_CleanAir;
  Ro_MQ3 = (sumRs3/totalSamples)/MQ3_Ratio_CleanAir;
  Ro_MQ7 = (sumRs7/totalSamples)/MQ7_Ratio_CleanAir;
  Ro_MQ8 = (sumRs8/totalSamples)/MQ8_Ratio_CleanAir;
  Ro_MQ135 = (sumRs135/totalSamples)/MQ135_Ratio_CleanAir;

  digitalWrite(relay1_PIN, LOW); 
  digitalWrite(relay2_PIN, LOW); 
  
  Serial.println("Calibration completed!");
  Serial.printf("Ro_MQ2: %.2f | Ro_MQ3: %.2f | Ro_MQ7: %.2f | Ro_MQ8: %.2f | Ro_MQ135: %.2f\n", 
                Ro_MQ2, Ro_MQ3, Ro_MQ7, Ro_MQ8, Ro_MQ135);
  
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("Calibrate Done!");
  delay(2000);
}

String getRiskAssessment() {
  // ประเมินความเสี่ยงตามค่าเซนเซอร์ (อ้างอิงจาก page2.html logic)
  if (f_mq3 > 50) return "Risk: Alcohol";
  if (f_mq135 > 150) return "Risk: Kidney/Amm";
  if (f_mq135 > 100 && f_mq2 > 50) return "Risk: Ketosis";
  if (f_mq8 > 50) return "Risk: Digestion";
  if (f_mq2 > 30) return "Caution: Smoke";
  if (f_mq7 > 20) return "Caution: CO";
  return "Status: Normal";
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init(); 
  lcd.backlight();
  
  // 1. Welcome Screen
  lcd.setCursor(0, 0); 
  lcd.print("  Welcome to    ");
  lcd.setCursor(0, 1); 
  lcd.print("Smart breath");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 1); 
  lcd.print("    analysis    ");
  delay(1500);

  dht.begin();
  pinMode(relay1_PIN, OUTPUT); 
  pinMode(relay2_PIN, OUTPUT);
  digitalWrite(relay1_PIN, LOW); 
  digitalWrite(relay2_PIN, LOW);

  // Connect WiFi
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Connecting WiFi");
  
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
    lcd.setCursor(dots % 16, 1);
    lcd.print(".");
    dots++;
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("WiFi Connected!");
  delay(1500);
  
  // 2. Calibrating Mode
  calibrateSensors();
  
  // 3. Back to Welcome after calibration
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("  Welcome to    ");
  lcd.setCursor(0, 1); 
  lcd.print("Smart breath    ");
}

void loop() {
  unsigned long now = millis();
  
  // อัปเดตข้อมูลและส่งเข้า Server ทุก 5 วินาที
  if (now - lastMsg > 5000) { 
    lastMsg = now;
    
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    int r2 = analogRead(MQ2_PIN);
    int r3 = analogRead(MQ3_PIN);
    int r7 = analogRead(MQ7_PIN);
    int r8 = analogRead(MQ8_PIN);
    int r135 = analogRead(MQ135_PIN);
    
    float rs2 = calculateResistance(r2);
    float rs3 = calculateResistance(r3);
    float rs7 = calculateResistance(r7);
    float rs8 = calculateResistance(r8);
    float rs135 = calculateResistance(r135);

    // คำนวณ PPM พร้อม Exponential Moving Average
    f_mq2 = (alpha * calculatePPM_New(rs2/Ro_MQ2, MQ2_C)) + (1.0 - alpha) * f_mq2;
    f_mq3 = (alpha * calculatePPM_New(rs3/Ro_MQ3, MQ3_C)) + (1.0 - alpha) * f_mq3;
    f_mq7 = (alpha * calculatePPM_New(rs7/Ro_MQ7, MQ7_C)) + (1.0 - alpha) * f_mq7;
    f_mq8 = (alpha * calculatePPM_New(rs8/Ro_MQ8, MQ8_C)) + (1.0 - alpha) * f_mq8;
    f_mq135 = (alpha * calculatePPM_New(rs135/Ro_MQ135, MQ135_C)) + (1.0 - alpha) * f_mq135;

    // --- Serial Monitor Full (ละเอียดเหมือนเดิม) ---
    Serial.println("======================================================");
    Serial.printf("ENV | Temp: %.1f C | Humid: %.1f %%\n", t, h);
    Serial.println("------------------------------------------------------");
    Serial.println("SENSOR  |  RAW ADC  |   Rs (kOhm)  |  Ratio  |   PPM");
    Serial.printf("MQ2     |  %4d     |   %.2f       |  %.3f  |   %.2f\n", r2, rs2, rs2/Ro_MQ2, f_mq2);
    Serial.printf("MQ3     |  %4d     |   %.2f       |  %.3f  |   %.2f\n", r3, rs3, rs3/Ro_MQ3, f_mq3);
    Serial.printf("MQ7     |  %4d     |   %.2f       |  %.3f  |   %.2f\n", r7, rs7, rs7/Ro_MQ7, f_mq7);
    Serial.printf("MQ8     |  %4d     |   %.2f       |  %.3f  |   %.2f\n", r8, rs8, rs8/Ro_MQ8, f_mq8);
    Serial.printf("MQ135   |  %4d     |   %.2f       |  %.3f  |   %.2f\n", r135, rs135, rs135/Ro_MQ135, f_mq135);
    Serial.println("------------------------------------------------------");
    Serial.printf("Risk Assessment: %s\n", getRiskAssessment().c_str());
    Serial.println("======================================================\n");

    // --- Send HTTP POST & Get Status ---
    if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");
      
      StaticJsonDocument<512> doc;
      doc["dht_temperature"] = isnan(t) ? 0 : t;
      doc["humidity"] = isnan(h) ? 0 : h;
      
      JsonObject s = doc.createNestedObject("sensors");
      s["mq2_ppm"] = f_mq2; 
      s["mq3_ppm"] = f_mq3; 
      s["mq7_ppm"] = f_mq7;
      s["mq8_ppm"] = f_mq8; 
      s["mq135_ppm"] = f_mq135;
      
      String body; 
      serializeJson(doc, body);
      
      int httpCode = http.POST(body);
      if (httpCode > 0) {
        String payload = http.getString();
        StaticJsonDocument<200> resDoc;
        DeserializationError error = deserializeJson(resDoc, payload);
        
        if (!error) {
          isMeasuringOnWeb = resDoc["is_measuring"]; // รับสถานะจาก Flask
          Serial.printf("Server Status: is_measuring = %s\n", isMeasuringOnWeb ? "true" : "false");
        }
      } else {
        Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }

  // --- LCD Display Management ---
  
  // ตรวจจับการเปลี่ยนสถานะ: เริ่มต้นการวัด
  if (isMeasuringOnWeb && !lastMeasuringStatus) {
    Serial.println(">>> Start measuring mode");
    lastMeasuringStatus = true;
    showingResult = false;
    lcdPage = 0;
    lastLcdUpdate = now;
    lcd.clear();
  }
  
  // ตรวจจับการเปลี่ยนสถานะ: หยุดการวัด
  if (!isMeasuringOnWeb && lastMeasuringStatus) {
    Serial.println(">>> Stop measuring mode");
    lastMeasuringStatus = false;
    showingResult = true;
    
    // แสดงผลสรุป
    lastRiskAssessment = getRiskAssessment();
    lcd.clear();
    lcd.setCursor(0, 0); 
    lcd.print("Calculating Done");
    lcd.setCursor(0, 1); 
    lcd.print(lastRiskAssessment);
    
    Serial.println(">>> Showing result on LCD");
    return; // ไม่ต้องทำอะไรต่อในรอบนี้
  }

  if (isMeasuringOnWeb) {
    // โหมดกำลังทดสอบ: สลับหน้าจอทุก 10 วินาที
    if (now - lastLcdUpdate > 10000) {
      lastLcdUpdate = now;
      lcd.clear();
      
      // บรรทัดแรกแสดง "Calculating . . ." เสมอ
      lcd.setCursor(0, 0); 
      lcd.print("Calculating...  ");
      
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      
      if (lcdPage == 0) { 
        // หน้าแรก: Temp & Humid
        lcd.setCursor(0, 1);
        lcd.printf("T:%.1fC H:%.0f%%   ", isnan(t) ? 0.0 : t, isnan(h) ? 0.0 : h);
        lcdPage = 1;
      } 
      else if (lcdPage == 1) { 
        // หน้าสอง: MQ2, MQ3, MQ7
        lcd.setCursor(0, 1);
        lcd.printf("2:%.0f 3:%.0f 7:%.0f", f_mq2, f_mq3, f_mq7);
        lcdPage = 2;
      }
      else { 
        // หน้าสาม: MQ8, MQ135
        lcd.setCursor(0, 1);
        lcd.printf("8:%.0f 135:%.0f ppm", f_mq8, f_mq135);
        lcdPage = 0;
      }
    }
  } 
  else if (!showingResult) {
    // โหมดไม่ได้ทดสอบ และยังไม่ได้แสดงผล -> แสดง Welcome
    lcd.setCursor(0, 0); 
    lcd.print("  Welcome to    ");
    lcd.setCursor(0, 1); 
    lcd.print("Smart breath    ");
  }
  // ถ้า showingResult = true จะค้างแสดงผลการประเมินไว้
}
