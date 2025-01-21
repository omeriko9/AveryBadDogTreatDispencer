#include <Arduino.h>
#include <U8g2lib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <time.h>


#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#define SDA_PIN 5
#define SCL_PIN 6

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);   // EastRising 0.42" OLED

WiFiManager wifiManager;
bool portalUsed = false; // Flag to track if the portal was used


// Servo setup
#define SERVO_PIN 2
Servo myServo;

int servo_angle = 0;
bool increasing = true;


WebServer server(8080);

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600*2;   // Adjust according to your timezone (e.g., 3600 for GMT+1)
const int daylightOffset_sec = 3600; // Adjust for daylight saving time

const int speakerPinPositive = 3; // Positive terminal of the piezo speaker
const int speakerPinNegative = 4; // Simulated ground

// Define the notes and durations for the tune
int melody[] = {262, 294, 330, 349, 392, 440, 494, 523}; // Notes (C4 to C5 in Hz)
int noteDurations[] = {100, 100, 100, 100, 100, 100, 100, 100}; // Duration of each note in ms


void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.printf("Current Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_8x13B_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

// Function to list files in SPIFFS
String listFiles() {
    String json = "[";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        if (json != "[") json += ",";
        json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
        file = root.openNextFile();
    }
    json += "]";
    return json;
}



// Handle file listing
void handleList() {
    String json = listFiles();
    server.send(200, "application/json", json);
}

// Handle file upload
void handleUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = "/" + upload.filename;
        File file = SPIFFS.open(filename, FILE_WRITE);
        if (!file) {
            server.send(500, "text/plain", "Failed to open file for writing");
            return;
        }
        file.close();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        File file = SPIFFS.open("/" + upload.filename, FILE_APPEND);
        if (file) {
            file.write(upload.buf, upload.currentSize);
            file.close();
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        server.send(200, "text/plain", "File successfully uploaded");
    }
}

// Handle file download
void handleDownload() {
    if (server.hasArg("file")) {
        String fileName = "/" + server.arg("file");
        if (SPIFFS.exists(fileName)) {
            File file = SPIFFS.open(fileName, FILE_READ);
            server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("file"));
            server.streamFile(file, "application/octet-stream");
            file.close();
        } else {
            server.send(404, "text/plain", "File not found");
        }
    } else {
        server.send(400, "text/plain", "File name not provided");
    }
}


// Handle file deletion
void handleDelete() {
    if (server.hasArg("file")) {
        String fileName = "/" + server.arg("file");
        if (SPIFFS.exists(fileName)) {
            SPIFFS.remove(fileName);
            server.send(200, "text/plain", "File deleted");
        } else {
            server.send(404, "text/plain", "File not found");
        }
    } else {
        server.send(400, "text/plain", "File name not provided");
    }
}

// Handle root request to serve index.html from SPIFFS
void handleRoot() {
    if (SPIFFS.exists("/index.html")) {
        File file = SPIFFS.open("/index.html", FILE_READ);
        server.sendHeader("ngrok-skip-browser-warning", "true");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        server.send(404, "text/plain", "index.html not found");
    }
}

void handleAdmin() {
    if (SPIFFS.exists("/admin.html")) {
        File file = SPIFFS.open("/admin.html", FILE_READ);
        server.sendHeader("ngrok-skip-browser-warning", "true");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        server.send(404, "text/plain", "admin.html not found");
    }
}

void playT()
{
   for (int i = 0; i < 8; i++) {
    playTone(melody[i], noteDurations[i]);
    delay(50); // Short pause between notes
  }

  int BUZZER_PIN = 1;

  digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
  delay(1000);                    // Wait 1 second
  digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
  delay(1000);                    // Wait 1 second
}

void playTone(int frequency, int duration) {
  int period = 1000000 / frequency; // Period of the waveform in microseconds
  int halfPeriod = period / 2; // Half of the period
  long cycles = (long)frequency * duration / 1000; // Total cycles for the duration

  for (long i = 0; i < cycles; i++) {
    digitalWrite(speakerPinPositive, HIGH);
    digitalWrite(speakerPinNegative, LOW);
    delayMicroseconds(halfPeriod);
    digitalWrite(speakerPinPositive, LOW);
    digitalWrite(speakerPinNegative, HIGH);
    delayMicroseconds(halfPeriod);
  }

  // Ensure the speaker is silent after playing the tone
  digitalWrite(speakerPinPositive, LOW);
  digitalWrite(speakerPinNegative, LOW);
}

void writeString(String str)
{
  u8g2.clearBuffer();
  u8g2_prepare();
  u8g2.drawStr( 0, 0, str.c_str());
  u8g2.sendBuffer();
}

