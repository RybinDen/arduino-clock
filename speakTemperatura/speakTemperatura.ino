/*
* говорящий термометр
* автор Денис Рыбин rybinden.ru
скопировать папку 01 в корень флешки
звуковые файлы созданы с помощью сервиса https://cloud.yandex.ru/services/speechkit
Поддержать денежкой этот проект https://yoomoney.ru/to/41001667410836
*/

// подключаем необходимые библиотеки
#include <OneWire.h> // http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <SoftwareSerial.h> // стандартная
#include <DFMiniMp3.h> // https://github.com/Makuna/DFMiniMp3/wiki

// пины ардуины, к которым подключены плеер и датчик
#define ONE_WIRE_BUS 12 // пин ардуины, к которому подключен вывод data датчика температуры
const uint8_t MP3_RX = 11; // пин ардуины, к которому подключен RX плеера
const uint8_t MP3_TX = 10; // пин ардуины, к которому подключен TX плеера

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// этот класс скопировал из примера библиотеки плеера
class Mp3Notify{
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action){
    if (source & DfMp3_PlaySources_Sd){
        Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb){
        Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash){
        Serial.print("Flash, ");
    }
    Serial.println(action);
  }
  static void OnError(uint16_t errorCode){
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track){
    Serial.print("Play finished for #");
    Serial.println(track);  
  }
  static void OnPlaySourceOnline(DfMp3_PlaySources source){
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted(DfMp3_PlaySources source){
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved(DfMp3_PlaySources source){
    PrintlnSourceAction(source, "removed");
  }
};

SoftwareSerial secondarySerial(MP3_RX, MP3_TX); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(secondarySerial);

void setup(){
    Serial.begin(9600);
  Serial.println("initializing...");
  sensors.begin();
  mp3.begin();
  mp3.reset(); 
  uint16_t volume = mp3.getVolume();
  Serial.print("volume ");
  Serial.println(volume);
  mp3.setVolume(10);
  uint16_t count = mp3.getTotalTrackCount(DfMp3_PlaySource_Sd);
  Serial.print("files ");
  Serial.println(count);
  uint16_t mode = mp3.getPlaybackMode();
  Serial.print("playback mode ");
  Serial.println(mode);
  Serial.println("starting...");
}

void loop(){
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  int8_t tempC = sensors.getTempCByIndex(0);
  if(tempC != DEVICE_DISCONNECTED_C){
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(tempC);
  uint8_t file =   126; // номер файла для 0 градусов , по умолчанию
if(tempC != 0){ // если температура не равна 0 градусов
if(tempC < 0){ // температура отрицательная, вычисляем номер звук.файла
  file = 126 - tempC;
}else{ // температура положительная
  file = tempC;
  }
}
mp3.playFolderTrack(1, file);
  }else{
    Serial.println("Error: Could not read temperature data");
  }
  delay(5000);
}
