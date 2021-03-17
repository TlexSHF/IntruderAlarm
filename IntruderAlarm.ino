#include <Keypad.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

//SIMONES TODO LIST:
// #1 Få all tekst fra seriell monitor -> over til tft display!
// NOTE: fjernet options for størrelse og farge, kan adde igjen senere om nødvendig
// * En gang: Fiks så wrapping ikke skjer midt i ord


/* Adafruit defines */
#define TFT_CS    10
#define TFT_RST   -1
#define TFT_DC    8

/* --- INITIALIZATIONS --- */
/* Keypad */
const byte ROWS = 4;
const byte COLS = 4;

byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {5, 4, 3, 2};

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

Keypad myKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

/* Adafruit Display */
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

/* -------------------------------------- */

/*  FIRST Address in Eeprom reserved to say whether
    the device has a stored pin or not.
    If 1 -> Pin Code exists  */
uint8_t gStartAddr = 1;
uint8_t gPinSize = 4;
char gPinCode[] = {'0', '0', '0', '0'}; //default code for new devices

unsigned long timeNow = 0;
uint8_t period = 1000; //Weird bug... this is not 1 sec... why?

int gTextColor = ST77XX_GREEN;
int gBackColor = ST77XX_BLACK;
uint8_t gTextSize = 1;

/* --- FUNCTION DECLARATIONS --- */
bool gatherPinCode(char* pin, bool timeoutEnabled = false, char initKey = 0);
void writeText(const char* text, uint8_t textSize = gTextSize, int textColor = gTextColor);
void appendText(const char* text, uint8_t textSize = gTextSize, int textColor = gTextColor);


void setup() {
  Serial.begin(9600);
  Serial.println("Init begin");

  tft.init(135, 240);
  writeText("Starting up device...");

  if (EEPROM.read(0) == 1) {
    retrievePinFromEEPROM();    // setting gPinCode to what is stored in EEPROM
  } else {
    updatePinInEEPROM(gPinCode);    // put the default pinCode into the EEPROM
    EEPROM.update(0, 1);    //First addr: If 1 -> Pin Code Exists
    Serial.println("No pincode found, intializing factory default code");
  }

  Serial.println("Init complete");
  delay(2000); //wait before showing main menu
  Serial.println("A: Turn on alarm \nB: Change Pin Code \nC: Save pin to keyCard");
}

void loop() {
  //Only writes once every change of text
  writeText("A: Turn on alarm \n\nB: Change Pin Code \n\nC: Save pin to keyCard\n");

  char key = myKeypad.getKey();

  if (key) {
    Serial.println(key);

    switch (key) {
      case 'A':
        Serial.println("Turning on alarm");
        alarmOn();
        break;
      case 'B':
        Serial.println("Entering changing pin mode");
        changePinCode();
        break;
      case 'C':
        Serial.println("Saving pin to keyCard mode");
        /* DEBUG MATERIAL - Prints pin code*/
        Serial.print(gPinCode[0]);
        Serial.print(gPinCode[1]);
        Serial.print(gPinCode[2]);
        Serial.println(gPinCode[3]);
        break;
    }
  }
}

void alarmOn() {
  char pinAttempt[gPinSize];
  bool pinCorrect = false;
  uint8_t wrongAttempts = 0;

  while (!pinCorrect && wrongAttempts < 4) {
    writeText("ALARM IS ON!\n");

    bool pinGathered = false;

    // --- Getting pin from user ---
    char key = myKeypad.getKey();
    //If gotten a pin -> start collecting pins

    if (key) {
      //Gathering Pin
      Serial.println("gathering pin code...\n");
      appendText("gathering pin code...");
      if (gatherPinCode(pinAttempt, true, key)) {
      Serial.println("Pin Gathered");
        appendText("Pin Gathered\n");

        //Validating
        pinCorrect = isCorrectPin(pinAttempt);
        if (pinCorrect == false) {
          Serial.println("Error: Incorrect pin code");
          appendText("ERROR: INCORRECT PIN CODE");
          wrongAttempts++;
          if (wrongAttempts == 4) {
            Serial.println("ERROR: TOO MANY INCORRECT ATTEMPTS. CALLING POLICE!");
            appendText("ERROR: TOO MANY INCORRECT ATTEMPTS. CALLING POLICE!");

          }
        }
      }
    }
  }
  if (pinCorrect) {
    Serial.println("Success: Alarm turned off");
    writeText("Alarm turns off...");
  }
}

