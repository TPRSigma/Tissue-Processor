/*
Koupač vzorků - verze 21b logování a trochu úklid
*/

#include "SPI.h"
#include "TFT_eSPI.h"
#include "Wire.h"
#include "RTClib.h"
#include "FS.h"
#include "SD.h"
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>
#include <AccelStepper.h>
 
TFT_eSPI tft = TFT_eSPI(); //inicializace displeje
RTC_DS3231 rtc; //inicializace RTC

// Externí RTC objekt 
extern RTC_DS3231 rtc;

// TADY JE DEFINICE VSECH PINU 
// [v20b] pause flags
bool protPaused = false;
long protPausedRemainMs = 0;
/*pro displej 4" 480*320
!!! Nastavení v souboru User_Setup.h :
#define ST7796_DRIVER
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    27  // Data Command control pin
#define TFT_RST   -1  // RST connected to RST pin
*/

//proměnné pro SPI SD karty - SD karta je připojena na druhý SPI
#define REASSIGN_PINS
int sck = 18;
int miso = 19;
int mosi = 23;
int cs = 05;


//tlacitko enkoderu
#define ENC_KEY 4    // KEY tlačítko enkodéru
#define ENC_S1  39   // Enkodér pin A
#define ENC_S2  36   // Enkodér pin B

#define SDA 21   // definice pinu I2C sběrnice
#define SCL 22 

// tlacitka menu a enter
const int TL_Menu = 0;  //tlačítko Enter na GPIO0
const int TL_Enter = 17;  //tlačítko Enter na GPIO2


//proměnné pro práci s SD kartou
#define MAX_LATEK 20  // max 20 látek = 40 řádků (od 4 do 43)
String AktivniLatky[MAX_LATEK];
int aktivniCasy[MAX_LATEK];
int pocetLatek = 0; // počet skutečně načtených látek
// --- Pro výběr protokolu z SD karty ---
String seznamProtokolu[20];   // max 20 protokolů (lze zvětšit)
int pocetProtokolu = 0;
int aktivniVolba = 0;         // index označeného řádku
bool rezimVyberu = false;     // jsme ve výběru souboru?

//define some colour values
#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0 
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define BARVA_RAMU       tft.color565(43, 165, 200)     // tmavší cyan
#define BARVA_ZALOZKY_T  tft.color565(0, 65, 130)       // tmavá modrá
#define BARVA_ZALOZKY_S  tft.color565(0, 102, 204)      // střední modrá
#define BARVA_ZALOZKY_PT tft.color565(0, 101, 202)      // tmavý popis záložky
#define BARVA_ZALOZKY_PS tft.color565(204, 230, 255)    // světlý popis záložky
#define BARVA_VYPISU     tft.color565(230, 242, 255)    // pozadí výpisu

//proměnné pro záložky
int aktivnizalozka = 0;  // aktuální záložka
const int pocetZalozek = 5;
uint8_t Podtrzitko = 0;     //proměná pro vykreslování podtržítka během nastavování
int fillScrollOffset = 0;


// Stav programu 0: IDLE, 1: WAIT_START, 2: RUNNING
volatile int programState = 0; // 
bool statGreenBox = false;     // po 1. ENTER ukázat zelené okno

/*** [v19e] Countdown state (STAT line) ***/
int  statCountX = 0, statCountY = 0;
int  statCountdown = -1;
int  statCountdownPrev = -1;
bool statCountdownActive = false;
bool dwnPrev = false;
bool protocolActive = false;   // běží interpret protokolu
int  protLine = 4;             // začínáme od dvojice 4/5
int  protPhase = 0;            // 0: načti pár, 1: DWN, 2: čekám, 3: UP
unsigned long protUntil = 0;   // millis do kterého čekáme
bool protFirstPair = true;     // první dvojice bez posunu karuselu
String statStatusWord = "";    // zobrazené slovo vedle "Status:"
int    statStatusSecs = -1;    // zobrazený počet sekund

//proměnné pro debounce tlačítek a encodéru
unsigned long lastEncoderEvent = 0;
const unsigned long encoderDebounceDelay = 300;  // debounce 300 ms

// Proměnné pro čas
DateTime now;
char cas[9];   // "HH:MM:SS"
char datum[11]; // "DD.MM.YYYY"
unsigned long lastRTCUpdate = 0;
const unsigned long rtcUpdateInterval = 1000; // 1 sekunda
bool nastavCas = false;        // režim nastavování hodin
int hodin = 11;
int minut = 22;
int den = 33;
int mesic = 44;
int rok = 55555;

//proměnné pro provoz
bool konam = false; //hlavní semafor ukazující, že se provádí operace (chod hlavního motoru)

// definice a přerušení tlačítek
volatile bool menuButtonInterruptFlag = false;
volatile bool enterButtonInterruptFlag = false; // proměnné pro debounce
unsigned long lastEncoderEvent3 = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 300;  // 300 ms debounce

// ISR – přerušení na tlačítko MENU
void IRAM_ATTR menuButtonISR() {
  if (millis() - lastEncoderEvent3 > encoderDebounceDelay) {
    lastEncoderEvent3 = millis(); // aktualizace času
    menuButtonInterruptFlag = true;
  }
}

// ISR – přerušení na tlačítko ENTER
void IRAM_ATTR enterButtonISR() {
  if (millis() - lastEncoderEvent3 > encoderDebounceDelay) {
    lastEncoderEvent3 = millis();
    enterButtonInterruptFlag = true;
  }
}


//proměnné a přerušení encodéru
volatile bool keyPressed = false;
bool encLastState = HIGH;
bool encCW = false;
bool encCCW = false;
int progScrollOffset = 0;   // Posun výpisu na kartě PROG (0 až 4)

void IRAM_ATTR ENC_key_ISR() { // zvlášť ošetřené zákmity tlačítka enkodéru
  if (millis() - lastEncoderEvent > encoderDebounceDelay) {
    keyPressed = true;
  }
}

// funkce SD karty
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

// nalezení souboru s aktivním protokolem v config.txt
String AktivniProtokol() {
  File configFile = SD.open("/config.txt");
  String aktivniprotokol = "";

  if (!configFile) {
    Serial.println("Nepodařilo se otevřít config.txt");
    return "";
  }

  int radek = 0;
  while (configFile.available()) {
    String line = configFile.readStringUntil('\n');
    radek++;

    if (radek == 4) {
      aktivniprotokol = line;
      break;
    }
  }

  configFile.close();

  // Oříznutí případného \r nebo mezery na konci
  aktivniprotokol.trim();

  Serial.print("Aktivní protokol: ");
  Serial.println(aktivniprotokol);
  return aktivniprotokol;
}
// konec aktivního protokolu

// načtení údajů z aktivního protokolu do polí
void NactiProtokol() {
  String jmenoSouboru = AktivniProtokol();
  File soubor = SD.open("/" + jmenoSouboru);
  if (!soubor) {
    Serial.print("Nelze otevřít aktivní protokol");
    Serial.println(jmenoSouboru);
    return;
  }

  int radek = 0;
  int index = 0;

  while (soubor.available() && radek < 43) {
    String line = soubor.readStringUntil('\n');
    radek++;

    // Od 4. řádku dál
    if (radek >= 4 && radek <= 43) {
      line.trim();  // odstraní \r, \n a mezery

      // Sudé řádky: látky (4,6,42)
      if (radek % 2 == 0) {
        if (index < MAX_LATEK) {
          AktivniLatky[index] = line;
        }
      }
      // Liché řádky za nimi: časy (5,7,43)
      else {
        if (index < MAX_LATEK) {
          aktivniCasy[index] = line.toInt();
          index++;
        }
      }
    }

  }

  soubor.close();
  pocetLatek = index;
  LogEvent("Protocol read");
}
// konec načítání aktivního protokolu do polí

//funkce pro výběr aktivního protokolu z SD karty
void NactiSeznamProtokolu() {
  File root = SD.open("/");
  pocetProtokolu = 0;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    String name = entry.name();

    if (name.startsWith("proto") && !entry.isDirectory()) {
      seznamProtokolu[pocetProtokolu++] = name;
      if (pocetProtokolu >= 20) break; // bezpečnostní limit
    }

    entry.close();
  }
  root.close();
}


// Pomocná funkce: zapiš čas a hlášku do log.txt
void LogEvent(const char* message) {
  DateTime now = rtc.now();   // přečti aktuální čas z RTC

  // Otevři (nebo vytvoř) soubor log.txt v režimu přidávání
  File logFile = SD.open("/log.txt", FILE_APPEND);
  if (logFile) {
    // Sestav řádek: "YYYY-MM-DD HH:MM:SS - hláška"
    logFile.print(now.year());
    logFile.print("-");
    if (now.month() < 10) logFile.print('0');
    logFile.print(now.month());
    logFile.print("-");
    if (now.day() < 10) logFile.print('0');
    logFile.print(now.day());
    logFile.print(" ");

    if (now.hour() < 10) logFile.print('0');
    logFile.print(now.hour());
    logFile.print(":");
    if (now.minute() < 10) logFile.print('0');
    logFile.print(now.minute());
    logFile.print(":");
    if (now.second() < 10) logFile.print('0');
    logFile.print(now.second());

    logFile.print(" - ");
    logFile.println(message);
    logFile.close();
  } else {
    Serial.println("Nelze otevřít log.txt pro zápis!");
  }
}
// [v20_log] --- konec funkce ---

