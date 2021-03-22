#include <Keypad.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <DS3231.h>   //Clock module
#include <NewPing.h>  //Ultrasonic Sound Sensor

/* Adafruit */
#define TFT_CS    10
#define TFT_RST   -1
#define TFT_DC    9

//MOSI is pin 11
//SCLK is pin 13

/* Keypad */
#define ROWS 4
#define COLS 4

/* EEPROM */
#define START_ADDR 1
#define PIN_SIZE 4

/* Ultrasonic */
#define TRIGGER_PIN  7
#define ECHO_PIN     6
#define MAX_DISTANCE 200

/* BUZZER */
#define BUZZ_PIN 8

/* --- INITIALIZATIONS --- */

byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {5, 4, 3, 2};

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

Keypad myKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

DS3231  rtc(A4, A5);

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

/* -------------------------------------- */

/*  FIRST Address in Eeprom reserved to say whether
    the device has a stored pin or not.
    If 1 -> Pin Code exists  */

char gPinCode[] = {'0', '0', '0', '0'}; //default code for new devices

unsigned long sonicTime = 0;
unsigned long clockTime = 0;

int calcDistance = 0;

uint8_t displayX = 135;
uint8_t displayY = 240;

const char* prevParaText = 0;

unsigned int pitch = 330;

/* --- FUNCTION DECLARATIONS --- */
bool gatherPinCode(char* pin, bool timeoutEnabled = false, char initKey = 0);


void setup() {
  Serial.begin(9600);
  Serial.println(F("Init begin"));

  /* SET PINMODE */
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);

  /* INIT DISPLAY */
  tft.init(displayX, displayY);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextSize(1);

  writeTitle("Starting up device...");
  rtc.begin();

  /* INIT ULTRASONIC */
  calcDistance = sonar.ping_cm();
  Serial.print(F("INITIAL DISTANCE: "));
  Serial.println(calcDistance);

  /* INIT PIN CODE FROM EEPROM */
  if (EEPROM.read(0) == 1) {
    retrievePinFromEEPROM();    // setting gPinCode to what is stored in EEPROM
  } else {
    Serial.println(F("No pincode found, intializing factory default code"));
    updatePinInEEPROM(gPinCode);    // put the default pinCode into the EEPROM
    EEPROM.update(0, 1);    //First addr: If 1 -> Pin Code Exists
  }

  Serial.println(F("Init complete"));
  delay(2000); //wait before showing main menu
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  //Only writes once every change of text
  writeTitle("A: Turn on alarm\n\nB: Change Pin Code\n");

  //Every 1 second
  if (millis() > clockTime + 1000) {
    showTime();
    clockTime = millis();
  }

  char key = myKeypad.getKey();

  if (key) {
    Serial.println(key);

    switch (key) {
      case 'A':
        clearDisplayParts(true);
        alarmOn();
        break;
      case 'B':
        clearDisplayParts(true);
        changePinCode();
        break;
    }
  }
}

void alarmOn() {
  char pinAttempt[PIN_SIZE];
  bool pinCorrect = false;
  uint8_t pinsCollect = 0;
  uint8_t wrongAttempts = 0;
  bool intruder = false;
  bool collectBegun = false;
  unsigned long beepTime = 0;

  int initDistance = sonar.ping_cm();
  Serial.println(initDistance);

  writeTitle("ALARM IS ON!\n");

  //Initiate alarm with 10 buzzes
  int i = 0;
  while (i < 10) {
    if (millis() > clockTime + 1000) {
      showTime();
      tone(BUZZ_PIN, pitch, 500);
      clockTime = millis();
      i++;
    }
  }

  while (!pinCorrect && wrongAttempts < 4) {

    if (millis() > clockTime + 1000) {
      showTime();
      clockTime = millis();
    }

    if (intruder) {
      if (millis() > beepTime + 500) {
        tone(BUZZ_PIN, pitch, 200);
        beepTime = millis();
      }
      tft.setCursor(0, 80);
      tft.print("INTRUDER!! \nENTER PIN CODE \nTO SHUT OFF ALARM\n");
    }

    calcDistance = sonar.ping_cm();
    if ((calcDistance < (initDistance - 20)) || (calcDistance > (initDistance + 20))) {
      intruder = true;
    }

    // --- Getting pin from user ---
    char key = myKeypad.getKey();
    validatePin(key, pinsCollect, collectBegun, pinAttempt);

    //Pin attempt collected and ready to be tested
    if (pinsCollect == PIN_SIZE) {
      pinsCollect = 0; //reset counter
      pinCorrect = isCorrectPin(pinAttempt);
      if (!pinCorrect) {
        writeParagraph("ERROR:            \nINCORRECT PIN CODE\n");
        tone(BUZZ_PIN, 131, 1000);
        delay(200);
        noTone(BUZZ_PIN);
        wrongAttempts++;
        collectBegun = false;
        delay(1000);
        clearDisplayParts(false);
      }
    }

    if (wrongAttempts == 4) {
      writeParagraph("ERROR: \nTOO MANY INCORRECT ATTEMPTS. \nCALLING POLICE!\n");
    }
  }

  if (pinCorrect) {
    tft.print("\nAlarm turns off...");
  }

  delay(2000);
  clearDisplayParts(false);
}

