import processing.serial.*;

// ============================================================
// EVA RADAR VISUALIZATION
// Processing 4
// ESP32 sends:
// RADAR,angle,distance,detected
// Example:
// RADAR,90,45.2,1
// RADAR,92,-1,0
// ============================================================


// ---------------- SERIAL SETTINGS ----------------

// CHANGE THIS TO YOUR ESP32 COM PORT
String PORT_NAME = "COM9";

int BAUD_RATE = 115200;

Serial myPort;


// ---------------- WINDOW ----------------

int WINDOW_WIDTH = 1300;
int WINDOW_HEIGHT = 800;


// ---------------- RADAR SETTINGS ----------------

float MAX_DISTANCE = 100.0;

float RADAR_RADIUS = 300;

float CENTER_X;
float CENTER_Y;


// ---------------- CURRENT SENSOR DATA ----------------

float currentAngle = 90;
float currentDistance = -1;

boolean currentDetected = false;

long lastPacketTime = 0;


// ---------------- RADAR HISTORY ----------------

// One value for every angle from 0 to 180
float[] objectDistance = new float[181];

boolean[] objectSeen = new boolean[181];


// ---------------- COLORS ----------------

int radarGreen;
int detectionRed;
int backgroundColor;
int textColor;


// ============================================================
// SETUP
// ============================================================

void setup() {

  size(1300, 800);

  smooth(8);

  CENTER_X = width / 2.0;
  CENTER_Y = height - 80;

  radarGreen = color(0, 255, 80);
  detectionRed = color(255, 40, 40);
  backgroundColor = color(3, 10, 8);
  textColor = color(180, 255, 190);


  // Initialize history

  for (int i = 0; i <= 180; i++) {

    objectDistance[i] = -1;
    objectSeen[i] = false;

  }


  // ---------------- SERIAL ----------------

  println("Available serial ports:");

  String[] ports = Serial.list();

  for (int i = 0; i < ports.length; i++) {

    println(i + " : " + ports[i]);

  }


  try {

    myPort = new Serial(this, PORT_NAME, BAUD_RATE);

    myPort.bufferUntil('\n');

    println("Connected to: " + PORT_NAME);

  }

  catch (Exception e) {

    println("------------------------------------------------");
    println("SERIAL CONNECTION ERROR");
    println("Could not open: " + PORT_NAME);
    println("");
    println("Available ports:");

    for (String p : ports) {
      println(p);
    }

    println("");
    println("Change PORT_NAME near the top of the code.");
    println("------------------------------------------------");

  }


  background(backgroundColor);

}


// ============================================================
// DRAW
// ============================================================

void draw() {

  background(backgroundColor);


  // Draw radar

  drawRadarGrid();

  drawAngleLines();

  drawRangeLabels();

  drawAngleLabels();


  // Draw historical detections

  drawDetectionHistory();


  // Draw current sweep

  drawSweepLine();


  // Draw current detection

  drawCurrentDetection();


  // Draw information panel

  drawInformationPanel();


  // Draw connection status

  drawConnectionStatus();

}


// ============================================================
// RADAR GRID
// ============================================================

void drawRadarGrid() {

  pushStyle();

  noFill();

  strokeWeight(2);

  stroke(radarGreen, 180);


  // Distance rings

  for (int d = 20; d <= 100; d += 20) {

    float radius = map(
      d,
      0,
      MAX_DISTANCE,
      0,
      RADAR_RADIUS
    );

    arc(
      CENTER_X,
      CENTER_Y,
      radius * 2,
      radius * 2,
      PI,
      TWO_PI
    );

  }


  // Outer border

  stroke(radarGreen, 230);

  strokeWeight(3);

  arc(
    CENTER_X,
    CENTER_Y,
    RADAR_RADIUS * 2,
    RADAR_RADIUS * 2,
    PI,
    TWO_PI
  );


  // Center horizontal line

  line(
    CENTER_X - RADAR_RADIUS,
    CENTER_Y,
    CENTER_X + RADAR_RADIUS,
    CENTER_Y
  );


  popStyle();

}


// ============================================================
// ANGLE LINES
// ============================================================

