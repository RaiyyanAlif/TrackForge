
/*
  OPERATION 1122 - Arduino Mega Mini Pro
  PID line following + serial commands + line-loss recovery

  Hardware:
    Arduino Mega Mini Pro (ATmega2560)
    TB6612FNG motor driver
    8 analog sensors: A7 (LEFT) -> A0 (RIGHT)
    ESP8266 bridge on Serial1, 9600 baud

  Safety:
    Keep motor battery disconnected during first upload.
    Test motor direction with wheels lifted before floor testing.
*/

// ==================== MOTOR PINS ====================
const uint8_t PWMA = 3;
const uint8_t AIN1 = 4;
const uint8_t AIN2 = 2;

const uint8_t PWMB = 11;
const uint8_t BIN1 = 7;
const uint8_t BIN2 = 6;

const uint8_t STBY = 13;

// Single physical button: D8 connects to GND
const uint8_t BTN_TOGGLE = 8;

// Sensors ordered LEFT -> RIGHT
const uint8_t sensorPins[8] = {
  A7, A6, A5, A4, A3, A2, A1, A0
};

// ==================== CALIBRATION ====================
int sensorMin[8];
int sensorMax[8];

// Keep the polarity from your original sketch.
// Change to false only if testing proves black gives lower readings.
const bool SENSOR_BLACK_IS_HIGH = true;

const unsigned long CALIBRATION_MS = 6000UL;

// ==================== PID ====================
int baseSpeed = 135;
const int MAX_PWM = 220;

float Kp = 0.075f;
float Kd = 0.22f;

int previousError = 0;
bool firstSample = true;

// Sensor position weights, left negative and right positive
const int weights[8] = {
  -3500, -2500, -1500, -500,
   500,  1500,  2500, 3500
};

const int SENSOR_THRESHOLD = 300;
const long LINE_SIGNAL_THRESHOLD = 350;

// ==================== NAVIGATION ====================
bool following = false;
bool recovering = false;

unsigned long lastLineSeenMs = 0;
unsigned long recoveryStartMs = 0;
unsigned long lastButtonMs = 0;

const unsigned long LINE_LOST_DELAY_MS = 150;
const unsigned long RECOVERY_PHASE_MS = 1200;
const unsigned long BUTTON_DEBOUNCE_MS = 250;

// Negative error means line was toward the left.
// Positive error means line was toward the right.
int lastLineDirection = -1;
int recoveryDirection = -1;
uint8_t recoveryPhase = 0;

// ==================== SERIAL ====================
String espBuffer;

const unsigned long SERIAL_BAUD = 9600;

// ==================== MOTORS ====================
void stopMotors() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  digitalWrite(STBY, LOW);
}

void driveOneMotor(
  uint8_t pwmPin,
  uint8_t in1,
  uint8_t in2,
  int speedValue
) {
  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, speedValue);
  } else if (speedValue < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(pwmPin, -speedValue);
  } else {
    analogWrite(pwmPin, 0);
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
}

void setMotors(int leftSpeed, int rightSpeed) {
  digitalWrite(STBY, HIGH);
  driveOneMotor(PWMA, AIN1, AIN2, leftSpeed);
  driveOneMotor(PWMB, BIN1, BIN2, rightSpeed);
}

// ==================== STATUS MESSAGES ====================
void sendMessage(const String &message) {
  // ESP8266 receives these messages from Mega Serial1
  Serial1.println(message);

  // USB diagnostic output; do not read commands from USB Serial
  Serial.println(message);
}

void sendAck(const String &command, const String &detail = "") {
  String message = "ACK:" + command;

  if (detail.length() > 0) {
    message += ":" + detail;
  }

  sendMessage(message);
}

