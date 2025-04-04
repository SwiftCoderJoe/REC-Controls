/* E-STOP */
const int ESTOP_NORMALLY_HIGH = 11;
// const int ESTOP_NORMALLY_LOW = 12; // Testing without second relay to microcontroller

/* CONTROL PANEL INPUTS */
const int BEGIN_SIGNAL = 7;
const int END_SIGNAL = 8;
const int ESTOP_RESET_SIGNAL = 10;

/* MOTORS */
const int BASE_ROTATION_MOTOR_PWM = 5;  // Accurate as of 3/23
const int UPPER_ROTATION_MOTOR_PWM = 3; // Accurate as of 3/23
const int LINEAR_ACTUATOR_PWM = 2;      // Accurate as of 3/23
const int LINEAR_ACTUATOR_DIR_ONE = 6;  // Accurate as of 3/23
const int LINEAR_ACTUATOR_DIR_TWO = 4;  // Accurate as of 3/23

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
bool contRun = false;

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

/* RIDE PROFILE STATE MACHINE */
enum ProfileState {
  spinUp, // offsets rotation speed with base speed
  spinUpBase,
  spinUpRotation,
  spinDown,
  spinDownBase,
  spinDownRotation,
  liftHinge,
  lowerHinge,
  run,
  done
};

struct RideProfile {
  ProfileState states[20];
  int timings[20];
};

constexpr RideProfile basicProfile = {
  { spinUp, liftHinge, spinUpRotation, run, spinDownRotation, run, spinUpRotation, run, spinDownRotation, lowerHinge, spinDown, done },
  { 10,      6,       6,              10,  10,               5,   10,             10,  6,                6,         5,        1,   }
};

constexpr RideProfile activeProfile = basicProfile;

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
  //pinMode(ESTOP_NORMALLY_LOW, INPUT);
  pinMode(ESTOP_NORMALLY_HIGH, INPUT);

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
  Serial.println("Serial initialized.");
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

  if (digitalRead(ESTOP_NORMALLY_HIGH) == 0) {
    Serial.println("EMERGENCY: E-STOP NORMALLY CLOSED WAS DETECTED OPEN!");
    baseRotationMotorSpeed = 0;
    upperRotationMotorSpeed = 0;
    linearActuatorSpeed = 0;
    state = emergencyStopped;
    contRun = false;
  }

  // if (digitalRead(ESTOP_NORMALLY_LOW) == 1) {
  //   Serial.println("EMERGENCY: E-STOP NORMALLY OPEN WAS DETECTED CLOSED!");
  //   baseRotationMotorSpeed = 0;
  //   upperRotationMotorSpeed = 0;
  //   linearActuatorSpeed = 0;
  //   state = emergencyStopped;
  // }
}

