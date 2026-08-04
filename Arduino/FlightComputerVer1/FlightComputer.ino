#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <SD.h>
#include <Adafruit_MPU6050.h>

Adafruit_BMP3XX bmp;
Adafruit_MPU6050 mpu;
File logFile;

enum FlightState{
  IDLE,
  ASCENT,
  DESCENT,
  LANDED
};

FlightState state = IDLE;

const float SEALEVELPRESSURE_HPA = 1013.5;
float baselineAltitude;

unsigned long launchTime;
int launchCounter = 0;

const float LAUNCH_THRESHOLD = 15.0;
const int LAUNCH_COUNT_REQUIRED = 3;

int descentCounter = 0;
float maxAltitude = 0.0;

const float DESCENT_THRESHOLD = 0.10;
const int DESCENT_COUNT_REQUIRED = 5;

float previousAltitude = 0.0;
int stableCounter = 0;

const float LANDING_CHANGE_THRESHOLD = 0.10;
const int LANDING_COUNT_REQUIRED = 50;

char fileName[32];
char summaryFileName[32];

float maxAcceleration = 0.0;

unsigned long apogeeTime = 0;
unsigned long landingTime = 0;

bool summaryPrinted = false;



void setup() {
  // put your setup code here, to run once:
    Serial.begin(9600);
    delay(2000);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.println("SD init...");
    delay(2000);
    if(!SD.begin(BUILTIN_SDCARD)){
      while(1){
        Serial.println("SD init failed");
        delay(1000);
      }
    }

    int flightNumber = 1;
    
    while(true){
      snprintf(fileName, sizeof(fileName), "flight%03d.csv", flightNumber);

      if(!SD.exists(fileName)){
        break;
      }

      flightNumber++;
    }

    snprintf(summaryFileName, sizeof(summaryFileName), "summary%03d.txt", flightNumber);

    Serial.println("File init...");
    delay(2000);

    logFile = SD.open(fileName, FILE_WRITE);

    if(!logFile){
      while(1){
        Serial.println("File failed to open");
        delay(1000);
      }
    }

    logFile.println("time_ms,temp_c,pressure_hpa,altitude_m,accel_mag,ax,ay,az,gx,gy,gz,flight_state");

    logFile.close();

    Serial.println("File write init done");
    Serial.print("Logging to: ");
    Serial.println(fileName);


    Serial.println("BMP init...");
    delay(2000);
    if(!bmp.begin_I2C()){
      Serial.println("BMP390 not found");
      while(1){
        Serial.println("BMP390 not found");
        delay(1000);
      }
    }
    Serial.println("BMP found");

    Serial.println("MPU init...");
    delay(2000);
    if(!mpu.begin()){
      Serial.println("MPU6050 not found");
      while(1){
        Serial.println("MPU6050 not found");
        delay(1000);
      }
    }
    Serial.println("MPU found");

    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);

    mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
    mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    for(int i = 0; i < 5; i++){
      bmp.performReading();
      delay(50);
    }

    int successfulReadings = 0;
    int attempts = 0;
    float sum = 0;

    while(successfulReadings < 20 && attempts < 50){
      attempts++;

      if(bmp.performReading()){
        sum += bmp.readAltitude(SEALEVELPRESSURE_HPA);
        successfulReadings++;
      }

      delay(50);
    }

    if(successfulReadings < 10){
      while(1){
        Serial.println("Failed to establish altitude baseline");
        delay(1000);
      }
    }

    baselineAltitude = sum / successfulReadings;
    Serial.printf("Baseline altitude: %.2f m\n", baselineAltitude);

    Serial.println("time_ms,temp_c,pressure_hpa,altitude_m,accel_mag,ax,ay,az,gx,gy,gz,flight_state");

    delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;

  mpu.getEvent(&accel, &gyro, &temp);

  if(!bmp.performReading()){
    Serial.println("Reading failed");
    return;
  }

  float altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
  float relativeAltitude = altitude - baselineAltitude;

  float accel_mag = sqrt(accel.acceleration.x * accel.acceleration.x + 
                        accel.acceleration.y * accel.acceleration.y + 
                        accel.acceleration.z * accel.acceleration.z);

  if(state == ASCENT || state == DESCENT){
    if(accel_mag > maxAcceleration){
      maxAcceleration = accel_mag;
    }
  }