// ==================== SENSOR CALIBRATION ====================
void calibrateSensors() {
  sendMessage("STATUS:CALIBRATION_STARTED");
  Serial.println(
    "Move the sensor array over BOTH black tape and white floor."
  );

  for (uint8_t i = 0; i < 8; i++) {
    sensorMin[i] = 1023;
    sensorMax[i] = 0;
  }

  unsigned long start = millis();

  while (millis() - start < CALIBRATION_MS) {
    for (uint8_t i = 0; i < 8; i++) {
      int value = analogRead(sensorPins[i]);

      if (value < sensorMin[i]) {
        sensorMin[i] = value;
      }

      if (value > sensorMax[i]) {
        sensorMax[i] = value;
      }
    }

    delay(5);
  }

  for (uint8_t i = 0; i < 8; i++) {
    // Preserve the fallback from the original calibration.
    if (sensorMax[i] - sensorMin[i] < 80) {
      sensorMin[i] = 0;
      sensorMax[i] = 1023;

      Serial.print("WARNING: Sensor ");
      Serial.print(i);
      Serial.println(" had insufficient calibration range.");
    }
  }

  Serial.println("Calibration complete.");

  sendMessage("STATUS:CALIBRATION_DONE");

  Serial.println("Starting in 3...");
  delay(700);
  Serial.println("Starting in 2...");
  delay(700);
  Serial.println("Starting in 1...");
  delay(700);

  sendMessage("STATUS:READY");
}

// Returns approximately 0 for white and 1000 for black.
int readBlackness(uint8_t index) {
  int raw = analogRead(sensorPins[index]);
  int range = sensorMax[index] - sensorMin[index];

  if (range < 1) {
    return 0;
  }

  int normalized =
    (long)(raw - sensorMin[index]) * 1000L / range;

  normalized = constrain(normalized, 0, 1000);

  return SENSOR_BLACK_IS_HIGH
    ? normalized
    : 1000 - normalized;
}

// Returns true if enough black-line signal is detected.
bool getLineError(int &error) {
  long weightedTotal = 0;
  long signalTotal = 0;

  for (uint8_t i = 0; i < 8; i++) {
    int signal = readBlackness(i);

    if (signal > SENSOR_THRESHOLD) {
      weightedTotal += (long)signal * weights[i];
      signalTotal += signal;
    }
  }

  if (signalTotal < LINE_SIGNAL_THRESHOLD) {
    return false;
  }

  error = weightedTotal / signalTotal;
  return true;
}

// ==================== FOLLOWING CONTROL ====================
void startFollowing(const String &source) {
  recovering = false;
  following = true;
  firstSample = true;
  lastLineSeenMs = millis();

  sendMessage("STATUS:FOLLOWING");
  sendAck("START", source);
}

void stopFollowing(const String &source) {
  following = false;
  recovering = false;
  stopMotors();

  sendMessage("STATUS:STOPPED");
  sendAck("STOP", source);
}

void followLineStep() {
  int error = 0;

  if (!getLineError(error)) {
    // Never continue driving forward blindly.
    setMotors(0, 0);

    if (!recovering &&
        millis() - lastLineSeenMs >= LINE_LOST_DELAY_MS) {

      recovering = true;
      recoveryStartMs = millis();
      recoveryPhase = 0;

      // First search toward the last observed line direction.
      recoveryDirection = lastLineDirection;

      sendMessage("STATUS:LINE_LOST");
      sendMessage("STATUS:RECOVERY_PHASE_1");
    }

    if (recovering) {
      unsigned long elapsed = millis() - recoveryStartMs;

      if (elapsed >= RECOVERY_PHASE_MS &&
          recoveryPhase == 0) {

        // Try the opposite direction.
        recoveryPhase = 1;
        recoveryDirection = -lastLineDirection;

        sendMessage("STATUS:RECOVERY_PHASE_2");
      }

      if (elapsed >= 2UL * RECOVERY_PHASE_MS) {
        recovering = false;
        following = false;
        stopMotors();

        sendMessage("ERROR:LINE_RECOVERY_FAILED");
        sendAck("STOP", "LINE_NOT_FOUND");
        return;
      }

      // Rotate in place. Verify the physical direction with wheels
      // lifted before testing on the floor.
      if (recoveryDirection < 0) {
        setMotors(-baseSpeed, baseSpeed);
      } else {
        setMotors(baseSpeed, -baseSpeed);
      }
    }

    return;
  }

  // A valid line has been detected.
  lastLineSeenMs = millis();

  if (recovering) {
    recovering = false;
    firstSample = true;
    stopMotors();

    sendMessage("STATUS:LINE_RECOVERED");
  }

  // Remember which side the line was on for future recovery.
  if (error < -150) {
    lastLineDirection = -1;
  } else if (error > 150) {
    lastLineDirection = 1;
  }

  if (firstSample) {
    previousError = error;
    firstSample = false;
  }

  int derivative = error - previousError;
  previousError = error;

  int correction = (int)(
    Kp * error + Kd * derivative
  );

  int leftSpeed = constrain(
    baseSpeed + correction, 0, MAX_PWM
  );

  int rightSpeed = constrain(
    baseSpeed - correction, 0, MAX_PWM
  );

  setMotors(leftSpeed, rightSpeed);
}

