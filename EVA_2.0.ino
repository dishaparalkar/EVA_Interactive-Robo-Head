#include <Wire.h> //file of I2C to able to communicate external circuit 
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ======================================================
// EVA 2.0.2 - STABLE HAPPY + FULL OBSERVATION SCAN
// =====================================================

// ---------- Pins ----------
#define OLED_SDA 21
#define OLED_SCL 22
#define TRIG_PIN 5
#define ECHO_PIN 23
#define TOUCH_PIN 4
#define SERVO_PIN 19

// Distance indicator LEDs
#define LED1_PIN 13
#define LED2_PIN 12
#define LED3_PIN 14
#define LED4_PIN 26
#define LED5_PIN 25

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- Expressions ----------
enum EVAState {
  STATE_SLEEPY,
  STATE_HAPPY,
  STATE_LOVE
};

enum EVAMode {
  MODE_NORMAL,
  MODE_OBSERVATION
};

EVAState currentState = STATE_SLEEPY;
EVAState previousState = STATE_SLEEPY;
EVAMode currentMode = MODE_NORMAL;

// ---------- Distance ----------
const float MIN_VALID_DISTANCE = 3.0f;
const float MAX_VALID_DISTANCE = 200.0f;
const float PERSON_ENTER_DISTANCE = 60.0f;
const float PERSON_EXIT_DISTANCE = 80.0f;

// When the servo is scanning, the person may temporarily disappear
// from the ultrasonic beam. Keep them "present" for this long.
const unsigned long PERSON_LOST_HOLD_MS = 900;

// Require multiple readings before changing detection state.
const uint8_t ENTER_CONFIRM_COUNT = 2;
const uint8_t EXIT_CONFIRM_COUNT = 3;

// ---------- Scheduler ----------
const unsigned long SENSOR_PERIOD_MS = 60;
const unsigned long TOUCH_PERIOD_MS = 20;
const unsigned long DISPLAY_PERIOD_MS = 40;
const unsigned long SERVO_PERIOD_MS = 20;
const unsigned long TELEMETRY_PERIOD_MS = 1000;

// ---------- Sensor filter ----------
const uint8_t DIST_FILTER_SIZE = 5;
const uint8_t SENSOR_BAD_LIMIT = 8;

float distanceSamples[DIST_FILTER_SIZE] = {0};
uint8_t distanceSampleCount = 0;
uint8_t distanceSampleIndex = 0;

float distanceCM = 999.0f;
float filteredDistanceCM = 999.0f;


uint8_t consecutiveBadReadings = 0;
bool sensorFault = false;

// ---------- Ultrasonic interrupt driver ----------
volatile bool echoMeasuring = false;
volatile bool echoReceived = false;
volatile unsigned long echoStartUs = 0;
volatile unsigned long echoDurationUs = 0;

unsigned long echoDeadlineUs = 0;
unsigned long lastMeasurementStartUs = 0;

enum UltrasonicPhase {
  US_IDLE,
  US_WAIT_ECHO
};
UltrasonicPhase ultrasonicPhase = US_IDLE;

// ---------- Person detection ----------
bool personDetected = false;

uint8_t enterConfirm = 0;
uint8_t exitConfirm = 0;
unsigned long lastPersonSeenMs = 0;

// ---------- Touch ----------
bool rawTouch = false;
bool lastRawTouch = false;
bool stableTouch = false;

unsigned long rawTouchChangedMs = 0;
unsigned long touchPressedMs = 0;
bool longPressHandled = false;

const unsigned long TOUCH_DEBOUNCE_MS = 40;
const unsigned long LONG_PRESS_MS = 1800;
const unsigned long LOVE_HOLD_MS = 1200;

// ---------- Servo ----------
Servo evaServo;

const int SERVO_MIN = 0;
const int SERVO_MAX = 180;
const int SERVO_CENTER = 90;

// Normal-mode head scan remains gentle.
const int SCAN_MIN = 30;
const int SCAN_MAX = 150;

// Observation-mode scan covers the full 0-180 degree servo range.
const int OBS_SCAN_MIN = 0;
const int OBS_SCAN_MAX = 180;

// Observation detection zone
const float OBS_MIN_DISTANCE = 20.0f;
const float OBS_MAX_DISTANCE = 100.0f;

const int SCAN_STEP = 2;
const unsigned long SCAN_INTERVAL_MS = 70;

