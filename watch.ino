#include <avr/sleep.h>
#include <Wire.h>
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include "RTClib.h"
//#include "DallasTemperature\DallasTemperature.h"


/*
блюпуп
температура
оповещения
индикация
будильник
анимация
*/


#define BRIGHTNESS_LEVELS 3

#define BT_PWR_PIN 8
#define BT_STATE_PIN 9
#define DISPLAY_RST_PIN A2
#define TEMP_PIN
#define BUZ_PIN


enum { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT };
enum btState { BT_OFF, BT_ON,  BT_CONNECTED };
enum { WATCH, SETTINGS, TABLE };
enum week { RED_WEEK, BLUE_WEEK };


struct Button {
  uint8_t pin;
  bool lastState, processed;
  unsigned long lastPress;

  Button(uint8_t pin) {
    this->pin = pin;
    digitalWrite(pin, 1);
  }

  bool wasPressed() { return !processed && lastState; }

  void reset() { processed = true; }

  void update() {
    unsigned long now = millis();

    if (digitalRead(pin) == lastState && (now - lastPress > 150 || now < lastPress)) {
      lastState = !lastState;
      processed = false;
      lastPress = now;
    }
  }
} btnUp(3), btnDown(4), btnLeft(5), btnRight(6);

Button* buttons[] = { &btnUp, &btnDown, &btnLeft, &btnRight };


struct Bluetooth {
  uint8_t pwrPin, statePin;
  btState state, tmpState;

  Bluetooth(uint8_t pwrPin, uint8_t statePin, btState state = BT_ON) {
    this->pwrPin = pwrPin;
    this->statePin = statePin;

    pinMode(pwrPin, 1);
    setState(state);
  }

  void setState(btState state) {
    this->state = state;
    digitalWrite(pwrPin, !state);
  }

  void update() {
    if (state == BT_ON && digitalRead(statePin)) state = BT_CONNECTED;
    else if (state == BT_CONNECTED && !digitalRead(statePin)) state = BT_ON;
  }
} bt(BT_PWR_PIN, BT_STATE_PIN);


const char r1[] PROGMEM = "\n2. инфа(п) 0-214\n3. история(л) 1-412";
const char r2[] PROGMEM = "\n\n3. физра";
const char r3[] PROGMEM = "1. инфа в проф 0-215\n2. оси(л) 0-122\n3. матан(п) 0-420";
const char r4[] PROGMEM = "1. оси 0-214\n2. //-//\n3. практикум 0-223\n4. программ. 0-211\n5. практикум 0-211";
const char r5[] PROGMEM = "\n\n3. физра\n4. практикум 4-405\n5. //-//";
PGM_P red_table[] PROGMEM = { r1, r2, r3, r4, r5 };

const char b1[] PROGMEM = "\n2. матан(п) 1-309\n3. матан(л) 1-309";
const char b2[] PROGMEM = "1. история(п) 0-405\n2. физика(п) 1-301";
const char b3[] PROGMEM = "1. инфа в проф 0-215\n2. практикум 0-223\n3. //-//\n4. оси(л) 1-203";
const char b4[] PROGMEM = "1. оси(п) 0-214\n2. программ.(л) 0-113";
const char b5[] PROGMEM = "1. инфа(л) 0-403\n2. физика(л) 1-301\n3. физра\n4. практикум 4-405\n5. //-//";
PGM_P blue_table[] PROGMEM = { b1, b2, b3, b4, b5 };


Adafruit_SSD1306 display(DISPLAY_RST_PIN);
//RTC_DS1307 rtc;
RTC_Millis rtc;

uint8_t location, menuSelect;
week viewWeek, currentWeek = BLUE_WEEK;
unsigned long lastRedraw = 0;
int years;
uint8_t weekDay, months, days, hours, mins;
uint8_t brightness = 0;



void setup () {
  //analogReference(INTERNAL);        //!
  display.begin();
  rtc.begin(DateTime(2016, 4, 6, 21, 10, 0));
  Serial.begin(19200);
  //rtc.adjust(DateTime(2016, 4, 6, 21, 10, 0));
  display.dim(brightness * 255 / (BRIGHTNESS_LEVELS - 1));
  display.fillScreen(0);
  display.setTextColor(WHITE);

  digitalWrite(2, 1); //!!!!!!!!!!!!!!

  EICRA |= _BV(ISC01) | _BV(ISC00);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  goWatch();
}


