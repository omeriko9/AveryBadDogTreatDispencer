#include <Arduino.h>
#include <U8g2lib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <WiFiManager.h>


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


const int speakerPinPositive = 5; // Positive terminal of the piezo speaker
const int speakerPinNegative = 6; // Simulated ground

// Define the notes and durations for the tune
int melody[] = {262, 294, 330, 349, 392, 440, 494, 523}; // Notes (C4 to C5 in Hz)
int noteDurations[] = {100, 100, 100, 100, 100, 100, 100, 100}; // Duration of each note in ms


void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

void u8g2_box_frame(uint8_t a) {
  u8g2.drawStr( 0, 0, "drawBox");
  u8g2.drawBox(5,10,20,10);
  u8g2.drawBox(10+a,15,30,7);
  u8g2.drawStr( 0, 30, "drawFrame");
  u8g2.drawFrame(5,10+30,20,10);
  u8g2.drawFrame(10+a,15+30,30,7);
}

void u8g2_disc_circle(uint8_t a) {
  u8g2.drawStr( 0, 0, "drawDisc");
  u8g2.drawDisc(10,18,9);
  u8g2.drawDisc(24+a,16,7);
  u8g2.drawStr( 0, 30, "drawCircle");
  u8g2.drawCircle(10,18+30,9);
  u8g2.drawCircle(24+a,16+30,7);
}

void u8g2_r_frame(uint8_t a) {
  u8g2.drawStr( 0, 0, "drawRFrame/Box");
  u8g2.drawRFrame(5, 10,40,30, a+1);
  u8g2.drawRBox(50, 10,25,40, a+1);
}

void u8g2_string(uint8_t a) {
  u8g2.setFontDirection(0);
  u8g2.drawStr(30+a,31, " 0");
  u8g2.setFontDirection(1);
  u8g2.drawStr(30,31+a, " 90");
  u8g2.setFontDirection(2);
  u8g2.drawStr(30-a,31, " 180");
  u8g2.setFontDirection(3);
  u8g2.drawStr(30,31-a, " 270");
}

void u8g2_line(uint8_t a) {
  u8g2.drawStr( 0, 0, "drawLine");
  u8g2.drawLine(7+a, 10, 40, 55);
  u8g2.drawLine(7+a*2, 10, 60, 55);
  u8g2.drawLine(7+a*3, 10, 80, 55);
  u8g2.drawLine(7+a*4, 10, 100, 55);
}

void u8g2_triangle(uint8_t a) {
  uint16_t offset = a;
  u8g2.drawStr( 0, 0, "drawTriangle");
  u8g2.drawTriangle(14,7, 45,30, 10,40);
  u8g2.drawTriangle(14+offset,7-offset, 45+offset,30-offset, 57+offset,10-offset);
  u8g2.drawTriangle(57+offset*2,10, 45+offset*2,30, 86+offset*2,53);
  u8g2.drawTriangle(10+offset,40+offset, 45+offset,30+offset, 86+offset,53+offset);
}

void u8g2_ascii_1() {
  char s[2] = " ";
  uint8_t x, y;
  u8g2.drawStr( 0, 0, "ASCII page 1");
  for( y = 0; y < 6; y++ ) {
    for( x = 0; x < 16; x++ ) {
      s[0] = y*16 + x + 32;
      u8g2.drawStr(x*7, y*10+10, s);
    }
  }
}

void u8g2_ascii_2() {
  char s[2] = " ";
  uint8_t x, y;
  u8g2.drawStr( 0, 0, "ASCII page 2");
  for( y = 0; y < 6; y++ ) {
    for( x = 0; x < 16; x++ ) {
      s[0] = y*16 + x + 160;
      u8g2.drawStr(x*7, y*10+10, s);
    }
  }
}

void u8g2_extra_page(uint8_t a)
{
  u8g2.drawStr( 0, 0, "Unicode");
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.setFontPosTop();
  u8g2.drawUTF8(0, 24, "☀ ☁");
  switch(a) {
    case 0:
    case 1:
    case 2:
    case 3:
      u8g2.drawUTF8(a*3, 36, "☂");
      break;
    case 4:
    case 5:
    case 6:
    case 7:
      u8g2.drawUTF8(a*3, 36, "☔");
      break;
  }
}

