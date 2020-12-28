/*
   часы с мини mp3 плеером
   версия 1.0
   для подключения дисплея раскоментировать нужный файл, строка 117
   дата: 9 июля 2018г.
   Автор: Рыбин Денис
   mejdugorka@yandex.ru
*/
const bool debug = 0; // отладка (0 - Serial отключен, 1 - включен)

#include "OneButton.h" //библиотека для кнопок
#include <OneWire.h> // для датчика температуры
#include <DallasTemperature.h> // тоже для температуры
#include <Wire.h> // I2C
#include "RTClib.h"  // real time clock
#include <SoftwareSerial.h> // стандартная библиотека (для mp3 плеера)
#include <DFMiniMp3.h>
const byte pin = 3; //сигнал от модуля часов, вывод sqw
const byte PIN_SENSOR = 4; //датчик температуры
//const byte busyPin = 5; // busy пин плеера

// инициализация модуля часов RTC
RTC_DS1307 RTC;
Ds1307SqwPinMode mode = SquareWave1HZ; // включить генерацию импульсов на выводе sqw с частотой 1 герц

//температура
OneWire oneWire(PIN_SENSOR);
DallasTemperature sensors(&oneWire);
int8_t temperatura; //текущая температура

enum states { //режимы часов
  showTime, //1 часы
  showDate, // 2 дата
  showAlarm, // 3 будильник
  showTemperatura, // 4 температура
  setMinute,
  setHour,
  setDay,
  setMonth,
  setYear,
  setAlarmMinute,
  setAlarmHour,
  setVolume,
};
states state;
volatile bool stateDot = false; //флаг мигания точками

volatile bool requestRtc = false; // флаг для запроса данных с модуля часов
volatile bool flagUpdateDisplay = false; //флаг обновления дисплея
bool modeSet = false; // необходимо для кнопок, флаг что включены настройки
uint8_t speak = 0; // 1-time, 2-date, 3-alarm, 4 - temperatura
uint8_t secondValue = 0;
uint8_t minutValue = 0;
uint8_t hourValue = 0;
uint8_t dayValue;
uint8_t monthValue;
uint8_t weekValue;
uint8_t yearValue = 18;
uint8_t alarmHour; //часы будильника, хранятся в модуле rtc по адресу 0
uint8_t alarmMinut; //минуты будильника, хранятся по аресу 1
uint8_t volume; // громкость плеера
bool alarmFlag = 1; // флаг включения будильника
uint8_t timeShowDisplay = 0; //время, идущее когда включен показ не времени, чтоб обратно переключить дисплей на показ времени

OneButton button1(A0, true); // установка первой кнопки на пин A1.
OneButton button2(A1, true); // установка второй кнопки на пин A2.
OneButton button3(A2, true); // установка третьей кнопки на пин A3.
// implement a notification class,
// its member methods will get called
class Mp3Notify {
  public:
    static void OnError(uint16_t errorCode) {
      // see DfMp3_Error for code meaning
      if (debug) {
        Serial.println();
        Serial.print("Com Error ");
        Serial.println(errorCode);
      }
    }

    static void OnPlayFinished(uint16_t globalTrack) {
      if (debug) {
        Serial.println();
        Serial.print("Play finished for #");
        Serial.println(globalTrack);
      }
    }

    static void OnCardOnline(uint16_t code) {
      if (debug) {
        Serial.println();
        Serial.print("Card online ");
        Serial.println(code);
      }
    }

    static void OnCardInserted(uint16_t code) {
      if (debug) {
        Serial.println();
        Serial.print("Card inserted ");
        Serial.println(code);
      }
    }

    static void OnCardRemoved(uint16_t code) {
      if (debug) {
        Serial.println();
        Serial.print("Card removed ");
        Serial.println(code);
      }
    }
};

SoftwareSerial secondarySerial(7, 8); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(secondarySerial);

