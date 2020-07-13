
//#include <SoftwareSerial.h>
//SoftwareSerial wiFiSerial (3, 2);
#include <LiquidCrystal.h>
LiquidCrystal lcd(9, 8, 7, 6, 5, 4);
#include <Wire.h>


void setup() {
  Wire.begin();  
  lcd.begin(16, 2);
  Serial.begin(9600);
  printRow("Arduino ready!", 1000);
}

String outp;
uint8_t b;
uint8_t h;
uint8_t l;
uint16_t total;
String input;
char temp[3];

void loop() {
  if (Serial.available()) {
    delay(1);
    b = Serial.read();
    if (b == 255) { // Прочитать регистр
      if (Serial.readBytes(temp, 2) == 2) {
        if (!(temp[1] & temp[0])) {
          Wire.beginTransmission(0x11);
          Wire.write(temp[0]);
          Wire.endTransmission(false);
          Wire.requestFrom(0x11, 2, true);
          h = Wire.read();
          l = Wire.read();
          Serial.write(h);
          Serial.write(l);
          Serial.write(h|l);
          total = h;
          total = total << 8;
          total |= (uint8_t)l;
          outp = "Get r:" + String(temp[0], DEC) + "  v:" + String(total, HEX);
          printRow(outp, 0);     
        }   
      }
    }
    else if (b == 254) { // Записать регистр
      if (Serial.readBytes(temp, 4) == 4) {
        if ((temp[0] & temp[1] | temp[2]) == temp[3]) {
          Wire.beginTransmission(0x11);
          Wire.write(temp[0]);
          Wire.write(temp[1]);
          Wire.write(temp[2]);
          Wire.endTransmission(true);
          Serial.write(temp[0]&temp[1]|temp[2]);         
          total = temp[1];          
          total = total << 8;
          total |= (uint8_t)temp[2];
          outp = "Set r:" + String(temp[0], DEC) + "  v:" + String(total, HEX);
          printRow(outp, 0);    
        }    
      }
    }
    else if (b == 253) { // Прочитать RDS
      if (Serial.readBytes(temp, 2) == 2) {
        if (!(temp[0] & temp[1])) {
          readRDS();
          Serial.write(170); 
          Serial.write(170); 
          Serial.write(170); 
          Serial.write(170); 
          Serial.write(170); 
          Serial.write(170); 
          Serial.write(170); 
          Serial.write(170); 
          printRow("RDS read", 0);           
        }    
      }
    }
    else {
      input = Serial.readStringUntil('\r');
      h = Serial.readBytes(temp, 1);
      printRow((char)b + input, 0);
    }
  }
  
}

boolean first=true;
String row="";
void printRow(String text, int time) {
  if (first) {
    lcd.setCursor(0,0);
    lcd.print(text);
    first=false;
  }else if (row == "") {  
    lcd.setCursor(0,1);
    lcd.print(text);
    row=text;
  } else {
    lcd.clear();      
    lcd.setCursor(0,0);
    lcd.print(row);
    lcd.setCursor(0,1);
    lcd.print(text);
    row=text;
  }
  delay(time);
}

#define RDSR_CHECK_INTERVAL 30
#define RDSR_CHECK_PERIOD 2000
#define RDA5807M_REG_STATUS1 0x0A
#define RDA5807M_REG_STATUS2 0x0B
#define RDA5807M_FLG_RDS word(0x0008)
#define RDA5807M_REG_BLOCK_A 0x0C
#define RDA5807M_REG_BLOCK_B 0x0D
#define RDA5807M_REG_BLOCK_C 0x0E
#define RDA5807M_REG_BLOCK_D 0x0F
#define REPEATS_TO_BE_REAL_ID 3 // Количество повторений чтобы признать ID корректным
#define RDA5807M_BLERB_MASK word(0x0003)
#define RDA5807M_FLAG_RDSR 0x8000
#define RDA5807M_REG_BLER_CD 0x10
#define RDA5807M_BLERC_MASK 0xC000
#define RDA5807M_BLERC_SHIFT 14
#define RDA5807M_BLERD_MASK 0x3000
#define RDA5807M_BLERD_SHIFT 12
#define RDA5807M_RANDOM_ACCESS_ADDRESS 0x11