const int LOVE_MIN = 84;
const int LOVE_MAX = 96;
const int LOVE_STEP = 2;
const unsigned long LOVE_INTERVAL_MS = 180;

int currentServoAngle = SERVO_CENTER;
int targetServoAngle = SERVO_CENTER;
int scanDirection = 1;
int loveDirection = 1;

unsigned long lastServoUpdateMs = 0;
unsigned long lastScanMs = 0;
unsigned long lastLoveMotionMs = 0;

enum MotionMode {
  MOTION_CENTER,
  MOTION_SCAN,
  MOTION_LOVE
};
MotionMode motionMode = MOTION_CENTER;

// ---------- Expression ----------
int happyWidth = 18;
uint8_t sleepyFrame = 0;
uint8_t loveFrame = 0;

unsigned long loveUntilMs = 0;
unsigned long lastDisplayFrameMs = 0;
bool expressionFirstFrame = true;

// ---------- Observation ----------
/*
  IMPORTANT:
  With ONE HC-SR04, EVA cannot identify unique people or know
  how many people are simultaneously in a room.

  observationDetections = number of DISTINCT DETECTION EPISODES.
  A person must disappear from the sensing zone long enough
  before another entry is counted.

  observationCurrent = estimated occupancy.
*/
uint32_t observationDetections = 0;
uint32_t observationExits = 0;
uint32_t observationCurrent = 0;
uint32_t observationMaxSimultaneous = 0;

unsigned long lastObservationEntryMs = 0;
const unsigned long OBS_ENTRY_COOLDOWN_MS = 1200;

// Dedicated Observation Mode presence tracking.
// The HC-SR04 is scanning, so a person can temporarily disappear
// from the beam without actually leaving.
bool observationPersonPresent = false;
unsigned long observationLastSeenMs = 0;
unsigned long observationLostSinceMs = 0;
uint8_t observationSeenConfirm = 0;
uint8_t observationLostConfirm = 0;

const uint8_t OBS_SEEN_CONFIRM = 2;
const uint8_t OBS_LOST_CONFIRM = 4;

// Longer than one complete 60->120->60 scan.
const unsigned long OBS_LOST_TIMEOUT_MS = 5200;

// ---------- Events ----------
enum EventType {
  EVENT_PERSON_ENTERED,
  EVENT_PERSON_LEFT,
  EVENT_TOUCH_PRESSED,
  EVENT_MODE_CHANGED
};

struct Event {
  EventType type;
  unsigned long timestamp;
};

const uint8_t EVENT_QUEUE_SIZE = 12;
Event eventQueue[EVENT_QUEUE_SIZE];
uint8_t eventHead = 0;
uint8_t eventTail = 0;

bool pushEvent(EventType type) {
  uint8_t next = (eventHead + 1) % EVENT_QUEUE_SIZE;

  if (next == eventTail)
    return false;

  eventQueue[eventHead].type = type;
  eventQueue[eventHead].timestamp = millis();
  eventHead = next;
  return true;
}

bool popEvent(Event &event) {
  if (eventTail == eventHead)
    return false;

  event = eventQueue[eventTail];
  eventTail = (eventTail + 1) % EVENT_QUEUE_SIZE;
  return true;
}

// =====================================================
// UTILITY
// =====================================================

const char* stateName(EVAState state) {
  switch (state) {
    case STATE_SLEEPY: return "SLEEPY";
    case STATE_HAPPY:  return "HAPPY";
    case STATE_LOVE:   return "LOVE";
    default:           return "UNKNOWN";
  }
}

void setServoTarget(int angle) {
  targetServoAngle = constrain(angle, SERVO_MIN, SERVO_MAX);
}

void setState(EVAState newState) {
  if (newState == currentState)
    return;

  previousState = currentState;
  currentState = newState;

  happyWidth = 18;
  sleepyFrame = 0;
  loveFrame = 0;
  expressionFirstFrame = true;

  if (newState == STATE_SLEEPY) {
    motionMode = MOTION_CENTER;
    setServoTarget(SERVO_CENTER);
  }
  else if (newState == STATE_HAPPY) {
    motionMode = MOTION_SCAN;
  }
  else if (newState == STATE_LOVE) {
    motionMode = MOTION_LOVE;
    loveUntilMs = millis() + LOVE_HOLD_MS;
    setServoTarget(SERVO_CENTER);
  }

  Serial.print("STATE -> ");
  Serial.println(stateName(newState));
}