void kresliVyberProtokolu() {
  tft.fillRect(7, 81, 306, 392, BARVA_RAMU);
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);

  int xLeft = 17;
  int yBase = 100;
  int lineHeight = 22;

  tft.drawString("Protocol:", xLeft, yBase - 5);
  tft.drawString(AktivniProtokol(), xLeft + 110, yBase - 5);

  for (int i = 0; i < pocetProtokolu; i++) {
    int y = yBase + (i + 1) * lineHeight;
    tft.drawString(seznamProtokolu[i], xLeft + 20, y);

    // Podtržítko pod aktuální volbou
    if (i == aktivniVolba) {
      tft.drawLine(xLeft + 18, y + 20, xLeft + 18 + seznamProtokolu[i].length() * 11, y + 20, BARVA_ZALOZKY_PS);
    }
  }
}
//konec funkcí výběru protokolu



// === [v13] začátek doplnění z test_obou_motoru_06 ===
// "test obou motoru 06" – krokový motor (karusel) + DC motor (rameno) + víko
// zajištěno najití nulové polohy čidlem nulové polohy a dorovnáním o 100 kroků, což kompenzuje šířku výřezu čidla

#include <AccelStepper.h>                        // S4
#include <Wire.h>                                // D5
#include <Adafruit_PWMServoDriver.h>             // D5

// ====== TB6600 piny ======
const int DIR  = 32;                             // S4
const int STEP = 26;                             // S4
const int EN   = 25;                             // S4
const int EN_ACTIVE_LEVEL = HIGH;                // S4
const int EN_IDLE_LEVEL   = (EN_ACTIVE_LEVEL == HIGH) ? LOW : HIGH; // S4

// ====== krokování ======
const int  FULL_STEPS_PER_REV = 200*60/12;       // S4
const int  MICROSTEP_DIV      = 16;              // S4
const long STEPS_PER_REV      = (long)FULL_STEPS_PER_REV * MICROSTEP_DIV; // S4

// ====== rychlosti/akcelerace ======
const float NORM_MAX_SPEED   = 1000;             // M
const float NORM_ACCEL       = 30;               // M
const float SEEK_FAST_MAX    = 2200;             // M – rychlý běh vpřed
const float SEEK_FAST_ACCEL  = 120;              // M – rozjezd vpřed
const float SEEK_BRAKE_ACCEL = 2400;             // M – JEŠTĚ prudší brzdění (≈ poloviční překmit)
const float SEEK_SLOW_CONST  = 120;              // M – 2× rychlejší zpětný pomalý chod (bylo 60)
const int   ZERO_EXTRA_MICROSTEPS = 100;           // M – dofázování po návratu do LOW

AccelStepper motor(AccelStepper::DRIVER, STEP, DIR); // S4

// ====== PCA9685 / DC motor / víko ======
#define I2C_SDA    21                            // D5
#define I2C_SCL    22                            // D5
#define PCA_ADDR   0x40                          // D5
#define CH_ENA     0                             // D5
#define CH_IN1     1                             // D5
#define CH_IN2     2                             // D5
#define CH_VIKO    3                             // D5
#define PIN_LIMIT_UP   35                        // D5
#define PIN_LIMIT_DWN  34                        // D5
#define PIN_ZERO       33                        // M – nulová poloha (LOW)
#define PWM_FREQ_HZ    1000                      // D5
#define PWM_SPEED      2048                      // D5

Adafruit_PWMServoDriver pwm(PCA_ADDR);           // D5
unsigned long vikoUntil = 0;                     // D5
bool nextIsUp = true;                            // D5

// ====== util ======
void pcaWrite(uint8_t ch, bool high) { if (high) pwm.setPWM(ch,4096,0); else pwm.setPWM(ch,0,0); } // D5
inline bool upHit()   { return digitalRead(PIN_LIMIT_UP)  == LOW; }  // D5
inline bool dwnHit()  { return digitalRead(PIN_LIMIT_DWN) == LOW; }  // D5
inline bool zeroHit() { return digitalRead(PIN_ZERO)       == LOW; } // M
void Viko_UP()  { Serial.println("Viko"); pcaWrite(CH_VIKO,true);  vikoUntil = millis()+4000; } // D5
void Viko_DWN() { Serial.println("Viko"); pcaWrite(CH_VIKO,false); }                           // D5
void Motor_STOP(){ pwm.setPWM(CH_ENA,0,0); pcaWrite(CH_IN1,false); pcaWrite(CH_IN2,false); pcaWrite(CH_VIKO,false); vikoUntil=0; Serial.println("Motor STOP"); } // D5
void STOP_UP()  { if (upHit())  { Serial.println("stisk UP");  Motor_STOP(); nextIsUp=false; } } // D5
void STOP_DWN() { if (dwnHit()) { Serial.println("stisk DWN"); Motor_STOP(); nextIsUp=true;  } } // D5

void Motor_UP(){                                    // D5
  if (upHit()) { STOP_UP(); return; }
  pcaWrite(CH_IN1,true); pcaWrite(CH_IN2,false);
  pwm.setPWM(CH_ENA,0,PWM_SPEED);
  Serial.print("Motor "); Serial.println(PWM_SPEED);
  while (!upHit()) { if (vikoUntil && millis()>=vikoUntil){ pcaWrite(CH_VIKO,false); vikoUntil=0; } delay(2); }
  STOP_UP();
  LogEvent("Arm UP");
}
void Motor_DWN(){                                   // D5
  if (dwnHit()) { STOP_DWN(); return; }
  pcaWrite(CH_IN1,false); pcaWrite(CH_IN2,true);
  pwm.setPWM(CH_ENA,0,PWM_SPEED);
  Serial.print("Motor "); Serial.println(PWM_SPEED);
  while (!dwnHit()) { if (vikoUntil && millis()>=vikoUntil){ pcaWrite(CH_VIKO,false); vikoUntil=0; } delay(2); }
  STOP_DWN();
  LogEvent("Arm DWN");
}
// [v19c] --- Neblokující řízení DC motoru ramene (pouze pro interpret protokolu) ---
int nbMotorState = 0; // 0=idle, 1=UP, 2=DWN

inline void Motor_UP_nb_start(){
  if (upHit()) { STOP_UP(); nbMotorState = 0; return; }
  pcaWrite(CH_IN1,true); pcaWrite(CH_IN2,false);
  pwm.setPWM(CH_ENA,0,PWM_SPEED);
  nbMotorState = 1;
      LogEvent("Motor UP");
}

inline void Motor_DWN_nb_start(){
  if (dwnHit()) { STOP_DWN(); nbMotorState = 0; return; }
  pcaWrite(CH_IN1,false); pcaWrite(CH_IN2,true);
  pwm.setPWM(CH_ENA,0,PWM_SPEED);
  nbMotorState = 2;
      LogEvent("Motor DWN");
}

inline bool Motor_nb_tick(){
  if (nbMotorState == 1){
    if (upHit()){ STOP_UP(); nbMotorState = 0; return true; }
    return false;
  } else if (nbMotorState == 2){
    if (dwnHit()){ STOP_DWN(); nbMotorState = 0; return true; }
    return false;
  }
  return true; // idle -> hotovo
}


// ====== rychlé nulování: FAST vpřed + prudké brzdění + SLOW zpět + dofázování 4 mikrokroky ======
void ZeroPositionFast(){                           // M – samostatná procedura
  Serial.println("nulova pozice karuselu");        // M

  // víko UP + 250 ms
  Viko_UP(); unsigned long t0 = millis(); while (millis()-t0 < 250) { yield(); } // M

  // FAST vpřed – kontinuálně
  motor.setMaxSpeed(SEEK_FAST_MAX);                // M
  motor.setAcceleration(SEEK_FAST_ACCEL);          // M
  digitalWrite(EN, EN_ACTIVE_LEVEL);               // S4
  const long Z_FWD_FAR = STEPS_PER_REV * 50L;     // M
  motor.moveTo(motor.currentPosition() + Z_FWD_FAR);
  while (true){
    motor.run();
    if (zeroHit()){                                // první LOW → prudká brzda
      motor.setAcceleration(SEEK_BRAKE_ACCEL);     // M – rychlejší zastavení
      motor.stop();
      break;
    }
    if (motor.distanceToGo()==0){                  // pojistka
      motor.setAcceleration(SEEK_BRAKE_ACCEL);
      motor.stop();
      break;
    }
  }
  while (motor.run()) {}                           // doběh do 0 rychlosti

  // SLOW zpět – KONSTANTNÍ rychlost, 2× rychlejší než předtím
  motor.setSpeed(-SEEK_SLOW_CONST);                // M (runSpeed bez ramp)
  // Nejprve opusť okno (LOW→HIGH), aby další LOW byla hrana:
  while ( zeroHit() ){ motor.runSpeed(); yield(); }
  // Hledej opětovný nájezd do LOW:
  while (!zeroHit() ){ motor.runSpeed(); yield(); }
  
  // LOW dosaženo – DOFÁZUJ ješte kousek:
  motor.setMinPulseWidth(3); motor.setMaxSpeed(NORM_MAX_SPEED); motor.setAcceleration(NORM_ACCEL);
  motor.runToNewPosition(motor.currentPosition() - (ZERO_EXTRA_MICROSTEPS)); 

  // nastav nulu + víko DOLŮ + EN vypnout + návrat rychlostí
  motor.setCurrentPosition(0);                      // M ★ nula
  Viko_DWN();                                       // D5
  t0 = millis(); while (millis()-t0 < 250) { yield(); } // M
  digitalWrite(EN, EN_IDLE_LEVEL);                  // S4
  motor.setMaxSpeed(NORM_MAX_SPEED);                // M
  motor.setAcceleration(NORM_ACCEL);                // M
  LogEvent("Zero position set");
}

