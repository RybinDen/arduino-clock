/*говорящие часы на спаяном 7 сигментном дисплее со сдвиг регистрами
  10 ноябрь 2020
  Скетч использует 18298 байт  59%памяти устройства. Всего доступно 30720 байт.
  Глобальные переменные используют 1223 байт (59%) динамической памяти, оставляя 825 байт для локальных переменных. Максимум: 2048 байт.
  изменил воспроизведение режима, теперь воспроизводится в цикле loop
  сделать для будильника: однократное воспроизведение мелодии, нарастающее, цикличное воспроизведение файла, повтор через несколько минут.
  плеер: подсчет файлов чтоб запоминать номер воспроизводимого файла (при переключении папки сбрасывать файл на первый),
  выключение плеера после воспроизведения всех файлов в папке,
  воспроизведение в случайном порядке,
*/
#include <avr/pgmspace.h> // библиотека для хранения постоянных данных во флеш памяти
#include <avr/power.h>
#include <EEPROM.h> // подключение библиотеки энергонезависимой памяти для сохранения настроек будильника/таймера
#include <TimerOne.h> // библиотека аппаратного таймера
#include <MD_YX5300.h> //библиотека для плеера yx5300
#include <Wire.h> // подключаем библиотеку для работы с шиной I2C - эту шину используем модуль часов и фм радио
#include "RTClib.h" //подключение библиотеки модуля часов
#include <radio.h>
#include <RDA5807M.h> //выбор используемого чипа на модуле фм радио
#include "OneButton.h"
OneButton button1(A0, true);
OneButton button2(A1, true);
OneButton button3(A2, true);
OneButton button4(A3, true);
//подключение mp3 плеера
const uint8_t ARDUINO_RX = 7; // подключается к выводу TX плеера
const uint8_t ARDUINO_TX = 8; // подключается к выводу RX плеера
//пины для подключения к экрану
const uint8_t dataPin = 9; // 3 Пин подключен к DS входу 74HC595
const uint8_t latchPin = 10; // 2 Пин подключен к ST_CP входу 74HC595
const uint8_t clockPin = 11; // 1 Пин подключен к SH_CP входу 74HC595

MD_YX5300 mp3(ARDUINO_RX, ARDUINO_TX);
RTC_DS1307 rtc; //выбираем используемую микросхему в модуле часов
RDA5807M radio;    // создание объекта фм радио на чипе RDA5807
const byte num[] PROGMEM = {//таблица чисел для экрана
  B11111100, // 0
  B01100000, // 1
  B11011010, // 2
  B11110010, // 3
  B01100110, // 4
  B10110110, // 5
  B10111110, // 6
  B11100000, // 7
  B11111110, // 8
  B11110110, // 9
  B00000010, // -
  B00000000, // выключить
};
bool showDisplay = 1; // включен/выключен дисплей
bool isPlaying = false; // сигнал что плеер воспроизводит файл
bool alarmFlag = 0; //флаг срабатывания будильника и таймера
bool resetVolume = 0; // сбросить громкость после плеера, будильника и таймера
bool alarm = 1; // флаг включения будильника
bool requestRtc = false; // запрос времени с модуля часов
bool timerStart = 0; //флаг что таймер стартовал
bool radioSeekFlag = true;  // 1-автонастройка, 0 ручная настройка радио
bool speakMode = true; // говорить режим или нет
uint16_t frequency = 8800; //частота станции
uint8_t volume = 5; // текущая громкость
uint8_t volumeMusic = 5; //громкость плеера для музыки
uint8_t avtoSignal; //автоматически говорить время 0отключ, 1 каждый час, 2 каждые полчаса, 3 каждые 15 мин
uint8_t hBeginSignal; //начальное время автосигнала
uint8_t hEndSignal; //конечное время автосигнала
uint8_t alarmHours, alarmMinutes, alarmMelody, alarmVolume, timerMelody, timerVolume; // время будильника, мелодия и громкость
uint8_t timerValue = 10; // минуты таймера, хранятся в модуле часов по аресу 2
uint8_t musicStatus = 0; // включено 1радио, 2mp3, 3 плеер на паузе
uint8_t modeShow = 1;           // режим вывода: 1-время 2-дата, 3-будильник 4-таймер, 5-fmRadio, 6-player
uint8_t speak = 1; // воспроизвести 1-часы, 2-минуты, 3-день, 4 месяц, 5-день недели, 6- год
uint8_t totalFolders; //всего папок на флешке
uint8_t currentFolder = 10; // номер воспроизводимой папки
uint8_t totalFiles; // количество файлов в папке 9 (мелодии для таймера и будильника)

volatile bool flagUpdateDisplay = true; //сигнал, то что обновились данные для отображения для дисплея
volatile uint16_t timerCount = 0; // для подсчета времени, оставшегося до срабатывания таймера
volatile uint8_t timeShowDisplay; // запоминание предшествовавшего времени (для для возврата показа времени)
volatile bool halfsecond = 0; //для мигания точками изменяется в таймере  через 500мс
volatile uint8_t s;         // секунды
volatile uint8_t i;     // минуты
volatile uint8_t h;      // часы
volatile uint8_t d;      // дни
volatile uint8_t m;      // месяцы
volatile uint8_t y;      // год
volatile uint8_t w;      // дни недели
//процедура прерывания таймера для подсчета времени
void TimingISR() {
  if (halfsecond) {
    s ++;
    if (modeShow != 1) timeShowDisplay++;
    if (timerStart) timerCount++;
    if (s == 60) {
      i ++;
      if (i == 60) {
        h ++;
        if (h == 24)h = 0;
        i = 0;
      }
      s = 0;
    }
  }
  if (showDisplay) flagUpdateDisplay = true;
  halfsecond = !halfsecond;
}//TimingISR
// функция обратного вызова, используемая для обработки незапрошенных  сообщений плеера или ответы на запросы данных
void cbResponse(const MD_YX5300::cbData *status) {
  switch (status->code) {
    case MD_YX5300::STS_FILE_END:   // воспроизведение файла закончилось
      isPlaying = false;
      break;
    case MD_YX5300::STS_FLDR_FILES: // количество файлов в папке
      totalFiles = status->data; // число файлов
      break;
    case MD_YX5300::STS_TOT_FLDR:
      totalFolders = status->data; //количество папок
      break;
  }
} //cbResponse