switch(state){
  
  case IDLE:
      if(accel_mag > LAUNCH_THRESHOLD){
        launchCounter++;
      }else{
        launchCounter = 0;
      }

      if(launchCounter >= LAUNCH_COUNT_REQUIRED){
        launchTime = millis();
        apogeeTime = launchTime;
        maxAltitude = relativeAltitude;
        maxAcceleration = accel_mag;
        Serial.println("LAUNCH DETECTED");
        state = ASCENT;
      }

      break;

  case ASCENT:
    if(relativeAltitude > maxAltitude){
      maxAltitude = relativeAltitude;
      descentCounter = 0;
      apogeeTime = millis();

    }else if(relativeAltitude < maxAltitude - DESCENT_THRESHOLD){
      descentCounter++;

    }else{
      descentCounter = 0;
    }
    
    if(descentCounter >= DESCENT_COUNT_REQUIRED){
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("APOGEE REACHED");

      previousAltitude = relativeAltitude;
      stableCounter = 0;
      state = DESCENT;
    }
    
    break;

  case DESCENT:
    if(fabs(relativeAltitude - previousAltitude) < LANDING_CHANGE_THRESHOLD && fabs(relativeAltitude) < .30){
      stableCounter++;
    }else{
      stableCounter = 0;
    }

    previousAltitude = relativeAltitude;

    if(stableCounter >= LANDING_COUNT_REQUIRED){
      Serial.println("LANDED");
      landingTime = millis();
      state = LANDED;
    }

    break;

  case LANDED:{
    digitalWrite(LED_BUILTIN, LOW);

    float timeToApogee = (apogeeTime - launchTime)/1000.0;
    float flightDuration = (landingTime - launchTime)/1000.0;

    if(!summaryPrinted){

      Serial.println("Flight Summary:");
      Serial.printf("Max Altitude: %.2f m\n", maxAltitude);
      Serial.printf("Maximum Acceleration: %.2f m/s^2\n", maxAcceleration);
      Serial.printf("Time to Apogee: %.2f s\n", timeToApogee);
      Serial.printf("Flight Duration: %.2f s\n", flightDuration);

      File summaryFile = SD.open(summaryFileName, FILE_WRITE);
      if(summaryFile){
        summaryFile.println("Flight Summary");
        summaryFile.printf("Flight Log: %s\n", fileName);
        summaryFile.printf("Maximum Altitude: %.2f m\n", maxAltitude);
        summaryFile.printf("Maximum Acceleration: %.2f m/s^2\n", maxAcceleration);
        summaryFile.printf("Time to Apogee: %.2f s\n", timeToApogee);
        summaryFile.printf("Flight Duration: %.2f s\n", flightDuration);

        summaryFile.close();

        Serial.print("Summary saved to: ");
        Serial.println(summaryFileName);

      }else{
        Serial.print("Failed to create summary file: ");
        Serial.println(summaryFileName);
      }


      summaryPrinted = true;


    }

    return;
  }

}

  logFile = SD.open(fileName, FILE_WRITE);
  if(!logFile){
    Serial.print("Could not open: ");
    Serial.println(fileName);
    delay(1000);
    return;
  }

  logFile.printf(
    "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",

  millis(),
  bmp.temperature,
  bmp.pressure/100.0,
  relativeAltitude,
  accel_mag,
  accel.acceleration.x,
  accel.acceleration.y,
  accel.acceleration.z,
  gyro.gyro.x,
  gyro.gyro.y,
  gyro.gyro.z,
  (int)state);

  logFile.close();

  Serial.printf(
    "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", 

  millis(), 
  bmp.temperature, 
  bmp.pressure / 100.0, 
  relativeAltitude,
  accel_mag,
  accel.acceleration.x, 
  accel.acceleration.y, 
  accel.acceleration.z, 
  gyro.gyro.x, 
  gyro.gyro.y, 
  gyro.gyro.z,
  (int)state);

  delay(20);
}
