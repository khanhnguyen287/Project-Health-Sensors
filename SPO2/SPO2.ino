void setup() {
  // Khởi tạo Serial
  Serial.begin(115200);
  
  // Vòng lặp chờ USB kết nối thành công
  while (!Serial) {
    delay(10);
  }
  
  delay(1000);
  Serial.println("\n\n====================================");
  Serial.println("TEST THANH CONG: ESP32-S3 DANG CHAY!");
  Serial.println("====================================");
}

void loop() {
  Serial.println("ESP32-S3 dang hoat dong...");
  delay(1000);
}