
/* E-STOP */
const int ESTOP_NORMALLY_CLOSED = 7;
const int ESTOP_NORMALLY_OPEN = 8;

/* CONTROL PANEL INPUTS */
const int BEGIN_SIGNAL = 12;
const int END_SIGNAL = 11;
const int ESTOP_RESET_SIGNAL = 10;

/* MOTORS */
const int BASE_ROTATION_MOTOR_PWM = 3;
const int UPPER_ROTATION_MOTOR_PWM = 5;
const int LINEAR_ACTUATOR_PWM = 2;      // Accurate as of 3/20
const int LINEAR_ACTUATOR_DIR_ONE = 6;  // Accurate as of 3/22
const int LINEAR_ACTUATOR_DIR_TWO = 4;  // Accurate as of 3/22

/* OTHER OUTPUTS */
const int ESTOP_TRIGGERED_INDICATOR = 9;
const int READY_INDICATOR = 13;
const int ENABLE_LIGHTING = 14;

/* STATE MACHINE */
enum State {
  initializing,
  ready,
  running,
  windingDown,
  emergencyStopped
};
State state = initializing;
State lastState = emergencyStopped;

/* ESTOP RESET VALUES */
int estopResetBeginTime = -1;

/* MOTOR SPEED AND POSITION VALUES: */
unsigned char baseRotationMotorSpeed = 0;
unsigned char upperRotationMotorSpeed = 0;

enum LinearActuatorDirection { up, down };
LinearActuatorDirection linearActuatorDirection = down;
unsigned char linearActuatorSpeed = 0;
int linearActuatorPosition = 0;
int linearActuatorLastRunTime = -1;

/* OTHER OUTPUT VALUES */
bool lightingEnabled = false;
bool readyIndicator = false;
bool estopTriggeredIndicator = false;

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(ESTOP_NORMALLY_CLOSED, INPUT);
  pinMode(ESTOP_NORMALLY_OPEN, INPUT);

  pinMode(BEGIN_SIGNAL, INPUT);
  pinMode(END_SIGNAL, INPUT);
  pinMode(ESTOP_RESET_SIGNAL, INPUT);

  pinMode(BASE_ROTATION_MOTOR_PWM, OUTPUT);
  pinMode(UPPER_ROTATION_MOTOR_PWM, OUTPUT);
  pinMode(LINEAR_ACTUATOR_PWM, OUTPUT);
  pinMode(LINEAR_ACTUATOR_DIR_ONE, OUTPUT);
  pinMode(LINEAR_ACTUATOR_DIR_TWO, OUTPUT);

  pinMode(ESTOP_TRIGGERED_INDICATOR, OUTPUT);
  pinMode(READY_INDICATOR, OUTPUT);
  pinMode(ENABLE_LIGHTING, OUTPUT);

  Serial.begin(115200);
}

// the loop function runs over and over again forever
void loop() {
  checkForEStop();
  writeMotorSpeeds();
  writeLighting();
  switch(state) {
    case emergencyStopped: emergencyStop(); break;
    case initializing: initialize(); break;
    case ready: runReady(); break;
    case running: runStandard(); break;
    case windingDown: windDown(); break;
  }
}

void checkForEStop() {
  if (state == emergencyStopped) { return; }

  if (digitalRead(ESTOP_NORMALLY_CLOSED) == 1) {
    Serial.println("EMERGENCY: E-STOP NORMALLY CLOSED WAS DETECTED OPEN!");
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = 0;
    linearActuatorSpeed = 0;
    state = emergencyStopped;
  }

  if (digitalRead(ESTOP_NORMALLY_OPEN) == 1) {
    Serial.println("EMERGENCY: E-STOP NORMALLY OPEN WAS DETECTED CLOSED!");
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = 0;
    linearActuatorSpeed = 0;
    state = emergencyStopped;
  }
}

void writeMotorSpeeds() {
  if (linearActuatorDirection == up) {
    digitalWrite(LINEAR_ACTUATOR_DIR_ONE, HIGH);
    digitalWrite(LINEAR_ACTUATOR_DIR_TWO, LOW);
  } else {
    digitalWrite(LINEAR_ACTUATOR_DIR_ONE, LOW);
    digitalWrite(LINEAR_ACTUATOR_DIR_TWO, HIGH);
  }

  analogWrite(UPPER_ROTATION_MOTOR_PWM, upperRotationMotorSpeed);
  analogWrite(BASE_ROTATION_MOTOR_PWM, baseRotationMotorSpeed);
  analogWrite(LINEAR_ACTUATOR_PWM, linearActuatorSpeed);
}

