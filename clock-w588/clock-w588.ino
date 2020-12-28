/*Talking Clock with WT588D - U 32M audio module
  это предпоследняя версия, в которой ошибка таймера
  часы при нажатии кнопки сначала говорят, а только потом обновляют экран.
  Сделать переменную для воспроизведения и проверять ее статус в цикле чтоб сначала обновлялся экран а потом говорили
  и в зависимости от режима показа -говорить
*/
#include <avr/power.h>
//#include <avr/sleep.h>
#include "OneButton.h" //библиотека для кнопок
#include "WT588D.h"  // audio module
#include <Wire.h> // I2C
#include "RTClib.h"  // real time clock
#include "TM1637.h" // подключаем дисплей
#define pin 3 //сигнал от модуля часов
// дисплей
#define CLK 4 // TM1637 clk
#define DIO 5 // TM1637 dio

// подключение звукового чипа WT588D
//14(20)-vcc, 8(14)-gnd для модуля w588-16d 1mbite(в скобках 4mb)
//динамик + к 3(9), - к 4(10)
//далее после табов указаны выводы для модуля 1мб
#define WT588D_RST 7  //Module pin "REST" or pin # 7	1
#define WT588D_BUSY 10 //Module pin "LED/BUSY" or pin # 21	15
#define WT588D_SDA 8  //Module pin "P01" or pin # 18	12
#define WT588D_CS 6   //Module pin "P02" or pin # 17	11
#define WT588D_SCL 9  //Module pin "P03" or pin # 16	10

#define PIN_BUZZER 11 //пищалка

// Setup RTC
RTC_DS1307 RTC;
Ds1307SqwPinMode mode = SquareWave1HZ;
TM1637 display(CLK, DIO);

volatile bool requestRtc = false; // флаг для запроса данных с модуля часов
volatile bool stateDot = false; //флаг мигания точками
volatile bool flagUpdateDisplay = false; //флаг обновления дисплея
enum states { //режим часов
  showTime,
  showDate,
  showAlarm,
  setMinute,
  setHour,
  setDay,
  setMonth,
  setYear,
  setAlarmMinute,
  setAlarmHour,
  timer,
  //volume,
};
states state;
bool modeSet = 0; //режим настроек
uint8_t secondValue = 0;
uint8_t minutValue = 0;
uint8_t hourValue = 0;
uint8_t dayValue;
uint8_t monthValue;
uint8_t weekValue;
uint8_t yearValue = 19;
uint8_t alarmHour; //часы будильника, хранятся в модуле rtc по адресу 0
uint8_t alarmMinut; //минуты будильника, хранятся по аресу 1
bool alarmFlag = 1; // флаг включения будильника
bool timerStart = 0; //флаг что таймер стартовал
uint8_t timerValue; // минуты таймера, хранятся в модуле часов по аресу 2
uint16_t timerCount = 0; // для подсчета времени, оставшегося до срабатывания таймера
bool buzzer = 0; // если 1, то включена пищалка
uint8_t buzzerCount = 0;
uint8_t timeShowDisplay = 0; //время, идущее когда включен показ не времени, чтоб обратно переключить дисплей на показ времени

WT588D myWT588D(WT588D_RST, WT588D_CS, WT588D_SCL, WT588D_SDA, WT588D_BUSY);
OneButton button1(A1, true); // установка первой кнопки на пин A1.
OneButton button2(A2, true); // установка второй кнопки на пин A2.
OneButton button3(A3, true); // установка третьей кнопки на пин A3.

void speakPhrase(uint8_t phrase, bool cikl = 1) {
  myWT588D.playSound(phrase);
  // waits for WT588D to finish sound
  delay(50);
  if (cikl) {
    while (myWT588D.isBusy()) {}
  }
}//speakPhrase