void setup() {
  // отключаем неиспользуемое
  power_adc_disable(); // выключить аналоговые входы
  power_spi_disable(); // отключение spi
  power_usart0_disable(); // отключить uart
  //power_timer0_disable();используется самой ардуиной, его трогать нельзя
  //power_timer1_disable(); //используется библиотекой для сервомашинок, не отключать, часы остановятся.
  power_timer2_disable(); //если отключить функция tone() не работает
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  alarmHours = EEPROM.read(0);
  if (alarmHours > 23) alarmHours = 0;
  alarmMinutes = EEPROM.read(1);
  if (alarmMinutes > 59) alarmMinutes = 0;
  timerValue = EEPROM.read(2);
  if (timerValue > 59) timerValue = 10;
  //частота сохраненной радиостанции
  //Чтобы из двух отдельных байтов получить одно значение int, нужно
  //сдвинуть старший байт влево на 8 двоичных разрядов (high << 8) и затем
  //прибавить младший байт.
  byte high = EEPROM.read(3);
  byte low = EEPROM.read(4);
  if (high != 255 || high != 0) {
    frequency = (high << 8) + low;
  }
  volume = EEPROM.read(5);
  if (volume > 20) volume = 5;
  alarmVolume = EEPROM.read(6);
  if (alarmVolume > 20) alarmVolume = 5;
  // проверить количество файлов в папке 9
  alarmMelody = EEPROM.read(7);
  if (alarmMelody > 30) alarmMelody = 1;
  timerMelody = EEPROM.read(8);
  if (timerMelody > 30) timerMelody = 1;
  timerVolume = EEPROM.read(9);
  if (timerVolume > 20) timerVolume = 5;
  avtoSignal = EEPROM.read(10);
  if (avtoSignal > 3) avtoSignal = 1;
  hBeginSignal = EEPROM.read(11);
  if (hBeginSignal > 23) hBeginSignal = 0;
  hEndSignal = EEPROM.read(12);
  if (hEndSignal > 23) hEndSignal = 0;
  mp3.begin();
  //mp3.setSynchronous(true);
  mp3.setCallback(cbResponse);
  mp3.queryFolderCount(); //делаем запрос на количество папок
  delay(200);
  mp3.queryFolderFiles(9); //количество файлов в папке 9
  delay(200);
  mp3.volume(volume);
  if (! rtc.begin()) {
    //Serial.println(F("Couldn't find RTC"));
    //сделать если часы не найдены, то что-то надо делать?
    while (1);
  }
  if (! rtc.isrunning()) {
    //Serial.println(F("RTC is NOT running!"));
    //если часы не запущены, то включить режим настроек?
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2018 at 3am you would call:
    // rtc.adjust(DateTime(2018, 1, 21, 3, 0, 0));
  }
  readRtc();
  Timer1.initialize(500000);        // таймер на 500мс
  Timer1.attachInterrupt(TimingISR);// привяжем процедуру к прерыванию таймера
  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1);
  button1.attachLongPressStart(longPressStart1);
  button2.attachClick(click2);
  button2.attachDuringLongPress(longPress2);
  button2.attachLongPressStop(longPressStop2);
  button3.attachClick(click3);
  button3.attachDuringLongPress(longPress3);
  button3.attachLongPressStop(longPressStop3);
  button4.attachClick(click4);
  button4.attachLongPressStart(longPressStart4);
} //setup

