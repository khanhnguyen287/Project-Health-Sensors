#include <EEPROM.h>

#define EEPROM_SIZE 512

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 EEPROM CLEAR");
  Serial.println("================================");

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("EEPROM initialization FAILED!");
    return;
  }

  Serial.println("EEPROM initialized.");

  // Clear every EEPROM byte
  for (int i = 0; i < EEPROM_SIZE; i++)
  {
    EEPROM.write(i, 0xFF);
  }

  // Save changes to flash
  EEPROM.commit();

  Serial.println("EEPROM CLEARED!");
  Serial.println("All EEPROM bytes set to 0xFF.");
  Serial.println("You can now reset the ESP32.");
}

void loop()
{
  // Nothing to do
}