// =====================================================
// HC-SR04
// =====================================================

void IRAM_ATTR echoISR() {
  int level = digitalRead(ECHO_PIN);

  if (level == HIGH) {
    echoStartUs = micros();
    echoMeasuring = true;
  }
  else if (echoMeasuring) {
    echoDurationUs = micros() - echoStartUs;
    echoReceived = true;
    echoMeasuring = false;
  }
}

void startUltrasonicMeasurement() {
  if (ultrasonicPhase != US_IDLE)
    return;

  noInterrupts();
  echoReceived = false;
  echoMeasuring = false;
  echoDurationUs = 0;
  interrupts();

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  lastMeasurementStartUs = micros();
  echoDeadlineUs = lastMeasurementStartUs + 30000UL;
  ultrasonicPhase = US_WAIT_ECHO;
}

float medianFilter(float value) {
  distanceSamples[distanceSampleIndex] = value;
  distanceSampleIndex =
      (distanceSampleIndex + 1) % DIST_FILTER_SIZE;

  if (distanceSampleCount < DIST_FILTER_SIZE)
    distanceSampleCount++;

  float temp[DIST_FILTER_SIZE];

  for (uint8_t i = 0; i < distanceSampleCount; i++)
    temp[i] = distanceSamples[i];

  for (uint8_t i = 0; i < distanceSampleCount; i++) {
    for (uint8_t j = i + 1; j < distanceSampleCount; j++) {
      if (temp[j] < temp[i]) {
        float t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }
    }
  }

  return temp[distanceSampleCount / 2];
}

void acceptDistance(float measuredDistance) {
  if (measuredDistance >= MIN_VALID_DISTANCE &&
      measuredDistance <= MAX_VALID_DISTANCE) {

    sensorFault = false;
    consecutiveBadReadings = 0;

    distanceCM = measuredDistance;
    filteredDistanceCM = medianFilter(measuredDistance);
  }
  else {
    consecutiveBadReadings++;

    if (consecutiveBadReadings >= SENSOR_BAD_LIMIT)
      sensorFault = true;
  }
}

void failUltrasonicMeasurement() {
  consecutiveBadReadings++;

  if (consecutiveBadReadings >= SENSOR_BAD_LIMIT)
    sensorFault = true;
}

void updateUltrasonicDriver() {
  if (ultrasonicPhase == US_IDLE)
    return;

  noInterrupts();
  bool received = echoReceived;
  unsigned long duration = echoDurationUs;
  interrupts();

  if (received) {
    ultrasonicPhase = US_IDLE;

    float measuredDistance =
        duration * 0.0343f / 2.0f;

    acceptDistance(measuredDistance);
    return;
  }

  if ((long)(micros() - echoDeadlineUs) >= 0) {
    ultrasonicPhase = US_IDLE;
    failUltrasonicMeasurement();
  }
}

// =====================================================
// PERSON DETECTION
// =====================================================

void updatePersonDetection() {
  if (sensorFault)
    return;

  unsigned long now = millis();

  bool closeObject =
      filteredDistanceCM >= MIN_VALID_DISTANCE &&
      filteredDistanceCM <= PERSON_ENTER_DISTANCE;

  if (!personDetected) {

    if (closeObject) {
      if (enterConfirm < ENTER_CONFIRM_COUNT)
        enterConfirm++;

      if (enterConfirm >= ENTER_CONFIRM_COUNT) {
        personDetected = true;
        lastPersonSeenMs = now;
        enterConfirm = 0;
        exitConfirm = 0;

        Serial.println("[OBS] NEW PERSON DETECTED");
        pushEvent(EVENT_PERSON_ENTERED);
      }
    }
    else {
      enterConfirm = 0;
    }

  }
  else {

    // Person is currently detected.
    if (closeObject) {
      // Refresh presence timer whenever the person is seen.
      lastPersonSeenMs = now;
      exitConfirm = 0;
    }
    else {
      if (exitConfirm < EXIT_CONFIRM_COUNT)
        exitConfirm++;

      // During servo scanning, the sensor may temporarily lose
      // the person. Do NOT immediately count that as OUT.
      if (now - lastPersonSeenMs >= PERSON_LOST_HOLD_MS &&
          exitConfirm >= EXIT_CONFIRM_COUNT) {

        personDetected = false;
        exitConfirm = 0;
        enterConfirm = 0;

        Serial.println("[OBS] PERSON LEFT");
        pushEvent(EVENT_PERSON_LEFT);
      }
    }
  }
}