void loop() {
  uint8_t i, buttonsCount = sizeof(buttons) / sizeof(Button*);

  for (i = 0; i < buttonsCount; i++) buttons[i]->update();

  switch (location) {
  case WATCH: loopWatch(); break;
  case SETTINGS: loopSettings(); break;
  case TABLE: loopTable(); break;
  }

  for (i = 0; i < buttonsCount; i++) buttons[i]->reset();
}


//------------- loops


void loopWatch() {
  bt.update();

  if (btnLeft.wasPressed()) return goSettings();
  else if (btnRight.wasPressed()) return goTable();
  else if (btnDown.wasPressed()) return sleep();

  if (Serial.available() > 0) {
    char cmd = Serial.read();
    String msg;

    switch (cmd) {
    case 'T':
      int hours = readNum();
      int mins = readNum();

      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), hours, mins, 0));
      break;
    case 'D':
      int days = readNum();
      int months = readNum();
      int years = readNum();

      DateTime now = rtc.now();
      rtc.adjust(DateTime(years, months, days, now.hour(), now.minute(), now.second()));
      break;
    case 'M':
      delay(10);
      display.fillRect(0, 40, 128, 24, 0);
      display.setTextSize(1);
      display.setCursor(0, 40);

      while (Serial.available() > 0) {
        uint8_t c = Serial.read();

        if (c == 209) msg += (char)(Serial.read() + 112);
        else if (c == 208) msg += (char)(Serial.read() + 48);
        else msg += (char)c;
      }

      display.print(msg);
      break;
    default: break;
    }
  }

  if (millis() - lastRedraw > 1000) {
    redrawWatch();
    lastRedraw = millis();
  }
}


void loopSettings() {
  if (btnUp.wasPressed()) {
    switch (menuSelect) {
      case 0: days = (days + 1) % 31; break;
      case 1: months = (months + 1) % 12; break;
      case 2: years++; break;
      case 3: hours = (hours + 1) % 24; break;
      case 4: mins = (mins + 1) % 60; break;
      case 5: bt.tmpState = (btState)!bt.tmpState; break;
      case 6:
        brightness = (brightness + 1) % BRIGHTNESS_LEVELS;
        display.dim(brightness * 255 / (BRIGHTNESS_LEVELS - 1));
        break;
      default: break;
    }
  } else if (btnDown.wasPressed()) {
    switch (menuSelect) {
      case 0: days = (days + 30) % 31; break;
      case 1: months = (months + 11) % 12; break;
      case 2: years--; break;
      case 3: hours = (hours + 23) % 24; break;
      case 4: mins = (mins + 59) % 60; break;
      case 5: bt.tmpState = (btState)!bt.tmpState; break;
      case 6:
        brightness = (brightness - 1 + BRIGHTNESS_LEVELS) % BRIGHTNESS_LEVELS;
        display.dim(brightness * 255 / (BRIGHTNESS_LEVELS - 1));
        break;
      default: break;
    }
  } else if (btnLeft.wasPressed()) {
    rtc.adjust(DateTime(years, months, days, hours, mins, 0));
    bt.setState(bt.tmpState);
    goWatch();
    return;
  } else if (btnRight.wasPressed()) menuSelect = (menuSelect + 1) % 7;
  
  redrawSettings();
}


void loopTable() {
  if (btnUp.wasPressed()) redrawTable((weekDay - 1 + 5) % 5);
  else if (btnDown.wasPressed()) redrawTable((weekDay + 1) % 5);
  else if (btnLeft.wasPressed()) goWatch();
  else if (btnRight.wasPressed()) {
    viewWeek = (week)!viewWeek;
    redrawTable();
  }
}


//------------- turns


void goWatch() {
  display.fillScreen(0);
  redrawWatch();
  location = WATCH;
}


void goSettings() {
  DateTime now = rtc.now();
  years = 2016;
  months = now.month();
  days = now.day();
  hours = now.hour();
  mins = now.minute();
  menuSelect = 0;
  bt.tmpState = (btState)(bool)bt.state;
  location = SETTINGS;
}


void goTable() {
  viewWeek = currentWeek;
  weekDay = rtc.now().dayOfTheWeek();

  if (weekDay == 6) weekDay--;
  if (weekDay) weekDay--;

  location = TABLE;
  redrawTable();
}