// Funkce provede posun karuselu o +1 pozici (STEPS_PER_REV/20) bezpečným způsobem.
void karusel_plus1(){
  Viko_UP(); 
  digitalWrite(EN, EN_ACTIVE_LEVEL);
  motor.runToNewPosition(motor.currentPosition() + (STEPS_PER_REV/20));
  digitalWrite(EN, EN_IDLE_LEVEL);
  Serial.println("Karusel: posun o jednu");
  LogEvent("Caroussel moved +1");
  Viko_DWN(); 
}
// [v19a] --- Neblokující řízení karuselu pouze pro interpret protokolu ---
bool protCarouselActive = false;
long protCarouselTarget = 0;

void protCarouselStartPlus1(){
  Viko_UP(); 
  long step = (STEPS_PER_REV/20);
  protCarouselTarget = motor.currentPosition() + step;
  digitalWrite(EN, EN_ACTIVE_LEVEL);
  motor.moveTo(protCarouselTarget);
  protCarouselActive = true;
  Serial.println("[v19a] Carousel start +1 (non-blocking)");
  LogEvent("Caroussel moved +1");
  Viko_DWN(); 
}

bool protCarouselTick(){
  Viko_UP(); 
  if (!protCarouselActive) return true;
  motor.run();
  if (motor.distanceToGo() == 0){
    digitalWrite(EN, EN_IDLE_LEVEL);
    protCarouselActive = false;
    Serial.println("[v19a] Carousel reached target");
    return true;
  }
  return false;
   Viko_DWN(); 

}


void karusel_minus1(){
  Viko_UP(); 
  digitalWrite(EN, EN_ACTIVE_LEVEL);
  motor.runToNewPosition(motor.currentPosition() - (STEPS_PER_REV/20));
  digitalWrite(EN, EN_IDLE_LEVEL);
  Serial.println("Karusel: posun o jednu");
  LogEvent("Caroussel moved -1");
  Viko_DWN(); 
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLE2901.h>

String WIFI_SSID = "";
String WIFI_PASSWORD = "";

String SMTP_HOST = "";
uint SMTP_PORT = 0;

String SENDER_EMAIL = "";
String SENDER_PASSWORD = "";

String RECIPIENT_EMAIL = "";

bool shouldSendEmail = false;

const String DEVICE_BT_NAME = "TP Control";

const String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const String FILE_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const String SETTINGS_CHARACTERISTIC_UUID = "10f251f7-5412-4e27-8a4f-2867812659dd";

BLEServer *pServer = NULL;
BLECharacteristic *pFileCharacteristic = NULL;
BLE2901 *descriptor_2901 = NULL;
BLECharacteristic *pSettingsCharacteristic = NULL;
BLE2901 *descriptor_2901_1 = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;

ulong lastBtCommReceivedInMillis = 0;
const uint btCommTimeout = 30000;

bool receivingBtData = false;
bool receivingBtDataEnded = false;
String receivedFileData = "";

bool receivingBtName = false;
bool receivingBtNameEnded = false;
String receivedFileName = "";

bool receivingSettingsBtData = false;
bool receivingSettingsBtDataEnded = false;
String receivedSettingsData = "";

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class FileCharacteristicCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String rxValue = pChar->getValue();

    if (rxValue == "Send email") {
      shouldSendEmail = true;
    } else if (rxValue == "Start recipe tx") {
      lastBtCommReceivedInMillis = millis();
      receivingBtData = true;
      receivedFileData = "";
      Serial.println(rxValue);
    } else if (rxValue == "Stop recipe tx") {
      lastBtCommReceivedInMillis = millis();
      receivingBtDataEnded = true;
      Serial.println(rxValue);
    } else if (receivingBtData) {
      lastBtCommReceivedInMillis = millis();
      receivedFileData += rxValue;
    } else if (rxValue == "Start name tx") {
      lastBtCommReceivedInMillis = millis();
      receivingBtName = true;
      receivedFileName = "";
      Serial.println(rxValue);
    } else if (rxValue == "Stop name tx") {
      lastBtCommReceivedInMillis = millis();
      receivingBtNameEnded = true;
      Serial.println(rxValue);
    } else if (receivingBtName) {
      lastBtCommReceivedInMillis = millis();
      receivedFileName += rxValue;
    }
  }
};

class SettingsCharacteristicCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String rxValue = pChar->getValue();

    if (rxValue == "Start settings tx") {
      lastBtCommReceivedInMillis = millis();
      receivingSettingsBtData = true;
      receivedSettingsData = "";
      Serial.println(rxValue);
    } else if (rxValue == "Stop settings tx") {
      lastBtCommReceivedInMillis = millis();
      receivingSettingsBtDataEnded = true;
      Serial.println(rxValue);
    } else if (receivingSettingsBtData) {
      lastBtCommReceivedInMillis = millis();
      receivedSettingsData += rxValue;
    }
  }
};