void writeLighting() {
  digitalWrite(ENABLE_LIGHTING, lightingEnabled ? HIGH : LOW);
  digitalWrite(READY_INDICATOR, readyIndicator ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, readyIndicator ? HIGH : LOW);
  digitalWrite(ESTOP_TRIGGERED_INDICATOR, estopTriggeredIndicator ? HIGH : LOW);
}

void emergencyStop() {
  if (lastState != state) {
    Serial.println("EMERGENCY: E-Stop triggered. Waiting for reset...");
    lastState = emergencyStopped;
  }

  if (digitalRead(ESTOP_NORMALLY_CLOSED) == 1 || digitalRead(ESTOP_NORMALLY_OPEN) == 1) { return; }

  if (digitalRead(ESTOP_RESET_SIGNAL) == 0) {
    estopResetBeginTime = -1;
    return;
  }

  if (estopResetBeginTime == -1) {
    estopResetBeginTime = millis();
  }

  if (millis() - estopResetBeginTime >= 5000) {
    Serial.println("EMERGENCY: E-Stop reset triggered. Resuming normal operation.");
    state = initializing;
    return;
  }
}

void initialize() {
  if (lastState != state) {
    Serial.println("Beginning initialization ...");
    lastState = initializing;

    linearActuatorLastRunTime = millis();
    linearActuatorDirection = down;
    linearActuatorSpeed = 255;
  }

  if (millis() - linearActuatorLastRunTime > 10000 || digitalRead(BEGIN_SIGNAL) == 1) {
    Serial.println("Initialization complete.");
    state = ready;
  }
}

void runReady() {
  if (lastState != state) {
    Serial.println("Waiting for a begin signal...");
    readyIndicator = true;
    lastState = ready;
  }
  if (digitalRead(BEGIN_SIGNAL) == 0) { return; }

  Serial.println("Begin signal detected.");
  readyIndicator = false;
  state = running;
}

int timeBegan = 0;
void runStandard() {
  if (lastState != state) {
    Serial.println("Beginning a ride cycle.");
    lastState = running;
    timeBegan = millis();
  }

  if (digitalRead(END_SIGNAL) == 1) {
    state = windingDown;
    return;
  }
  
  // this will be its own state machine at some point

  int elapsedTime = millis() - timeBegan;
  if (elapsedTime < 1000) {
    baseRotationMotorSpeed = 255;
    upperRotationMotorSpeed = 0;
  } else if (elapsedTime < 2000) {
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = 255;
  } else if (elapsedTime < 3000) {
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = 0;
  } else if (elapsedTime < 8000) {
    baseRotationMotorSpeed = min(255, (elapsedTime - 3000) / 19);
    upperRotationMotorSpeed = 0;
  } else if (elapsedTime < 13000) {
    baseRotationMotorSpeed = max(0, 255 - (elapsedTime - 8000) / 19);
    upperRotationMotorSpeed = 0;
  } else if (elapsedTime < 18000) {
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = min(255, (elapsedTime - 13000) / 19);
  } else if (elapsedTime < 23000) {
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = max(0, 255 - (elapsedTime - 18000) / 19);
  } else if (elapsedTime < 28000) {
    linearActuatorDirection = up;
    linearActuatorSpeed = 255;
  } else if (elapsedTime < 32000) {
    linearActuatorDirection = down;
    linearActuatorSpeed = 255;
  } else {
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = 0;
    linearActuatorSpeed = 0;
    state = windingDown;
  }

}

int lastWindDownMotorTick = 0;
bool linearActuatorReturnBegan = false;
void windDown() {
  if (lastState != state) {
    Serial.println("Beginning ride-wide wind down...");
    lastState = windingDown;
  }

  bool shouldWindDownMotors = (baseRotationMotorSpeed != 0 || upperRotationMotorSpeed != 0) && millis() - lastWindDownMotorTick > 50;
  if (shouldWindDownMotors) {
    lastWindDownMotorTick = millis();
    baseRotationMotorSpeed = max(0, baseRotationMotorSpeed - 1);
    baseRotationMotorSpeed = max(0, upperRotationMotorSpeed - 1);
  }

  bool shouldWindDownActuator = linearActuatorPosition != 0;
  if (shouldWindDownActuator) {
    if (!linearActuatorReturnBegan) {
      linearActuatorLastRunTime = millis();
      linearActuatorDirection = down;
      linearActuatorSpeed = 255;
      linearActuatorReturnBegan = true;
    }

    if (linearActuatorPosition < millis() - linearActuatorLastRunTime - 1000) {
      linearActuatorPosition = 0;
      linearActuatorReturnBegan = false;
      linearActuatorSpeed = 0;
    }
  }

  if (!shouldWindDownMotors && !shouldWindDownActuator) {
    state = ready;
  }
}