unsigned long RDSCheckTime = 0;
unsigned long RDSPeriod = 0;
bool RDS_ready = false; // Предыдущее значение RDSR
uint16_t ID = 0; // ID радиостанции
uint16_t MaybeThisIDIsReal = 0; // Предыдущее значение ID
uint8_t IDRepeatCounter = 0; // Счетчик повторений ID

void readRDS() {
  uint16_t reg0Ah, reg0Bh, reg10h;
  uint16_t blockA, blockB, blockC, blockD;
  uint8_t errLevelB, errLevelC, errLevelD, groupType, groupVer;

  RDSPeriod = millis();
  while (millis() - RDSPeriod >= RDSR_CHECK_PERIOD) {
    if (millis() - RDSCheckTime >= RDSR_CHECK_INTERVAL) {
      // Пора проверить флаг RDSR
      RDSCheckTime = millis();
      reg0Ah = getRegister(RDA5807M_REG_STATUS1);
      if ((reg0Ah & RDA5807M_FLAG_RDSR) and (!RDS_ready)) { // RDSR изменился с 0 на 1
        blockA = getRegister(RDA5807M_REG_BLOCK_A);
        blockB = getRegister(RDA5807M_REG_BLOCK_B);
        blockC = getRegister(RDA5807M_REG_BLOCK_C);
        blockD = getRegister(RDA5807M_REG_BLOCK_D);
        reg0Bh = getRegister(RDA5807M_REG_STATUS2);
        reg10h = getRegister(RDA5807M_REG_BLER_CD);
              
        // Сравним содержимое блока A (ID станции) с предыдущим значением
        if (blockA == MaybeThisIDIsReal) {
          if (IDRepeatCounter < REPEATS_TO_BE_REAL_ID) {
            IDRepeatCounter++; // Значения совпадают, отразим это в счетчике
            if (IDRepeatCounter == REPEATS_TO_BE_REAL_ID)
              ID = MaybeThisIDIsReal; // Определились с ID станции
          }
        }
        else {
          IDRepeatCounter = 0; // Значения не совпадают, считаем заново
          MaybeThisIDIsReal = blockA;
        }
  
        if (IDRepeatCounter == 3) {
          errLevelB = (reg0Bh & RDA5807M_BLERB_MASK);
          errLevelC = (reg10h & RDA5807M_BLERC_MASK) >> RDA5807M_BLERC_SHIFT;
          errLevelD = (reg10h & RDA5807M_BLERD_MASK) >> RDA5807M_BLERD_SHIFT;
          if ((errLevelB < 3) and (errLevelC < 3) and (errLevelD < 3)){
            Serial.write(lowByte(blockA));
            Serial.write(highByte(blockA));
            Serial.write(lowByte(blockB));
            Serial.write(highByte(blockB));
            Serial.write(lowByte(blockC));
            Serial.write(highByte(blockC));
            Serial.write(lowByte(blockD));
            Serial.write(highByte(blockD));          
          }
        }
      }
      RDS_ready = reg0Ah & RDA5807M_FLAG_RDSR;
    }  
  }
}

void setRegister(uint8_t reg, const uint16_t value) {
  Wire.beginTransmission(0x11);
  Wire.write(reg);
  Wire.write(highByte(value));
  Wire.write(lowByte(value));
  Wire.endTransmission(true);
}

uint16_t getRegister(uint8_t reg) {
  uint16_t result;
  Wire.beginTransmission(RDA5807M_RANDOM_ACCESS_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(0x11, 2, true);
  result = (uint16_t)Wire.read() << 8;
  result |= Wire.read();
  return result;
}