void startBluetoothLE() {
  BLEDevice::init(DEVICE_BT_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pFileCharacteristic = pService->createCharacteristic(
    FILE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  pFileCharacteristic->addDescriptor(new BLE2902());
  descriptor_2901 = new BLE2901();
  descriptor_2901->setDescription("For receiving recipes");
  descriptor_2901->setAccessPermissions(ESP_GATT_PERM_READ);  // enforce read only - default is Read|Write
  pFileCharacteristic->addDescriptor(descriptor_2901);
  pFileCharacteristic->setCallbacks(new FileCharacteristicCallback());

  pSettingsCharacteristic = pService->createCharacteristic(
    SETTINGS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  pSettingsCharacteristic->addDescriptor(new BLE2902());
  descriptor_2901_1 = new BLE2901();
  descriptor_2901_1->setDescription("For receiving settings");
  descriptor_2901_1->setAccessPermissions(ESP_GATT_PERM_READ);  // enforce read only - default is Read|Write
  pSettingsCharacteristic->addDescriptor(descriptor_2901);
  pSettingsCharacteristic->setCallbacks(new SettingsCharacteristicCallback());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection...");
}

void writeProtocolFromBt() {
  File configFile = SD.open("/proto_" + receivedFileName + ".txt", FILE_WRITE);
  if (configFile) {
    configFile.seek(0);
    configFile.println(receivedFileData);
    configFile.close();
    Serial.println("Vytvořen nový protokol na SD kartu");
    LogEvent("Protocol added");
  }

  receivedFileData = "";
  receivedFileName = "";
}

void fillSettingsFromFile() {
  File configFile = SD.open("/config.txt", FILE_READ);
  
  if (!configFile) {
    Serial.println("Nepodařilo se otevřít config.txt");
    return;
  }

  int row = 0;
  while (configFile.available()) {
    String line = configFile.readStringUntil('\n');
    row++;

    switch (row) {
      case 5: WIFI_SSID = line; break;
      case 6: WIFI_PASSWORD = line;  break;
      case 7: SMTP_HOST = line; break;
      case 8: SMTP_PORT = line.toInt(); break;
      case 9: SENDER_EMAIL = line;  break;
      case 10: SENDER_PASSWORD = line;  break;
      case 11: RECIPIENT_EMAIL = line;  break;
    }
  }

  configFile.close();
}

void setSettingsFromFile() {
  fillSettingsFromFile();
  startBluetoothLE();
}

void writeSettingsFromBt() {

  const int firstIndexFromFile = 4;
  File configFile = SD.open("/config.txt", FILE_READ);
  if (configFile) {
    String radky[firstIndexFromFile];
    int idx = 0;
    while (configFile.available() && idx < firstIndexFromFile) {
      radky[idx++] = configFile.readStringUntil('\n');
    }
    configFile.close();
    
    // Zapiš zpět
    configFile = SD.open("/config.txt", FILE_WRITE);
    if (configFile) {
      configFile.seek(0);
      for (int i = 0; i < idx; i++) {
        configFile.println(radky[i]);
      }
      configFile.println(receivedSettingsData);
      configFile.close();
      Serial.println("Zapsané nastavení z BT do config.txt");
      LogEvent("Settings from BT set");
    }
  }

  receivedSettingsData = "";
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////











// ====== setup ======
void setup() {
  // inicializace výpisu na sériový port
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Koupač vzorků"); 
  
    //definice I2C sběrnice  
Wire.begin(SDA, SCL);  pwm.begin(); pwm.setPWMFreq(PWM_FREQ_HZ); // porty I2C (hodiny, DC motor): SDA = 21, SCL = 22
  pinMode(EN,OUTPUT); digitalWrite(EN,EN_IDLE_LEVEL);  //inicializace pro motory a čidla
      pinMode(PIN_LIMIT_UP, INPUT); pinMode(PIN_LIMIT_DWN, INPUT); pinMode(PIN_ZERO,INPUT); // motory a koncáky 
  
  // inicializace zobrazení
  Serial.println("TFT_eSPI library test!");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);

  // inicializace SD karty a výpis jejího typu a velikosti
#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
    Serial.println("Card Mount Failed");

    // Zobrazit chybovou hlášku na displej a zastavit program
    tft.fillRect(20, 40, 280, 200, TFT_RED);       // červený obdélník
    tft.setTextColor(TFT_WHITE, TFT_RED);          // bílý text na červeném pozadí
    tft.setTextSize(3);
    tft.setCursor(70, 100);                         // první řádek textu
    tft.println("No card");
    tft.setCursor(50, 150);                         // druhý řádek textu
    tft.println("No fun :-)");
    while (1);                                      // zastavení programu
  } else {
#else
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");

    // Zobrazit chybovou hlášku na displej a zastavit program
    tft.fillRect(20, 40, 280, 200, TFT_RED);       // červený obdélník
    tft.setTextColor(TFT_WHITE, TFT_RED);          // bílý text na červeném pozadí
    tft.setTextSize(3);
    tft.setCursor(70, 100);                         // první řádek textu
    tft.println("No card");
    tft.setCursor(50, 150);                         // druhý řádek textu
    tft.println("No fun :-)");
    while (1);                                      // zastavení programu
  } else {
#endif
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
    } else {
      Serial.print("SD Card Type: ");
      if (cardType == CARD_MMC) {
        Serial.println("MMC");
      } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
      } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
      } else {
        Serial.println("UNKNOWN");
      }

      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %lluMB\n", cardSize);
      Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
      Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
      listDir(SD, "/", 0);
    }
    AktivniProtokol();
  }

  // tlačítka
  pinMode(TL_Menu, INPUT_PULLUP);  // TL_Menu = GPIO 0
  attachInterrupt(digitalPinToInterrupt(TL_Menu), menuButtonISR, FALLING);
  Serial.println("Inicializace tlacitka MENU"); 
  
  pinMode(TL_Enter, INPUT_PULLUP);  // TL_Enter = GPIO 17
  attachInterrupt(digitalPinToInterrupt(TL_Enter), enterButtonISR, FALLING);
  Serial.println("Inicializace tlacitka ENTER"); 

    
  // hodiny
  if (!rtc.begin()) {
    Serial.println("RTC modul nebyl nalezen!");
    while (1);
  }

  // Pokud RTC nemá platný čas, nastav testovací 
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(2025, 5, 22, 11, 55, 0)); // 22.5.2025 11:55
    Serial.println("Nastaveno testovací datum"); 
  }

  // inicializace encoderu
  pinMode(ENC_KEY, INPUT_PULLUP);
  pinMode(ENC_S1, INPUT);
  pinMode(ENC_S2, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENC_KEY), ENC_key_ISR, FALLING);

  kresliramecek();
  kreslizalozku();
  switch (aktivnizalozka) {
    case 0: kresliSTAT(); break;
    case 1: kresliRUN();  break;
    case 2: kresliPROG(); break;
    case 3: kresliFILL(); break;
    case 4: kresliSET();  break;
  }

 // === [v13] doplnění setup z test_obou_motoru_06 (beze změn) ===
       // S4
  Serial.println("Test obou motoru 06 - setup"); // M
    motor.setMinPulseWidth(3); motor.setMaxSpeed(NORM_MAX_SPEED); motor.setAcceleration(NORM_ACCEL); motor.setCurrentPosition(0); // S4
    Motor_STOP(); if (upHit()) nextIsUp=false; else if (dwnHit()) nextIsUp=true; // D5

    // Požadavek: nejprve rameno do UP, pak nulování
    if (!upHit()) Motor_UP();                                   // D5
    ZeroPositionFast();                              // M
    Serial.println("Probehl setup");
    LogEvent("Setup done");
  programState = 1; // [v18] čeká se na Enter
  if (aktivnizalozka == 0) { kresliSTAT(); } // [v18] překresli STAT

  // === [v13] konec doplnění setup === 

  setSettingsFromFile();
}





// obsluha tlačítka Enter
void zpracujEnter(){
  // Zpracování stisku tlačítka Enter s odrušením
  static bool stiskEnter = false;  // uchovává stav (zobrazený / nezobrazený)
  
  if (enterButtonInterruptFlag) {
    unsigned long currentTime3 = millis();
    if (currentTime3 - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime3;
Serial.println("Enter stisknut");
      switch (aktivnizalozka) {
  
case 0:

// [v20a] Toggle pause overlay on STAT with consecutive ENTER presses
static bool statPauseOverlay = false; // local static to avoid new globals
if (programState == 2 && statCountdownActive && statCountdown > 0) {
  if (!statPauseOverlay) {
   
    // [v20b_fix] zapnutí pauzy při prvním ENTERu
if (!protPaused) {
  long nowMs = (long)millis();
  // protUntil = „deadline“ DWN fáze v ms – proměnná u vás už existuje
  protPausedRemainMs = (protUntil > nowMs) ? (protUntil - nowMs) : 0;
  protPaused = true;
}
    int boxX = 50, boxY = 375, boxW = 220, boxH = 45;
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 5, RED);
    tft.setTextColor(WHITE, RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Paused", boxX + boxW / 2, boxY + boxH / 2 - 10);
    tft.drawString("Enter to continue", boxX + boxW / 2, boxY + boxH / 2 + 14);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
    statPauseOverlay = true;
        LogEvent("Protocol paused");
    return;
  } else {
    statPauseOverlay = false;
    // [v20b_fix] vypnutí pauzy při druhém ENTERu
if (protPaused) {
  protPaused = false;
  protUntil = millis() + (unsigned long)protPausedRemainMs;  // navázání dojezdu DWN
  // protPausedRemainMs ponecháme – pro další pauzu se přepočítá znovu
      LogEvent("Protocol continues after pause");
}
    kresliSTAT(); // hide overlay
    return;
  }
}
       
    // [v19] STAT: 1. ENTER -> zelené okno; 2. ENTER -> start protokolu
    if (programState == 1 && !statGreenBox) {
      statGreenBox = true;
      kresliSTAT();
      Serial.println("[v19] STAT: show green box");
    } else if (programState == 1 && statGreenBox) {
      statGreenBox = false;
      protocolActive = true;
      programState = 2;
      protLine = 4; protPhase = 0; protFirstPair = true;
      statStatusWord = ""; statStatusSecs = -1;
      kresliSTAT();
      Serial.println("[v19] STAT: start protocol");
      LogEvent("Protocol started");
    }
    break;

        case 1:
  // RUN: potvrzování vybrané položky podobně jako FILL
  if (!stiskEnter && rezimVyberu) {
    // 1. stisk ENTER: zobraz potvrzovací dialog se jménem položky
    tft.fillRoundRect(50, 100, 220, 105, 5, GREEN);
    tft.setTextColor(WHITE, GREEN);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);

    String runText;
    switch (aktivniVolba) {
      case 0: runText = "Lift UP"; break;
      case 1: runText = "Lift DWN"; break;
      case 2: runText = "Carousel zero"; break;
      case 3: runText = "Carousel +1 (FWD)"; break;
      case 4: runText = "Carousel -1 (BCW)"; break;
      case 5: runText = "STOP"; break;
      case 6: runText = "ZERO Position"; break;
      default: runText = "RUN action"; break;
    }

    tft.drawString("RUN action:", 60, 110);
    tft.drawString(runText,      60, 140);
    tft.drawString("Press Enter",60, 170);

    Serial.print("RUN: vybrana polozka = ");
    Serial.println(runText);
    stiskEnter = true;
  } else if (stiskEnter) {
    // 2. stisk ENTER: proveď akci dle výběru
    Serial.print("RUN: provadim akci ");
    Serial.println(aktivniVolba);

    switch (aktivniVolba) {
      case 0: Motor_UP(); break;
      case 1: Motor_DWN(); break;
      case 2: ZeroPositionFast(); break;
      case 3: karusel_plus1(); break;
      case 4: karusel_minus1(); break;
      case 5: Motor_STOP(); motor.stop(); break;  // okamžité zastavení všech pohybů
      case 6: ZeroPositionFast(); Motor_UP(); break; // jako při setupu: karusel na nulu + rameno UP
    }

    // překreslit RUN a znovu podtrhnout aktuální volbu
    kresliRUN();
    int xLeft = 17;
    int yBase = 100;
    int lineHeight = 45;
    int y = yBase + (aktivniVolba) * lineHeight;
    tft.fillRect(xLeft - 3, y + lineHeight - 20, 280, 2, TFT_WHITE);

    stiskEnter = false;
  }
  break;

        case 2: break;
        
case 3:
  if (!stiskEnter && aktivniVolba > 0) {
    tft.fillRoundRect(50, 100, 220, 105, 5, GREEN);
    Serial.println("FILL - Enter ON");
    stiskEnter = true;

    tft.setTextColor(WHITE, GREEN);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Fill tank with", 60, 110);                // první řádek
    tft.drawString(AktivniLatky[aktivniVolba - 1], 60, 140);  // druhý řádek – název látky
    tft.drawString("and press Enter", 60, 170);               // třetí řádek

    Serial.print("Budeme plnit: ");
    Serial.println(aktivniVolba);
    Serial.print("Plnena latka: ");
    Serial.println(AktivniLatky[aktivniVolba - 1].c_str());

  } else if (stiskEnter) {
    kresliFILL();  // překresli displej
    Serial.println("Naplneno - posun o jednu");
   karusel_plus1(); 
    stiskEnter = false;
  }
  break;


        case 4: break;
      }
    }
    enterButtonInterruptFlag = false;  // vlajku resetuj vždy až po vyhodnocení
  }
}


