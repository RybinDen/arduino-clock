#include <dht11.h> // https://github.com/adidax/dht11
dht11 DHT11;
#define DHT11PIN 4

//player
//папка 7: температура файл 18, влажность файл 41
// папка 14. x градусов f126 - 0 градусов, 127 - -1 градус и далее
// числа с процентами для влажности папка 15. 0% - 101.mp3
#include <MD_YX5300.h> // https://github.com/MajicDesigns/MD_YX5300
#include <SoftwareSerial.h>
const uint8_t ARDUINO_RX = 2;    // connect to TX of MP3 Player module
const uint8_t ARDUINO_TX = 3;    // connect to RX of MP3 Player module
SoftwareSerial  MP3Stream(ARDUINO_RX, ARDUINO_TX);  // MP3 player serial stream for comms
long previousMillis = 0;        // храним время последнего запроса
long interval = 10000;           // интервал между запросами
bool speak = false;
bool isPlaying = false;
uint8_t phrase = 1;
int8_t temp;
int8_t humidity;

MD_YX5300 mp3(MP3Stream);
// функция обратного вызова, используемая для обработки незапрошенных  сообщений плеера или ответы на запросы данных
void cbResponse(const MD_YX5300::cbData *status) {
  if(status->code == MD_YX5300::STS_FILE_END){ // воспроизведение файла закончилось
      isPlaying = false;
  }
} //cbResponse

void setup(){
  Serial.begin(9600);
  MP3Stream.begin(MD_YX5300::SERIAL_BPS);
  mp3.begin();
  mp3.setCallback(cbResponse);
  mp3.volume(12);
}//setup()

void getData(){
  int chk = DHT11.read(DHT11PIN);
  if(chk == 0){ //0 данные получены, ошибок нет
    temp = DHT11.temperature;
    humidity = DHT11.humidity;
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.println(" C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    speak = true;
  }
}//getData()

void speaking(){
  switch(phrase){
    case 1:
      mp3.playSpecific(7, 18); // температура
      phrase = 2;
      break;
    case 2:
      mp3.playSpecific(14, temp);
      phrase = 3;
      break;
    case 3:
      mp3.playSpecific(7, 41); // влажность
      phrase = 4;
      break;
    case 4:
      mp3.playSpecific(15, humidity);
      phrase = 1;
      speak = false;
    break;
  }
  isPlaying = true;
}//speaking()

void loop(){
  mp3.check();        // run the mp3 receiver
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval){ //проверяем не прошел ли нужный интервал, если прошел то
    previousMillis = currentMillis; // сохраняем время последнего переключения
    getData();
  }
  if (speak && isPlaying==false){
    speaking();
  }
}//end loop