void Log(String str)
{
  File testFile = SPIFFS.open("/log.txt", FILE_WRITE);
  if (testFile) {
      
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        testFile.printf("[Current Time: ]%02d:%02d:%02d %02d/%02d/%04d]: ",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      }
      
      testFile.println(str);
      testFile.close();
      
  } 
}

void handleTreat() {

    playT();
    Log("Treat given");
    Serial.println("Releasing Treat...");
    server.send(200, "text/plain", "Treat received!");
    int new_angle = 30;
    myServo.write(50);
    delay(250);
    myServo.write(150);

}


void testSPIFFS()
{
  File testFile = SPIFFS.open("/test.txt", FILE_WRITE);
  if (testFile) {
      testFile.println("Hello, SPIFFS!");
      testFile.close();
      Serial.println("File written");
  } else {
      Serial.println("Failed to write file");
  }

  File readFile = SPIFFS.open("/test.txt", FILE_READ);
  if (readFile) {
      while (readFile.available()) {
          Serial.print((char)readFile.read());
      }
      readFile.close();
  } else {
      Serial.println("Failed to read file");
  }
}

void handleFirmwareUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update Start: %s\n", upload.filename.c_str());
        // Start the update process
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  // Start with unknown size
            Serial.println("Update.begin() failed:");
            Update.printError(Serial);
        } else {
            Serial.println("Update.begin() succeeded.");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write firmware chunks to flash
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.println("Update.write() failed:");
            Update.printError(Serial);
        } else {
            //Serial.printf("Wrote %d bytes\n", upload.currentSize);
            Serial.printf(".");
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        // Finalize the update
        if (Update.end(true)) {  // True to set this as valid OTA
            Serial.printf("Update Success: %u bytes\n", upload.totalSize);
            Serial.println("Rebooting...");
            ESP.restart();
        } else {
            Serial.println("Update.end() failed:");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("Update Aborted");
        Update.abort();
    }
}



void setup(void) {

  // WiFi.disconnect(true);
  // wifiManager.resetSettings();
  // return;

  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("Initializing...");
  
  // Speaker
  pinMode(speakerPinPositive, OUTPUT);
  pinMode(speakerPinNegative, OUTPUT);
  digitalWrite(speakerPinNegative, LOW);

  // Initalize Screen
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  // WiFi Portal

  // Override startConfigPortal to set the portalUsed flag
  wifiManager.setAPCallback([](WiFiManager* wm) {
      portalUsed = true; // Set the flag when the portal starts
      Serial.println("Captive portal started.");
  });

  wifiManager.autoConnect("Dog_Treat_For_Tzlil"); // Start the portal
  
  if (portalUsed) {
      Serial.println("Captive portal was used. Restarting ESP...");
      delay(1000);
      ESP.restart(); // Restart the ESP
  } 
  
  Serial.print("Connected to WiFi, IP: ");
  Serial.println(WiFi.localIP());

  // Init SPIFFS
  if (!SPIFFS.begin(true)) { // (true)
    Serial.println("Failed to mount SPIFFS");
  } else {
      Serial.println("SPIFFS mounted successfully");
  }

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.println(file.name());
    file = root.openNextFile();
  } 

  // Initialize and set time from NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronized with NTP server.");


  // Init server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/admin.html", HTTP_GET, handleAdmin);
  server.on("/list", HTTP_GET, handleList);
  server.on("/upload", HTTP_POST, []() {}, handleUpload);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/treat", HTTP_POST, handleTreat);
  server.on("/update", HTTP_POST, 
    // Final response after upload
      []() {
          if (Update.hasError()) {
              server.send(500, "text/plain", "Update Failed!");
          } else {
              server.send(200, "text/plain", "Update Success! Rebooting...");
          }
          delay(100);  // Give time for the response to be sent
          ESP.restart();  // Restart after responding
      },
      // Firmware upload handler
      handleFirmwareUpload
  );


  server.begin();

  // Initialize servo
  myServo.attach(SERVO_PIN);
  myServo.write(servo_angle);
  Serial.println("Initialization complete.");

  myServo.write(150);

  String ipstr = WiFi.localIP().toString();
  int lastDot = ipstr.lastIndexOf('.');
  String lastPart = ipstr.substring(lastDot + 1);
  Serial.print("Last Part IP: ");
  Serial.println(lastPart);

  // Screen init
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  
  writeString(lastPart.c_str());

  // Display the current time
  Serial.println("Current time:");
  printLocalTime();

}

void loop(void) {
  // Check for user input on Serial
  // if (Serial.available() > 0) {
  //   String input = Serial.readStringUntil('\n');
  //   int new_angle = 30; //input.toInt();
  //   myServo.write(50);
  //   delay(500);
  //   myServo.write(150);
    
  // }

   
  

  server.handleClient();
}