//funkce obsluhy encodéru
void ENC_key() {
  Serial.println("KEY stisknut!");

      switch (aktivnizalozka) {
        case 0:  break;
        case 1: {
      // Karta RUN: po stisku KEY začni výběr a podtrhni první položku
      // Používáme stejné proměnné jako jinde (rezimVyberu, aktivniVolba)
      if (!rezimVyberu) {
        rezimVyberu = true;
        aktivniVolba = 0;  // první položka
        // vykreslit podtržítko pod první položkou
        int xLeft = 17;
        int yBase = 100;
        int lineHeight = 45;
        int y = yBase + (aktivniVolba) * lineHeight;
        // podtržení
        tft.fillRect(xLeft - 3, y + lineHeight - 20, 280, 2, TFT_WHITE);
        Serial.println("RUN: rezimVyberu ON, podtrzena 1. položka");
      } else {
        // již jsme ve výběru – nic nepotvrzujeme, pouze držíme režim
        Serial.println("RUN: KEY v rezimu vyberu (bez akce)");
      }
      break;
}
        case 2:  // PROG karta
                   if (!rezimVyberu) {
                    rezimVyberu = true;
                    aktivniVolba = 0;
                    NactiSeznamProtokolu();  // Opravený název procedury
                    kresliVyberProtokolu();
                  } else {
                    // Pokud jsme v režimu výběru, zapíšeme vybraný protokol do config.txt
                    File configFile = SD.open("/config.txt", FILE_READ);
                    if (configFile) {
                      String radky[10];
                      int idx = 0;
                      while (configFile.available() && idx < 10) {
                        radky[idx++] = configFile.readStringUntil('\n');
                      }
                      configFile.close();
            
                      // Přepiš čtvrtý řádek (index 3) novým protokolem
                      if (aktivniVolba < pocetProtokolu) {
                        radky[3] = seznamProtokolu[aktivniVolba];
                      }
            
                      // Zapiš zpět
                      configFile = SD.open("/config.txt", FILE_WRITE);
                      if (configFile) {
                        configFile.seek(0);
                        for (int i = 0; i < idx; i++) {
                          configFile.println(radky[i]);
                        }
                        configFile.close();
                        Serial.println("Zapsán nový protokol do config.txt");
                        LogEvent("Protocol set");
                      }
                    }
            
                    rezimVyberu = false;
                    NactiProtokol();
                    kresliPROG();
                  } break;
        
        
        case 3:       // Aktivace režimu výběru a podtržení první položky
      if (!rezimVyberu) {
        rezimVyberu = true;
        aktivniVolba = 0;

        int xLeft = 17;
        int yBase = 63;
        int lineHeight = 25;
        int y = yBase + (aktivniVolba + 1) * lineHeight;
        tft.fillRect(xLeft - 3, y + lineHeight - 3, 280, 2, TFT_WHITE);
        Serial.println("Režim výběru aktivován, podtržena první položka");
      }
      break;

        
        case 4:  nastavCas = true;   Serial.println("nastavuju cas"); // Přepni režim nastavování času a data                 
                 Podtrzitko = Podtrzitko+1;
                 switch (Podtrzitko) {                                                  
                 case 1: 
                     // tady se načte aktuální čas a uloží do proměnných pro změnu
                     hodin = now.hour();
                     minut = now.minute();
                     den = now.day();
                     mesic = now.month();                
                     rok = now.year();
                          Serial.print("nacetl jsem cas pro upravy:");         
                          Serial.print(hodin);Serial.print(minut);Serial.print(den);Serial.print(mesic);Serial.println(rok);                
                          tft.fillRect(138, 118, 25, 2, TFT_WHITE); //nakreslí podtržítko pod dny
                          Serial.println("nastavování dd ");break;
 
                  case 2: tft.fillRect(138, 118, 25, 2,  BARVA_RAMU); //nakreslí podtržítko pod měsíci
                          tft.fillRect(172, 118, 25, 2, TFT_WHITE); 
                          Serial.println("nastavování mm ");break;
                 
                  case 3: tft.fillRect(172, 118, 25, 2,  BARVA_RAMU); //nakreslí podtržítko pod roky
                          tft.fillRect(231, 118, 25, 2, TFT_WHITE); 
                          Serial.println("nastavování yy ");break;
                  
                  case 4:tft.fillRect(231, 118, 25, 2, BARVA_RAMU); 
                         tft.fillRect(138, 162, 25, 2, TFT_WHITE); //nakreslí podtržítko pod hodinami 
                         Serial.println("nastavování hh");break;
 
                  case 5: tft.fillRect(138, 162, 25, 2,  BARVA_RAMU); //nakreslí podtržítko pod minutami 
                          tft.fillRect(172, 162, 25, 2, TFT_WHITE); 
                          Serial.println("nastavování mm");break;
                  
                  case 6: tft.fillRect(172, 162, 25, 2,  BARVA_RAMU); //zruší podtržítko pod minutami 
                  Podtrzitko =0;       // vynuluje podtržítko a tím ukončí nastavení času 
                  SetTIME();// sem přijde uložení nastaveného času , zatím test 
                  Serial.println("KONEC nastavování, čas uložen");
                  LogEvent("Time set");
                  nastavCas = false;  // Přepni režim nastavování času    break;   
                   }
                  
                  break;
      }
}

void ENC_CW() {
  Serial.println("Otočeno doprava (CW)");
  switch (aktivnizalozka) {
    case 0: break;
    case 1: {
      // Karta RUN: posun podtržítka dolů (cyklicky)
      if (rezimVyberu) {
        const int runPolozek = 7; // počet položek v RUN menu
        int xLeft = 17;
        int yBase = 100;
        int lineHeight = 45;
        // smazat staré podtržítko
        int yOld = yBase + (aktivniVolba) * lineHeight;
        tft.fillRect(xLeft - 3, yOld + lineHeight - 20, 280, 2, BARVA_RAMU);
        // posunout výběr
        aktivniVolba = (aktivniVolba + 1) % runPolozek;
        // vykreslit nové podtržítko
        int yNew = yBase + (aktivniVolba) * lineHeight;
        tft.fillRect(xLeft - 3, yNew + lineHeight - 20, 280, 2, TFT_WHITE);
        Serial.print("RUN: vybraná položka "); Serial.println(aktivniVolba+1);
      }
      break;
}

    case 2: // karta PROG
      if (rezimVyberu) {
        // režim výběru protokolu – posun výběru dolů
        aktivniVolba = (aktivniVolba + 1) % pocetProtokolu;
        kresliVyberProtokolu();
      } else {
        // běžné scrollování položek protokolu
        if (progScrollOffset < 4) {
          progScrollOffset++;

          // Výmaz prostoru pro výpis
          tft.fillRect(7, 81, 306, 392, BARVA_RAMU);
          tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
          tft.setTextSize(2);
          tft.setTextDatum(TL_DATUM);

          int xLeft = 17;
          int yBase = 100;
          int lineHeight = 22;

          // Záhlaví
          tft.drawString("Protocol:", xLeft, yBase - 5);
          tft.drawString(AktivniProtokol(), xLeft + 110, yBase - 5);

          // Výpis 16 položek s posunem
          for (int i = 0; i < 16; i++) {
            int index = i + progScrollOffset;
            if (index >= 20) break;

            int y = yBase + (i + 1) * lineHeight;
            String radek = String(index + 1) + ".  " + AktivniLatky[index] + " " + String(aktivniCasy[index]) + " s";
            tft.drawString(radek, xLeft, y);
          }

          Serial.print("Scroll dolů: offset = ");
          Serial.println(progScrollOffset);
        }
      }
      break;

    case 3:  // Posun podtržítka dolů v režimu výběru na kartě FILL
      if (rezimVyberu && aktivniVolba < 20) {
        int xLeft = 17;
        int yBase = 63;
        int lineHeight = 25;

        // Zruš staré podtržítko
        int yOld = yBase + (aktivniVolba - fillScrollOffset) * lineHeight;
        tft.fillRect(xLeft - 3, yOld + lineHeight - 5, 280, 2, BARVA_RAMU);

        aktivniVolba++;

        // Pokud podtržítko mělo zůstat na řádku 15, ale přibyla další položka, scrolluj
        if ((aktivniVolba - fillScrollOffset) > 15) {
          fillScrollOffset++;
        }

        // Překresli celý výpis 15 položek
        tft.fillRect(15, yBase+24, 320, 15 * lineHeight, BARVA_RAMU);
        for (int i = 1; i < 16; i++) {
          int index = fillScrollOffset + i;
          if (index < 21) {
            tft.setCursor(xLeft, yBase + i * lineHeight);
            tft.setTextColor(TFT_WHITE, BARVA_RAMU);
            tft.setTextSize(2);
            tft.printf("%2d. %s ", index, AktivniLatky[index-1].c_str());
          }
        }

        // Vykresli nové podtržítko
        int yNew = yBase + (aktivniVolba - fillScrollOffset) * lineHeight;
        tft.fillRect(xLeft - 3, yNew + lineHeight - 5, 280, 2, TFT_WHITE);

        Serial.print("Podtrženo položka: ");
        Serial.println(aktivniVolba);
      }
      break;


    case 4:
      switch (Podtrzitko) {
        case 1: den++; if (den > 31) den = 1;
          tft.fillRect(134, 100, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(den), 137, 100);
          Serial.println(den); break;

        case 2: mesic++; if (mesic > 12) mesic = 1;
          tft.fillRect(172, 100, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(mesic), 173, 100);
          Serial.println(mesic); break;

        case 3: rok++; if (rok > 2050) rok = 2024;
          tft.fillRect(208, 100, 55, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(rok), 209, 100);
          Serial.println(rok); break;

        case 4: hodin++; if (hodin > 23) hodin = 0;
          tft.fillRect(134, 145, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(hodin), 137, 145);
          Serial.println(hodin); break;

        case 5: minut++; if (minut > 59) minut = 0;
          tft.fillRect(172, 145, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(minut), 173, 145);
          Serial.println(minut); break;

        case 6: break;
      }
      break;
  }
}