void setup() {
  //чтобы включить enable
  power_adc_disable(); // выключить аналоговые входы
  power_spi_disable(); // отключение spi
  // power_twi_disable(); // отключение i2c
  power_usart0_disable(); // отключить uart
  //power_timer0_disable();используется самой ардуиной, его трогать нельзя
  power_timer1_disable(); //используется библиотекой для сервомашинок
  //power_timer2_disable(); //если отключить функция tone() не работает
  // power_all_disable();
  //Serial.begin(9600);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(pin, INPUT_PULLUP);
  myWT588D.begin(); // initialize the chip and port mappiing
  // Setup RTC
  Wire.begin();
  RTC.begin();
  if (! RTC.isrunning()) {
    //Serial.println(F("RTC is NOT running!"));
    // following line sets the RTC to the date & time this sketch was compiled
    // RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  RTC.writeSqwPinMode(mode);
  //get alarm and timer
  alarmHour = RTC.readnvram(0);
  if (alarmHour > 23) alarmHour = 0;
  alarmMinut = RTC.readnvram(1);
  if (alarmMinut > 59) alarmMinut = 0;
  timerValue = RTC.readnvram(2);
  if (timerValue > 59) timerValue = 0;
  readRtc();
  display.set();
  display.clearDisplay();
  tone(PIN_BUZZER, 2000, 50);
  delay(400);
  // play boot up sound
  speakPhrase(0xe7); // E0H~E7H громкость
  speakPhrase(hourValue);
  speakPhrase(minutValue, 0);
  attachInterrupt(digitalPinToInterrupt(pin), blink, CHANGE); //прерывания для подсчета времени
  //detachInterrupt(digitalPinToInterrupt(pin));  отключение прерывания
  //noInterrupts запрещает
  //interrupts разрешает
  //Эти функции не имеют параметров
  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1); //если используются двойные клики то кнопки подтормаживают
  button1.attachLongPressStart(longPressStart1);
  button2.attachClick(click2);
  button2.attachLongPressStart(longPressStart2);
  button3.attachClick(click3);
  button3.attachLongPressStart(longPressStart3);
}//setup
//функция прерывания для подсчета времени от импульсов модуля часов
void blink() {
  flagUpdateDisplay = true;
  timeShowDisplay++;
  if (stateDot == true) {
    secondValue++;
    if (timerStart) timerCount++;
  }
  if (buzzer && stateDot) {
    buzzerCount++;
    tone(PIN_BUZZER, 2000, 250);
  }

  if (secondValue == 60) {
    minutValue++;
    if (minutValue == 60) {
      hourValue++;
      if (hourValue == 24) requestRtc = true;
      minutValue = 0;
    }
    secondValue = 0;
  }
  stateDot = !stateDot; //меняем состояние мигания точек
} //blink
void readRtc() { // Get time
  DateTime now = RTC.now();
  hourValue = now.hour();
  minutValue = now.minute();
  secondValue = now.second();
  dayValue = now.day();
  monthValue = now.month();
  yearValue = now.year() - 2000;
  if (yearValue < 19) yearValue = 19;
  weekValue = now.dayOfTheWeek();
  requestRtc = false;
}