// =====================================================
// OBSERVATION MODE DETECTION
// =====================================================

void registerObservationEntry() {

  unsigned long now = millis();

  observationPersonPresent = true;
  observationLastSeenMs = now;
  observationLostSinceMs = 0;
  observationLostConfirm = 0;

  observationDetections++;
  observationCurrent++;

  if (observationCurrent > observationMaxSimultaneous)
    observationMaxSimultaneous = observationCurrent;

  lastObservationEntryMs = now;

  // Radar mode:
  // Keep sweeping so the radar can map the object
  // across different angles.
  if (currentMode == MODE_OBSERVATION) {
    motionMode = MOTION_SCAN;
  }

  Serial.print("[OBS] ENTRY #");
  Serial.println(observationDetections);

  Serial.print("[OBS] CURRENT=");
  Serial.println(observationCurrent);
}

void registerObservationExit() {

  observationPersonPresent = false;
  observationLostSinceMs = 0;
  observationSeenConfirm = 0;
  observationLostConfirm = 0;

  if (observationCurrent > 0)
    observationCurrent--;

  observationExits++;

  Serial.print("[OBS] EXIT #");
  Serial.println(observationExits);

  Serial.print("[OBS] CURRENT=");
  Serial.println(observationCurrent);

  // Person has left: start a new complete observation sweep.
  if (currentMode == MODE_OBSERVATION) {
    scanDirection = 1;
    motionMode = MOTION_SCAN;
    targetServoAngle = OBS_SCAN_MIN;
  }
}

void updateObservationDetection() {

  if (currentMode != MODE_OBSERVATION || sensorFault)
    return;

  unsigned long now = millis();

  bool objectInObservationZone =
    filteredDistanceCM >= OBS_MIN_DISTANCE &&
    filteredDistanceCM <= OBS_MAX_DISTANCE;

  if (objectInObservationZone) {

    observationLastSeenMs = now;
    observationLostSinceMs = 0;
    observationLostConfirm = 0;

    if (!observationPersonPresent) {

      if (observationSeenConfirm < OBS_SEEN_CONFIRM)
        observationSeenConfirm++;

      if (observationSeenConfirm >= OBS_SEEN_CONFIRM) {

        // This also counts a person who was already present
        // when Observation Mode was started.
        registerObservationEntry();
      }

    } else {

      observationSeenConfirm = 0;
    }

  } else {

    observationSeenConfirm = 0;

    if (observationPersonPresent) {

      if (observationLostSinceMs == 0)
        observationLostSinceMs = now;

      if (observationLostConfirm < OBS_LOST_CONFIRM)
        observationLostConfirm++;

      // Ignore temporary losses caused by servo scanning.
      if (now - observationLastSeenMs >= OBS_LOST_TIMEOUT_MS &&
          observationLostConfirm >= OBS_LOST_CONFIRM) {

        registerObservationExit();
      }

    } else {

      observationLostSinceMs = 0;
      observationLostConfirm = 0;
    }
  }
}

// =====================================================
// SENSOR MANAGER
// =====================================================

void updateSensorManager() {
  if (ultrasonicPhase == US_IDLE)
    startUltrasonicMeasurement();

  updateUltrasonicDriver();
  updateDistanceLEDs(filteredDistanceCM);

  if (currentMode == MODE_OBSERVATION)
    updateObservationDetection();
  else
    updatePersonDetection();
}

// =====================================================
// TOUCH
// =====================================================

