#include <Wire.h>
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include "RTClib.h"
#include <avr/sleep.h>


/*
struct, date
дребезг кнопок
температура
блюпуп
оповещения
индикация
будильник
анимация
*/


#define max_brightness 2

#define buttons_n 4

#define bt_pin 8
#define bt_state_pin 9

enum { btn_up, btn_down, btn_left, btn_right };
enum { BT_OFF, BT_ON,  BT_CONNECTED };
enum { WATCH, SET_TIME, TABLE };
enum { RED_WEEK, BLUE_WEEK };

const char r1[] PROGMEM = "\n2. инфа(п) 0-214\n3. история(л) 1-412";
const char r2[] PROGMEM = "\n\n3. физра";
const char r3[] PROGMEM = "1. инфа в проф 0-215\n2. оси(л) 0-122\n3. матан(п) 0-420";
const char r4[] PROGMEM = "1. оси 0-214\n2. //-//\n3. практикум 0-223\n4. программ. 0-211\n5. практикум 0-211";
const char r5[] PROGMEM = "\n\n3. физра\n4. практикум 4-405\n5. //-//";
const char* const red_table[] PROGMEM = {r1, r2, r3, r4, r5};

const char b1[] PROGMEM = "\n2. матан(п) 1-309\n3. матан(л) 1-309";
const char b2[] PROGMEM = "1. история(п) 0-405\n2. физика(п) 1-301";
const char b3[] PROGMEM = "1. инфа в проф 0-215\n2. практикум 0-223\n3. //-//\n4. оси(л) 1-203";
const char b4[] PROGMEM = "1. оси(п) 0-214\n2. программ.(л) 0-113";
const char b5[] PROGMEM = "1. инфа(л) 0-403\n2. физика(л) 1-301\n3. физра\n4. практикум 4-405\n5. //-//";
const char* const blue_table[] PROGMEM = {b1, b2, b3, b4, b5};


Adafruit_SSD1306 display(A2);
//RTC_DS1307 rtc;
RTC_Millis rtc;


byte buttons[] = {3, 4, 5, 6};
bool last_states_buttons[buttons_n], processed_buttons[buttons_n];

byte location, menu_select;
bool cur_week = BLUE_WEEK, view_week;
unsigned long last_redraw = 0, last_act;
int years;
byte week_day, months, days, hours, mins;
byte bt_state = ON, bt_state_tmp;
byte brightness = 0;


void setup () {
  //analogReference(INTERNAL);        //!
  display.begin();
  rtc.begin(DateTime(2016, 4, 6, 21, 10, 0));
  Serial.begin(19200);
  //rtc.adjust(DateTime(2016, 4, 6, 21, 10, 0));
  display.dim(brightness * 255 / max_brightness);
  display.fillScreen(0);
  display.setTextColor(WHITE);
  pinMode(bt_pin, 1);              //!
  digitalWrite(bt_pin, !bt_state); //!
  for(byte i = 0; i < buttons_n; i++) digitalWrite(buttons[i], 1); //!
  digitalWrite(2, 1);
  EICRA |= _BV(ISC01) | _BV(ISC00);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  watch();
}


void loop() {
  buttons_callback();
  if (location == WATCH) loop_watch();
  else if (location == SET_TIME) loop_set();
  else if (location == TABLE) loop_table();
  for (byte i = 0; i < buttons_n; i++) processed_buttons[i] = 1;
}


void buttons_callback() {
  for (byte i = 0; i < buttons_n; i++) {
    if (digitalRead(buttons[i]) == last_states_buttons[i]) { //!
      last_states_buttons[i] = !last_states_buttons[i];
      processed_buttons[i] = 0;
    }
  }
}


