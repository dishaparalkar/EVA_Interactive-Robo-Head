#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// =====================================================
// EVA WIFI + SERVO + OLED PROTOTYPE
// =====================================================
//
// Phone/Laptop
//      ↓
//     Wi-Fi
//      ↓
//   ESP32 Web Server
//      ↓
//  Command Parser
//      ↓
//  Servo + OLED
//
// This is a SEPARATE prototype
//
// =====================================================


// =====================================================
// WIFI CONFIGURATION
// =====================================================

const char* AP_SSID = "EVA_ESP32";
const char* AP_PASSWORD = "EVA12345";

WebServer server(80);


// =====================================================
// OLED CONFIGURATION
// =====================================================

#define OLED_SDA 21
#define OLED_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// =====================================================
// SERVO CONFIGURATION
// =====================================================

Servo evaServo;

const int SERVO_PIN = 19;

const int SERVO_CENTER = 90;
const int SERVO_LEFT = 30;
const int SERVO_RIGHT = 150;

int currentServoAngle = SERVO_CENTER;


// =====================================================
// COMMAND PROTOCOL
// =====================================================

enum EVACommand {

  CMD_NONE,

  CMD_HELLO,

  CMD_LOOK_LEFT,
  CMD_LOOK_RIGHT,
  CMD_LOOK_CENTER,

  CMD_HAPPY,
  CMD_SLEEP,

  CMD_STATUS,

  CMD_STOP
};


// =====================================================
// SYSTEM VARIABLES
// =====================================================

String lastCommand = "NONE";

unsigned long lastCommandTime = 0;

unsigned long commandCount = 0;

unsigned long invalidCommandCount = 0;


// =====================================================
// OLED BASIC DISPLAY FUNCTION
// =====================================================

void showMessage(
  const char* line1,
  const char* line2 = "",
  const char* line3 = ""
) {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);


  // -----------------------------
  // First line
  // -----------------------------

  display.setCursor(0, 5);

  display.println(line1);


  // -----------------------------
  // Second line
  // -----------------------------

  if (strlen(line2) > 0) {

    display.setCursor(0, 25);

    display.println(line2);

  }


  // -----------------------------
  // Third line
  // -----------------------------

  if (strlen(line3) > 0) {

    display.setCursor(0, 45);

    display.println(line3);

  }


  display.display();
}


// =====================================================
// COMMAND PARSER
// =====================================================

EVACommand parseCommand(String command) {

  command.trim();

  command.toUpperCase();


  if (command == "HELLO")
    return CMD_HELLO;


  if (command == "LOOK_LEFT")
    return CMD_LOOK_LEFT;


  if (command == "LOOK_RIGHT")
    return CMD_LOOK_RIGHT;


  if (command == "LOOK_CENTER")
    return CMD_LOOK_CENTER;


  if (command == "HAPPY")
    return CMD_HAPPY;


  if (command == "SLEEP")
    return CMD_SLEEP;


  if (command == "STATUS")
    return CMD_STATUS;


  if (command == "STOP")
    return CMD_STOP;


  return CMD_NONE;
}


// =====================================================
// COMMAND NAME
// =====================================================

const char* commandName(EVACommand command) {

  switch (command) {

    case CMD_HELLO:
      return "HELLO";

    case CMD_LOOK_LEFT:
      return "LOOK_LEFT";

    case CMD_LOOK_RIGHT:
      return "LOOK_RIGHT";

    case CMD_LOOK_CENTER:
      return "LOOK_CENTER";

    case CMD_HAPPY:
      return "HAPPY";

    case CMD_SLEEP:
      return "SLEEP";

    case CMD_STATUS:
      return "STATUS";

    case CMD_STOP:
      return "STOP";

    default:
      return "NONE";
  }
}


// =====================================================
// SERVO CONTROL
// =====================================================

void moveServo(int angle) {

  angle = constrain(angle, 0, 180);

  currentServoAngle = angle;

  evaServo.write(angle);


  Serial.print("SERVO -> ");

  Serial.print(angle);

  Serial.println(" degrees");
}


// =====================================================
// HELLO RESPONSE
// =====================================================

void helloResponse() {

  Serial.println("EVA: HELLO");


  showMessage(
    "EVA",
    "HELLO!",
    "Nice to meet you"
  );


  moveServo(75);

  delay(150);

  moveServo(105);

  delay(150);

  moveServo(SERVO_CENTER);
}


// =====================================================
// HAPPY RESPONSE
// =====================================================

void happyResponse() {

  Serial.println("EVA: HAPPY");


  showMessage(
    "EVA",
    "HAPPY :)",
    "Feeling good!"
  );


  moveServo(80);

  delay(120);

  moveServo(100);

  delay(120);

  moveServo(SERVO_CENTER);
}


// =====================================================
// STATUS
// =====================================================