void updateTouchDriver() {
  unsigned long now = millis();

  rawTouch = (digitalRead(TOUCH_PIN) == HIGH);

  // Detect a change in the RAW signal and restart debounce timer.
  if (rawTouch != lastRawTouch) {
    lastRawTouch = rawTouch;
    rawTouchChangedMs = now;
  }

  // Accept raw signal as stable only after debounce time.
  if (rawTouch != stableTouch &&
      now - rawTouchChangedMs >= TOUCH_DEBOUNCE_MS) {

    stableTouch = rawTouch;

    if (stableTouch) {
      touchPressedMs = now;
      longPressHandled = false;

      // FIX: LOVE starts immediately on touch.
      if (currentMode == MODE_NORMAL)
        pushEvent(EVENT_TOUCH_PRESSED);
    }
  }

  // Long press toggles mode.
  if (stableTouch &&
      !longPressHandled &&
      now - touchPressedMs >= LONG_PRESS_MS) {

    longPressHandled = true;

    if (currentMode == MODE_NORMAL) {
      currentMode = MODE_OBSERVATION;

      // Reset observation counters whenever Observation Mode starts.
      observationDetections = 0;
      observationExits = 0;
      observationCurrent = 0;
      observationMaxSimultaneous = 0;
      lastObservationEntryMs = 0;

      observationPersonPresent = false;
      observationLastSeenMs = millis();
      observationLostSinceMs = 0;
      observationSeenConfirm = 0;
      observationLostConfirm = 0;

      // Start every observation sweep from the left end.
      scanDirection = 1;
      motionMode = MOTION_SCAN;
      targetServoAngle = OBS_SCAN_MIN;

      Serial.println();
      Serial.println("========== OBSERVATION START ==========");
      Serial.println("Counters reset");
      Serial.println("=======================================");

    }
    else {
      currentMode = MODE_NORMAL;

      setState(personDetected ? STATE_HAPPY : STATE_SLEEPY);

      Serial.println("========== NORMAL MODE ==========");
    }

    pushEvent(EVENT_MODE_CHANGED);
  }

  // Release after a short touch.
  if (!stableTouch && longPressHandled) {
    // Long press already handled; nothing else to do.
  }
}

// =====================================================
// EVENTS
// =====================================================

void handleEvent(const Event &event) {

  // Observation Mode has its own scan-aware detection logic.
  // Person counting is handled by updateObservationDetection().
  if (currentMode == MODE_OBSERVATION)
    return;

  // ---------- NORMAL MODE ----------
  switch (event.type) {

    case EVENT_PERSON_ENTERED:
      setState(STATE_HAPPY);
      break;

    case EVENT_PERSON_LEFT:
      setState(STATE_SLEEPY);
      break;

    case EVENT_TOUCH_PRESSED:
      setState(STATE_LOVE);
      break;

    default:
      break;
  }
}

void updateEventManager() {
  Event event;

  uint8_t processed = 0;

  while (processed < 8 && popEvent(event)) {
    handleEvent(event);
    processed++;
  }

  if (currentMode != MODE_NORMAL)
    return;

  // LOVE while touched.
  if (stableTouch && !longPressHandled) {
    if (currentState != STATE_LOVE)
      setState(STATE_LOVE);
  }

  // Return from LOVE.
  if (!stableTouch &&
      currentState == STATE_LOVE &&
      (long)(millis() - loveUntilMs) >= 0) {

    setState(personDetected ?
             STATE_HAPPY :
             STATE_SLEEPY);
  }

  // Normal person tracking.
  if (!stableTouch &&
      currentState != STATE_LOVE) {

    if (personDetected &&
        currentState != STATE_HAPPY) {

      setState(STATE_HAPPY);
    }

    if (!personDetected &&
        currentState != STATE_SLEEPY) {

      setState(STATE_SLEEPY);
    }
  }
}

// =====================================================
// OLED EXPRESSIONS
// =====================================================

void drawHappyFrame() {

  display.clearDisplay();

  // HAPPY animation grows the smile gradually.
  const int w = happyWidth;
  const int cx = 64;

  // -----------------------------
  // LEFT SIDE
  // -----------------------------

  display.drawLine(
      cx - w, 25,
      cx - w + 5, 31,
      SSD1306_WHITE
  );

  display.drawLine(
      cx - w + 5, 31,
      cx - w + 12, 36,
      SSD1306_WHITE
  );

  display.drawLine(
      cx - w + 12, 36,
      cx - w + 20, 40,
      SSD1306_WHITE
  );

  display.drawLine(
      cx - w + 20, 40,
      cx - w + 28, 42,
      SSD1306_WHITE
  );

  display.drawLine(
      cx - w + 28, 42,
      cx, 44,
      SSD1306_WHITE
  );

  // -----------------------------
  // RIGHT SIDE
  // -----------------------------

  display.drawLine(
      cx, 44,
      cx + w - 28, 42,
      SSD1306_WHITE
  );

  display.drawLine(
      cx + w - 28, 42,
      cx + w - 20, 40,
      SSD1306_WHITE
  );

  display.drawLine(
      cx + w - 20, 40,
      cx + w - 12, 36,
      SSD1306_WHITE
  );

  display.drawLine(
      cx + w - 12, 36,
      cx + w - 5, 31,
      SSD1306_WHITE
  );

  display.drawLine(
      cx + w - 5, 31,
      cx + w, 25,
      SSD1306_WHITE
  );

  display.display();
}

