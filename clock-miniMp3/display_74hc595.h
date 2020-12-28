const byte analogInPin = A3;  // фоторезистор к A3 (второй вывод к минусу) и 10к резистор к плюсу
int sensorValue = 0;        // показания фоторезистора
uint8_t outputValue = 0;        // шим сигнал для подсветки дисплея PWM (analog out)
const byte analogOutPin = 9; // pwm шим сигнал на вывод 13 74hc595
const byte dataPin = 10; // 3 Пин подключен к DS (14) входу 74HC595
const byte latchPin = 11; // 2 Пин подключен к ST_CP (12) входу 74HC595
const byte clockPin = 12; // 1 Пин подключен к SH_CP (11) входу 74HC595

//массив чисел для отображения
const byte num[] = {
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
  B00000001, // точка
  //B00000000, // выключить
};
void displayBegin(){
      pinMode(latchPin, OUTPUT); 
  pinMode(clockPin, OUTPUT); 
  pinMode(dataPin, OUTPUT); 
}

void updateDisplay(){
  //шим сигнал для регулировки яркости от фоторезистора
  sensorValue = analogRead(analogInPin);
  outputValue = map(sensorValue, 0, 1023, 0, 255);
  analogWrite(analogOutPin, outputValue);
  //Serial.print(sensorValue);
  //Serial.print("output = ");
  //Serial.println(outputValue);

  //отображение данных на дисплее
  digitalWrite(latchPin, LOW); // устанавливаем синхронизацию "защелки" на LOW
  switch(state){
    case showTime:
      shiftOut(dataPin, clockPin, LSBFIRST, num[minutValue%10]); // единицы минут
      shiftOut(dataPin, clockPin, LSBFIRST, num[minutValue/10]+stateDot); // десятки минут 
      shiftOut(dataPin, clockPin, LSBFIRST, num[hourValue%10]+stateDot); // единицы часов
      shiftOut(dataPin, clockPin, LSBFIRST, num[hourValue/10]); // десятки часов
    break;
    case showDate:
      shiftOut(dataPin, clockPin, LSBFIRST, num[monthValue%10]); //единицы месяца
      shiftOut(dataPin, clockPin, LSBFIRST, num[monthValue/10]); // десятки месяца
      shiftOut(dataPin, clockPin, LSBFIRST, num[dayValue%10]+1); // единицы дня
      shiftOut(dataPin, clockPin, LSBFIRST, num[dayValue/10]); // десятки дня
    break;
    case showAlarm:
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmMinut%10]); // единицы минут
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmMinut/10]+1); // десятки минут
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmHour%10]+1); // единицы часов
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmHour/10]); // десятки часов
    break;
    case showTemperatura:
      shiftOut(dataPin, clockPin, LSBFIRST, num[temperatura%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[temperatura/10]); //
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
    break;
    case setMinute:
      if(stateDot){ //мигаем минутами
      shiftOut(dataPin, clockPin, LSBFIRST, num[minutValue%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[minutValue/10]+1); // 
    }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
      shiftOut(dataPin, clockPin, LSBFIRST, 1); // 
    }
      shiftOut(dataPin, clockPin, LSBFIRST, num[hourValue%10]+1); // 
      shiftOut(dataPin, clockPin, LSBFIRST, num[hourValue/10]); // 
  break;
    case setHour:  //мигаем часами
      shiftOut(dataPin, clockPin, LSBFIRST, num[minutValue%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[minutValue/10]+1); // 
      if(stateDot){//мигаем часами
      shiftOut(dataPin, clockPin, LSBFIRST, num[hourValue%10]+1); // 
      shiftOut(dataPin, clockPin, LSBFIRST, num[hourValue/10]); // 
    }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 1); // 
      shiftOut(dataPin, clockPin, LSBFIRST, 0); // 
    }
    break;
    case setDay:
      shiftOut(dataPin, clockPin, LSBFIRST, num[monthValue%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[monthValue/10]); //
      if(stateDot){ //мигаем днем
      shiftOut(dataPin, clockPin, LSBFIRST, num[dayValue%10]+1); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[dayValue/10]); //
    }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 1); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
    }
    break;
    case setMonth:
      if(stateDot){ //мигаем месяцем
      shiftOut(dataPin, clockPin, LSBFIRST, num[monthValue%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[monthValue/10]); //
    }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
    }
      shiftOut(dataPin, clockPin, LSBFIRST, num[dayValue%10]+1); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[dayValue/10]); //
    break;
    case setYear:
    if(stateDot){
      shiftOut(dataPin, clockPin, LSBFIRST, num[yearValue%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[yearValue/10]); //
      shiftOut(dataPin, clockPin, LSBFIRST, num[0]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[2]); //
          }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
          }
    break;
    case setAlarmHour:
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmMinut%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmMinut/10]+1); //
      if(stateDot){ //мигаем часами будильника
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmHour%10]+1); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmHour/10]); //
    }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 1); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //
    }
    break;
    case setAlarmMinute:
      if(stateDot){ //мигаем минутами будильника
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmMinut%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmMinut/10]+1); //
    }else{
      shiftOut(dataPin, clockPin, LSBFIRST, 0); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, 1); //
    }
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmHour%10]+1); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[alarmHour/10]); //
    break;
    case setVolume:
      shiftOut(dataPin, clockPin, LSBFIRST, num[volume%10]); //показываем цифру
      shiftOut(dataPin, clockPin, LSBFIRST, num[volume/10]); // 
      shiftOut(dataPin, clockPin, LSBFIRST, 0); // 
      shiftOut(dataPin, clockPin, LSBFIRST, 0); // 
    break;
  }
  digitalWrite(latchPin, HIGH); //"защелкиваем" регистр, тем самым устанавливая значения на выходах 
  flagUpdateDisplay = false;
} //updateDisplay