void loop() {
  button1.tick();
  button2.tick();
  button3.tick();
  delay(10);
  if (requestRtc) readRtc();
  if (flagUpdateDisplay) updateDisplay();
  if (hourValue >= 7 && (minutValue == 0 || minutValue == 30) && secondValue == 0) {
    speakPhrase(0x3e); //время
    speakPhrase(hourValue);
    speakPhrase(minutValue);
  }
  if (state != showTime && timeShowDisplay == 20) {
    if (modeSet) modeSet = 0;
    tone(PIN_BUZZER, 2000, 50);
    state = showTime;
    timeShowDisplay = 0;
    readRtc();
  }
  if (timerStart && timerValue * 60 - 60 == timerCount) {
    tone(PIN_BUZZER, 2000, 1000);
  }
  if (timerStart && timerValue * 60 == timerCount) {
    buzzer = true;
  }
  if (alarmFlag && alarmHour == hourValue && alarmMinut == minutValue && secondValue == 8) {
    buzzer = true;
  }
  if (buzzer && buzzerCount == 10) { //если пропикало 10 раз все сбрасываем
    buzzer = 0;
    buzzerCount = 0;
    if (timerStart) {
      timerStart = 0;
      timerCount = 0;
    }
  }
}// end loop
void updateDisplay() {
  switch (state) {
    case showTime:
      display.point(stateDot);
      display.display(0, hourValue / 10);
      display.display(1, hourValue % 10);
      display.display(2, minutValue / 10);
      display.display(3, minutValue % 10);
      break;
    case showDate:
      display.point(1);
      display.display(0, dayValue / 10);
      display.display(1, dayValue % 10);
      display.display(2, monthValue / 10);
      display.display(3, monthValue % 10);
      break;
    case showAlarm:
      display.point(1);
      display.display(0, alarmHour / 10);
      display.display(1, alarmHour % 10);
      display.display(2, alarmMinut / 10);
      display.display(3, alarmMinut % 10);
      break;
    case setMinute:
      display.point(1); // отображаем точку
      display.display(0, hourValue / 10);
      display.display(1, hourValue % 10);
      if (stateDot) { //мигаем минутами
        display.display(2, minutValue / 10);
        display.display(3, minutValue % 10);
      } else {
        display.display(2, 0x7f); //ничего не показываем
        display.display(3, 0x7f); // ничего не показываем
      }
      break;
    case setHour:
      display.point(1); // отображаем точку
      if (stateDot) { //мигаем часами
        display.display(0, hourValue / 10);
        display.display(1, hourValue % 10);
      } else {
        display.display(0, 0x7f); //ничего не показываем
        display.display(1, 0x7f); // ничего не показываем
      }
      display.display(2, minutValue / 10);
      display.display(3, minutValue % 10);
      break;
    case setDay:
      display.point(1); // отображаем точку
      if (stateDot) { //мигаем днем
        display.display(0, dayValue / 10);
        display.display(1, dayValue % 10);
      } else {
        display.display(0, 0x7f); //ничего не показываем
        display.display(1, 0x7f); // ничего не показываем
      }
      display.display(2, monthValue / 10);
      display.display(3, monthValue % 10);
      break;
    case setMonth:
      display.point(1);
      display.display(0, dayValue / 10);
      display.display(1, dayValue % 10);
      if (stateDot) { //мигаем месяцем
        display.display(2, monthValue / 10);
        display.display(3, monthValue % 10);
      } else {
        display.display(2, 0x7f); //ничего не показываем
        display.display(3, 0x7f); // ничего не показываем
      }
      break;
    case setAlarmHour:
      display.point(1);
      if (stateDot) { //мигаем часами будильника
        display.display(0, alarmHour / 10);
        display.display(1, alarmHour % 10);
      } else {
        display.display(0, 0x7f); //ничего не показываем
        display.display(1, 0x7f); // ничего не показываем
      }
      display.display(2, alarmMinut / 10);
      display.display(3, alarmMinut % 10);
      break;
    case setAlarmMinute:
      display.point(1);
      display.display(0, alarmHour / 10);
      display.display(1, alarmHour % 10);
      if (stateDot) { //мигаем минутами будильника
        display.display(2, alarmMinut / 10);
        display.display(3, alarmMinut % 10);
      } else {
        display.display(2, 0x7f); //ничего не показываем
        display.display(3, 0x7f); // ничего не показываем
      }
      break;
    case timer:
      display.point(1);
      display.display(0, (timerValue * 60 - timerCount) / 60 / 10); //показываем десятки оставшихся минут
      display.display(1, (timerValue * 60 - timerCount) / 60 % 10); // показываем оставшиеся единицы минут
      display.display(2, (timerValue * 60 - timerCount) % 60 / 10); //десятки секунд
      display.display(3, (timerValue * 60 - timerCount) % 60 % 10); //единицы секунд
      break;
  }
  flagUpdateDisplay = false;
} //updateDisplay