void drawSleepyFrame() {
  display.clearDisplay();

  display.drawLine(43,39,49,42,SSD1306_WHITE);
  display.drawLine(49,42,56,44,SSD1306_WHITE);
  display.drawLine(56,44,64,45,SSD1306_WHITE);
  display.drawLine(64,45,72,44,SSD1306_WHITE);
  display.drawLine(72,44,79,42,SSD1306_WHITE);
  display.drawLine(79,42,85,39,SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(80+sleepyFrame,22-sleepyFrame);
  display.print("Z");

  display.setCursor(91+sleepyFrame,14-sleepyFrame);
  display.print("Z");

  display.display();
}

void drawLoveFrame() {
  display.clearDisplay();

  int s = loveFrame;
  int cx = 64;

  display.drawLine(cx,49+s,56-s,41,SSD1306_WHITE);
  display.drawLine(56-s,41,48-s,33,SSD1306_WHITE);
  display.drawLine(48-s,33,47-s,27,SSD1306_WHITE);
  display.drawLine(47-s,27,49-s,22,SSD1306_WHITE);
  display.drawLine(49-s,22,54-s,20,SSD1306_WHITE);
  display.drawLine(54-s,20,59-s,22,SSD1306_WHITE);
  display.drawLine(59-s,22,cx,31,SSD1306_WHITE);

  display.drawLine(cx,31,69+s,22,SSD1306_WHITE);
  display.drawLine(69+s,22,74+s,20,SSD1306_WHITE);
  display.drawLine(74+s,20,79+s,22,SSD1306_WHITE);
  display.drawLine(79+s,22,81+s,27,SSD1306_WHITE);
  display.drawLine(81+s,27,80+s,33,SSD1306_WHITE);
  display.drawLine(80+s,33,72+s,41,SSD1306_WHITE);
  display.drawLine(72+s,41,cx,49+s,SSD1306_WHITE);

  display.display();
}

// ---------- Observation screen ----------

void drawObservationScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(31,1);
  display.print("OBSERVING");

  // THIS is the important number:
  // total distinct detection episodes.
  display.setTextSize(2);

  if (observationDetections < 10) {
    display.setCursor(43,19);
  }
  else {
    display.setCursor(37,19);
  }

  display.print(observationDetections);
  display.print(" PERSON");

  if (observationDetections != 1)
    display.print("S");

  display.setTextSize(1);
  display.setCursor(31,39);
  display.print("DETECTED");

  display.setCursor(8,54);
  display.print("IN:");
  display.print(observationDetections);

  display.setCursor(70,54);
  display.print("OUT:");
  display.print(observationExits);

  display.display();
}

void updateExpressionEngine() {

  unsigned long now = millis();

  if (!expressionFirstFrame &&
      now - lastDisplayFrameMs < DISPLAY_PERIOD_MS)
    return;

  lastDisplayFrameMs = now;
  expressionFirstFrame = false;

  if (currentMode == MODE_OBSERVATION) {
    drawObservationScreen();
    return;
  }

  switch (currentState) {

    case STATE_SLEEPY:
      drawSleepyFrame();
      sleepyFrame++;
      if (sleepyFrame >= 3)
        sleepyFrame = 0;
      break;

    case STATE_HAPPY:

      drawHappyFrame();

      happyWidth += 1;

      if (happyWidth > 40)
        happyWidth = 40;

      break;

    case STATE_LOVE:
      drawLoveFrame();
      loveFrame++;
      if (loveFrame > 4)
        loveFrame = 0;
      break;
  }
}

// =====================================================
// SERVO MOTION
// =====================================================