void playRadio() {
  isPlaying = false;
  mp3.sleep();
  radio.init();
  radio.setBandFrequency(RADIO_BAND_FM, frequency); //10280
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(1);
  delay(50);
  musicStatus = 1;
  modeShow = 5;
} // playRadio
void readRtc() {
  DateTime now = rtc.now();
  s = now.second();
  i = now.minute();
  h = now.hour();
  d = now.day();
  m = now.month();
  y = now.year() - 2000;
  w = now.dayOfTheWeek();
  requestRtc = false;
} //readRtc
void updateDisplay() {
  digitalWrite(latchPin, LOW); // устанавливаем синхронизацию "защелки" на LOW
  switch (modeShow) {
    case 1:
      if (alarm) {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i % 10]) + 1); //единицы показать точку что включен будильник
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i % 10])); //единицы
      }
      if (halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i / 10]) + 1); //десятки
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h % 10]) + 1); //сотни
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i / 10])); //десятки
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h % 10])); //сотни
      }
      if (h >= 10) {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h / 10])); //тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем если время менее 10 часов. тысячи
      }
      break;
    case 2: //date
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[m % 10])); //единицы
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[m / 10]) + 1); //десятки
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[d % 10]) + 1); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[d / 10])); //тысячи
      break;
    case 3: //alarm
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMinutes % 10])); //единицы
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMinutes / 10]) + 1); //десятки
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmHours % 10]) + 1); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmHours / 10])); //тысячи
      break;
    case 4: //show timer
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[(timerValue * 60 - timerCount) % 60 % 10])); //единицы секунд
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[(timerValue * 60 - timerCount) % 60 / 10]) + 1); //десятки секунд
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[(timerValue * 60 - timerCount) / 60 % 10]) + 1); //единицы минут
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[(timerValue * 60 - timerCount) / 60 / 10])); //десятки минут
      break;
    case 5: //show radio frequency
      if (frequency >= 10000) {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency / 10 % 100 % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency / 10 % 100 / 10])); //десятки
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency / 10 / 100 % 10])); //сотни
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency / 10 / 100 / 10])); //тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency % 100 / 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency / 100 % 10])); //десятки
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[frequency / 100 / 10])); //сотни
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи не показываем
      }
      break;
    case 6: //show player folder
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[currentFolder % 10])); //единицы
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[currentFolder / 10])); //десятки
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи не показываем
      break;
    //включен режим настроек мигаем цифрами
    case 8: //настройка минут
      if (halfsecond == 0) { //мигаем минутами
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. единицы
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //отображаем только точку. десятки
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i / 10]) + 1); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h % 10]) + 1); //сотни
      if (h >= 10) {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h / 10])); //тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем если время менее 10 часов. тысячи
      }
      break;
    case 9: // настройка часов
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i % 10])); //единицы
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[i / 10]) + 1); //десятки
      if (!halfsecond) { //мигаем часами
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //ничего не отображаем. сотни
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h % 10]) + 1); //сотни
        if (h >= 10) {
          shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[h / 10])); //тысячи
        } else {
          shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем если время менее 10 часов. тысячи
        }
      }
      break;
    case 10: // настройка дня
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[m % 10])); //единицы
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[m / 10]) + 1); //десятки
      if (!halfsecond) { //мигаем днем
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //ничего не отображаем. сотни
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[d % 10]) + 1); //сотни
        if (d > 9) {
          shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[d / 10])); //тысячи
        } else {
          shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем если дни менее 10. тысячи
        }
      }
      break;
    case 11: // настройка месяца
      if (!halfsecond) { //мигаем месяцем
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. единицы
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //отображаем только точку. десятки
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[m % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[m / 10]) + 1); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[d % 10]) + 1); //сотни
      if (d > 9) {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[d / 10])); //тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем если время менее 10 часов. тысячи
      }
      break;
    case 12: // настройка года
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. единицы
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //десятки
        shiftOut(dataPin, clockPin, LSBFIRST, 0);// ничего не отображаем. сотни
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[y % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[y / 10])); //десятки
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[0])); //сотни
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[2])); //тысячи
      }
      break;
    case 13: //минуты будильника
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. единицы
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //десятки, отображаем только точку
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMinutes % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMinutes / 10]) + 1); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmHours % 10]) + 1); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmHours / 10])); //тысячи
      break;
    case 14: //часы будильника
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMinutes % 10])); //единицы
      shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMinutes / 10]) + 1); //десятки
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //отображаем только точку. сотни
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmHours % 10]) + 1); //сотни
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmHours / 10])); //тысячи
      }
      break;
    case 15: // мелодия будильника
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMelody % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmMelody / 10])); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи
      break;
    case 16: //  громкость будильника
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmVolume % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[alarmVolume / 10])); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи
      break;
    case 17: //  минуты таймера
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //единицы секунд
      shiftOut(dataPin, clockPin, LSBFIRST, 1); //десятки секунд
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 1); //отображаем только точку. сотни
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем тысячи
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[(timerValue * 60 - timerCount) / 60 % 10]) + 1); //единицы минут
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[(timerValue * 60 - timerCount) / 60 / 10])); //десятки минут
      }
      break;
    case 18: //  мелодия таймера
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[timerMelody % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[timerMelody / 10])); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи
      break;
    case 19: //  громкость таймера
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[timerVolume % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[timerVolume / 10])); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи
      break;

    case 33: //show volume
      if (!halfsecond) {
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
        shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем
      } else {
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[volume % 10])); //единицы
        shiftOut(dataPin, clockPin, LSBFIRST, pgm_read_byte(&num[volume / 10])); //десятки
      }
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //сотни
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи не показываем
      break;
  }
  digitalWrite(latchPin, HIGH); //"защелкиваем" регистр, тем самым устанавливая значения на выходах
  flagUpdateDisplay = false;
}//updateDisplay
void click1() {
  if (alarmFlag) {
    alarmFlag = 0;
    if (timerStart) {
      timerStart = 0;
      timerCount = 0;
    }
  }
  if (showDisplay) flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (musicStatus == 0) {
    isPlaying = false;
    speakMode = 1;
    switch (modeShow) { // режим вывода: 1-время 2-дата, 3-будильник 4-таймер, 5-fmRadio, 6-player
      case 1: //время
        speak = 1;
        break;
      case 2: //дата
        speak = 3;
        break;
      case 3:  //режим будильника
        modeShow = 13; //режим настроек минут будильника
        speak = 2;
        break;
      case 4: //таймер
        if (timerStart) {
          timerStart = 0;
          timerCount = 0;
        }
        modeShow = 17; // включаем режим настроек минут таймера
        mp3.playSpecific(7, 105); // время таймера
        isPlaying = true;
        speakMode = false;
        speak = 2;
        break;
      case 5: //fmRadio
        mp3.playSpecific(7, modeShow);
        break;
      case 6: //mp3 player
        mp3.playSpecific(7, modeShow);
        break;
      //settings
      case 8: //минуты
        modeShow = 9; //часы
        speak = 1;
        break;
      case 9: //часы
        //настройка даты
        modeShow = 10; //настройка дня
        speak = 3;
        break;
      case 10: //дни
        modeShow = 11; //месяцы
        speak = 4;
        break;
      case 11: //месяц
        modeShow = 12; //год
        if (y < 20) y = 20;
        speak = 6;
        break;
      case 12: //год
        modeShow = 8; // режим настройки минут
        speak = 2;
        break;
      //настройки будильника
      case 13:
        modeShow = 14; //часы будильника
        speak = 1;
        break;
      case 14:
        modeShow = 15; // мелодия будильника
        mp3.volume(alarmVolume); //установка громкости
        resetVolume = 1;
        speak = 9;
        break;
      case 15:
        modeShow = 16; // громкость будильника
        mp3.volume(alarmVolume); //установка громкости
        resetVolume = 1;
        speak = 9;
        break;
      case 16:
        modeShow = 13; //минуты будильника
        speak = 2;
        break;

      //настройки таймера
      case 17:
        modeShow = 18; //мелодия таймера
        mp3.volume(timerVolume); //установка громкости
        speak = 9;
        resetVolume = 1;
        break;
      case 18:
        modeShow = 19; // громкость таймера
        mp3.volume(timerVolume); //установка громкости
        speak = 9;
        resetVolume = 1;
        break;
      case 19:
        modeShow = 17; // настройка минут таймера
        mp3.playSpecific(7, 105); // время таймера
        isPlaying = true;
        speakMode = false;
        speak = 2;
        break;

      //20dateTime, 21 display, 22 sound
      case 20:
        //включаем настройку часов
        modeShow = 8; //настройка минут
        speak = 2;
        break;
      case 22:
        modeShow = 33; //громкость
        speak = 7;
        break;
      case 33: //громкость
        EEPROM.update(5, volume);
        modeShow = 34; //автосигнал времени
        switch (avtoSignal) {
          case 0:
            mp3.playSpecific(7, 67); //выключено
            break;
          case 1:
            mp3.playSpecific(7, 68); //каждый час
            break;
          case 2:
            mp3.playSpecific(7, 69); //каждый полчаса
            break;
          case 3:
            mp3.playSpecific(7, 70); //каждые 15 минут
        }
        break;
      case 34://автосигнал
        EEPROM.update(10, avtoSignal);
        modeShow = 35; //начальное время автосигнала
        speak = 7;
        break;
      case 35://начальное время автосигнала
        EEPROM.update(11, hBeginSignal);
        modeShow = 36; //конечное время автосигнала
        speak = 7;
        break;
      case 36://конечное м время автосигнала
        EEPROM.update(12, hEndSignal);
        mp3.playSpecific(7, 100); //выход из настроек
        isPlaying = true;
        modeShow = 1;
        speak = 1;
        break;
    }

  } else if (musicStatus == 1) { //включено радио
    radioSeekFlag = !radioSeekFlag; //автонастройка(вкл/откл)
  } else { // musicStatus 2 | 3 включен плеер
    currentFolder++;
    if (currentFolder > (totalFolders)) currentFolder = 10;
    mp3.playSpecific(6, currentFolder); //сказать номер папки
    delay(1200);
    mp3.playFolderRepeat(currentFolder);//цикл воспроизведение папки
  }
}//end click1
void doubleclick1() {
  if (showDisplay) flagUpdateDisplay = true;
  timeShowDisplay = 0;
  if (!musicStatus) {
    playRadio();
  } else if (musicStatus == 1) {
    radio.term(); // отключить радио
    mp3.wakeUp();
    musicStatus = 0;
    isPlaying = false;
    modeShow = 1;
    speak = 1;
  } else { // включен плеер, выключаем. Сделать выключение плеера на 4 кнопке?
    musicStatus = 0;
    isPlaying = false;
    modeShow = 1;
    speak = 1;
  }
} // doubleclick1
void longPressStart1() {
  if (!musicStatus) {
    if (modeShow == 1 || modeShow == 2) {
      //включаем общее меню настроек
      modeShow = 20;
      speakMode = true;
    } else if (modeShow == 6) { //player
      EEPROM.update(5, volume);
      //EEPROM.update(13, volumeNight);
    } else if (modeShow >= 8 && modeShow <= 12) { //clock
      //был включен режим настроек, выключаем его и сохраняем новое время
      rtc.adjust(DateTime(y, m, d, h, i, 0));
      delay(50);
      readRtc();
      mp3.playSpecific(7, 100);//выход из настроек
      isPlaying = true;
      modeShow = 1;
      speak = 1;
    } else if (modeShow >= 13 && modeShow <= 16) { //alarm
      EEPROM.update(0, alarmHours);
      EEPROM.update(1, alarmMinutes);
      EEPROM.update(6, alarmMelody);
      EEPROM.update(7, alarmVolume);
      //поменять звуковой файл на "будильник сохранен +время"
      mp3.playSpecific(7, 100);//выход из настроек
      isPlaying = true;
      modeShow = 1;
      speak = 1;
    } else if (modeShow >= 17 && modeShow <= 19) { //timer
      EEPROM.update(2, timerValue);
      EEPROM.update(8, timerMelody);
      EEPROM.update(9, timerVolume);
      // поменять звк файл на таймер сохранен
      mp3.playSpecific(7, 100);//выход из настроек
      isPlaying = true;
      modeShow = 1;
      speak = 1;
    }
  } else if (musicStatus == 1) {
    //сохраняем станцию, запись в два этапа
    frequency = radio.getFrequency();
    EEPROM.update(3, highByte(frequency));
    EEPROM.update(4, lowByte(frequency));
  }
} // longPressStart1
void click2() {
  isPlaying = false;
  if (alarmFlag) {
    alarmFlag = 0;
    if (timerStart) {
      timerStart = 0;
      timerCount = 0;
    }
    speak = 1;
  } else { //флаг будильника не установлен
    if (showDisplay) flagUpdateDisplay = true;
    timeShowDisplay = 0;
    if (musicStatus == 0) {
      switch (modeShow) {
        case 1: //переключить на режим 6 mp3 player
          modeShow = 6;
          speakMode = true;
          speak = 0;
          break;
        case 2:  //переключить в режим 1 время
          modeShow = 1;
          speakMode = true;
          speak = 1;
          break;
        case 3: // переключить в режим даты
          modeShow = 2;
          speakMode = true;
          speak = 3; //дата
          break;
        case 4: //переключить в режим будильника
          modeShow = 3;
          if (alarm) { //  будильник включен
            mp3.playSpecific(7, 102); //будильник включен
            isPlaying = true;
            speak = 1;
          } else {// будильник выключен
            mp3.playSpecific(7, 101); //будильник выключен
            isPlaying = true;
            speak = 0;
          }
          speakMode = false;
          break;
        case 5: //переключить в режим таймера
          modeShow = 4;
          if (timerStart == 0) {
            mp3.playSpecific(7, 103); //таймер выключен
            isPlaying = true;
            speak = 0;
          } else {
            mp3.playSpecific(7, 104);
            isPlaying = true;
            speak = 7;
          }
          speakMode = false;
          break;
        case 6: // переключить в режим  fm Radio
          modeShow = 5;
          speakMode = true;
          speak = 0;
          break;
        //настройка времени
        case 8:
          i == 0 ? i = 59 : i--; //минуты
          speak = 2;
          break;
        case 9:
          h <= 0 ? h = 23 : h--; //настройка часов
          speak = 1;
          break;
        case 10:
          d == 1 ? d = 31 : d--; //дни
          speak = 3;
          break;
        case 11:
          m == 1 ? m = 12 : m--; //месяц
          speak = 4;
          break;
        case 12:
          if (y > 20)y--;
          isPlaying = false;
          speak = 6;
          break;
        case 13:       //alarm
          alarmMinutes == 0 ? alarmMinutes = 59 : alarmMinutes--; //минуты
          speak = 2;
          break;
        case 14:
          alarmHours == 0 ? alarmHours = 23 : alarmHours--; //установка часов
          speak = 1;
          break;
        case 15:
          alarmMelody == 1 ? alarmMelody = totalFiles : alarmMelody--; //выбор мелодии
          mp3.playSpecific(9, alarmMelody);
          break;
        case 16:
          if (alarmVolume > 1) {
            alarmVolume--; //установка громкости
            mp3.volume(alarmVolume); //установка громкости
            mp3.playSpecific(9, alarmMelody);
          }
          resetVolume = 1;
          break;
        case 17:  //timer
          timerCount = 0;
          timerValue == 1 ?          timerValue = 59 : timerValue--;
          mp3.playSpecific(5, timerValue); //значение таймера
          break;
        case 18:
          timerMelody == 1 ? timerMelody = totalFiles : timerMelody--; //выбор мелодии
          mp3.playSpecific(9, timerMelody);
          break;
        case 19:
          if (!resetVolume) resetVolume = 1;
          if (timerVolume > 1) {
            timerVolume--; //уменьшение громкости
            mp3.volume(timerVolume); //установка громкости
            mp3.playSpecific(9, timerMelody);
          }
          break;
        case 20:
          modeShow = 22;
          speakMode = true;
          break;
        case 21:
          modeShow = 20;
          speakMode = true;
          break;
        case 22:
          modeShow = 21;
          speakMode = true;
          break;
        case 33: //громкость
          if (volume > 1) volume--;
          mp3.volume(volume); //установка громкости
          mp3.playSpecific(6, volume);
          break;
        case 34: //автосигнал времени
          avtoSignal == 0 ? avtoSignal = 3 : avtoSignal--;
          switch (avtoSignal) {
            case 0:
              mp3.playSpecific(7, 67); //выключено
              break;
            case 1:
              mp3.playSpecific(7, 68); //каждый час
              break;
            case 2:
              mp3.playSpecific(7, 69); //каждый полчаса
              break;
            case 3:
              mp3.playSpecific(7, 70); //каждые 15 минут
          }
          break;
        case 35: // начальное время автосигнала
          hBeginSignal == 0 ? hBeginSignal = 23 : hBeginSignal--;
          mp3.playSpecific(4, hBeginSignal ? hBeginSignal : 24);
          break;
        case 36: //конечное время автосигнадла
          hEndSignal == 0 ? hEndSignal = 23 : hEndSignal--;
          mp3.playSpecific(4, hEndSignal ? hEndSignal : 24);
      }
    } else if (musicStatus == 1) { //radio
      radio.seekDown(radioSeekFlag); //поставить в скобках true для авто настройки на предыдущую станцию
      delay(100);
      frequency = radio.getFrequency();
      modeShow = 5;
    } else { // musicStatus==2 || musicStatus==3
      mp3.playPrev(); //currentFolder предыдущая песня
    }
  }
} // click2