void click1() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  // нажатие кнопки отключает таймер
  if (buzzer || timerStart) {
    buzzer = false;
    buzzerCount = 0;
    timerCount = 0;
    timerStart = 0;
  }
  if (!modeSet) { //если не включен режим настроек
    if (state != showTime) state = showTime;
    speakPhrase(0x3e); //время
    speakPhrase(hourValue);
    speakPhrase(minutValue);
  } else { // включен режим настроек
    switch (state) {
      case setMinute:
        state = setHour;
        speakPhrase(0x3c); //настройка часов
        speakPhrase(hourValue, 0);
        break;
      case setHour:
        state = setDay;
        speakPhrase(0x44); //настройка дня
        speakPhrase(dayValue, 0);
        break;
      case setDay:
        state = setMonth;
        speakPhrase(0x45); //настройка месяца
        speakPhrase(monthValue, 0);
        break;
      case setMonth:
        state = setYear;
        speakPhrase(0x4d); //настройка года
        speakPhrase(yearValue, 0);
        break;
      case setYear:
        state = setAlarmHour;
        speakPhrase(0x53); //настройка часов будильника
        speakPhrase(alarmHour, 0);
        break;
      case setAlarmHour:
        state = setAlarmMinute;
        speakPhrase(0x54); //настройка минут будильника
        speakPhrase(alarmMinut, 0);
        break;
      case setAlarmMinute:
        state = setMinute;
        speakPhrase(0x3d); //настройка минут
        speakPhrase(minutValue, 0);
        break;
        //можно сделать тут же настройку таймера
    } //switch
  }
}//click1

void doubleclick1() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  state = showDate;
  speakPhrase(0x43); //дата
  speakPhrase(dayValue);
  speakPhrase(monthValue);
  speakPhrase(weekValue + 0x46);
} //doubleclick1
void longPressStart1() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) {
    switch (state) {
      case showTime:
        modeSet = 1;
        state = setMinute;
        speakPhrase(0x3d); //настройка минут
        speakPhrase(minutValue, 0);
        break;
      case timer:
        //переходить в режим настроек для таймера не нужно, сразу сохраняем
        RTC.writenvram(2, timerValue);
        tone(PIN_BUZZER, 1000, 500);
        state = showTime;
        break;
      case showDate:
        modeSet = 1;
        state = setDay;
        speakPhrase(0x44); //настройка дня
        speakPhrase(dayValue, 0);
        break;
      case showAlarm:
        modeSet = 1;
        state = setAlarmHour;
        speakPhrase(0x53); //настройка часов будильника
        speakPhrase(alarmHour, 0);
        break;
    } //switch
  } else { //режим настроек включен, выключаем и все сохраняем
    modeSet = 0;
    if (state == setMinute || state == setHour || state == setYear) {
      RTC.adjust(DateTime(yearValue + 2000, monthValue, dayValue, hourValue, minutValue, 0));
      delay(50);
      readRtc();
    } else {
      RTC.writenvram(0, alarmHour);
      RTC.writenvram(1, alarmMinut);
    }
    tone(PIN_BUZZER, 1000, 500);
    state = showTime;
  }
}//longPressStart1

