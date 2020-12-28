#include "TM1637.h" // подключаем библиотеку для дисплея tm1637
//подключить дисплей к свободным выводам
#define CLK 6 // TM1637 clk
#define DIO 7 // TM1637 dio
TM1637 display(CLK,DIO);
void displayBegin(){
  //эти функции должны быть в setup скетча
  display.set();
  display.clearDisplay();
  }
void updateDisplay(){
  switch(state){
    case showTime:
      display.point(stateDot);
      display.display(0, hourValue/10);
      display.display(1, hourValue%10);
      display.display(2, minutValue/10);
      display.display(3, minutValue%10);
    break;
    case showDate:
    display.point(1);
      display.display(0, dayValue/10);
      display.display(1, dayValue%10);
      display.display(2, monthValue/10);
      display.display(3, monthValue%10);
    break;
    case showAlarm:
    display.point(1);
      display.display(0, alarmHour/10);
      display.display(1, alarmHour%10);
      display.display(2, alarmMinut/10);
      display.display(3, alarmMinut%10);
    break;
    case showTemperatura:
    display.point(0);
    display.display(0, 0x7f); //ничего не показываем
    display.display(1, 0x7f); // ничего не показываем
    display.display(2, temperatura/10);
    display.display(3, temperatura%10);
    break;
    case setMinute:
      display.point(1); // очкой не мигаем
      display.display(0, hourValue/10);
      display.display(1, hourValue%10);
      if(stateDot){ //мигаем минутами
      display.display(2, minutValue/10);
      display.display(3, minutValue%10);
    }else{
   display.display(2, 0x7f); //ничего не показываем
    display.display(3, 0x7f); // ничего не показываем
    }
  break;
case setHour:
      display.point(1); // очкой не мигаем
      if(stateDot){//мигаем часами
        display.display(0, hourValue/10);
      display.display(1, hourValue%10);
    }else{
   display.display(0, 0x7f); //ничего не показываем
    display.display(1, 0x7f); // ничего не показываем
    }
      display.display(2, minutValue/10);
      display.display(3, minutValue%10);
    break;
    case setDay:
    display.point(1);
      if(stateDot){ //мигаем днем
      display.display(0, dayValue/10);
      display.display(1, dayValue%10);
    }else{
   display.display(0, 0x7f); //ничего не показываем
    display.display(1, 0x7f); // ничего не показываем
    }
      display.display(2, monthValue/10);
      display.display(3, monthValue%10);
    break;
    case setMonth:
    display.point(1);
            display.display(0, dayValue/10);
      display.display(1, dayValue%10);
      if(stateDot){ //мигаем месяцем
      display.display(2, monthValue/10);
      display.display(3, monthValue%10);
    }else{
   display.display(2, 0x7f); //ничего не показываем
    display.display(3, 0x7f); // ничего не показываем
    }
    break;

    case setYear:
    display.point(0);
      if(stateDot){ //мигаем годом
      display.display(0, 2);
      display.display(1, 0);
            display.display(2, yearValue/10);
      display.display(3, yearValue%10);
    }else{
   display.display(0, 0x7f); //ничего не показываем
    display.display(1, 0x7f); // ничего не показываем
            display.display(2, 0x7f);
      display.display(3, 0x7f);
    }
    break;
    case setAlarmHour:
    display.point(1);
      if(stateDot){ //мигаем часами будильника
      display.display(0, alarmHour/10);
      display.display(1, alarmHour%10);
    }else{
   display.display(0, 0x7f); //ничего не показываем
    display.display(1, 0x7f); // ничего не показываем
    }
      display.display(2, alarmMinut/10);
      display.display(3, alarmMinut%10);
    break;
    case setAlarmMinute:
    display.point(1);
      display.display(0, alarmHour/10);
      display.display(1, alarmHour%10);
      if(stateDot){ //мигаем минутами будильника
      display.display(2, alarmMinut/10);
      display.display(3, alarmMinut%10);
    }else{
   display.display(2, 0x7f); //ничего не показываем
    display.display(3, 0x7f); // ничего не показываем
    }
    break;
    case setVolume:
    display.point(0);
    display.display(0, 0x7f); //ничего не показываем
    display.display(1, 0x7f); // ничего не показываем
    display.display(2, volume/10);
    display.display(3, volume%10);
    break;
  }
  flagUpdateDisplay = false;
} //updateDisplay
  


