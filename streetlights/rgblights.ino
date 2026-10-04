// Khai báo chân GPIO kết nối với Module Đèn Giao Thông
const int RED_PIN    = 4; // Chân Đèn Đỏ
const int YELLOW_PIN = 5; // Chân Đèn Vàng
const int GREEN_PIN  = 6; // Chân Đèn Xanh

void setup() {
  // Cấu hình các chân GPIO là OUTPUT
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  // Bắt đầu bằng việc tắt hết các đèn
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
}

void loop() {
  // --- BƯỚC 1: Bật đèn ĐỎ (5 giây) ---
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  delay(500);

  // --- BƯỚC 2: Bật đèn XANH (4 giây) ---
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, HIGH);
  delay(500);

  // --- BƯỚC 3: Bật đèn VÀNG (2 giây) ---
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, HIGH);
  digitalWrite(GREEN_PIN, LOW);
  delay(500);
}