void longPress2() {
  if (!musicStatus) {
    if (volume > 1) {
      volume--;
      mp3.volume(volume); //установка громкости
      modeShow = 33;
    }
  } else if (musicStatus == 1) { // fmRadio
    uint8_t v = radio.getVolume();
    if (v > 0) radio.setVolume(--v);
  } else { // if(musicStatus==2 || musicStatus==3){
    if (volumeMusic > 1) {
      volumeMusic--;
      resetVolume = 1;
      mp3.volume(volumeMusic); //установка громкости
    }
  }
  delay(250);
} // longPress2
void longPressStop2() {
  if (!musicStatus) { // если плеер и радио отключены, говорим текущую громкость
    modeShow = 33;
    speak = 7;
  }
} // longPressStop2

void click3() {
  isPlaying = false;
  if (alarmFlag) {
    alarmFlag = 0;
    if (timerStart) {
      timerStart = 0;
      timerCount = 0;
    }
    speak = 1;
  } else { //флаг будильника не установлен
    if (showDisplay) flagUpdateDisplay = true;
    timeShowDisplay = 0;
    if (musicStatus == 0) {
      switch (modeShow) { // режим вывода: 1-время 2-дата, 3-будильник 4-год, 5-таймер, 6-fmRadio, 7-player, 8-file, 9-громкость
        case 1: // переключить в режим date
          modeShow = 2;
          speakMode = true;
          speak = 3;
          break;
        case 2: // переключить в режим будильника
          modeShow = 3;
          if (alarm) { //  будильник включен
            mp3.playSpecific(7, 102); //будильник включен
            isPlaying = true;
            speak = 1;
          } else {//  будильник выключен
            mp3.playSpecific(7, 101); // будильник выключен
            isPlaying = true;
            speak = 0;
          }
          speakMode = false;
          break;
        case 3: // переключаем в режим таймера
          modeShow = 4;
          if (timerStart == 0) {
            mp3.playSpecific(7, 103); //таймер выключен
            isPlaying = true;
            speak = 0;
          } else {
            mp3.playSpecific(7, 104); //таймер включен
            isPlaying = true;
            speak = 7;
          }
          speakMode = false;
          break;
        case 4: // переключить на режим fmRadio
          modeShow = 5;
          speakMode = true;
          speak = 0;
          break;
        case 5: // переключить в режим player
          modeShow = 6;
          speakMode = true;
          speak = 0;
          break;
        case 6: // в режим показа времени
          modeShow = 1;
          speakMode = true;
          speak = 1;
          break;
        //настройка
        case 8: i == 59 ? i = 0 : i++; //минуты
          speak = 2;
          break;
        case 9: h == 23 ? h = 0 : h++; //часы
          speak = 1;
          break;
        case 10: d == 31 ? d = 1 : d++; //дни
          speak = 3;
          break;
        case 11: m == 12 ? m = 1 : m++; //месяц
          speak = 4;
          break;
        case 12:
          y == 30 ? y = 20 : y++;
          isPlaying = false;
          speak = 6;
          break; //год
        //alarm
        case 13: alarmMinutes == 59 ? alarmMinutes = 0 : alarmMinutes++; //минуты
          speak = 2;
          break;
        case 14: alarmHours == 23 ? alarmHours = 0 : alarmHours++; //часы
          speak = 1;
          break;
        case 15: alarmMelody == totalFiles ? alarmMelody = 1 : alarmMelody++; //выбор мелодии
          mp3.volume(alarmVolume); //установка громкости
          mp3.playSpecific(9, alarmMelody);
          resetVolume = 1;
          break;
        case 16: alarmVolume == 30 ? alarmVolume = 1 : alarmVolume++; //установка громкости
          resetVolume = 1;
          mp3.volume(alarmVolume); //установка громкости
          mp3.playSpecific(9, alarmMelody);
          break;
        //timer
        case 17:
          timerCount = 0;
          timerValue++;
          if (timerValue > 59) timerValue = 1;
          mp3.playSpecific(5, timerValue); //значение таймера
          break;
        case 18: timerMelody == totalFiles ? timerMelody = 1 : timerMelody++; //выбор мелодии
          mp3.volume(timerVolume); //установка громкости
          mp3.playSpecific(9, timerMelody);
          resetVolume = 1;
          break;
        case 19: timerVolume == 30 ? timerVolume = 1 : timerVolume++; //установка громкости
          mp3.volume(timerVolume); //установка громкости
          mp3.playSpecific(9, timerMelody);
          resetVolume = 1;
          break;
        case 20:
          modeShow = 21;
          speakMode = true;
          break;
        case 21:
          modeShow = 22;
          speakMode = true;
          break;
        case 22:
          modeShow = 20;
          speakMode = true;
          break;
        case 33: //громкость
          if (volume < 20) volume++;
          mp3.volume(volume); //установка громкости
          mp3.playSpecific(6, volume);
          break;
        case 34:
          avtoSignal == 3 ? avtoSignal = 0 : avtoSignal++;
          switch (avtoSignal) {
            case 0:
              mp3.playSpecific(7, 67); //выключено
              break;
            case 1:
              mp3.playSpecific(7, 68); //каждый час
              break;
            case 2:
              mp3.playSpecific(7, 69); //каждый полчаса
              break;
            case 3:
              mp3.playSpecific(7, 70); //каждые 15 минут
          }
          break;
        case 35:// начальное время автосигнала
          hBeginSignal == 23 ? hBeginSignal = 0 : hBeginSignal++;
          mp3.playSpecific(4, hBeginSignal ? hBeginSignal : 24);
          break;
        case 36: //конечное время автосигнадла
          hEndSignal == 23 ? hEndSignal = 0 : hEndSignal++;
          mp3.playSpecific(4, hEndSignal ? hEndSignal : 24);
          break;
      }
    } else if (musicStatus == 1) { //radio
      radio.seekUp(radioSeekFlag); //поставить в скобках true для авто настройки на следующую станцию
      delay(100);
      frequency = radio.getFrequency();
      modeShow = 5;
    } else { // musicStatus= 2 | musicStatus=3 player
      mp3.playNext(); //currentFolder следующая песня
    }
  }
} // click3