void loop_watch(){
  if (!processed_buttons[btn_left] && last_states_buttons[btn_left]){
    DateTime now = rtc.now();
    years = 2016;
    months = now.month();
    days = now.day();
    hours = now.hour();
    mins = now.minute();
    menu_select = 0;
    bt_state_tmp = (bt_state == BT_OFF) ? 0 : 1;
    location = SET_TIME;
    return;
  } else if(!processed_buttons[btn_right] && last_states_buttons[btn_right]){
    view_week = cur_week;
    week_day = rtc.now().dayOfTheWeek();
    if(week_day == 6) week_day--;
    if(week_day)      week_day--;
    location = TABLE;
    redraw_table();
    return;
  } else if(!processed_buttons[btn_down] && last_states_buttons[btn_down]){// || millis()-last_act>5000){
    EIMSK |= _BV(INT0);
    display.off();
    sleep_cpu();
    display.on();
    EIMSK &= ~_BV(INT0);
    watch();
  }

  if (Serial.available()){
    char cmd = Serial.read();
    if(cmd == 'T'){
      int hours = int(Serial.read() - '0') * 10 + int(Serial.read() - '0');
      Serial.read();
      int mins = int(Serial.read() - '0') * 10 + int(Serial.read() - '0');
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), hours, mins, 0));
    }else if(cmd == 'D'){
      int days = int(Serial.read() - '0') * 10 + int(Serial.read() - '0');
      Serial.read();
      int months = int(Serial.read() - '0') * 10 + int(Serial.read() - '0');
      Serial.read();
      int years = int(Serial.read() - '0') * 10 + int(Serial.read() - '0');
      DateTime now = rtc.now();
      rtc.adjust(DateTime(years, months, days, now.hour(), now.minute(), now.second()));
    } else if(cmd == 'M'){
      display.fillRect(0, 40, 128, 24, 0);
      delay(10);
      String a;
      display.setTextSize(1);
      display.setCursor(0, 40);
      while(Serial.available()){
        byte c = Serial.read();
        if(c == 209) a += char(Serial.read() + 112);
        else if(c == 208) a += char(Serial.read() + 48);
        else a += char(c);
      }
      display.print(a);
    }
  }
  
  if(bt_state == BT_ON && digitalRead(bt_state_pin)) bt_state = BT_CONNECTED;        //!
  else if(bt_state == BT_CONNECTED && !digitalRead(bt_state_pin)) bt_state = BT_ON;  //!

  if(millis() - last_redraw > 1000){
    redraw_watch();
    last_redraw = millis();
  }
}


void loop_set() {
  if (!processed_buttons[btn_up] && last_states_buttons[btn_up]){
    switch(menu_select){
      case 0: days = (days + 1) % 31; break;
      case 1: months = (months + 1) % 12; break;
      case 2: years++; break;
      case 3: hours = (hours + 1) % 24; break;
      case 4: mins = (mins + 1) % 60; break;
      case 5: bt_state_tmp = !bt_state_tmp; break;
      case 6:
        brightness = (brightness + 1) % (max_brightness + 1);
        display.dim(brightness * 255 / max_brightness);
        break;
      default: break;
    }
  }
  if (!processed_buttons[btn_down] && last_states_buttons[btn_down]){
    switch(menu_select){
      case 0: days = (days + 31) % 32; break;
      case 1: months = (months + 12) % 13; break;
      case 2: years--; break;
      case 3: hours = (hours + 23) % 24; break;
      case 4: mins = (mins + 59) % 60; break;
      case 5: bt_state_tmp = !bt_state_tmp; break;
      case 6:
        brightness = (brightness + max_brightness) % (max_brightness + 1);
        display.dim(brightness * 255 / max_brightness);
        break;
      default: break;
    }
  }
  if (!processed_buttons[btn_left] && last_states_buttons[btn_left]){
    rtc.adjust(DateTime(years, months, days, hours, mins, 0));
    if(bt_state_tmp && bt_state == BT_OFF){
      digitalWrite(bt_pin, 0);        //!
      bt_state = BT_ON;
    } else if(!bt_state_tmp && bt_state != BT_OFF){
      digitalWrite(bt_pin, 1);        //!
      bt_state = BT_OFF;
    }
    watch();
    return;
  }
  if (!processed_buttons[btn_right] && last_states_buttons[btn_right])
    menu_select = (menu_select + 1) % 7;
  redraw_set();
}