/* подключение дисплея, раскоментировать нужныйДругой закоментировать */
//#include "display_tm1637.h" // дисплей tm1637
#include "display_74hc595.h" //дисплей со сдвиговыми регистрами 74hc595
void setup() {
  displayBegin(); // запускается из файла для подключенного дисплея
  pinMode(pin, INPUT_PULLUP);
  if (debug) Serial.begin(9600);
  sensors.begin();
  // Setup RTC
  Wire.begin();
  RTC.begin();
  if (! RTC.isrunning()) {
    if (debug) Serial.println(F("RTC is NOT running!"));
    // following line sets the RTC to the date & time this sketch was compiled
    //RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  RTC.writeSqwPinMode(mode);
  readRtc();
  //get alarm and timer
  alarmHour = RTC.readnvram(0);
  if (alarmHour > 23) alarmHour = 0;
  alarmMinut = RTC.readnvram(1);
  if (alarmMinut > 59) alarmMinut = 0;
  volume = RTC.readnvram(2);
  if (volume > 30) volume = 18;
  attachInterrupt(digitalPinToInterrupt(pin), blink, CHANGE); //прерывания для подсчета времени
  //detachInterrupt(digitalPinToInterrupt(pin));  отключение прерывания
  //noInterrupts запрещает
  //interrupts разрешает
  //Эти функции не имеют параметров
  mp3.begin();
  mp3.setVolume(volume);
  state = showTime;
  speak = 1;
  //mp3.playFolderTrack(7, 1);
  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1);
  button1.attachLongPressStart(longPressStart1);
  button2.attachClick(click2);
  button3.attachClick(click3);
}//setup
void waitMilliseconds(uint16_t msWait) { //для плеера
  uint32_t start = millis();
  while ((millis() - start) < msWait) {
    mp3.loop();
    delay(1);
  }
}
//функция прерывания для подсчета времени от импульсов модуля часов
void blink() {
  flagUpdateDisplay = true;
  timeShowDisplay++;
  if (stateDot == true) secondValue++;
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
  weekValue = now.dayOfTheWeek();
  requestRtc = false;
}

void loop() {
  button1.tick();
  button2.tick();
  button3.tick();
  delay(10);
  //if (digitalRead(busyPin) == true){
  //mp3.playFolderTrack(folder, file);
  //waitMilliseconds(200);
  //}

  if (requestRtc) readRtc();
  if (flagUpdateDisplay) updateDisplay();
  if (speak) {
    switch (speak) {
      case 1: speakTime(); break;
      case 2: speakDate(); break;
      case 3: speakAlarmTime(); break;
      case 4: speakTemperatura(); break;
    }
    speak = 0;
  }
  if (hourValue >= 7 && (minutValue == 0 || minutValue == 30) && secondValue == 0) {
    // сказать время
    speak = 1;
  }
  if (state != showTime && timeShowDisplay == 20) {
    if (modeSet) modeSet = false;
    state = showTime;
    timeShowDisplay = 0;
    readRtc();
  }
  if (alarmFlag && alarmHour == hourValue && alarmMinut == minutValue && secondValue == 5) {
    mp3.playFolder(10); // циклическое воспроизведение папки 10
  }
}// end loop

void click1() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) { //если не включен режим настроек
    if (state != showTime) state = showTime;
    speak = 1;
  } else { // включен режим настроек
    switch (state) {
      case setMinute:
        state = setHour;
        mp3.playFolderTrack(7, 3); //настройка часов
        waitMilliseconds(1400);
        if (hourValue) {
          mp3.playFolderTrack(4, hourValue); //время
        } else {
          mp3.playFolderTrack(4, 60); //время
        }
        break;
      case setHour:
        state = setDay;
        mp3.playFolderTrack(7, 6); //настройка дня
        waitMilliseconds(1400);
        mp3.playFolderTrack(1, dayValue); //время
        break;
      case setDay:
        state = setMonth;
        mp3.playFolderTrack(7, 7); //настройка месяца
        waitMilliseconds(1400);
        mp3.playFolderTrack(2, monthValue + 12); //месяц в именит. падеже
        break;
      case setMonth:
        state = setYear; // отображение года
        mp3.playFolderTrack(7, 8); //настройка года
        waitMilliseconds(1400);
        mp3.playFolderTrack(8, yearValue); //время
        break;
      case setYear:
        state = setAlarmHour;
        mp3.playFolderTrack(7, 37); //настройка часов будильника
        waitMilliseconds(2000);
        if (alarmHour) {
          mp3.playFolderTrack(4, alarmHour); //время
        } else {
          mp3.playFolderTrack(4, 24); //время
        }
        break;
      case setAlarmHour:
        state = setAlarmMinute;
        mp3.playFolderTrack(7, 38); //настройка минут будильника
        waitMilliseconds(2000);
        if (alarmMinut) {
          mp3.playFolderTrack(5, alarmMinut); //время
        } else {
          mp3.playFolderTrack(5, 60); //
        }
        break;
      case setAlarmMinute:
        state = setVolume;
        volume = mp3.getVolume();
        mp3.playFolderTrack(7, 41); //громкость
        waitMilliseconds(1200);
        mp3.playFolderTrack(9, volume); //значение громкости
        break;
      case setVolume:
        state = setMinute;
        mp3.playFolderTrack(7, 4); //настройка минут
        waitMilliseconds(1400);
        if (minutValue) {
          mp3.playFolderTrack(5, minutValue); //время
        } else {
          mp3.playFolderTrack(5, 60); //
        }
        break;
        //можно сделать тут же настройку таймера
    } //switch
  }
}//click1