#define cross_width 24
#define cross_height 24
static const unsigned char cross_bits[] U8X8_PROGMEM  = {
  0x00, 0x18, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x42, 0x00, 
  0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 
  0xC0, 0x00, 0x03, 0x38, 0x3C, 0x1C, 0x06, 0x42, 0x60, 0x01, 0x42, 0x80, 
  0x01, 0x42, 0x80, 0x06, 0x42, 0x60, 0x38, 0x3C, 0x1C, 0xC0, 0x00, 0x03, 
  0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 
  0x00, 0x42, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x18, 0x00, };

#define cross_fill_width 24
#define cross_fill_height 24
static const unsigned char cross_fill_bits[] U8X8_PROGMEM  = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x64, 0x00, 0x26, 
  0x84, 0x00, 0x21, 0x08, 0x81, 0x10, 0x08, 0x42, 0x10, 0x10, 0x3C, 0x08, 
  0x20, 0x00, 0x04, 0x40, 0x00, 0x02, 0x80, 0x00, 0x01, 0x80, 0x18, 0x01, 
  0x80, 0x18, 0x01, 0x80, 0x00, 0x01, 0x40, 0x00, 0x02, 0x20, 0x00, 0x04, 
  0x10, 0x3C, 0x08, 0x08, 0x42, 0x10, 0x08, 0x81, 0x10, 0x84, 0x00, 0x21, 
  0x64, 0x00, 0x26, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

#define cross_block_width 14
#define cross_block_height 14
static const unsigned char cross_block_bits[] U8X8_PROGMEM  = {
  0xFF, 0x3F, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 
  0xC1, 0x20, 0xC1, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 
  0x01, 0x20, 0xFF, 0x3F, };

void u8g2_bitmap_overlay(uint8_t a) {
  uint8_t frame_size = 28;

  u8g2.drawStr(0, 0, "Bitmap overlay");

  u8g2.drawStr(0, frame_size + 12, "Solid / transparent");
  u8g2.setBitmapMode(false /* solid */);
  u8g2.drawFrame(0, 10, frame_size, frame_size);
  u8g2.drawXBMP(2, 12, cross_width, cross_height, cross_bits);
  if(a & 4)
    u8g2.drawXBMP(7, 17, cross_block_width, cross_block_height, cross_block_bits);

  u8g2.setBitmapMode(true /* transparent*/);
  u8g2.drawFrame(frame_size + 5, 10, frame_size, frame_size);
  u8g2.drawXBMP(frame_size + 7, 12, cross_width, cross_height, cross_bits);
  if(a & 4)
    u8g2.drawXBMP(frame_size + 12, 17, cross_block_width, cross_block_height, cross_block_bits);
}

void u8g2_bitmap_modes(uint8_t transparent) {
  const uint8_t frame_size = 24;

  u8g2.drawBox(0, frame_size * 0.5, frame_size * 5, frame_size);
  u8g2.drawStr(frame_size * 0.5, 50, "Black");
  u8g2.drawStr(frame_size * 2, 50, "White");
  u8g2.drawStr(frame_size * 3.5, 50, "XOR");
  
  if(!transparent) {
    u8g2.setBitmapMode(false /* solid */);
    u8g2.drawStr(0, 0, "Solid bitmap");
  } else {
    u8g2.setBitmapMode(true /* transparent*/);
    u8g2.drawStr(0, 0, "Transparent bitmap");
  }
  u8g2.setDrawColor(0);// Black
  u8g2.drawXBMP(frame_size * 0.5, 24, cross_width, cross_height, cross_bits);
  u8g2.setDrawColor(1); // White
  u8g2.drawXBMP(frame_size * 2, 24, cross_width, cross_height, cross_bits);
  u8g2.setDrawColor(2); // XOR
  u8g2.drawXBMP(frame_size * 3.5, 24, cross_width, cross_height, cross_bits);
}

uint8_t draw_state = 0;