String getStatus() {

  String response = "";

  response += "EVA STATUS\n";
  response += "----------------\n";

  response += "WIFI: CONNECTED\n";

  response += "IP: ";
  response += WiFi.softAPIP().toString();
  response += "\n";

  response += "LAST COMMAND: ";
  response += lastCommand;
  response += "\n";

  response += "SERVO: ";
  response += String(currentServoAngle);
  response += " deg\n";

  response += "COMMAND COUNT: ";
  response += String(commandCount);
  response += "\n";

  response += "INVALID COMMANDS: ";
  response += String(invalidCommandCount);
  response += "\n";

  response += "UPTIME: ";
  response += String(millis() / 1000);
  response += " s";

  return response;
}


// =====================================================
// STATUS ON OLED
// =====================================================

void showStatusOnOLED() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);


  display.setCursor(0, 0);

  display.println("EVA STATUS");


  display.setCursor(0, 14);

  display.print("WiFi: OK");


  display.setCursor(0, 26);

  display.print("Servo: ");

  display.print(currentServoAngle);

  display.print(" deg");


  display.setCursor(0, 38);

  display.print("Cmd: ");

  display.println(lastCommand);


  display.setCursor(0, 50);

  display.print("Count: ");

  display.println(commandCount);


  display.display();
}


// =====================================================
// WEB PAGE
// =====================================================

const char MAIN_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>EVA Control</title>


<style>

body {

  font-family: Arial;

  text-align: center;

  background: #111;

  color: white;

  padding: 20px;
}


button {

  padding: 14px 20px;

  margin: 5px;

  font-size: 16px;

  border-radius: 6px;

  border: none;

}


input {

  padding: 12px;

  width: 80%;

  max-width: 350px;

  font-size: 16px;

}


.status {

  margin-top: 25px;

  padding: 15px;

  background: #222;

  border-radius: 8px;

}

</style>

</head>


<body>


<h1>EVA CONTROL</h1>

<p>Wi-Fi + Servo + OLED</p>


<input

  id="command"

  placeholder="Enter command"


>


<br><br>


<button onclick="send('HELLO')">
HELLO
</button>


<button onclick="send('HAPPY')">
HAPPY
</button>


<button onclick="send('SLEEP')">
SLEEP
</button>


<br>


<button onclick="send('LOOK_LEFT')">
LEFT
</button>


<button onclick="send('LOOK_CENTER')">
CENTER
</button>


<button onclick="send('LOOK_RIGHT')">
RIGHT
</button>


<br>


<button onclick="send('STATUS')">
STATUS
</button>


<button onclick="send('STOP')">
STOP
</button>


<div class="status">

<p id="response">
Waiting for EVA...
</p>

</div>


<script>


function send(command) {

  fetch(
    "/command?cmd=" +
    encodeURIComponent(command)
  )

  .then(response => response.text())

  .then(data => {

    document.getElementById("response")
      .innerText = data;

  })

  .catch(error => {

    document.getElementById("response")
      .innerText = "Communication error";

  });

}

</script>


</body>

</html>

)rawliteral";


// =====================================================
// ROOT PAGE
// =====================================================

void handleRoot() {

  server.send(

    200,

    "text/html",

    MAIN_PAGE

  );
}


// =====================================================
// COMMAND HANDLER
// =====================================================