void ENC_CCW() {
  Serial.println("Otočeno doleva (CCW)");

  switch (aktivnizalozka) {
    case 0:
      break;

    case 1: {
      // Karta RUN: posun podtržítka nahoru (cyklicky)
      if (rezimVyberu) {
        const int runPolozek = 7; // počet položek v RUN menu
        int xLeft = 17;
        int yBase = 100;
        int lineHeight = 45;
        // smazat staré podtržítko
        int yOld = yBase + (aktivniVolba) * lineHeight;
        tft.fillRect(xLeft - 3, yOld + lineHeight - 20, 280, 2, BARVA_RAMU);
        // posunout výběr nahoru (cyklicky)
        aktivniVolba = (aktivniVolba - 1 + runPolozek) % runPolozek;
        // vykreslit nové podtržítko
        int yNew = yBase + (aktivniVolba) * lineHeight;
        tft.fillRect(xLeft - 3, yNew + lineHeight - 20, 280, 2, TFT_WHITE);
        Serial.print("RUN: vybraná položka "); Serial.println(aktivniVolba+1);
      }
      break;
}

    case 2: // karta PROG
      if (rezimVyberu) {
        // posun výběru souboru nahoru
        aktivniVolba = (aktivniVolba - 1 + pocetProtokolu) % pocetProtokolu;
        kresliVyberProtokolu();
      } else {
        if (progScrollOffset > 0) {
          progScrollOffset--;

          // Výmaz prostoru pro výpis
          tft.fillRect(7, 81, 306, 392, BARVA_RAMU);
          tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
          tft.setTextSize(2);
          tft.setTextDatum(TL_DATUM);

          int xLeft = 17;
          int yBase = 100;
          int lineHeight = 22;

          // Záhlaví
          tft.drawString("Protocol:", xLeft, yBase - 5);
          tft.drawString(AktivniProtokol(), xLeft + 110, yBase - 5);

          // Výpis 16 položek s posunem
          for (int i = 0; i < 16; i++) {
            int index = i + progScrollOffset;
            if (index >= 20) break;

            int y = yBase + (i + 1) * lineHeight;
            String radek = String(index + 1) + ".  " + AktivniLatky[index] + " " + String(aktivniCasy[index]) + " s";
            tft.drawString(radek, xLeft, y);
          }

          Serial.print("Scroll nahoru: offset = ");
          Serial.println(progScrollOffset);
        }
      }
      break;

    case 3:
      break;
  
    case 4:
      switch (Podtrzitko) {
        case 1:
          den--;
            if (den < 1) {
            den = 31;
            }
          tft.fillRect(134, 100, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(den), 137, 100);
          Serial.println(den);
          break;

        case 2:
          mesic--;
          if (mesic < 1) {
          mesic = 12;
          }

          tft.fillRect(172, 100, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(mesic), 173, 100);
          Serial.println(mesic);
          break;

        case 3:
          rok--;
          if (rok < 2024) {
            rok = 2050;
          }

          tft.fillRect(208, 100, 55, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(rok), 209, 100);
          Serial.println(rok);
          break;

        case 4:
          hodin--;
          if (hodin < 0) {
            hodin = 23;
          }

          tft.fillRect(134, 145, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(hodin), 137, 145);
          Serial.println(hodin);
          break;

        case 5:
          minut--;
          if (minut < 0) {
            minut = 59;
          }
          tft.fillRect(172, 145, 25, 16, BARVA_RAMU);
          tft.setTextColor(TFT_WHITE, BARVA_RAMU);
          tft.drawString(String(minut), 173, 145);
          Serial.println(minut);
          break;

        case 6:
          break;
      }
      break;
  }
}

void zpracujEnkoder() {
  static int lastEncoded = 0;
  int MSB = digitalRead(ENC_S1); // Most Significant Bit
  int LSB = digitalRead(ENC_S2); // Least Significant Bit

  int encoded = (MSB << 1) | LSB;  // kombinuje do 2bit čísla
  int sum = (lastEncoded << 2) | encoded; // 4bit historie

  unsigned long currentTime = millis();
  if (currentTime - lastEncoderEvent > encoderDebounceDelay) {
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
      ENC_CW();  // otočeno doprava
      lastEncoderEvent = currentTime;
    }
    else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
      ENC_CCW(); // otočeno doleva
      lastEncoderEvent = currentTime;
    }
  }

  lastEncoded = encoded;

if (keyPressed && (millis() - lastEncoderEvent > encoderDebounceDelay)) {
  ENC_key();
  keyPressed = false;
  lastEncoderEvent = millis();

}

}

// funkce pro kreslení obsahu jednotlivých karet menu

// [v19] === Pomocné čtení protokolu ===
String ReadProtocolLine(int lineNo){
  String prot = AktivniProtokol();
  prot.trim();
  if (prot.length()==0) return "";
  if (!prot.startsWith("/")) prot = "/" + prot;
  File f = SD.open(prot.c_str());
  if (!f) { Serial.println("[v19] Nelze otevřít protokol: " + prot); return ""; }
  int rn = 0;
  String line="";
  while (f.available()){
    line = f.readStringUntil('\n');
    rn++;
    if (rn==lineNo){ f.close(); line.trim(); return line; }
  }
  f.close();
  return "";
}
bool ReadProtocolPair(int baseLine, String &word, int &secs){
  word = ReadProtocolLine(baseLine);
  if (word.length()==0) return false;
  String num = ReadProtocolLine(baseLine+1);
  num.trim();
  if (num.length()==0) return false;
  secs = num.toInt();
  return true;
}
void kresliSTAT() {
  tft.fillRect(7, 81, 306, 392, BARVA_RAMU);   // Výmaz prostoru pro obsah
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);                         // Písmo menší (nebo 3, podle potřeby)
  tft.setTextDatum(TL_DATUM);                 // Zarovnání vlevo nahoře

  int x = 17;                                 // Odsazení od levého okraje
  int y = 100;                                // Posun dolů, aby nezasahovalo do záložek
  int lineHeight = 45;                        // Menší mezery, ale vyvážené

  tft.drawString("Date:",    x, y + lineHeight * 0);
  tft.drawString("Time:",    x, y + lineHeight * 1);
  tft.drawString("Power:",   x, y + lineHeight * 2);
  tft.drawString("Battery:", x, y + lineHeight * 3);
  tft.drawString("Network:", x, y + lineHeight * 4);
  //tft.drawString("Program:", x, y + lineHeight * 5);
  tft.drawString("Protocol: ",    x, y + lineHeight * 5);
  tft.drawString(AktivniProtokol (), x+110, y + lineHeight * 5);
  tft.drawString("Status:",  x, y + lineHeight * 6);;


  // --- Dynamika vedle "Status:" ---
  int sx = x + 86;                 // začátek textu za "Status:"
  int sy_status = y + lineHeight * 6;
  int sy_setup  = y + lineHeight * 7;

  // Vyčistit pouze oblast za "Status:" (nikoli samotný štítek)
  tft.fillRect(sx, sy_status - 5, 180, lineHeight + 10, BARVA_RAMU);

  // Vyčistit informační pás pod "Setup:" pro velký nápis
  tft.fillRect(160 - 110, sy_setup - 22, 220, 44, BARVA_RAMU);

  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);

 if (programState == 1 && !statGreenBox) {

    // Po setupu: výzva ke spuštění
    tft.drawString("to start program", sx, sy_status);
    // Velký nápis níže (mimo řádky Status a Setup)
    tft.setTextSize(3);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("press Enter", 160, sy_setup + 0);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
  }
  // [v19] zelené okénko dvouřádkově (centrované)
  if (statGreenBox) {
    int boxX = 50, boxY = sy_status + 50, boxW = 220, boxH = 48;
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("To start protocol", boxX + boxW/2, boxY + boxH/2 - 10);
    tft.drawString("press Enter",      boxX + boxW/2, boxY + boxH/2 + 14);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  } else if (programState == 2) {
    // Horní řádek "Status:"
    tft.drawString("protocol Running", sx, sy_status);
    // Další řádek: aktuální krok "<slovo> <sekundy>"
    if (statStatusWord.length()>0 && statStatusSecs>=0){
      String s = statStatusWord + " " + String(statStatusSecs) + " s";
      tft.fillRect(sx, sy_setup - 5, 220, 22 + 10, BARVA_RAMU);
      tft.drawString(s, sx, sy_setup);
      // [v19e] remember pixel position where the number starts so PrepisTIME can overwrite digits
      statCountY = sy_setup;
      int prefixW = tft.textWidth(statStatusWord + " ");
      statCountX  = sx + prefixW;
      // [v19e] initialize live countdown for the new step (will start ticking when DWN engages)
      statCountdown = statStatusSecs;
      statCountdownPrev = -1;

    } else {
      tft.fillRect(sx, sy_setup - 5, 220, 22 + 10, BARVA_RAMU);
    }
  }
  // [v19] Zelené okénko na STAT (styl jako na kartě RUN)
  if (statGreenBox) {
    int boxX = 50;
    int boxY = sy_status + 50;   // spodní část karty STAT
    int boxW = 220;
    int boxH = 45;
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 5, GREEN);
    tft.setTextColor(WHITE, GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    // [v19] rozdělení textu do dvou řádků, zarovnané na střed
    tft.drawString("To start protocol", boxX + boxW / 2, boxY + boxH / 2 - 10);
    tft.drawString("press Enter", boxX + boxW / 2, boxY + boxH / 2 + 14);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  }

  // Vracení písma na 2, aby se nezměnilo jinde (pokud potřebuješ jinou velikost, uprav tam)
  tft.setTextSize(2);
  Serial.println("Vykreslena karta STAT"); 

}



 //karta RUN