void sleep() {
  EIMSK |= _BV(INT0);
  display.off();
  sleep_cpu();
  display.on();
  EIMSK &= ~_BV(INT0);
  goWatch();
}


//------------- redraws


void redrawWatch() {
  DateTime now = rtc.now();

  display.fillRect(0, 0, 128, 40, 0);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print(now.day());
  display.print(".");
  display.print(now.month());
  display.setTextSize(1);
  //display.print(" ");
  drawWeek(now.dayOfTheWeek());
  display.setCursor(66, 8);
  display.print(readVcc() / 100);
  display.setCursor(84, 8);

  if (bt.state == BT_ON) display.print("B");
  else if (bt.state == BT_CONNECTED) display.print("BC");

  display.setCursor(10, 16);
  display.setTextSize(3);
  display.print(now.hour());
  display.print(':');
  drawLabel(now.minute());
  display.setTextSize(2);
  drawLabel(now.second());
  display.display();
}


void redrawSettings() {
  display.fillScreen(0);
  display.setTextSize(2);
  display.setCursor(10, 0);
  display.print(F("настройки"));
  display.setTextSize(1);
  
  display.setCursor(12, 16);
  display.print(F("дата: "));
  drawLabel(days, 48, 16, 12, menuSelect == 0);
  display.print(".");
  drawLabel(months, 66, 16, 12, menuSelect == 1);
  display.print(".");
  drawLabel(years, 84, 16, 24, menuSelect == 2);
  
  display.setCursor(12, 24);
  display.print(F("время: "));
  drawLabel(hours, 54, 24, 12, menuSelect == 3);
  display.print(':');
  drawLabel(mins, 72, 24, 12, menuSelect == 4);
  
  display.setCursor(12, 32);
  display.print(F("блюпуп: "));
  drawLabel(bt.tmpState ? F("ON") : F("OFF"), 60, 32, 18, menuSelect == 5);
   
  display.setCursor(12, 40);
  display.print(F("яркость: "));
  drawLabel(brightness, 66, 40, 18, menuSelect == 6);

  display.display();
}


void redrawTable(int8_t _weekDay = -1) {
  char* buffer;
  weekDay = (_weekDay == -1) ? weekDay : _weekDay;

  display.fillScreen(0);
  display.setTextSize(2);
  display.setCursor(6, 0);
  display.print(F("расписание"));
  display.setTextSize(1);
  display.setCursor(0, 16);
  drawWeek(weekDay + 1);

  if (viewWeek == RED_WEEK) {
    display.print(F(" (красная)"));
    display.setCursor(0, 24);
    strcpy_P(buffer, (PGM_P)pgm_read_word(&(red_table[weekDay])));
  } else {
    display.print(F(" (синяя)"));
    display.setCursor(0, 24);
    strcpy_P(buffer, (PGM_P)pgm_read_word(&(blue_table[weekDay])));
  }

  display.print(buffer);
  display.display();
}


//------------- others


int readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(75);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  uint8_t low = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH;
  int result = (high << 8) | low;
  result = 1125300L / result; // 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}


int readNum(uint8_t ln = 2, bool readAfter = true) {
  int num = 0;

  for (uint8_t i = 0; i < ln; i++) num = num * 10 + (uint8_t)(Serial.read() - '0');

  if (readAfter) Serial.read();

  return num;
}


void drawLabel(int i) {
  if (i < 10) display.print("0");
  display.print(i);
}


void drawLabel(int i, uint8_t x, uint8_t y, uint8_t width, bool sel, bool lpad = true) {
  String s = String(i);
  if (lpad && i < 10) s = '0' + s;

  drawLabel(s, x, y, width, sel);
}


void drawLabel(String s, uint8_t x, uint8_t y, uint8_t width, bool sel) {
  if (sel) {
    display.fillRect(x - 1, y, width + 1, 8, 1);
    display.setTextColor(BLACK);
    display.print(s);
    display.setTextColor(WHITE);
  } else display.print(s);
}


void drawWeek(uint8_t day) {
  switch (day) {
  case 0: display.print(F("воскресенье")); break;
  case 1: display.print(F("понедельник")); break;
  case 2: display.print(F("вторник"));     break;
  case 3: display.print(F("среда"));       break;
  case 4: display.print(F("четверг"));     break;
  case 5: display.print(F("пятница"));     break;
  case 6: display.print(F("суббота"));     break;
  default: break;
  }
}


ISR(INT0_vect){
  return;
}