void drawAngleLines() {

  pushStyle();

  stroke(radarGreen, 100);

  strokeWeight(1);

  for (int angle = 0; angle <= 180; angle += 30) {

    float radiansAngle = radians(angle);

    float x = CENTER_X + cos(radiansAngle) * RADAR_RADIUS;

    float y = CENTER_Y - sin(radiansAngle) * RADAR_RADIUS;

    line(
      CENTER_X,
      CENTER_Y,
      x,
      y
    );

  }

  popStyle();

}


// ============================================================
// RANGE LABELS
// ============================================================

void drawRangeLabels() {

  pushStyle();

  fill(textColor);

  textAlign(LEFT, CENTER);

  textSize(15);

  for (int d = 20; d <= 100; d += 20) {

    float radius = map(
      d,
      0,
      MAX_DISTANCE,
      0,
      RADAR_RADIUS
    );

    float x = CENTER_X + 8;

    float y = CENTER_Y - radius;

    text(
      d + " cm",
      x,
      y
    );

  }

  popStyle();

}


// ============================================================
// ANGLE LABELS
// ============================================================

void drawAngleLabels() {

  pushStyle();

  fill(textColor);

  textAlign(CENTER, CENTER);

  textSize(16);

  for (int angle = 0; angle <= 180; angle += 30) {

    float radiansAngle = radians(angle);

    float labelRadius = RADAR_RADIUS + 30;

    float x =
      CENTER_X +
      cos(radiansAngle) *
      labelRadius;

    float y =
      CENTER_Y -
      sin(radiansAngle) *
      labelRadius;

    text(
      angle + "°",
      x,
      y
    );

  }

  popStyle();

}


// ============================================================
// SWEEP LINE
// ============================================================

void drawSweepLine() {

  float radiansAngle = radians(currentAngle);

  float x =
    CENTER_X +
    cos(radiansAngle) *
    RADAR_RADIUS;

  float y =
    CENTER_Y -
    sin(radiansAngle) *
    RADAR_RADIUS;


  pushStyle();

  // Glow

  stroke(
    radarGreen,
    40
  );

  strokeWeight(12);

  line(
    CENTER_X,
    CENTER_Y,
    x,
    y
  );


  // Main sweep

  if (currentDetected) {

    stroke(detectionRed);

  }
  else {

    stroke(radarGreen);

  }

  strokeWeight(3);

  line(
    CENTER_X,
    CENTER_Y,
    x,
    y
  );

  popStyle();

}


// ============================================================
// CURRENT DETECTION
// ============================================================

void drawCurrentDetection() {

  if (!currentDetected) {
    return;
  }

  if (currentDistance < 0 ||
      currentDistance > MAX_DISTANCE) {
    return;
  }


  float radius = map(
    currentDistance,
    0,
    MAX_DISTANCE,
    0,
    RADAR_RADIUS
  );


  float radiansAngle = radians(currentAngle);


  float x =
    CENTER_X +
    cos(radiansAngle) *
    radius;

  float y =
    CENTER_Y -
    sin(radiansAngle) *
    radius;


  pushStyle();


  // Outer glow

  noStroke();

  fill(
    detectionRed,
    50
  );

  ellipse(
    x,
    y,
    30,
    30
  );


  // Detection point

  fill(detectionRed);

  ellipse(
    x,
    y,
    10,
    10
  );


  // Crosshair

  stroke(
    detectionRed,
    180
  );

  strokeWeight(1);

  line(
    x - 12,
    y,
    x + 12,
    y
  );

  line(
    x,
    y - 12,
    x,
    y + 12
  );


  popStyle();

}


// ============================================================
// DETECTION HISTORY
// ============================================================

void drawDetectionHistory() {

  pushStyle();

  noStroke();

  for (int angle = 0; angle <= 180; angle++) {

    if (!objectSeen[angle]) {
      continue;
    }

    float distance = objectDistance[angle];


    if (distance < 0 ||
        distance > MAX_DISTANCE) {
      continue;
    }


    float radius = map(
      distance,
      0,
      MAX_DISTANCE,
      0,
      RADAR_RADIUS
    );


    float radiansAngle = radians(angle);


    float x =
      CENTER_X +
      cos(radiansAngle) *
      radius;

    float y =
      CENTER_Y -
      sin(radiansAngle) *
      radius;


    // Historical point

    fill(
      detectionRed,
      130
    );

    ellipse(
      x,
      y,
      7,
      7
    );

  }

  popStyle();

}