void writeMotorSpeeds() {
  if (linearActuatorDirection == up) {
    digitalWrite(LINEAR_ACTUATOR_DIR_ONE, LOW);
    digitalWrite(LINEAR_ACTUATOR_DIR_TWO, HIGH);
  } else {
    digitalWrite(LINEAR_ACTUATOR_DIR_ONE, HIGH);
    digitalWrite(LINEAR_ACTUATOR_DIR_TWO, LOW);
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

// || digitalRead(ESTOP_NORMALLY_LOW) == 1
  if (digitalRead(ESTOP_NORMALLY_HIGH) == 0) { return; }

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
  if (digitalRead(BEGIN_SIGNAL) == 0 && !contRun) { return; }

  Serial.println("Begin signal detected.");
  contRun = true;
  readyIndicator = false;
  state = running;
}

int timeSwitched = 0;
int rideState = 0;
void runStandard() {
  int currentTime = millis();
  if (lastState != state) {
    Serial.println("Beginning a ride cycle.");
    lastState = running;
    timeSwitched = currentTime;
    rideState = 0;
  }

  // Update ride 
  if (digitalRead(END_SIGNAL) == 1) {
    contRun = false;
    state = windingDown;
    return;
  }

  ProfileState currentOperationMode = activeProfile.states[rideState];
  
  // Update ride state
  if (currentTime - timeSwitched > activeProfile.timings[rideState] * 1000) {
    endOperationMode(currentOperationMode, currentTime);
    timeSwitched = currentTime;
    rideState++;
    currentOperationMode = activeProfile.states[rideState];
    beginOperationMode(currentOperationMode, currentTime);
  }


  runOperationMode(currentOperationMode, currentTime);

}

void beginOperationMode(ProfileState operationMode, int time) {
  switch (operationMode) {
  case spinUp:
  case spinUpBase:
  case spinUpRotation:
    break;

  case spinDown:
  case spinDownBase:
  case spinDownRotation:
    break;

  case liftHinge:
    linearActuatorLastRunTime = time;
    linearActuatorDirection = up;
    linearActuatorSpeed = 255;
    break;
  case lowerHinge:
    linearActuatorLastRunTime = time;
    linearActuatorDirection = down;
    linearActuatorSpeed = 255;
    break;

  case run:
    break;
  
  case done:
    break;
  }
}

int lastMotorTick = -1;
void runOperationMode(ProfileState operationMode, int time) {
  switch (operationMode) {
  case spinUp:
    if (time - lastMotorTick > 40) {
      lastMotorTick = time;
      baseRotationMotorSpeed = constrain(baseRotationMotorSpeed + 1, 0, 255);
      upperRotationMotorSpeed = constrain(upperRotationMotorSpeed + 1, 0, 128);
    }
    break;
  case spinUpBase:
    Serial.println("Unimplemented.");
    break;
  case spinUpRotation:
    if (time - lastMotorTick > 40) {
      lastMotorTick = time;
      upperRotationMotorSpeed = constrain(upperRotationMotorSpeed + 1, 0, 255);
    }
    break;

  case spinDown:
    if (time - lastMotorTick > 40) {
      lastMotorTick = time;
      baseRotationMotorSpeed = constrain(baseRotationMotorSpeed - 1, 0, 255);
      upperRotationMotorSpeed = constrain(upperRotationMotorSpeed - 1, 0, 128);
    }
    break;
  case spinDownBase:
    Serial.println("Unimplemented.");
    break;
  case spinDownRotation:
    if (time - lastMotorTick > 40) {
      lastMotorTick = time;
      upperRotationMotorSpeed = constrain(upperRotationMotorSpeed - 1, 0, 255);
    }
    break;

  case liftHinge:
  case lowerHinge:
    break;

  case run:
    break;
  
  case done:
    state = windingDown;
    break;
  }
}

void endOperationMode(ProfileState operationMode, int time) {
  switch (operationMode) {
  case spinUp:
  case spinUpBase:
  case spinUpRotation:
    break;

  case spinDown:
  case spinDownBase:
  case spinDownRotation:
    break;

  case liftHinge:
    linearActuatorPosition = linearActuatorPosition + (time - linearActuatorLastRunTime);
    linearActuatorSpeed = 0;
    break;
  case lowerHinge:
    linearActuatorPosition = max(0, linearActuatorPosition - (time - linearActuatorLastRunTime - 100));
    linearActuatorSpeed = 0;
    break;

  case run:
    break;
  
  case done:
    break;
  }
}

long lastWindDownMotorTick = 0;
bool linearActuatorReturnBegan = false;
void windDown() {
  if (lastState != state) {
    Serial.println("Beginning ride-wide wind down...");
    lastState = windingDown;
    linearActuatorLastRunTime = millis();
    linearActuatorDirection = down;
    linearActuatorSpeed = 255;
  }

  // Serial.println(baseRotationMotorSpeed);
  // Serial.println(upperRotationMotorSpeed);
  // Serial.println(millis() - lastWindDownMotorTick);

  bool shouldWindDownMotors = (baseRotationMotorSpeed != 0 || upperRotationMotorSpeed != 0);
  if (shouldWindDownMotors && millis() - lastWindDownMotorTick > 50) {
    lastWindDownMotorTick = millis();
    baseRotationMotorSpeed = max(0, baseRotationMotorSpeed - 1);
    upperRotationMotorSpeed = max(0, upperRotationMotorSpeed - 1);
  }

  bool shouldWindDownActuator = millis() - linearActuatorLastRunTime < 10000;

  // bool shouldWindDownActuator = linearActuatorPosition != 0;
  // if (shouldWindDownActuator) {
  //   if (!linearActuatorReturnBegan) {
  //     linearActuatorLastRunTime = millis();
  //     linearActuatorDirection = down;
  //     linearActuatorSpeed = 255;
  //     linearActuatorReturnBegan = true;
  //   }

  //   if (linearActuatorPosition < millis() - linearActuatorLastRunTime - 1000) {
  //     linearActuatorPosition = 0;
  //     linearActuatorReturnBegan = false;
  //     linearActuatorSpeed = 0;
  //   }
  // }

  if (!shouldWindDownMotors && !shouldWindDownActuator) {
    state = ready;
  }
}