#include <BH1750.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <TinyGPS++.h>
#include <SdFat.h>

#define LONG_PRESS_TIME 3000
#define SENSOR_INTERVAL 2000
#define WRITE_INTERVAL 5000
#define BUAD_RATE 9600

/* Pin Declaration */
#define SD_PIN 0
#define TOGGLE_PIN 1

/* Variables */
String timeStamp, latitude, longitude, currentFile, temp, humid, lux;
long lastTime = millis();
bool sdState, ahtState, lightState;
bool fileState = false;

/* Toggle Button Variable */
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
bool isPressing, isLongDetected, toggleStatus = false;

/* AHT10 Module Init */
Adafruit_AHTX0 aht;

/* Neo-8M GPS Module Init */
TinyGPSPlus gps;

/* Radiance Sensor */
BH1750 lightMeter;

/* SD Card */
SdFat sd;
SdFile myFile;

void setup() {
  /* Input */
  pinMode(TOGGLE_PIN, INPUT_PULLUP);
  /* Begin Processes */
  Serial1.begin(BUAD_RATE);
  Wire.begin();

  if (aht.begin()) ahtState = true;
  else ahtState = false;

  if (lightMeter.begin()) lightState = true;
  else lightState = false;

  if (sd.begin(SD_PIN, SPI_HALF_SPEED)) sdState = true;
  else sdState = false;
}

void loop() {
  /* Get Time Elapsed */
  long elapsedTime = millis() - lastTime;
  lastTime = lastTime + elapsedTime;

  /* Run */
  readGPS();

  if (ahtState) sendAHT10(elapsedTime);
  else {
    Serial1.print(F("hLabel.txt=\"ERROR\""));
    endNextionCommand();
    Serial1.print(F("tLabel.txt=\"ERROR\""));
    endNextionCommand();
  }

  if (lightState) sendRadiance(elapsedTime);
  else {
    Serial1.print(F("lxLabel.txt=\"ERROR\""));
    endNextionCommand();
  }

  if (sdState) {
    readToggleButton();
    writeSDCard(elapsedTime);
  } else {
    Serial1.print(F("recordLabel.txt=\"SD Card Missing\""));
    endNextionCommand();
  }
}

void writeSDCard(long elapsedTime) {
  static long moduleTime = 0;
  moduleTime = moduleTime + elapsedTime;
  if (moduleTime >= WRITE_INTERVAL) {
    if (toggleStatus) {
      if (fileState) {
        char filename[currentFile.length() + 1];
        currentFile.toCharArray(filename, sizeof(filename));
        myFile.open(filename, O_RDWR | O_CREAT | O_AT_END);
        /* Write Content */
        myFile.println(timeStamp + F(", ") + latitude + F(", ") + longitude + F(", ") + temp + F(", ") + humid + F(", ") + lux);
        myFile.close();
      } else {
        /* Set New File Name */
        currentFile = timeStamp + ".txt";

        /* Open New File & Write Header */
        char filename[currentFile.length() + 1];
        currentFile.toCharArray(filename, sizeof(filename));
        myFile.open(filename, O_RDWR | O_CREAT | O_AT_END);
        myFile.println(F("Timestamp  Latitude  Longitude  Temperature  Humidity  Radiance"));
        myFile.close();

        fileState = true;
      }
    } else {
      myFile.close();
      fileState = false;
      currentFile = "";
    }
    moduleTime = moduleTime - WRITE_INTERVAL;
  }
}

void readToggleButton() {
  currentState = digitalRead(TOGGLE_PIN);
  if (lastState == HIGH && currentState == LOW) {       // button is pressed
    pressedTime = millis();
    isPressing = true;
    isLongDetected = false;
  } else if (lastState == LOW && currentState == HIGH) { // button is released
    isPressing = false;
  }
  if (isPressing == true && isLongDetected == false) {
    long pressDuration = millis() - pressedTime;
    if ( pressDuration > LONG_PRESS_TIME ) {
      toggleStatus = !toggleStatus;
      isLongDetected = true;
    }
  }
  lastState = currentState; // save the the last state

  if (toggleStatus == true) {
    Serial1.print(F("recordLabel.txt=\"Recording...\""));
    endNextionCommand();
  } else {
    Serial1.print(F("recordLabel.txt=\"Not Recording\""));
    endNextionCommand();
  }
}

void readGPS() {
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      timeStamp, latitude, longitude = "";
      sendGPS();
    }
  }
}