void doubleclick1() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  modeSet = 0;
  state = showDate;
  speak = 2;
} //doubleclick1
void longPressStart1() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) {
    switch (state) {
      case showTime:
        state = setMinute;
        mp3.playFolderTrack(7, 4); //настройка минут
        waitMilliseconds(1400);
        if (minutValue) {
          mp3.playFolderTrack(5, minutValue); //минуты
        } else {
          mp3.playFolderTrack(5, 60); //0 минут
        }
        break;
      case showDate:
        state = setDay;
        mp3.playFolderTrack(7, 6); //настройка дня
        waitMilliseconds(1400);
        mp3.playFolderTrack(1, dayValue); //время
        break;
      case showAlarm:
        state = setAlarmHour;
        mp3.playFolderTrack(7, 37); //настройка часов будильника
        waitMilliseconds(2000);
        if (alarmHour) {
          mp3.playFolderTrack(4, alarmHour); //время
        } else {
          mp3.playFolderTrack(4, 24); //0 часов
        }
        break;
    } //switch
    modeSet = true;
  } else { //режим настроек включен, выключаем и все сохраняем
    if (state == setVolume) {
      RTC.writenvram(2, volume);
    }
    if (state == setMinute || state == setHour || state == setDay || state == setMonth || state == setYear) {
      RTC.adjust(DateTime(yearValue + 2000, monthValue, dayValue, hourValue, minutValue, 0));
      delay(50);
      readRtc();
    } else if (state == setAlarmMinute || state == setAlarmHour) {
      RTC.writenvram(0, alarmHour);
      RTC.writenvram(1, alarmMinut);
    }
    modeSet = 0;
    state = showTime;
    mp3.playFolderTrack(7, 10); //выход из настроек
    waitMilliseconds(1450);
    speak = 1;
  }
}//longPressStart1

void click2() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) {
    if (state != showAlarm) { // первое нажатие кнопки
      state = showAlarm;
      if (alarmFlag) { //включен будильник
        speak = 3;
      } else {
        mp3.playFolderTrack(7, 16); // будильник выключен
      }

    } else { // кнопка уже была нажата(включен режим будильника, повторное нажатие)
      if (!alarmFlag) { //если выключен будильник, то включаем
        alarmFlag = true;
        speak = 3;
      } else { // будильник был включен, выключаем
        alarmFlag = false;
        mp3.playFolderTrack(7, 16); // будильник выключен
      }
    }
  } else { //включен режим настроек
    switch (state) {
      case setMinute:
        if (minutValue == 0) minutValue = 59;
        else minutValue--;
        if (minutValue != 0) {
          mp3.playFolderTrack(5, minutValue);
        } else {
          mp3.playFolderTrack(5, 60);
        }
        break;
      case setHour:
        if (hourValue == 0) hourValue = 23;
        else hourValue--;
        if (hourValue != 0) {
          mp3.playFolderTrack(4, hourValue);
        } else {
          mp3.playFolderTrack(4, 24);
        }
        break;
      case setDay:
        if (dayValue == 1) dayValue = 31;
        else dayValue--;
        mp3.playFolderTrack(1, dayValue);
        break;
      case setMonth:
        if (monthValue == 1) monthValue = 12;
        else monthValue--;
        mp3.playFolderTrack(2, monthValue + 12);
        break;
      case setYear:
        if (yearValue != 18) yearValue--;
        mp3.playFolderTrack(8, yearValue);
        break;
      case setAlarmHour:
        if (alarmHour == 0) alarmHour = 23;
        else alarmHour--;
        if (alarmHour != 0) {
          mp3.playFolderTrack(4, alarmHour);
        } else {
          mp3.playFolderTrack(4, 24);
        }
        break;
      case setAlarmMinute:
        if (alarmMinut == 0) alarmMinut = 59;
        else alarmMinut--;
        if (alarmMinut != 0) {
          mp3.playFolderTrack(5, alarmMinut);
        } else {
          mp3.playFolderTrack(5, 60);
        }
        break;
      case setVolume:
        if (volume == 1) volume = 30;
        else volume--;
        mp3.setVolume(volume);
        mp3.playFolderTrack(7, 41); //громкость
        waitMilliseconds(1000);
        mp3.playFolderTrack(9, volume); //значение громкости
        break;
    } //switch
  }
} //click2