void click2() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (buzzer) {
    buzzer = false;
    buzzerCount = 0;
    timerCount = 0;
    timerStart = 0;
  } else {
    if (!modeSet) {
      if (state == timer) {
        timerCount = 0;
        timerValue--;
        if (timerValue < 1) timerValue = 59;
        speakPhrase(timerValue, 0);
      } else { //таймер не включен
        if (state != showAlarm) { //первое нажатие кнопки
          state = showAlarm;
          if (alarmFlag) { //если будильник включен
            speakPhrase(0x41); // будильник
            speakPhrase(alarmHour);
            speakPhrase(alarmMinut, 0);
          } else { //  будильник не включен
            speakPhrase(0x52, 0); // будильник выключен
          }
        } else { //повторное нажатие
          if (!alarmFlag) { //если режим будильник не включен
            alarmFlag = 1;
            speakPhrase(0x41); // будильник
            speakPhrase(alarmHour);
            speakPhrase(alarmMinut, 0);
          } else { // режим будильник включен
            alarmFlag = false;
            speakPhrase(0x52, 0); // будильник выключен
          }
        }
      }
    } else { //включен режим настроек
      switch (state) {
        case setMinute:
          if (minutValue == 0) minutValue = 59;
          else minutValue--;
          speakPhrase(minutValue, 0);
          break;
        case setHour:
          if (hourValue == 0) hourValue = 23;
          else hourValue--;
          speakPhrase(hourValue, 0);
          break;
        case setDay:
          if (dayValue == 1) dayValue = 31;
          else dayValue--;
          speakPhrase(dayValue, 0);
          break;
        case setMonth:
          if (monthValue == 1) monthValue = 12;
          else monthValue--;
          speakPhrase(monthValue, 0);
          break;
        case setYear:
          if (yearValue > 19) yearValue--;
          speakPhrase(yearValue, 0);
          break;
        case setAlarmHour:
          if (alarmHour == 0) alarmHour = 23;
          else alarmHour--;
          speakPhrase(alarmHour, 0);
          break;
        case setAlarmMinute:
          if (alarmMinut == 0) alarmMinut = 59;
          else alarmMinut--;
          speakPhrase(alarmMinut, 0);
          break;
      } //switch
    }
  }
} //click2

void longPressStart2() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) {
    if (state == showAlarm) {
      alarmHour = RTC.readnvram(0);
      alarmMinut = RTC.readnvram(1);
      speakPhrase(0x41); // будильник
      speakPhrase(alarmHour);
      speakPhrase(alarmMinut, 0);
    }
  }
}//longPressStart2

void click3() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (buzzer) { // сработала пищалка, отключаем ее и больше ничего не делаем
    buzzer = false;
    buzzerCount = 0;
    timerCount = 0;
    timerStart = 0;
  } else { //пищалка не сработала что нибудь делаем
    if (!modeSet) {
      if (state == timer) {
        timerCount = 0;
        timerValue++;
        if (timerValue > 59) timerValue = 1;
        speakPhrase(timerValue, 0);
      } else { // таймер не включен, включаем
        if (state != timer) state = timer;
        if (timerStart == 0) {
          timerStart = 1;
          //timerValue = RTC.readnvram(2);
          timerCount = 0;
          speakPhrase(0x42); //таймер
          speakPhrase(timerValue, 0);
        } else { //таймер был включен, считаем чколько осталось
          speakPhrase(0x42); //timer сказать сколько осталосоь
          speakPhrase((timerValue * 60 - timerCount) / 60); //минуты
          speakPhrase((timerValue * 60 - timerCount) % 60, 0); //секунды
        }
      }
    } else { //включен режим настроек
      switch (state) {
        case setMinute:
          minutValue++;
          if (minutValue > 59) minutValue = 0;
          speakPhrase(minutValue, 0);
          break;
        case setHour:
          hourValue++;
          if (hourValue > 23) hourValue = 0;
          speakPhrase(hourValue, 0);
          break;
        case setDay:
          dayValue++;
          if (dayValue > 31) dayValue = 1;
          speakPhrase(dayValue, 0);
          break;
        case setMonth:
          monthValue++;
          if (monthValue > 12) monthValue = 1;
          speakPhrase(monthValue, 0);
          break;
        case setYear:
          yearValue++;
          if (yearValue > 59) yearValue = 19;
          speakPhrase(yearValue, 0);
          break;
        case setAlarmHour:
          alarmHour++;
          if (alarmHour > 23) alarmHour = 0;
          speakPhrase(alarmHour, 0);
          break;
        case setAlarmMinute:
          alarmMinut++;
          if (alarmMinut > 59) alarmMinut = 0;
          speakPhrase(alarmMinut, 0);
          break;
      } //switch
    }
  }
}//click3

void longPressStart3() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) {
    if (state != timer) state = timer;
    timerValue = RTC.readnvram(2);
    if (timerValue > 59) timerValue = 59;
    timerCount = 0;
    speakPhrase(0x42, 1); //таймер
    speakPhrase(timerValue, 0);
  }
}//longPressStart3