void loop_table(){
  if (!processed_buttons[btn_left] && last_states_buttons[btn_left]) watch();
  else if (!processed_buttons[btn_right] && last_states_buttons[btn_right]){
    view_week = !view_week;
    redraw_table();
  }
  else if (!processed_buttons[btn_up] && last_states_buttons[btn_up]){
    if(week_day > 0) week_day--;
    else week_day = 4;
    redraw_table();
  }
  else if (!processed_buttons[btn_down] && last_states_buttons[btn_down]){
    if(week_day < 4) week_day++;
    else week_day = 0;
    redraw_table();
  }
}


void watch(){
  display.fillScreen(0);
  redraw_watch();
  last_act = millis();
  location = WATCH;
}


int readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(75);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH;
  int result = (high << 8) | low;
  result = 1125300L / result; // 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}


void redraw_watch() {
  DateTime now = rtc.now();
  display.fillRect(0, 0, 128, 40, 0);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print(now.day());
  display.print(".");
  display.print(now.month());
  display.setTextSize(1);
  //display.print(" ");
  draw_week(now.dayOfTheWeek());
  display.setCursor(66, 8);
  display.print(readVcc() / 100);
  display.setCursor(84, 8);
  if(bt_state == BT_ON) display.print("B");
  else if(bt_state == BT_CONNECTED) display.print("BC");
  display.setCursor(10, 16);
  display.setTextSize(3);
  display.print(now.hour());
  display.print(':');
  if(now.minute() < 10) display.print("0");
  display.print(now.minute());
  display.setTextSize(2);
  if(now.second() < 10) display.print("0");
  display.println(now.second());
  display.display();
}


void draw_label(int i, byte x, byte y, bool sel, byte width){
  if(sel){
    display.fillRect(x - 1, y, width + 1, 8, 1);
    display.setTextColor(BLACK);
    if(i < 10) display.print("0");
    display.print(i);
    display.setTextColor(WHITE);
  } else{
    if(i < 10) display.print("0");
    display.print(i);
  }
}


void draw_week(byte week){
  switch(week){
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


void redraw_set(){
  display.fillScreen(0);
  display.setTextSize(2);
  display.setCursor(10, 0);
  display.print(F("настройки"));
  display.setTextSize(1);
  
  display.setCursor(12, 16);
  display.print(F("дата: "));
  draw_label(days, 48, 16, menu_select == 0, 12);
  display.print(".");
  draw_label(months, 66, 16, menu_select == 1, 12);
  display.print(".");
  draw_label(years, 84, 16, menu_select == 2, 24);
  
  display.setCursor(12, 24);
  display.print(F("время: "));
  draw_label(hours, 54, 24, menu_select == 3, 12);
  display.print(':');
  draw_label(mins, 72, 24, menu_select == 4, 12);
  
  display.setCursor(12, 32);
  display.print(F("блюпуп: "));
  if(menu_select == 5){
    display.fillRect(59, 32, 19, 8, 1);
    display.setTextColor(BLACK);
    display.print(bt_state_tmp?F("ON"):F("OFF"));
    display.setTextColor(WHITE);
  } else display.print(bt_state_tmp?F("ON"):F("OFF"));
   
  display.setCursor(12, 40);
  display.print(F("яркость: "));
  if(menu_select == 6){
    display.fillRect(65, 40, 19, 8, 1);
    display.setTextColor(BLACK);
    display.print(brightness);
    display.setTextColor(WHITE);
  } else display.print(brightness);
  display.display();
}


void redraw_table(){
  display.fillScreen(0);
  display.setTextSize(2);
  display.setCursor(6, 0);
  display.print(F("расписание"));
  //display.print(week_day);
  display.setTextSize(1);
  display.setCursor(0, 16);
  char buffer[100];
  draw_week(week_day + 1);
  if(view_week == RED_WEEK){
    display.print(F(" (красная)"));
    display.setCursor(0, 24);
    strcpy_P(buffer, (char*)pgm_read_word(&(red_table[week_day])));
  } else{
    display.print(F(" (синяя)"));
    display.setCursor(0, 24);
    strcpy_P(buffer, (char*)pgm_read_word(&(blue_table[week_day])));
  }
  display.print(buffer);
  display.display();
}

ISR(INT0_vect){
  return;
}