void click3() {
  flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!modeSet) {
    state = showTemperatura;
    sensors.requestTemperatures(); // отправка команды для получения температуры
    temperatura = round(sensors.getTempCByIndex(0)); //округлить round(temp)
    speak = 4;
  } else { //включен режим настроек
    switch (state) {
      case setMinute:
        minutValue++;
        if (minutValue > 59) minutValue = 0;
        if (minutValue != 0) {
          mp3.playFolderTrack(5, minutValue);
        } else {
          mp3.playFolderTrack(5, 60);
        }
        break;
      case setHour:
        hourValue++;
        if (hourValue > 23) hourValue = 0;
        if (hourValue != 0) {
          mp3.playFolderTrack(4, hourValue);
        } else {
          mp3.playFolderTrack(4, 24);
        }
        break;
      case setDay:
        dayValue++;
        if (dayValue > 31) dayValue = 1;
        mp3.playFolderTrack(1, dayValue);
        break;
      case setMonth:
        monthValue++;
        if (monthValue > 12) monthValue = 1;
        mp3.playFolderTrack(2, monthValue + 12);
        break;
      case setYear:
        yearValue++;
        if (yearValue > 59) yearValue = 18;
        mp3.playFolderTrack(8, yearValue);
        break;
      case setAlarmHour:
        alarmHour++;
        if (alarmHour > 23) alarmHour = 0;
        if (alarmHour != 0) {
          mp3.playFolderTrack(4, alarmHour);
        } else {
          mp3.playFolderTrack(4, 24);
        }
        break;
      case setAlarmMinute:
        alarmMinut++;
        if (alarmMinut > 59) alarmMinut = 0;
        if (alarmMinut != 0) {
          mp3.playFolderTrack(5, alarmMinut);
        } else {
          mp3.playFolderTrack(5, 60);
        }
        break;
      case setVolume:
        volume++;
        if (volume > 30) volume = 1;
        mp3.setVolume(volume);
        mp3.playFolderTrack(7, 41); //громкость
        waitMilliseconds(1000);
        mp3.playFolderTrack(9, volume); //значение громкости
        break;
    } //switch
  }
}//click3
void speakTime() {
  mp3.playFolderTrack(7, 36); //время
  waitMilliseconds(1000);
  if (hourValue != 0) {
    mp3.playFolderTrack(4, hourValue);
  } else {
    mp3.playFolderTrack(4, 24);
  }
  waitMilliseconds(1500);
  if (minutValue == 0) {
    //mp3.playFolderTrack(5, 60);
    mp3.playFolderTrack(7, 31); //ровно
  } else {
    mp3.playFolderTrack(5, minutValue);
  }
} //speakTime
void speakDate() {
  mp3.playFolderTrack(7, 30); //дата
  waitMilliseconds(1000);
  mp3.playFolderTrack(1, dayValue);
  waitMilliseconds(1200);
  mp3.playFolderTrack(2, monthValue);
  waitMilliseconds(1000);
  if (weekValue != 0) {
    mp3.playFolderTrack(3, weekValue);
  } else {
    mp3.playFolderTrack(3, 7);
  }
} // speakDate
void speakAlarmTime() {
  mp3.playFolderTrack(7, 15); // будильник включен
  waitMilliseconds(1500);
  if (alarmHour != 0) {
    mp3.playFolderTrack(4, alarmHour);
  } else {
    mp3.playFolderTrack(4, 24);
  }
  waitMilliseconds(1200);
  if (alarmMinut != 0) {
    mp3.playFolderTrack(5, alarmMinut);
  } else {
    mp3.playFolderTrack(5, 60);
  }
} // speakAlarmTime
void speakTemperatura() {
  mp3.playFolderTrack(7, 18);
  waitMilliseconds(1200);
  mp3.playFolderTrack(6, temperatura);
} // speakTemperatura