/* --- PIN CODE --- */
void changePinCode() {
  char oldPin[PIN_SIZE];
  char newPin[PIN_SIZE];
  char key;
  uint8_t pinsCollect = 0;
  bool collectBegun = false;

  writeTitle("Enter old pinCode:\n");

  while (1) {
    if (millis() > clockTime + 1000) {
      showTime();
      clockTime = millis();
    }

    key = myKeypad.getKey();
    validatePin(key, pinsCollect, collectBegun, oldPin);

    if (pinsCollect == PIN_SIZE) {
      if (isCorrectPin(oldPin)) {
        clearDisplayParts(false);
        break;
      } else {
        writeParagraph("ERROR:            \nINCORRECT PIN CODE\n");
        tone(BUZZ_PIN, 131, 1000);
        delay(200);
        noTone(BUZZ_PIN);
        collectBegun = false;
        delay(1000);
        clearDisplayParts(false);
        return;
      }
    }
  }

  pinsCollect = 0; //reset counter
  collectBegun = false;
  writeTitle("Enter new pinCode:\n");

  while (1) {
    if (millis() > clockTime + 1000) {
      showTime();
      clockTime = millis();
    }
    key = myKeypad.getKey();
    validatePin(key, pinsCollect, collectBegun, newPin);

    if (pinsCollect == PIN_SIZE) {
      assignNewPin(newPin);
      writeParagraph("Successfully changed \npin code\n");
      break;
    }
  }

  Serial.println(F("old values in EEPROM:"));
  readFromEEPROM(START_ADDR, PIN_SIZE);
  updatePinInEEPROM(gPinCode);
  Serial.println(F("new values in EEPROM:"));
  readFromEEPROM(START_ADDR, PIN_SIZE);
  showTime();
  delay(1000);
  clearDisplayParts(true);
}

bool isCorrectPin(char* pinAttempt) {
  bool pinCorrect = true;
  for (int i = 0; i < PIN_SIZE; i++) {
    if (pinAttempt[i] != gPinCode[i])
      pinCorrect = false;
  }
  return pinCorrect;
}

bool validatePin(char key, uint8_t& pinsCollect, bool& collectBegun, char* pinAttempt) {
  static int16_t cursorX = 0;
  static int16_t cursorY = 0;

  if (key) {
    tone(BUZZ_PIN, 440, 200);
    if (pinsCollect < PIN_SIZE) {
      if (key >= '0' && key <= '9') {
        if (!collectBegun) {
          writeParagraph("ENTER PIN: ");
          cursorX = tft.getCursorX();
          cursorY = tft.getCursorY();
          collectBegun = true;
        }
        //Gathering pin
        tft.setCursor(cursorX, cursorY);
        tft.print("*");
        cursorX = tft.getCursorX();
        cursorY = tft.getCursorY();
        Serial.print(key);
        pinAttempt[pinsCollect] = key;
        pinsCollect++;

      } else {
        //Non Digit Read
        writeParagraph("\nInvalid symbol");
        collectBegun = false;
        delay(1000);
        clearDisplayParts(false);
      }
    }
  }
}

void assignNewPin(char* newPin) {
  for (int i = 0; i < PIN_SIZE; i++) {
    gPinCode[i] = newPin[i];
  }
}

/* --- EEPROM --- */
void retrievePinFromEEPROM() {
  char tempPin[PIN_SIZE];
  for (int i = 0; i < PIN_SIZE && i < EEPROM.length(); i++) {
    byte val = EEPROM.read(i + START_ADDR);
    Serial.println(val);

    if (val >= '0' && val <= '9') {
      tempPin[i] = val;

    } else {
      writeTitle("\nERROR: Could not retrieve pin. contact product owner for more information");
      Serial.println(F("\nERROR: Could not retrieve pin. Contact product owner for more information"));
    }
  }
  assignNewPin(tempPin);
}

void readFromEEPROM(int fromAddress, int numOAddr) {
  for (int i = 0; i < numOAddr && i < EEPROM.length(); i++) {
    byte val = EEPROM.read(i + fromAddress);
    if (val != 0) {
      Serial.println(val);
    }
  }
}

void updatePinInEEPROM(char* pin) {
  for (int i = 0; i < PIN_SIZE && i < EEPROM.length(); i++) {
    Serial.println(F("updating address in EEPROM"));
    EEPROM.update(i + START_ADDR, pin[i]);
  }
}

/* --- DISPLAY --- */
void clearDisplayParts(bool includeTitle) {
  if (includeTitle) {
    tft.fillRect(0, 20, displayX, displayY, ST77XX_BLACK);
  } else {
    tft.fillRect(0, 50, displayX, displayY, ST77XX_BLACK);
  }
  prevParaText = 0;
}

void writeTitle(const char* text) {
  static const char* prevTitle = 0;
  if (prevTitle != text) {
    writeToDisplay(text, 20);
  }
}

void writeParagraph(const char* text) {
  //Only writes if the content changes
  if (prevParaText != text) {
    Serial.println(F("Writes to display"));
    writeToDisplay(text, 50);
    prevParaText = text;
  }
}

void writeToDisplay(const char* text, unsigned int cursorY) {
  tft.setCursor(0, cursorY);
  tft.print(text);
}

/* --- TIME --- */
void showTime() {
  //static char* prevDate = "";
  //static char* prevTime = "";

  //const char* currentDate = rtc.getDateStr();
  const char* currentTime = rtc.getTimeStr();

  /*if(currentDate != prevDate) {
    writeText(currentDate);
    }*/
  //if(currentTime != prevTime) {
  writeToDisplay(currentTime, 0);
  //}

  //prevDate = currentDate;
  //prevTime = currentTime;
}