void handleCommand() {

  if (!server.hasArg("cmd")) {

    server.send(
      400,
      "text/plain",
      "ERROR: NO COMMAND"
    );

    return;
  }


  String command = server.arg("cmd");

  command.trim();

  command.toUpperCase();


  Serial.println();

  Serial.println("-----------------------------");

  Serial.print("RX: ");

  Serial.println(command);


  // ---------------------------------------------------
  // Parse command
  // ---------------------------------------------------

  EVACommand parsedCommand =
    parseCommand(command);


  // ---------------------------------------------------
  // Validate command
  // ---------------------------------------------------

  if (parsedCommand == CMD_NONE) {

    invalidCommandCount++;


    showMessage(
      "ERROR",
      "UNKNOWN COMMAND",
      command.c_str()
    );


    Serial.println("INVALID COMMAND");


    server.send(
      400,
      "text/plain",
      "ERROR: UNKNOWN COMMAND"
    );

    return;
  }


  // ---------------------------------------------------
  // Store command
  // ---------------------------------------------------

  lastCommand = command;

  lastCommandTime = millis();

  commandCount++;


  Serial.print("VALID: ");

  Serial.println(
    commandName(parsedCommand)
  );


  // ---------------------------------------------------
  // Execute command
  // ---------------------------------------------------

  switch (parsedCommand) {


    // ================================================
    // HELLO
    // ================================================

    case CMD_HELLO:

      helloResponse();

      break;


    // ================================================
    // LOOK LEFT
    // ================================================

    case CMD_LOOK_LEFT:

      showMessage(
        "EVA",
        "LOOKING LEFT",
        "30 degrees"
      );


      moveServo(SERVO_LEFT);

      break;


    // ================================================
    // LOOK RIGHT
    // ================================================

    case CMD_LOOK_RIGHT:

      showMessage(
        "EVA",
        "LOOKING RIGHT",
        "150 degrees"
      );


      moveServo(SERVO_RIGHT);

      break;


    // ================================================
    // LOOK CENTER
    // ================================================

    case CMD_LOOK_CENTER:

      showMessage(
        "EVA",
        "LOOKING CENTER",
        "90 degrees"
      );


      moveServo(SERVO_CENTER);

      break;


    // ================================================
    // HAPPY
    // ================================================

    case CMD_HAPPY:

      happyResponse();

      break;


    // ================================================
    // SLEEP
    // ================================================

    case CMD_SLEEP:

      Serial.println("EVA: SLEEP");


      showMessage(
        "EVA",
        "SLEEPING...",
        "Good night"
      );


      moveServo(SERVO_CENTER);

      break;


    // ================================================
    // STATUS
    // ================================================

    case CMD_STATUS:

      showStatusOnOLED();


      Serial.println(
        getStatus()
      );


      server.send(
        200,
        "text/plain",
        getStatus()
      );


      return;


    // ================================================
    // STOP
    // ================================================

    case CMD_STOP:

      Serial.println("EVA: STOP");


      showMessage(
        "EVA",
        "STOPPED",
        "Servo centered"
      );


      moveServo(SERVO_CENTER);

      break;


    default:

      break;

  }


  // ---------------------------------------------------
  // Response to browser
  // ---------------------------------------------------

  String response = "OK: ";

  response += commandName(parsedCommand);


  server.send(
    200,
    "text/plain",
    response
  );
}


// =====================================================
// 404 HANDLER
// =====================================================

void handleNotFound() {

  server.send(
    404,
    "text/plain",
    "404 - NOT FOUND"
  );
}


// =====================================================
// WIFI INITIALIZATION
// =====================================================

void startWiFi() {

  Serial.println();

  Serial.println(
    "Starting Wi-Fi..."
  );


  WiFi.mode(WIFI_AP);


  bool result = WiFi.softAP(
    AP_SSID,
    AP_PASSWORD
  );


  if (!result) {

    Serial.println(
      "ERROR: WIFI AP FAILED"
    );

    return;
  }


  Serial.println();

  Serial.println(
    "WIFI AP STARTED"
  );


  Serial.print("SSID: ");

  Serial.println(AP_SSID);


  Serial.print("PASSWORD: ");

  Serial.println(AP_PASSWORD);


  Serial.print("IP: ");

  Serial.println(
    WiFi.softAPIP()
  );


  // ---------------------------------------------------
  // Web routes
  // ---------------------------------------------------

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/command",
    HTTP_GET,
    handleCommand
  );


  server.onNotFound(
    handleNotFound
  );


  server.begin();


  Serial.println(
    "HTTP SERVER STARTED"
  );
}


// =====================================================
// OLED INITIALIZATION
// =====================================================

void initOLED() {

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println(
      "OLED ERROR"
    );


    while (true) {

      delay(1000);

    }

  }


  display.clearDisplay();

  display.display();


  showMessage(
    "EVA",
    "Wi-Fi Starting...",
    "Please wait"
  );


  Serial.println(
    "OLED: OK"
  );
}


// =====================================================
// SERVO INITIALIZATION
// =====================================================

void initServo() {

  evaServo.setPeriodHertz(50);


  if (!evaServo.attach(
        SERVO_PIN,
        500,
        2400
      )) {

    Serial.println(
      "SERVO ATTACH ERROR"
    );

  }

  else {

    Serial.println(
      "SERVO: OK"
    );

  }


  moveServo(
    SERVO_CENTER
  );
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);


  delay(500);


  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.println(
    "    EVA WIFI + SERVO + OLED"
  );

  Serial.println(
    "======================================"
  );


  // ---------------------------------------------------
  // OLED
  // ---------------------------------------------------

  initOLED();


  // ---------------------------------------------------
  // Servo
  // ---------------------------------------------------

  initServo();


  // ---------------------------------------------------
  // Wi-Fi
  // ---------------------------------------------------

  startWiFi();


  showMessage(
    "EVA READY",
    "Connect to Wi-Fi",
    "192.168.4.1"
  );


  Serial.println();

  Serial.println(
    "--------------------------------------"
  );

  Serial.println(
    "Connect phone to:"
  );

  Serial.println(
    "EVA_ESP32"
  );

  Serial.println();

  Serial.println(
    "Open:"
  );

  Serial.println(
    "http://192.168.4.1"
  );

  Serial.println(
    "--------------------------------------"
  );

}


// =====================================================
// LOOP
// =====================================================

void loop() {

  server.handleClient();

}