void sendGPS() {
  String day, month, year, hour, minute, second, dateString, timeString;
  int hr;
  /* Location */
  if (gps.location.isValid()) {
    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    Serial1.print(F("GPSLabel.txt=\"Tracking...\""));
    endNextionCommand();
  } else {
    latitude = F("Error");
    longitude = F("Error");
    Serial1.print(F("GPSLabel.txt=\"No Signal\""));
    endNextionCommand();
    Serial1.print(F("dateLabel.txt=\"""""\""));
    endNextionCommand();
    Serial1.print(F("timeLabel.txt=\"""""\""));
    endNextionCommand();
  }

  /* Date & Time */
  if (gps.date.isValid() && gps.time.isValid()) {
    hr = gps.time.hour() + 8;

    if (hr > 23) {
      hr = hr - 24;
    } else {
      if (hr < 0) {
        hr = hr + 24;
      }
    }

    if (hr < 10) {
      hour = "0" + String(hr);
    } else {
      hour = hr;
    }

    /* Time Format */
    if (gps.time.minute() < 10) {
      minute = "0" + String(gps.time.minute());
    } else {
      minute = gps.time.minute();
    }

    if (gps.time.second() < 10) {
      second = "0" + String(gps.time.second());
    } else {
      second = gps.time.second();
    }

    /* Date Format */
    year = gps.date.year();

    if (gps.date.month() < 10) {
      month = "0" + String(gps.date.month());
    } else {
      month = gps.date.month();
    }

    if (gps.date.day() < 10) {
      day = "0" + String(gps.date.day());
    } else {
      day = gps.date.day();
    }

    timeString = hour + F(".") + minute + F(".") + second;
    dateString = year + F(".") + month + F(".") + day;
    timeStamp = dateString + F("-") + timeString;

    Serial1.print(F("dateLabel.txt=\""));
    Serial1.print(dateString);
    Serial1.print(F("\""));
    endNextionCommand();
    Serial1.print(F("timeLabel.txt=\""));
    Serial1.print(timeString);
    Serial1.print(F("\""));
    endNextionCommand();
  } else {
    timeStamp = F("Error");
    Serial1.print(F("dateLabel.txt=\"""""\""));
    endNextionCommand();
    Serial1.print(F("timeLabel.txt=\"""""\""));
    endNextionCommand();
  }
}

void sendRadiance(long elapsedTime) {
  static long sensorTime = 0;
  sensorTime = sensorTime + elapsedTime;

  if (sensorTime >= SENSOR_INTERVAL) {
    if (lightMeter.measurementReady()) {
      int readlLux = lightMeter.readLightLevel();
      if (isnan(readlLux) || readlLux < 0) {
        lux = F("Error");
        Serial1.print(F("lxLabel.txt=\"ERROR\""));
        endNextionCommand();
      } else {
        lux = String(readlLux);
        Serial1.print(F("lxLabel.txt=\""));
        Serial1.print(String(readlLux));
        Serial1.print(F("\""));
        endNextionCommand();
      }
    }
    sensorTime = sensorTime - SENSOR_INTERVAL;
  }
}

void sendAHT10(long elapsedTime) {
  static long sensorTime = 0;
  sensorTime = sensorTime + elapsedTime;

  if (sensorTime >= SENSOR_INTERVAL) {
    sensors_event_t sensor_humid, sensor_temp;
    aht.getEvent(&sensor_humid, &sensor_temp);// populate temp and humidity objects with fresh data
    float readHumid = sensor_humid.relative_humidity;
    float readTemp = sensor_temp.temperature;

    /* Check Humidity Value */
    if (isnan(readHumid) || readHumid <= 0) {
      Serial1.print(F("hLabel.txt=\"ERROR\""));
      endNextionCommand();
      humid = F("Error");
    } else {
      humid = String(readHumid);
      Serial1.print(F("hLabel.txt=\""));
      Serial1.print(String(readHumid));
      Serial1.print(F("\""));
      endNextionCommand();
    }

    /* Check Temperature Value */
    if (isnan(readTemp) || readTemp <= 0) {
      Serial1.print(F("tLabel.txt=\"ERROR\""));
      endNextionCommand();
      temp = F("Error");
    } else {
      temp = String(readTemp);
      Serial1.print(F("tLabel.txt=\""));
      Serial1.print(String(readTemp));
      Serial1.print(F("\""));
      endNextionCommand();
    }
    sensorTime = sensorTime - SENSOR_INTERVAL;
  }
}

void endNextionCommand() {
  Serial1.write(0xff);
  Serial1.write(0xff);
  Serial1.write(0xff);
}