void updateMotionEngine() {

  unsigned long now = millis();

  if (currentMode == MODE_OBSERVATION) {

  // Radar demonstration:
  // Always keep scanning, even when an object is detected.
  motionMode = MOTION_SCAN;
}

  if (motionMode == MOTION_CENTER) {

    setServoTarget(SERVO_CENTER);
  }

  else if (motionMode == MOTION_SCAN) {

    if (now - lastScanMs >= SCAN_INTERVAL_MS) {

      lastScanMs = now;

      targetServoAngle +=
          scanDirection * SCAN_STEP;

      int scanMin = (currentMode == MODE_OBSERVATION)
                         ? OBS_SCAN_MIN : SCAN_MIN;
      int scanMax = (currentMode == MODE_OBSERVATION)
                         ? OBS_SCAN_MAX : SCAN_MAX;

      if (targetServoAngle >= scanMax) {
        targetServoAngle = scanMax;
        scanDirection = -1;
      }

      if (targetServoAngle <= scanMin) {
        targetServoAngle = scanMin;
        scanDirection = 1;
      }
    }
  }

  else if (motionMode == MOTION_LOVE) {

    if (now - lastLoveMotionMs >= LOVE_INTERVAL_MS) {

      lastLoveMotionMs = now;

      targetServoAngle +=
          loveDirection * LOVE_STEP;

      if (targetServoAngle >= LOVE_MAX) {
        targetServoAngle = LOVE_MAX;
        loveDirection = -1;
      }

      if (targetServoAngle <= LOVE_MIN) {
        targetServoAngle = LOVE_MIN;
        loveDirection = 1;
      }
    }
  }

  if (now - lastServoUpdateMs >= SERVO_PERIOD_MS) {

    lastServoUpdateMs = now;

    if (currentServoAngle < targetServoAngle)
      currentServoAngle += 2;

    else if (currentServoAngle > targetServoAngle)
      currentServoAngle -= 2;

    currentServoAngle =
        constrain(currentServoAngle,
                  SERVO_MIN,
                  SERVO_MAX);

    evaServo.write(currentServoAngle);
  }
}

// =====================================================
// TELEMETRY
// =====================================================

void printTelemetry() {

  Serial.print("MODE=");
  Serial.print(currentMode == MODE_NORMAL ?
               "NORMAL" : "OBSERVATION");

  Serial.print(" | D=");
  if (sensorFault)
    Serial.print("FAULT");
  else {
    Serial.print(filteredDistanceCM,1);
    Serial.print("cm");
  }

  Serial.print(" | PERSON=");
  Serial.print(personDetected ? "YES" : "NO");

  Serial.print(" | SERVO=");
  Serial.print(currentServoAngle);

  Serial.print(" | DETECTED=");
  Serial.print(observationDetections);

  Serial.print(" | CURRENT=");
  Serial.print(observationCurrent);

  Serial.print(" | OUT=");
  Serial.print(observationExits);

  Serial.print(" | TARGET=");
  Serial.println(targetServoAngle);
}

// =====================================================
// INIT
// =====================================================

void initDisplay() {

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC,
                     OLED_ADDRESS)) {

    Serial.println("OLED ERROR");

    while (true)
      delay(100);
  }

  display.clearDisplay();
  display.display();
}

void initSensor() {

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);

  attachInterrupt(
      digitalPinToInterrupt(ECHO_PIN),
      echoISR,
      CHANGE);
}

void initTouch() {

  pinMode(TOUCH_PIN, INPUT);

  rawTouch = digitalRead(TOUCH_PIN) == HIGH;
  lastRawTouch = rawTouch;
  stableTouch = rawTouch;

  rawTouchChangedMs = millis();
}

void initServo() {

  evaServo.setPeriodHertz(50);

  if (!evaServo.attach(SERVO_PIN, 500, 2400))
    Serial.println("SERVO ATTACH ERROR");

  evaServo.write(SERVO_CENTER);

  currentServoAngle = SERVO_CENTER;
  targetServoAngle = SERVO_CENTER;

  // Startup hardware test: center -> left -> right -> center.
  delay(300);
  evaServo.write(70);
  delay(350);
  evaServo.write(110);
  delay(350);
  evaServo.write(SERVO_CENTER);
  delay(250);
}