void drawOld(void) {
  u8g2_prepare();
  switch(draw_state >> 3) {
    case 0: u8g2_box_frame(draw_state&7); break;
    case 1: u8g2_disc_circle(draw_state&7); break;
    case 2: u8g2_r_frame(draw_state&7); break;
    case 3: u8g2_string(draw_state&7); break;
    case 4: u8g2_line(draw_state&7); break;
    case 5: u8g2_triangle(draw_state&7); break;
    case 6: u8g2_ascii_1(); break;
    case 7: u8g2_ascii_2(); break;
    case 8: u8g2_extra_page(draw_state&7); break;
    case 9: u8g2_bitmap_modes(0); break;
    case 10: u8g2_bitmap_modes(1); break;
    case 11: u8g2_bitmap_overlay(draw_state&7); break;
  }
}

void draw() {
  // Example drawing code for the screen
  u8g2.setFont(u8g2_font_ncenB08_tr);
  //u8g2.drawStr(0, 10, "Servo Angle:");
  char angleText[10];
  sprintf(angleText, "%d", servo_angle);
  u8g2.drawStr(0, 10, angleText);
}











// HTML served inline
const char* adminPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SPIFFS Manager</title>
</head>
<body>
    <h1>SPIFFS File Manager</h1>
    <form id="uploadForm" method="POST" enctype="multipart/form-data">
        <label for="fileUpload">Upload a file:</label><br>
        <input type="file" id="fileUpload" name="fileUpload">
        <button type="submit">Upload</button>
    </form>
    <hr>
    <h2>Files in SPIFFS:</h2>
    <ul id="fileList"></ul>

    <script>
        // Fetch and display files
        async function fetchFiles() {
            const response = await fetch('/list');
            const files = await response.json();
            const fileList = document.getElementById('fileList');
            fileList.innerHTML = '';
            files.forEach(file => {
                const li = document.createElement('li');
                li.innerHTML = `${file.name} (${file.size} bytes) 
                    <button onclick="downloadFile('${file.name}')">Download</button>
                    <button onclick="deleteFile('${file.name}')">Delete</button>`;
                fileList.appendChild(li);
            });
        }

        // Download file
        function downloadFile(fileName) {
            window.location.href = `/download?file=${fileName}`;
        }

        // Delete file
        async function deleteFile(fileName) {
            const response = await fetch(`/delete?file=${fileName}`, { method: 'GET' });
            const result = await response.text();
            alert(result);
            fetchFiles();
        }

        // Handle file upload
        const uploadForm = document.getElementById('uploadForm');
        uploadForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const fileInput = document.getElementById('fileUpload');
            const formData = new FormData();
            formData.append('fileUpload', fileInput.files[0]);

            const response = await fetch('/upload', { method: 'POST', body: formData });
            const result = await response.text();
            alert(result);
            fetchFiles();
        });

        // Initial file fetch
        fetchFiles();
    </script>
</body>
</html>
)rawliteral";


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


void playT()
{
   for (int i = 0; i < 8; i++) {
    playTone(melody[i], noteDurations[i]);
    delay(50); // Short pause between notes
  }
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




void handleTreat() {

    playT();

    tone(2, 1000); // Send 1KHz sound signal...
    delay(1000);         // ...for 1 sec
    noTone(2);     // Stop sound...
    delay(1000);         // ...for 1sec

    Serial.println("Releasing Treat...");
    server.send(200, "text/plain", "Treat received!");
    int new_angle = 30; //input.toInt();
    myServo.write(50);
    delay(500);
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

  testSPIFFS();

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.println(file.name());
    file = root.openNextFile();
  } 

  // Init Server
  // server.serveStatic("/", SPIFFS, "/index.html");
  // server.onNotFound([]() {
  //   server.send(404, "text/plain", "Not Found");
  // });
  

  server.on("/", HTTP_GET, handleRoot);
  server.on("/admin.html", HTTP_GET, []() { server.send(200, "text/html", adminPage); });
  server.on("/list", HTTP_GET, handleList);
  server.on("/upload", HTTP_POST, []() {}, handleUpload);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/treat", HTTP_POST, handleTreat);

  server.begin();

  // Initialize servo
  myServo.attach(SERVO_PIN);
  myServo.write(servo_angle);
  Serial.println("Initialization complete.");

  myServo.write(150);
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
