// papayapeter
// 2019

// defines -------------------------------------------------
#define UTILS_ERROR 200

// libraries -----------------------------------------------
#include <Metro.h>
#include <Bounce2.h>
#include <SoftwareSerial.h>
#include <Adafruit_FONA.h>
#include <Keypad.h>
#include <Key.h>

// pins ----------------------------------------------------
// fona pins
const uint8_t FONA_RX  = 9;
const uint8_t FONA_TX  = 8;
const uint8_t FONA_RST = 4;
const uint8_t FONA_RI  = 7;

// hook pins
const uint8_t HOOK_GND = 19;
const uint8_t HOOK = 18;

// keaypad pins
const uint8_t row_pins[4]    = {14, 16, 13, 12};
const uint8_t column_pins[4] = {15, 23, 10, 11};

// constants -----------------------------------------------
// keymap
const char keys[4][4] = {{'1', '2', '3', 'X'},
                         {'4', '5', '6', 'A'},
                         {'7', '8', '9', 'D'},
                         {'*', '0', '#', 'R'}};

// objects -------------------------------------------------
// fona
SoftwareSerial fona_ss = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fona_serial = &fona_ss;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// timers
Metro serial_timer(1000); // DEBUG
Metro utils_timer(20000); // battery and signal timer

// keypad
Keypad keypad = Keypad(makeKeymap(keys), row_pins, column_pins, 4, 4);

// variables -----------------------------------------------
// fona communication c-strings
char replybuffer[255];
char number[30];

// keypad-input
String key_input = "";
String last_input = "";

// values for utitlies
uint16_t battery;
uint8_t rssi;

// timer variables
uint64_t last_hook;

// functions -----------------------------------------------
// output to display
bool display(String text, uint16_t b, uint8_t r);
// check for hook pick up and act
int8_t checkHook(uint8_t p, uint16_t i);
// check signal an battery
void checkUtils(Adafruit_FONA* f, uint16_t* b, uint8_t* r);

// setup ---------------------------------------------------
void setup()
{
  // setup serial (DEBUG)
  Serial.begin(115200);

  // set pins
  pinMode(HOOK_GND, OUTPUT);
  digitalWrite(HOOK_GND, LOW);

  // set debounce time for hook
  keypad.setDebounceTime(5);

  // set up sim800 communication
  fona_serial->begin(38400);
  while (!fona.begin(*fona_serial))
  {
    Serial.println("can't find fona");
    
    while (true) delay(10);
  }
  Serial.println("found fona");
  delay(1000);

  // set audio
  fona.setAudio(FONA_EXTAUDIO);
}

// loop  ----------------------------------------------------
void loop()
{
  // check hook status
  int8_t hook_status = checkHook(HOOK, 25);
  if (hook_status == 1) // if picked up
  {
    if (key_input == "")
    {
      fona.playUserTone(425, 500, 500, 30000);
    }
    else
    {
      // save redial number
      last_input = key_input;
      
      uint8_t call_status = fona.getCallStatus();
      if (call_status == 0) // ready -> call
      {
        if (!fona.callPhone(key_input.c_str())) Serial.println("failed to call!");
      }
      else if (call_status == 3) // incoming -> pick up
      {
        if (!fona.pickUp()) Serial.println("failed to pick up!");
      }
  
      // clear input
      key_input = "";
    }
  }
  else if (hook_status == -1) // if put down
  {
    uint8_t call_status = fona.getCallStatus();
    if (call_status == 4) // in progress -> hang up
    {
      if (!fona.hangUp()) Serial.println("failed to hang up!");
    }

    fona.stopUserTone();
  }

  // check keypad
  char key = keypad.getKey();

  // check key
  if (key != NO_KEY && key != 'A' && key != 'X' && key != '*' && key != '#')
  {
    // remove
    if (key == 'R' && key_input.length() > 0) key_input.remove(key_input.length() - 1);
    // redial
    else if (key == 'D') key_input = last_input;
    // number
    else if (key != 'R') key_input += String(key);
  }

  
  // check signal and battery
  if (utils_timer.check()) checkUtils(&fona, &battery, &rssi);

  // DEBUG
  if (serial_timer.check()) display(key_input + "\tL: " + last_input, battery, rssi);
}

// functions -----------------------------------------------
bool display(String text, uint16_t b, uint8_t r)
{
  Serial.println("S: " + String(r) + "\tB: " + String(b) + "\tK: " + String(text));
}

int8_t checkHook(uint8_t p, uint16_t i)
{
  // static variables
  static Bounce hook = Bounce();
  static bool first = true;

  if (first) // if first -> attach pin and set interval
  {
    hook.attach(HOOK, INPUT_PULLUP);
    hook.interval(i);

    first = false;
  }

  hook.update();
  // if picked up
  if (hook.rose()) return 1;
  // if put down
  else if (hook.fell()) return -1;
  // if nothing happens
  return 0;
}

void checkUtils(Adafruit_FONA* f, uint16_t* b, uint8_t* r)
{
  if (!f->getBattPercent(b)) *b = UTILS_ERROR;
  if (!f->getNetworkStatus()) *r = UTILS_ERROR; else *r = map(f->getRSSI(), 0, 31, 0, 5);
}