// ==================== COMMAND HANDLER ====================
void processCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) {
    return;
  }

  Serial.print("Command received: ");
  Serial.println(command);

  if (command == "STOP" ||
      command == "HALT" ||
      command == "ESTOP" ||
      command == "EMERGENCY_STOP") {

    stopFollowing("COMMAND");

  } else if (command == "START" ||
             command == "BEGIN" ||
             command == "RESUME" ||
             command == "CONTINUE" ||
             command == "CONTINUE FOLLOWING") {

    startFollowing("COMMAND");

  } else if (command == "STATUS") {

    sendAck(
      "STATUS",
      following
        ? (recovering ? "RECOVERING" : "RUNNING")
        : "STOPPED"
    );

  } else if (command == "CAPTURE") {

    // Stop before the laptop captures the camera image.
    stopFollowing("CAPTURE");
    sendAck("CAPTURE", "LAPTOP_CAPTURE");

  } else if (command == "TRANSMIT" ||
             command == "SEND DATA") {

    // Mission data will be collected/saved on the laptop.
    stopFollowing("TRANSMIT");
    sendAck("TRANSMIT", "LAPTOP_SAVE");

  } else if (command.startsWith("SPEED ")) {

    int requested = command.substring(6).toInt();
    baseSpeed = constrain(requested, 80, 170);

    sendAck("SPEED", String(baseSpeed));

  } else {

    sendAck("UNKNOWN", command);
  }
}

void readESPCommands() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();

    if (c == '\n') {
      processCommand(espBuffer);
      espBuffer = "";

    } else if (c >= 32 && c < 127) {
      if (espBuffer.length() < 80) {
        espBuffer += c;
      } else {
        espBuffer = "";
        sendMessage("ERROR:COMMAND_TOO_LONG");
      }
    }
  }
}

// ==================== PHYSICAL BUTTON ====================
bool buttonWasPressed = false;

void checkButtons() {
  bool pressed = (digitalRead(BTN_TOGGLE) == LOW);

  if (pressed && !buttonWasPressed &&
      millis() - lastButtonMs >= BUTTON_DEBOUNCE_MS) {
    lastButtonMs = millis();
    buttonWasPressed = true;

    if (following) {
      stopFollowing("PHYSICAL_BUTTON");
    } else {
      startFollowing("PHYSICAL_BUTTON");
    }
  }

  if (!pressed) {
    buttonWasPressed = false;
  }
}

// ==================== SETUP / LOOP ====================
void setup() {
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);

  pinMode(BTN_TOGGLE, INPUT_PULLUP);

  stopMotors();

  Serial.begin(115200);   // USB diagnostic output
  Serial1.begin(SERIAL_BAUD);  // Mega D18 TX1, D19 RX1

  delay(300);

  Serial.println();
  Serial.println("OPERATION 1122 - MEGA CONTROLLER");
  sendMessage("STATUS:BOOT");

  calibrateSensors();
}

void loop() {
  readESPCommands();
  checkButtons();

  if (following) {
    followLineStep();
  } else {
    stopMotors();
  }
}

