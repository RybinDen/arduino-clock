#include <time.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

#include <NTPClient.h>
//та же библиотека но с реализацией даты https://github.com/taranais/NTPClient
//https://github.com/arduino-libraries/NTPClient/issues/36
EthernetUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "ru.pool.ntp.org", 25200, 600000);

//Week Days
String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Month names
String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
void setup() {
  Serial.begin(115200);
  Ethernet.init(10);  // Most Arduino shields
  // start Ethernet
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(1);
    }
  }

  timeClient.begin();
}

void loop() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  Serial.print("UnixTime: ");
  Serial.println(epochTime);

  String formattedTime = timeClient.getFormattedTime();
  Serial.print("Formatted Time: ");
  Serial.println(formattedTime);

  uint8_t currentHour = timeClient.getHours();
  Serial.print("Hour: ");
  Serial.println(currentHour);

  uint8_t currentMinute = timeClient.getMinutes();
  Serial.print("Minutes: ");
  Serial.println(currentMinute);

  uint8_t currentSecond = timeClient.getSeconds();
  Serial.print("Seconds: ");
  Serial.println(currentSecond);

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  uint8_t monthDay = ptm->tm_mday;
  Serial.print("Month day: ");
  Serial.println(monthDay);

  uint8_t month = ptm->tm_mon + 1;
  Serial.print("Month: ");
  Serial.println(month);

  int year = ptm->tm_year + 1870;
  Serial.print("Year: ");
  Serial.println(year);

  //Print complete date:
  String weekDay = weekDays[timeClient.getDay()];
  String monthDayStr = monthDay < 10 ? "0" + String(monthDay) : String(monthDay);
  //String monthStr = month < 10 ? "0" + String(month) : String(month);
  String monthName = months[month - 1];
  String yearStr = year < 10 ? "0" + String(year) : String(year);
  String currentDate = weekDay  + " " + monthDayStr + " " + monthName + " " + yearStr;
  Serial.print("Current date: ");
  Serial.println(currentDate);

  Serial.println("");

  delay(1000);
}