void updateDistanceLEDs(float distanceCM) {

  // Turn all LEDs OFF first
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
  digitalWrite(LED5_PIN, LOW);

  // Ignore invalid readings
  if (distanceCM < MIN_VALID_DISTANCE ||
      distanceCM > MAX_VALID_DISTANCE) {
    return;
  }

  // Distance bar
  if (distanceCM <= 100.0) {
    digitalWrite(LED1_PIN, HIGH);
  }

  if (distanceCM <= 80.0) {
    digitalWrite(LED2_PIN, HIGH);
  }

  if (distanceCM <= 60.0) {
    digitalWrite(LED3_PIN, HIGH);
  }

  if (distanceCM <= 40.0) {
    digitalWrite(LED4_PIN, HIGH);
  }

  if (distanceCM <= 20.0) {
    digitalWrite(LED5_PIN, HIGH);
  }
}
// =====================================================
// SCHEDULER
// =====================================================

unsigned long lastSensorTaskMs = 0;
unsigned long lastTouchTaskMs = 0;
unsigned long lastTelemetryMs = 0;

// =====================================================
// RADAR SERIAL OUTPUT
// =====================================================

unsigned long lastRadarSendMs = 0;

const unsigned long RADAR_SEND_PERIOD_MS = 60;  // 20 Hz

// =====================================================
// RADAR DATA OUTPUT
// =====================================================
//
// Format sent to Processing:
//
// RADAR,angle,distance,detected
//
// Example:
// RADAR,75,42.3,1
//
// angle    = servo angle in degrees
// distance = filtered ultrasonic distance in cm
// detected = 1 if object is inside observation zone
//            0 otherwise
//

void updateRadarOutput() {

  unsigned long now = millis();

  if (now - lastRadarSendMs < RADAR_SEND_PERIOD_MS)
    return;

  lastRadarSendMs = now;

  // Radar is most useful in Observation Mode.
  if (currentMode != MODE_OBSERVATION)
    return;

  Serial.print("RADAR,");
  Serial.print(currentServoAngle);
  Serial.print(",");

  if (sensorFault ||
      filteredDistanceCM < MIN_VALID_DISTANCE ||
      filteredDistanceCM > MAX_VALID_DISTANCE) {

    Serial.print("-1");
    Serial.print(",0");
  }

  else {

    Serial.print(filteredDistanceCM, 1);
    Serial.print(",");

    bool detected =
      filteredDistanceCM >= OBS_MIN_DISTANCE &&
      filteredDistanceCM <= OBS_MAX_DISTANCE;

    Serial.print(detected ? 1 : 0);
  }

  Serial.println();
}

void schedulerUpdate() {

  unsigned long now = millis();

  // ---------- SENSOR ----------
  if (now - lastSensorTaskMs >= SENSOR_PERIOD_MS) {

    lastSensorTaskMs = now;
    updateSensorManager();

  } else {

    // Continue waiting for HC-SR04 echo
    updateUltrasonicDriver();
  }

  // ---------- TOUCH ----------
  if (now - lastTouchTaskMs >= TOUCH_PERIOD_MS) {

    lastTouchTaskMs = now;
    updateTouchDriver();
  }

  // ---------- EVENTS ----------
  updateEventManager();

  // ---------- SERVO ----------
  if (now - lastServoUpdateMs >= SERVO_PERIOD_MS) {
    updateMotionEngine();
  }

  // ---------- OLED ----------
  if (now - lastDisplayFrameMs >= DISPLAY_PERIOD_MS) {
    updateExpressionEngine();
  }

  // ---------- TELEMETRY ----------
  if (now - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {

    lastTelemetryMs = now;
    printTelemetry();
  }
  // ---------- RADAR ----------
  updateRadarOutput();
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("====================================");
  Serial.println(" EVA 2.0.2");
  Serial.println(" STABLE HAPPY + OBSERVATION SCAN");
  Serial.println("====================================");

  initDisplay();
  initSensor();
  initTouch();
  initServo();

  currentMode = MODE_NORMAL;
  currentState = STATE_SLEEPY;
  previousState = STATE_SLEEPY;

  expressionFirstFrame = true;

  updateExpressionEngine();

  Serial.println("OLED        : OK");
  Serial.println("HC-SR04     : READY");
  Serial.println("TTP223      : READY");
  Serial.println("SERVO       : READY");
  Serial.println("------------------------------------");
  Serial.println("SHORT TOUCH = LOVE");
  Serial.println("LONG TOUCH  = OBSERVATION");
  Serial.println("------------------------------------");


  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  pinMode(LED5_PIN, OUTPUT);

  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
  digitalWrite(LED5_PIN, LOW);
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  schedulerUpdate();
}