void kresliRUN() {
  tft.fillRect(7, 81, 306, 392, BARVA_RAMU);   // Výmaz prostoru pro obsah

  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);                         // Písmo menší (nebo 3, podle potřeby)
  tft.setTextDatum(TL_DATUM);                 // Zarovnání vlevo nahoře

  int x = 17;                                 // Odsazení od levého okraje
  int y = 100;                                // Posun dolů, aby nezasahovalo do záložek
  int lineHeight = 45;                        // Menší mezery, ale vyvážené

  tft.drawString("Lift UP:",    x, y + lineHeight * 0);
  tft.drawString("Lift DWN",    x, y + lineHeight * 1);
  tft.drawString("Carousssel zero",   x, y + lineHeight * 2);
  tft.drawString("Caroussel 1 FWD", x, y + lineHeight * 3);
  tft.drawString("Caroussel 1 BCW", x, y + lineHeight * 4);
  tft.drawString("STOP" , x, y + lineHeight * 5);
  tft.drawString("ZERO Position",  x, y + lineHeight * 6);
  tft.setTextSize(2);  // Vracení písma na 2
  Serial.println("Vykreslena karta RUN"); 
}

// Karta PROG
void kresliPROG() {
  tft.fillRect(7, 81, 306, 392, BARVA_RAMU);   // Výmaz prostoru pro obsah
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);                         // Velikost písma
  tft.setTextDatum(TL_DATUM);                 // Zarovnání vlevo nahoře

  int xLeft = 17;                             // X pozice textu
  int yBase = 100;                            // Y základna pro řádky
  int lineHeight = 22;                        // Výška řádku

  // Záhlaví s názvem protokolu (posunuto o 5 px výš bez zvláštní proměnné)
  tft.drawString("Protocol:", xLeft, yBase - 5);
  tft.drawString(AktivniProtokol(), xLeft + 110, yBase - 5);
  NactiProtokol();

  // Výpis pouze 16 položek
  for (int i = 0; i < 16; i++) {
    int y = yBase + (i + 1) * lineHeight;

    // Sestavení řádku s číslem, názvem a časem
    String radek = String(i + 1) + ".  " + AktivniLatky[i] + " " + String(aktivniCasy[i]) + " s";

    tft.drawString(radek, xLeft, y);
  }

  Serial.println("Vykreslena karta PROG (16 řádků, číslování, záhlaví -5px)");
}


 //karta FILL
void kresliFILL() {
  tft.fillRect(7, 81, 306, 392, BARVA_RAMU);   // Výmaz prostoru pro obsah
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);                         // Písmo menší (nebo 3, podle potřeby)
  tft.setTextDatum(TL_DATUM);                 // Zarovnání vlevo nahoře
 
  //následující kus je zkopírovaný z karty PROG aby to bylo stejné
  int xLeft = 17;                             // X pozice textu
  int yBase = 63;                            // Y základna pro řádky
  int lineHeight = 25;                        // Menší mezery, ale vyvážené

 NactiProtokol();
Serial.print("karta FILL");
  // Výpis pouze 15 položek aby se vešly
  for (int i = 0; i < 15; i++) {
    int y = yBase + (i + 1) * lineHeight;

    // Sestavení řádku s číslem, názvem a časem
    String radek = String(i + 1) + ".  " + AktivniLatky[i] ;

    tft.drawString(radek, xLeft, y);
  //Serial.print("karta FILL"); 
 // Serial.print(" radek "); 
 // Serial.println(radek); 
}
}


 //karta SET
void kresliSET() {
  tft.fillRect(7, 81, 306, 392, BARVA_RAMU);   // Výmaz prostoru pro obsah
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  tft.setTextSize(2);                         // Písmo menší (nebo 3, podle potřeby)
  tft.setTextDatum(TL_DATUM);                 // Zarovnání vlevo nahoře

  int x = 17;                                 // Odsazení od levého okraje
  int y = 100;                                // Posun dolů, aby nezasahovalo do záložek
  int lineHeight = 45;                        // Menší mezery, ale vyvážené

  tft.drawString("Set date:",    x, y + lineHeight * 0);
  tft.drawString("Set time:",    x, y + lineHeight * 1);
  tft.drawString("Set:",   x, y + lineHeight * 2);
  tft.drawString("Set:", x, y + lineHeight * 3);
  tft.drawString("Set:", x, y + lineHeight * 4);
  tft.drawString("Set:", x, y + lineHeight * 5);
  tft.drawString("Set:",  x, y + lineHeight * 6);
  tft.setTextSize(2);// Vracení písma na 2
  Serial.println("Vykreslena karta SET"); 
}

// Rámeček kolem displeje a hlavička s verzí
void kresliramecek()
{
  // Rámeček kolem displeje
  tft.fillRect(0, 0, 320, 12, BARVA_RAMU);   // Horní
  tft.fillRect(0, 0, 5, 480, BARVA_RAMU);    // Levý
  tft.fillRect(0, 475, 320, 5, BARVA_RAMU);  // Dolní
  tft.fillRect(315, 0, 5, 480, BARVA_RAMU);  // Pravý
  tft.fillRect(5, 12, 308, 461, TFT_BLACK);   // vnitřní černé pole znovu pro jistotu

  // Hlavička s verzí programi v rámečku
  tft.setTextColor(TFT_WHITE, BARVA_RAMU);  // Bílý text, pozadí rámečku
  tft.setTextSize(1);
  tft.setCursor(80, 2);
  tft.print("Koupac vzorku LFHK");
  tft.setCursor(285, 2); // ručně zvolená pozice
  tft.print("v.22a");     // TADY SE ZADAVA VERZE FIRMWARE TADY SE ZADAVA VERZE FIRMWARE TADY SE ZADAVA VERZE FIRMWARE TADY SE ZADAVA VERZE FIRMWARE
  tft.setTextSize(2); //vrácení písma na základní
  Serial.println("Vykreslen ramecek"); 
}

