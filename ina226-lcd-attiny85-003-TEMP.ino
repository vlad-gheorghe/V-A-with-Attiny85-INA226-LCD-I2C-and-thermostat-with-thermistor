#include <TinyWireM.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// --- PINI ATTINY85 ---
#define TEMP_PIN A3       // Pinul fizic 2 (PB3 / ADC3)
#define RELAY_PIN PB1     // Pinul fizic 6 (Ieșire control)

// Parametri Termistor NTC 10k
#define B_COEFFICIENT 3950.0 
#define NOMINAL_RESISTANCE 10000.0 
#define NOMINAL_TEMPERATURE 25.0   
#define SERIES_RESISTOR 9084.0    

bool outputState = false;

#define INA226_ADDR 0x44
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  
  pinMode(TEMP_PIN, INPUT);

  TinyWireM.begin();
  lcd.init();
  lcd.backlight();
  
  // Configurare INA226
  TinyWireM.beginTransmission(INA226_ADDR);
  TinyWireM.write(0x00);
  TinyWireM.write(0x41);
  TinyWireM.write(0x27); 
  TinyWireM.endTransmission();
}

uint16_t readRegister(uint8_t reg) {
  uint16_t res = 0;
  TinyWireM.beginTransmission(INA226_ADDR);
  TinyWireM.write(reg);
  TinyWireM.endTransmission();
  
  TinyWireM.requestFrom(INA226_ADDR, 2);
  if (TinyWireM.available() >= 2) {
    res = TinyWireM.read() << 8;
    res |= TinyWireM.read();
  }
  return res;
}

float getTemperature() {
  int adcValue = analogRead(TEMP_PIN);
  
  if (adcValue <= 0 || adcValue >= 1023) return 0.0;
  
  float resistance = SERIES_RESISTOR / ((1023.0 / (float)adcValue) - 1.0);
  
  float steinhart;
  steinhart = resistance / NOMINAL_RESISTANCE;      
  steinhart = log(steinhart);                       
  steinhart /= B_COEFFICIENT;                       
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
  steinhart = 1.0 / steinhart;                      
  steinhart -= 273.15;                              
  
  return steinhart;
}

void loop() {
  float sumVoltage = 0;
  float sumCurrentmA = 0;
  float sumTemp = 0;

  for (int i = 0; i < 20; i++) {
    uint16_t rawVoltage = readRegister(0x02);
    sumVoltage += rawVoltage * 0.00125;

    int16_t rawShunt = (int16_t)readRegister(0x01);
    sumCurrentmA += (rawShunt * 0.0025) / 0.01;

    sumTemp += getTemperature();

    delay(10); 
  }

  float avgVoltage = (sumVoltage / 20.0) * 0.95;
  float avgCurrentA = ((sumCurrentmA / 20.0) / 100.0) * 0.625;
  float avgTemp = sumTemp / 20.0;

  if (avgVoltage < 0) avgVoltage = 0;
  if (avgCurrentA < 0) avgCurrentA = 0;

  // Logica de Histerezis (50°C ON / 45°C OFF)
  if (avgTemp >= 50.0) {
    outputState = true; 
  } else if (avgTemp <= 45.0) {
    outputState = false; 
  }
  digitalWrite(RELAY_PIN, outputState ? HIGH : LOW);

  // 1. Afișare Rândul 1 (U: 00.00V T:48°C)
  lcd.setCursor(0, 0);
  lcd.print("U: ");
  if (avgVoltage < 10.0) lcd.print("0");
  lcd.print(avgVoltage, 2);
  lcd.print("V");

  lcd.setCursor(9, 0);
  lcd.print(" T:");
  if (avgTemp >= 0 && avgTemp < 10.0) lcd.print(" ");
  lcd.print((int)avgTemp); 
  lcd.write(223); // Afișează simbolul °
  lcd.print("C");

  // 2. Afișare Rândul 2 (I: 00.00 A    ON/OFF)
  lcd.setCursor(0, 1);
  lcd.print("I: ");
  if (avgCurrentA < 10.0) lcd.print("0");
  lcd.print(avgCurrentA, 2);
  lcd.print(" A");

  // Afișare stare PB1 în dreapta jos
  lcd.setCursor(13, 1);
  if (outputState) {
    lcd.print("ON "); // Spațiul de la final șterge restul caracterelor dacă au existat
  } else {
    lcd.print("OFF");
  }

  delay(200);
}