// ============================================================
// INFORMATION PANEL
// ============================================================

void drawInformationPanel() {

  pushStyle();


  // Panel

  fill(5, 20, 15, 230);

  stroke(
    radarGreen,
    120
  );

  strokeWeight(1);

  rect(
    20,
    20,
    260,
    175,
    10
  );


  fill(textColor);

  textAlign(LEFT, TOP);


  textSize(22);

  text(
    "EVA RADAR",
    40,
    40
  );


  textSize(16);


  fill(textColor);

  text(
    "Angle: " +
    nf(currentAngle, 0, 1) +
    "°",
    40,
    80
  );


  if (currentDistance >= 0) {

    text(
      "Distance: " +
      nf(currentDistance, 0, 1) +
      " cm",
      40,
      108
    );

  }
  else {

    text(
      "Distance: --",
      40,
      108
    );

  }


  if (currentDetected) {

    fill(detectionRed);

    text(
      "OBJECT: DETECTED",
      40,
      138
    );

  }
  else {

    fill(radarGreen);

    text(
      "OBJECT: NONE",
      40,
      138
    );

  }


  fill(textColor);

  text(
    "Range: 0 - " +
    int(MAX_DISTANCE) +
    " cm",
    40,
    168
  );


  popStyle();

}


// ============================================================
// CONNECTION STATUS
// ============================================================

void drawConnectionStatus() {

  pushStyle();

  textAlign(RIGHT, TOP);

  textSize(16);


  if (myPort != null &&
      millis() - lastPacketTime < 3000) {

    fill(radarGreen);

    text(
      "● ESP32 CONNECTED",
      width - 30,
      25
    );

  }
  else {

    fill(255, 180, 50);

    text(
      "● WAITING FOR ESP32",
      width - 30,
      25
    );

  }


  fill(textColor);

  textSize(13);

  text(
    "C = Clear    R = Reset",
    width - 30,
    50
  );


  popStyle();

}


// ============================================================
// SERIAL EVENT
// ============================================================

void serialEvent(Serial port) {

  String incoming = port.readStringUntil('\n');


  if (incoming == null) {
    return;
  }


  incoming = trim(incoming);


  println(incoming);


  // Expected format:
  //
  // RADAR,90,45.2,1
  //

  if (!incoming.startsWith("RADAR,")) {
    return;
  }


  String[] data =
    split(incoming, ',');


  if (data.length < 4) {
    return;
  }


  try {

    float angle =
      float(data[1]);

    float distance =
      float(data[2]);

    int detected =
      int(data[3]);


    // Clamp angle

    angle = constrain(
      angle,
      0,
      180
    );


    currentAngle = angle;

    currentDistance = distance;

    currentDetected =
      detected == 1;


    lastPacketTime = millis();


    // Save detection

    int index =
      round(currentAngle);


    if (index >= 0 &&
        index <= 180 &&
        currentDetected &&
        currentDistance >= 0 &&
        currentDistance <= MAX_DISTANCE) {

      objectDistance[index] =
        currentDistance;

      objectSeen[index] =
        true;

    }

  }

  catch (Exception e) {

    println(
      "Invalid RADAR packet: " +
      incoming
    );

  }

}


// ============================================================
// KEYBOARD CONTROLS
// ============================================================

void keyPressed() {


  // Clear radar history

  if (key == 'c' ||
      key == 'C') {

    clearHistory();

  }


  // Reset

  if (key == 'r' ||
      key == 'R') {

    clearHistory();

    currentAngle = 90;

    currentDistance = -1;

    currentDetected = false;

  }

}


// ============================================================
// CLEAR HISTORY
// ============================================================

void clearHistory() {

  for (int i = 0; i <= 180; i++) {

    objectDistance[i] = -1;

    objectSeen[i] = false;

  }

}