// kreslení záložek 
void kreslizalozku() {
  const int startX = 7;               // levý okraj vnitřního bloku
  const int plocha = 306;             // dostupná šířka bez rámu (320 - 2×7 px)
  const int mezera = 1;               // jednotná mezera mezi záložkami
  const int pocetMezer = pocetZalozek - 1;
  const int totalMezera = pocetMezer * mezera;

  // Celková šířka pro záložky
  const int sirkaZalozek = plocha - totalMezera;

  // Výpočet šířky jednotlivých záložek (většina bude stejná, poslední může být širší/delší)
  int beznaSirka = sirkaZalozek / pocetZalozek;
  int zbytek = sirkaZalozek % pocetZalozek;  // rozdělení zbytku do některých záložek

  const int zalozkaHeight = 41;
  const char* nazvy[] = {"STAT", "RUN", "PROG", "FILL", "SET"};

  int x = startX;

  // Neaktivní záložky
  for (int i = 0; i < pocetZalozek; i++) {
    int sirka = beznaSirka + (i < zbytek ? 1 : 0);  // přidat zbytek na začátek
    tft.fillRoundRect(x, 14, sirka, zalozkaHeight, 5, BARVA_ZALOZKY_T);
    tft.setTextColor(BARVA_RAMU, BARVA_ZALOZKY_T);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(nazvy[i], x + sirka / 2, 14 + zalozkaHeight / 2);
    x += sirka + mezera;
  }

  // Aktivní záložka
  x = startX;
  for (int i = 0; i < aktivnizalozka; i++) {
    int sirka = beznaSirka + (i < zbytek ? 1 : 0);
    x += sirka + mezera;
  }
  int aktivniSirka = beznaSirka + (aktivnizalozka < zbytek ? 1 : 0);
  tft.fillRoundRect(x, 14, aktivniSirka, zalozkaHeight, 5, BARVA_ZALOZKY_S);
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_ZALOZKY_S);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(nazvy[aktivnizalozka], x + aktivniSirka / 2, 14 + zalozkaHeight / 2);

  // Informační pás pod záložkami
  int infoPanelTop = 52;
  int infoPanelHeight = 29;
  tft.fillRect(startX, infoPanelTop, plocha, infoPanelHeight, BARVA_ZALOZKY_S);
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_ZALOZKY_S);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);

  switch (aktivnizalozka) {
    case 0: tft.drawString("Status of device", 160, infoPanelTop + 15); break;
    case 1: tft.drawString("Runing status", 160, infoPanelTop + 15); break;
    case 2: tft.drawString("Programming of device", 160, infoPanelTop + 15); break;
    case 3: tft.drawString("Filling program", 160, infoPanelTop + 15); break;
    case 4: tft.drawString("System Settings", 160, infoPanelTop + 15); break;
  }
  Serial.println("Vykresleny zalozky"); 
}

//funkce pro hodiny
 //načtení času
void NactiTIME() {
  now = rtc.now();
  sprintf(cas, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  sprintf(datum, "%02d.%02d.%04d", now.day(), now.month(), now.year());

}
 //nastavení času - zatím jen ruční tesovací funkce
void SetTIME() {
  rtc.adjust(DateTime(rok, mesic, den, hodin, minut, 0)); // nacteni hodnot z promennych
  Serial.println("Nastaven cas"); 
  sprintf(cas, "%02d:%02d", hodin, minut);
  sprintf(datum, "%02d.%02d.%04d", den, mesic, rok);
}

// překreslení časového údaje
void PrepisTIME() {
  tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
  switch (aktivnizalozka) {
    case 0: {  int x = 17, y = 100; //nastavení polohy pro překreslení času a data  záložky STAT
               int lineHeight = 45;
               tft.drawString(datum, x + 70, y + lineHeight * 0);
               tft.drawString(cas, x + 70, y + lineHeight * 1);
           break;}
    case 1:  break;
    case 2:  break;
    case 3:  break;
    case 4:  { int x = 17, y = 100; //nastavení polohy pro překreslení času a data  záložky SET
               int lineHeight = 45;
               tft.drawString(datum, x + 120, y + lineHeight * 0);
               tft.drawString(cas, x + 120, y + lineHeight * 1);
           break;}
    

  }

  // [v19e] Lightweight countdown update tied to time repaint (no clearing, no full redraw)
  if (programState == 2) {
    bool dwn = dwnHit();
    // detect edge when arm reaches DWN position -> start countdown
    if (dwn && !dwnPrev) {
      statCountdownActive = true;
      // Use the seconds currently shown from protocol (statStatusSecs).
      // If it is invalid, keep previous statCountdown.
      if (statStatusSecs >= 0) {
        statCountdown = statStatusSecs;
      }
      statCountdownPrev = -1;
    }
    if (!dwn && dwnPrev) {
      statCountdownActive = false;
    }
    dwnPrev = dwn;

    if (statCountdownActive && statCountdown > 0 && statCountX > 0) {
      // decrement once per PrepisTIME repaint (should be once per second)
      if (!protPaused) statCountdown--;
      // Před vykreslením nového čísla vymazání starého
      tft.fillRect(statCountX, statCountY - 3, 48, 20, BARVA_RAMU); // 48 px = místo pro 4 číslice
      // Draw new number over the old one without erasing background
      tft.setTextColor(BARVA_ZALOZKY_PS, BARVA_RAMU);
      tft.setTextSize(2);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(String(statCountdown), statCountX, statCountY);
    }
  }

}





// [v19] Jednoduchý interpret protokolu (neblokující)

// [v19a] Jednoduchý interpret protokolu (neblokující, s přesným časem DWN)
void ProtocolTick(){
  if (protPaused) return;
if (!protocolActive || programState != 2) return;

  switch (protPhase){
    case 0: { // načti dvojici (4/5, 6/7, atd)
      String w; int secs = 0;
      if (!ReadProtocolPair(protLine, w, secs)){
        protocolActive = false; programState = 1;
        statStatusWord=""; statStatusSecs=-1;
        kresliSTAT();
        return;
      }
      if (secs <= 0){
        ZeroPositionFast();
        protocolActive = false; programState = 1;
        statStatusWord=""; statStatusSecs=-1;
        kresliSTAT();
        return;
      }
      statStatusWord = w; statStatusSecs = secs; kresliSTAT();
      if (!protFirstPair){
        // karusel má vlastní neblokující automat (již hotovo)
        // stačí zavolat jeho start; pokud už máš helper, použij ho. Jinak stávající funkci.
        karusel_plus1();
      }
      protPhase = 1;
      break;
    }
    case 1: { // DWN start + čekání na dolní koncák (neblokující)
      if (nbMotorState == 0) Motor_DWN_nb_start();
      if (Motor_nb_tick()){
        protUntil = millis() + (unsigned long)statStatusSecs * 1000UL;
        protPhase = 2;
      }
      break;
    }
    case 2: { // čekání přesně na sekundy
      if ((long)(millis() - protUntil) < 0) return;
      protPhase = 3;
      break;
    }
    case 3: { // UP start + čekání na horní koncák (neblokující)
      if (nbMotorState == 0) Motor_UP_nb_start();
      if (Motor_nb_tick()){
        protLine += 2;
        protFirstPair = false;
        protPhase = 0;
      }
      break;
    }
  }

}

void loop() {
  
  zpracujEnkoder();  // obsluha enkodéru

  // Zpracování stisku tlačítka menu s odrušením
  if (menuButtonInterruptFlag) {
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;

      aktivnizalozka = (aktivnizalozka + 1) % pocetZalozek;
      kreslizalozku();

      switch (aktivnizalozka) {
        case 0: kresliSTAT(); break;
        case 1: kresliRUN();  break;
        case 2: kresliPROG(); break;
        case 3: kresliFILL(); break;
        case 4: kresliSET();  break;
      }
    }
    menuButtonInterruptFlag = false;  // vlajku resetuj vždy až po vyhodnocení
  }

  zpracujEnter(); //obsluha tlačítka Enter
  ProtocolTick();
  ProtocolTick();

  // Aktualizace hodin každou sekundu, pokud není v režimu nastavení
  if (!nastavCas && (millis() - lastRTCUpdate > rtcUpdateInterval)) {
    NactiTIME();
    PrepisTIME(); 
    lastRTCUpdate = millis();
  }




  //BLE---------------------------------------------------------------------
  unsigned long currentMillis = millis();
  if (deviceConnected) {
    // // pFileCharacteristic->setValue(message); //ořízne zprávu na 20 znaků...
    // // pFileCharacteristic->notify();
    // String rxValue = pFileCharacteristic->getValue();
    // Serial.println(rxValue);
    // if (rxValue == "Send email") {
    //   sendEmail();
    // }

    // String txValue = "Hello";
    // pFileCharacteristic->setValue(txValue);  //ořízne zprávu na 20 znaků...
    // Serial.println(txValue);

    if (receivingBtData || receivingBtName || receivingSettingsBtData) {
      if (currentMillis - lastBtCommReceivedInMillis >= btCommTimeout) {
        receivingBtData = false;
        receivingBtName = false;
        receivingSettingsBtData = false;
        Serial.println(F("Didn't received end of transmission in time. Throwing away incomplete file."));
      }
    }

    if (receivingBtDataEnded) {
      receivingBtData = false;
      receivingBtDataEnded = false;
      Serial.println(receivedFileData);
      //receivedFileData = "";
    }

    if (receivingBtNameEnded) {
      receivingBtName = false;
      receivingBtNameEnded = false;
      Serial.println(receivedFileName);
      //receivedFileName = "";
      writeProtocolFromBt();
    }

    if (receivingSettingsBtDataEnded) {
      receivingSettingsBtData = false;
      receivingSettingsBtDataEnded = false;
      Serial.println(receivedSettingsData);
      //receivedSettingsData = "";
      writeSettingsFromBt();
    }

    delay(50);
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println(F("start advertising"));
    oldDeviceConnected = deviceConnected;
  }

  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
  //END BLE---------------------------------------------------------------------
}