void longPress3() {
  if (!musicStatus) {
    if (volume < 20) volume++;
    mp3.volume(volume); //установка громкости
    modeShow = 33;
  } else if (musicStatus == 1) { // fmRadio
    uint8_t v = radio.getVolume();
    if (v < 15) radio.setVolume(++v);
  } else { // musicStatus = 2 | musicStatus = 3
    if (volumeMusic < 20) volumeMusic++;
    mp3.volume(volumeMusic); //установка громкости
    resetVolume = 1;
  }
  delay(250);
} // longPress3
void longPressStop3() {
  if (!musicStatus) { //плеер и радио отключены
    modeShow = 33;
    speak = 7;
  }
} // longPressStop3
void click4() {
  if (alarmFlag) {
    alarmFlag = 0;
    if (timerStart) {
      timerStart = 0;
      timerCount = 0;
    }
    isPlaying = false;
    speak = 1;
  } else { //флаг будильника не устанорвлен
    if (showDisplay) flagUpdateDisplay = true;
    timeShowDisplay = 0;
    if (musicStatus == 0) {
      if (modeShow == 1 || modeShow == 2) { // включаем таймер
        if (!timerStart) {
          modeShow = 4;
          timerStart = true;
          timerCount = 0;
          mp3.playSpecific(7, 104); // таймер включен
          isPlaying = true;
          speak = 2;
          speakMode = 0;
        } else { //таймер включен говорил сколько осталось
          modeShow = 4;
          speakMode = 1;
          speak = 7;
        }
      } else if (modeShow == 3) { //если отображается время будильника
        if (alarm) { //  выключить будильник
          mp3.playSpecific(7, 101); //выключить будильник
          alarm = 0;
        } else {// включить будильник
          alarm = 1;
          mp3.playSpecific(7, 102); //включить будильник
          isPlaying = true;
          speak = 1;
        }
      } else if (modeShow == 4) { //показан таймер
        if (!timerStart) {
          timerStart = true;
          timerCount = 0;
          mp3.playSpecific(7, 104); // таймер включен
          isPlaying = true;
          speak = 2;
          speakMode = 0;
        } else { //таймер включен выключаем
          timerStart = 0;
          timerCount = 0;
          mp3.playSpecific(7, 103); //таймер отключен
          isPlaying = true;
          modeShow = 1;
          speakMode = true;
          speak = 1;
        }
      } else if (modeShow == 5) {
        musicStatus = 1;
        playRadio();
      } else if (modeShow == 6) { //включить player
        musicStatus = 2;
        mp3.volume(volumeMusic); //установка громкости
        mp3.playFolderRepeat(currentFolder);//цикл воспроизведение папки
        resetVolume = 1;
      } else if ((modeShow >= 8) && (modeShow <= 12)) {
        rtc.adjust(DateTime(y, m, d, h, i, 0));
        requestRtc = true;
        mp3.playSpecific(7, 100); // вызод из настроек
        isPlaying = true;
        modeShow = 1;
        speak = 1;
      } else if ((modeShow >= 13) && (modeShow <= 16)) { // включена настройка будильника
        EEPROM.update(0, alarmHours);
        EEPROM.update(1, alarmMinutes);
        EEPROM.update(6, alarmMelody);
        EEPROM.update(7, alarmVolume);
        mp3.playSpecific(7, 100); // вызод из настроек
        isPlaying = true;
        modeShow = 1;
        speak = 1;
      } else if ((modeShow >= 17) && (modeShow <= 19)) {
        EEPROM.update(2, timerValue);
        EEPROM.update(8, timerMelody);
        EEPROM.update(9, timerVolume);
        timerStart = 1;
        timerCount = 0;
        mp3.playSpecific(7, 106); //таймер сохранен
        isPlaying = true;
        modeShow = 4;
        speak = 2;
        speakMode = false;
      } else {
        mp3.playSpecific(7, modeShow);
      }
    } else if (musicStatus == 1) { //radio
      //когда радио включено, 4 кнопка отключает, переделать?
      modeShow = 1;
      radio.term();
      mp3.wakeUp();
      musicStatus = 0;
      speak = 1;
    } else if (musicStatus == 2) {
      mp3.playPause(); //пауза
      modeShow = 6;
      musicStatus = 3;
    } else if (musicStatus == 3) { //плеер стоит на паузе, включаем воспроизведение
      mp3.playStart();
      modeShow = 6;
      musicStatus = 2;
    }
  }
} //click4
/*сделать автостоп
    if(modeShow == 6 || modeShow == 7){ //fmRadio, player
  mp3.playSpecific(7, 45); //автостоп
  mp3.playSpecific(5, avtostopTime); //автостоп
  avtostopFlag = !avtostopFlag;
    }
*/
void longPressStart4() {
  if (musicStatus == 1) {
    musicStatus = 0;
    radio.term();
    mp3.wakeUp();
  } else if (musicStatus > 1) {
    musicStatus = 0;
  }
  if (showDisplay) {
    showDisplay = 0;
    flagUpdateDisplay = 0;
    digitalWrite(latchPin, LOW); // устанавливаем синхронизацию "защелки" на LOW
    shiftOut(dataPin, clockPin, LSBFIRST, 0); //ничего не отображаем. единицы
    shiftOut(dataPin, clockPin, LSBFIRST, 0); //десятки
    shiftOut(dataPin, clockPin, LSBFIRST, 0);// ничего не отображаем. сотни
    shiftOut(dataPin, clockPin, LSBFIRST, 0); //тысячи
    digitalWrite(latchPin, HIGH); //"защелкиваем" регистр, тем самым устанавливая значения на выходах
    mp3.playSpecific(7, 62); //дисплей отключен
  } else {
    flagUpdateDisplay = 1;
    showDisplay = 1;
    mp3.playSpecific(7, 63); //дисплей включен
  }
} //longPressStart4
void speakHours() {
  if (modeShow == 14 || modeShow == 3) { //включен режим будильника
    if (alarmHours != 0) mp3.playSpecific(4, alarmHours);
    else mp3.playSpecific(4, 0x18);
  } else { //текущее время
    if (h != 0) mp3.playSpecific(4, h);
    else mp3.playSpecific(4, 0x18);
  }
  if (modeShow == 1 || modeShow == 3) {
    speak = 2;
    if (alarmFlag) alarmFlag = 0; //зачем?
  } else {
    speak = 0;
  }
  isPlaying = true;
}//end speakHours
void speakMinuts() {
  if (modeShow == 1) {
    if (i != 0) mp3.playSpecific(5, i);
    else mp3.playSpecific(7, 31); // ровно
  } else if (modeShow == 3) { //включен будильник
    if (alarmMinutes != 0) mp3.playSpecific(5, alarmMinutes);
    else mp3.playSpecific(7, 31); // ровно
  } else if (modeShow == 4) { //включен таймер
    mp3.playSpecific(5, timerValue);
  }
  //settings
  else if (modeShow == 8) {
    if (i != 0) mp3.playSpecific(5, i);
    else mp3.playSpecific(5, 0x3c); // file №60 0 минут
  } else if (modeShow == 13) {
    if (alarmMinutes != 0) mp3.playSpecific(5, alarmMinutes);
    else mp3.playSpecific(5, 0x3c); //file №60 - 0 минут
  } else if (modeShow == 17) {
    mp3.playSpecific(5, timerValue); //значение таймера
  }
  speak = 0;
  isPlaying = true;
}//end speakMinuts
void speakDay() {
  if (modeShow == 2) {
    mp3.playSpecific(1, d);
    speak = 4;
  } else {
    mp3.playSpecific(1, d);
    speak = 0;
  }
  isPlaying = true;
}//end speakDay
void speakMonth() {
  if (modeShow == 2) {
    mp3.playSpecific(2, m);
    speak = 5;
  } else {
    //воспроизводим месяц в именит падеже
    mp3.playSpecific(2, m + 12); //воспроизвести месяц со смещением 12 файлов
    speak = 0;
  }
  isPlaying = true;
}//end speakMonth
void speakYears() {
  mp3.playSpecific(8, y);
  speak = 0;
  isPlaying = true;
}//end speakYears
void speakWeek() {
  if (w == 0) {
    mp3.playSpecific(3, 7);
  } else {
    mp3.playSpecific(3, w);
  }
  speak = 0;
  isPlaying = true;
}//end speakWeek
void speakFolder6() {
  if (modeShow == 4) {
    uint8_t minutes = (timerValue * 60 - timerCount) / 60;
    uint8_t secunds = (timerValue * 60 - timerCount) % 60;
    if (minutes != 0) mp3.playSpecific(6, minutes); //минуты
    else mp3.playSpecific(6, 100); //0 минут
    delay(1600);
    if (secunds != 0) mp3.playSpecific(6, secunds); //секунды
    else mp3.playSpecific(6, 100); //0 secund
  } else if (modeShow == 19) {
    mp3.playSpecific(9, timerMelody);
  } else if (modeShow == 33) {
    mp3.playSpecific(6, volume);
  } else if (modeShow == 35) {
    mp3.playSpecific(4, hBeginSignal ? hBeginSignal : 24);
  } else if (modeShow == 36) {
    mp3.playSpecific(4, hEndSignal ? hEndSignal : 24);
  }

  speak = 0;
  isPlaying = true;
} //end speakFolder6
void speakFolder9() {
  if (modeShow == 3) {
    mp3.playSpecific(9, alarmMelody);
    modeShow = 1;
    speak = 1;
  } else if (modeShow == 4) {
    mp3.playSpecific(9, timerMelody);
    modeShow = 1;
    speakMode = true;
    speak = 1;
  } else     if (modeShow == 15) {
    mp3.playSpecific(9, alarmMelody);
  } else if (modeShow == 16) {
    mp3.playSpecific(9, alarmMelody);
  } else if (modeShow == 18) {
    mp3.playSpecific(9, timerMelody);
  } else if (modeShow == 19) {
    mp3.playSpecific(9, timerMelody);
  }
  isPlaying = true;
} //end speakFolder9
void loop() {
  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();
  delay(10);
  mp3.check();        // запуск обработки сообщений от mp3
  // если что то поменялось обновим запись времени и выведем на дисплей
  if (requestRtc) readRtc();
  if (flagUpdateDisplay) updateDisplay();

  //возврат к показу времени
  if (modeShow != 1 && timeShowDisplay >= 10) { // вернуться в отображение времени через 10 секунд
    if (modeShow >= 8 && modeShow <= 12) requestRtc = true;
    if (modeShow >= 13 && modeShow <= 16) {
      mp3.playSpecific(7, 102); //таймер включен
      isPlaying = true;
      modeShow = 3;
      speakMode = false;
      speak = 1;
    } else if ((modeShow >= 17) && (modeShow <= 19)) {
      timerStart = 1;
      timerCount = 0;
      mp3.playSpecific(7, 104); //таймер включен
      isPlaying = true;
      modeShow = 4;
      speakMode = false;
      speak = 2;
    } else {
      modeShow = 1;
      if (!musicStatus && !alarmFlag) {
        speakMode = 1;
        speak = 1;
      }
    }
    if (showDisplay) flagUpdateDisplay = true;
    timeShowDisplay = 0;
  }

  //таймер
  if (timerStart && timerValue * 60 == timerCount) {
    if (musicStatus == 1) { // выключаем fmRadio
      radio.term();
      musicStatus = 0;
      mp3.wakeUp();
    } else if (musicStatus > 1) musicStatus = 0;
    timerCount = 0;
    timerStart = 0;
    mp3.volume(timerVolume); //установка громкости
    alarmFlag = true;
    modeShow = 4;
    timeShowDisplay = 0;
    isPlaying = false;
    speakMode = true;
    speak = 9;
    resetVolume = 1;
  }

  //будильник
  if (alarmHours == h && alarmMinutes == i && s == 5 && alarm == 1 && alarmFlag == 0) {
    if (musicStatus == 1) { //fmRadio
      musicStatus = 0;
      radio.term();
      mp3.wakeUp();
    } else if (musicStatus > 1) musicStatus = 0;
    isPlaying = false;
    mp3.volume(alarmVolume); //установка громкости
    alarmFlag = 1;
    modeShow = 3;
    speakMode = true;
    speak = 9;
    resetVolume = 1;
  }

  //автосигнал
  if (!musicStatus && avtoSignal && !isPlaying) { //fmRadio, player
    //avtoSignal: 0 отключен, 1 каждый час, 2 каждые полчаса, 3 каждые 15 минут
    if (hBeginSignal == hEndSignal) { //одинаковое начальное и конечное значение часов
      switch (avtoSignal) {
        case 1: //каждый час
          if (i == 0 && s == 0) speak = 1;
          break;
        case 2: // каждый полчаса
          if ((i == 0 || i == 30) && s == 0) speak = 1;
          break;
        case 3: //каждые 15 минут
          if ((i == 0 || i == 15 || i == 30 || i == 45) && s == 0) speak = 1;
          break;
      }
    } else if (hBeginSignal < hEndSignal) { //начальное меньше конечного
      switch (avtoSignal) {
        case 1: //каждый час
          if ((h >= hBeginSignal && h <= hEndSignal) && i == 0 && s == 0) speak = 1;
          break;
        case 2: // каждый полчаса
          if ((h >= hBeginSignal && h <= hEndSignal) && (i == 0 || i == 30) && s == 0) speak = 1;
          break;
        case 3: //каждые 15 минут
          if ((h >= hBeginSignal && h <= hEndSignal) && (i == 0 || i == 15 || i == 30 || i == 45) && s == 0) speak = 1;
          break;
      }
    } else if (hBeginSignal > hEndSignal) { //начальное больше конечного
      switch (avtoSignal) {
        case 1: //каждый час
          if ((h >= hBeginSignal || h <= hEndSignal) && i == 0 && s == 0) speak = 1;
          break;
        case 2: // каждый полчаса
          if ((h >= hBeginSignal || h <= hEndSignal) && (i == 0 || i == 30) && s == 0) speak = 1;
          break;
        case 3: //каждые 15 минут
          if ((h >= hBeginSignal || h <= hEndSignal) && (i == 0 || i == 15 || i == 30 || i == 45) && s == 0) speak = 1;
          break;
      }
    }
  }//автосигнал

  if ((musicStatus == 0) && (isPlaying == false)) {
    if (resetVolume && modeShow < 3) {
      mp3.volume(volume); //установка громкости
      resetVolume = 0;
    }
    if (speakMode) {
      mp3.playSpecific(7, modeShow);
      speakMode = false;
      isPlaying = true;
    }
  }

  if ((speak != 0) && (isPlaying == false)) {
    switch (speak) {
      // сделать чтоб сначал воспроизводился режим (папка 9), потом время
      case 1:
        speakHours(); break;
      case 2:
        speakMinuts(); break;
      case 3:
        speakDay(); break;
      case 4:
        speakMonth(); break;
      case 5:
        speakWeek(); break;
      //переделать чтобы speak был равен воспроизводимой папке
      case 6:
        speakYears(); break;
      case 7:
        speakFolder6(); break;
      case 9:
        speakFolder9(); break;
    }
  }
}//end loop