/* --- PIN CODE --- */
void changePinCode() {
  //TODO Make so that pin code is not stored in plain text, but rather generate a key out of entered pin code, TODOOO
  // which have to match pin code XOR/OR/AND % something something --- EPROM i Arduino
  char oldPin[gPinSize];
  char newPin[gPinSize];
  char key;

  Serial.println("Enter old pinCode:");
  if (gatherPinCode(oldPin)) {
    if (isCorrectPin(oldPin)) {

      Serial.println("Enter new pinCode:");
      if (gatherPinCode(newPin)) {
        assignNewPin(newPin);

        Serial.println("Successfully changed pin code");
      }
    } else {
      //If wrong, exit changePin function
      Serial.println("ERROR: Incorrect pin code");
    }
  }
  Serial.println("old values in EEPROM:");
  readFromEEPROM(gStartAddr, gPinSize);
  updatePinInEEPROM(gPinCode);
  Serial.println("new values in EEPROM:");
  readFromEEPROM(gStartAddr, gPinSize);
}

bool isCorrectPin(char* pinAttempt) {
  bool pinCorrect = true;
  for (int i = 0; i < gPinSize; i++) {
    if (pinAttempt[i] != gPinCode[i])
      pinCorrect = false;
  }
  return pinCorrect;
}

bool gatherPinCode(char* pin, bool timeoutEnabled = false, char initKey = 0) {
  char key;
  unsigned long enterPinStartTime = millis();

  for (int i = 0; i < gPinSize; i++) {
    //Timeout
    if (timeoutEnabled) {
      if (millis() > enterPinStartTime + 8000) {
        Serial.println("ERROR: connection timeout");
        return false;
      }
    }
    //Get pin input
    if (initKey == 0) {
      key = myKeypad.waitForKey();
    } else {
      key = initKey;
      initKey = 0;
    }

    //Validating pin input
    if (key >= '0' && key <= '9') {
      Serial.print(key);
      pin[i] = key;

    } else {
      Serial.println("\nInvalid symbol");
      return false;
    }
  }
  Serial.println();
  return true;
}

void assignNewPin(char* newPin) {
  for (int i = 0; i < gPinSize; i++) {
    gPinCode[i] = newPin[i];
  }
}

/* --- EEPROM --- */
void retrievePinFromEEPROM() {
  char tempPin[gPinSize];
  for (int i = 0; i < gPinSize && i < EEPROM.length(); i++) {
    byte val = EEPROM.read(i + gStartAddr);
    Serial.println(val);

    if (val >= '0' && val <= '9') {
      tempPin[i] = val;

    } else {
      Serial.println("\nERROR: Could not retrieve pin. Contact product owner for more information");
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
  for (int i = 0; i < gPinSize && i < EEPROM.length(); i++) {
    Serial.println("updating address in EEPROM");
    EEPROM.update(i + gStartAddr, pin[i]);
  }
}

/* --- DISPLAY --- */
void textSetupClear() {
  tft.setTextWrap(true);
  tft.fillScreen(gBackColor);
  tft.setCursor(0, 30);
}

void writeText(const char* text, uint8_t textSize = gTextSize, int textColor = gTextColor) {
  static const char* prevText = 0;
  //Only writes if the content changes
  if (prevText != text) {
    Serial.println("Writes to display");
    textSetupClear();
    appendText(text);
    prevText = text;
  }
}

void appendText(const char* text, uint8_t textSize = gTextSize, int textColor = gTextColor) {
  //TODO Her trengs det egt ikke å settes color og størrelse, men kanskje beholde til vi evt bruker det senere?
  tft.setTextColor(gTextColor);
  tft.setTextSize(gTextSize);
  tft.println(text);
}

void toRunar() {
  writeText("Henlo Runar!", 1, ST77XX_YELLOW);
  appendText("My love!", 2, ST77XX_BLUE);
  appendText("Love u!", 3, ST77XX_GREEN);
  appendText("<3", 4, ST77XX_RED);
}
