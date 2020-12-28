// часы с дисплеем lcd 16*2
//сейчас дни недели не настроены, они расчитываются по текущей дате, сделать чтобы при настройке даты в часы сохранялась правильнный день недели.
//вычисляет правильный днеь дедели функция dayOfTheWeek()
// сделать сначала настройку года, затем месяц и день, для того чтобы правильно расчитать сколько дней в месяце
// Скетч использует 6572 байт (20%) памяти устройства. Всего доступно 32256 байт.
//Глобальные переменные используют 521 байт (25%) динамической памяти, оставляя 1527 байт для локальных переменных. Максимум: 2048 байт.
//#include <avr/pgmspace.h> // для хранения постоянных данных во влеш памяти
#include <TimerOne.h> // библиотека аппаратного таймера
#include <Wire.h> // I2C для модуля часов
#define DS1307_ADDRESS 0x68 //адрес часов ds1307
#include <LiquidCrystal.h>// библиотека экрана
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);// инициализируем выводы экрана
char line1[17]; //первая строка для экрана
char line2[17]; //вторая строка
volatile bool requestRtc = false; // флаг для запроса данных с модуля часов
volatile bool flagUpdateDisplay = false; //флаг обновления дисплея
volatile uint8_t s;         // секунды
volatile uint8_t i;     // минуты
volatile uint8_t h;      // часы
volatile uint8_t w;      // дни недели
volatile uint8_t d;      // дни
volatile uint8_t m;      // месяцы
volatile uint8_t y;      // год
const uint8_t daysInMonth [] PROGMEM = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const char daysOfTheWeek[7][12] PROGMEM = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};
uint8_t dayOfTheWeek(uint8_t y, uint8_t m, uint8_t d) { //вычисляет день недели по текущей дате
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0) ++days;
  days = days + 365 * y + (y + 3) / 4 - 1; //общее количество дней
  return (days + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}
byte bcdToDec(byte val) {
  return ((val / 16 * 10) + (val % 16));
}
byte decToBcd(byte val) {
  return ((val / 10 * 16) + (val % 10));
}
void writeByteDS1307(byte addr, byte toRecByte) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(addr);
  Wire.write(decToBcd(toRecByte));
  Wire.endTransmission();
}
void setDateDs1307(byte i, byte h, byte d, byte m, byte y) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0);
  Wire.write(decToBcd(0));
  Wire.write(decToBcd(i));
  Wire.write(decToBcd(h));
  Wire.write(decToBcd(dayOfTheWeek(y, m, d)));
  Wire.write(decToBcd(d));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(y));
  Wire.endTransmission();
}
void getDateDs1307(byte *s, byte *i, byte *h, byte *w, byte *d, byte *m, byte *y) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 7); //??????????? ?????? 7 ????
  *s = bcdToDec(Wire.read() & 0x7f); //????? ??? CH ?? ?????
  *i = bcdToDec(Wire.read());
  *h = bcdToDec(Wire.read() & 0x3f); //
  *w = bcdToDec(Wire.read());
  *d = bcdToDec(Wire.read());
  *m = bcdToDec(Wire.read());
  *y = bcdToDec(Wire.read());
  requestRtc = false;
}
//процедура прерывания таймера
void TimingISR() {
  s ++;
  if (s == 60) {
    i ++;
    if (i == 60) {
      h ++;
      if (h == 24)h = 0;
      //requestRtc проверить время по модулю
      i = 0;
    }
    s = 0;
  }
  flagUpdateDisplay = true;
}//TimingISR

void setup() {
  Serial.begin(9600);
  Serial.println(F("start of the clock"));
  Wire.begin();
  //  writeByteDS1307(0,0); // ????????? ???? (??????? ??? ??? CH?)
  //  writeByteDS1307(7,bcdToDec(0x010)); // ???????? ????? ????????? ????????? DS1307  ? ???????? 1Hz.
  //setDateDs1307(minute, hour, dayOfMonth, month, year);
  lcd.begin(16, 2); // сообщаем разрешение экрана
  getDateDs1307(&s, &i, &h, &w, &d, &m, &y);
  Timer1.initialize(1000000);        // таймер на 500мс
  Timer1.attachInterrupt(TimingISR);// привяжем процедуру к прерыванию таймера
}//end setup
void updateDisplay() {
  lcd.clear();//очищаем экран
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  Serial.println(line1);
  Serial.println(daysOfTheWeek[w]);
  Serial.println(line2);
  /*uint8_t len = strlen_P(daysOfTheWeek[dayOfTheWeek()]);
    char myChar ;
    for (uint8_t k = 0; k < len; k++){
    myChar = pgm_read_byte_near(daysOfTheWeek[dayOfTheWeek()] + k);
    Serial.print(myChar);
    }
  */
  Serial.println();
  flagUpdateDisplay = false;
}//updateDisplay
/*функция опроса кнопок
	//задаем действия при нажатии кнопок
  void buttonPressUp(){
  if (menu == 0){ //надписи меню 0
    line1 = "";
  line2 = "";
	}else if (menu == 1){ //надписи меню 1
    sprintf(line1, "Time: %2d:%02d:%02d", h, i, s);
    sprintf(line2, "Date: %02d/%02d/20%02d", d, m, y);
  }else if (menu == 2){ //надписи меню 2
    sprintf(line1, "Time: %2d:%02d:%02d", h, i, s);
    sprintf(line2, "Date: %02d/%02d/20%02d", d, m, y);
  }else if (menu == 3){ //надписи меню 3
    sprintf(line1, "Time: %2d:%02d:%02d", h, i, s);
    sprintf(line2, "Date: %02d/%02d/20%02d", d, m, y);
  }else if (menu == 4){ //надписи меню 4
    sprintf(line1, "Time: %2d:%02d:%02d", h, i, s);
    sprintf(line2, "Date: %02d/%02d/20%02d", d, m, y);
  }else if (menu == 5){//надписи меню 5
    sprintf(line1, "Time: %2d:%02d:%02d", h, i, s);
    sprintf(line2, "Date: %02d/%02d/20%02d", d, m, y);
  }
  flagUpdateDisplay = 1;
  }//buttonPressUp
  void buttonPressDown(){

  }//buttonPressDown
  void pressLeft(){

  }//buttonPressLeft
  void buttonPressRight(){

  }//buttonPressRight
  void buttonPressSelect(){
  if(menu == 0){
    line1 = "  SET DATETIME  ";
    sprintf(line2, "%02d:%02d %02d/%02d/20%02d", h, m, d, m, y);
  menu = 1;
    }else{
    line1 = "  SET DATETIME  ";
    line2 = "UPDATE SUCCESS!";
    menu = 0;
	}
  flagUpdateDisplay = 1;
  }//buttonPressSelect
  void loop(){
  //опрос кнопок
    uint16_t val = analogRead(0);//ожидаем нажатие кнопок
  if (val < 50) buttonPressRight();          //право   0
  else if (val < 200 && val > 50) buttonPressUp();  //верх  145
  else if (val < 400 && val > 200) buttonPressDown();  //низ   330
  else if (val < 600 && val > 400) buttonPressLeft();  //лево  505
  else if (val < 800 && val > 600) buttonPressSelect();  //выбор 742
  else menu = 0; //ближе к 1023, ничего
  // если что то поменялось обновим запись времени и выведем на дисплей
  if(requestRtc) getDateDs1307(&s, &i, &h, &w, &d, &m, &y);
  if (flagUpdateDisplay) updateDisplay();
  }//end loop
