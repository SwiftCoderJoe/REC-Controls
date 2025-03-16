const int ESTOP_BUTTON_IN = 2;
const int MOTOR_PWM_ONE = 8;
const int MOTOR_PWM_TWO = 9;

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ESTOP_BUTTON_IN, INPUT);
  pinMode(MOTOR_PWM_ONE, OUTPUT);
  pinMode(MOTOR_PWM_TWO, OUTPUT);
  Serial.begin(115200);
}

// the loop function runs over and over again forever
void loop() {
  if (digitalRead(ESTOP_BUTTON_IN) == 1) {
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(MOTOR_PWM_ONE, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(MOTOR_PWM_ONE, LOW);
  }
}
  