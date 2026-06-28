// Motor Control (L298N)
int IN1 = 8;        // Direction 1
int IN2 = 9;        // Direction 2
int ENA = 10;       // PWM Speed Control

// Sensors
int currentPin = A0; // ACS712 Current Sensor
int tempPin = A1;    // Analog Thermistor
int potPin = A2;     // 50k Potentiometer
int vibPin = 2;      // SW-420 Vibration Module
int led = 7;         // Fault Indicator LED

// Safety Limits
int vibrationThreshold = 7;   
int tempThreshold = 55;       
float currentThreshold = 1.0; 
int faultDelay = 1500;        

bool fault = false;
unsigned long lastBlink = 0;
bool ledState = LOW;
unsigned long faultTime = 0;
float zeroCurrentVoltage = 2.5; 

void setup() {
  Serial.begin(9600);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(vibPin, INPUT); pinMode(led, OUTPUT);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);

  // Auto-Calibrate Current Sensor
  float sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(currentPin) * (5.0 / 1023.0);
    delay(5);
  }
  zeroCurrentVoltage = sum / 100.0;
}

void loop() {
  float temp = analogRead(tempPin) * (5.0 / 1023.0) * 100;
  float voltage = analogRead(currentPin) * (5.0 / 1023.0);
  float current = (voltage - zeroCurrentVoltage) / 0.185;
  if (current < 0) current = 0; 

  int vibrationCount = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(vibPin) == HIGH) vibrationCount++;
    delay(2);
  }
  if (vibrationCount <= 3) vibrationCount = 0; // Filter noise

  int potValue = analogRead(potPin);
  int motorSpeed = map(potValue, 0, 1023, 0, 255);

  bool vibFault = (vibrationCount >= vibrationThreshold);
  bool tempFault = (temp >= tempThreshold);
  bool currentFault = (current >= currentThreshold);

  if (vibFault || tempFault || currentFault) {
    fault = true;
    faultTime = millis();
  }
  if (fault && (millis() - faultTime > faultDelay)) {
    fault = false;
  }

  // Motor Execution
  if (fault) {
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); analogWrite(ENA, 0); 
    if (millis() - lastBlink > 100) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(led, ledState);
    }
  } else {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, motorSpeed); 
    digitalWrite(led, LOW);
  }

  // Telemetry Output for AI
  Serial.print("Temp:"); Serial.print(temp);
  Serial.print("C | Current:"); Serial.print(current);
  Serial.print("A | Vib:"); Serial.print(vibrationCount);
  Serial.print(" | Speed PWM:"); Serial.print(motorSpeed); 
  if (vibFault) Serial.print(" | VIB FAULT");
  else if (tempFault) Serial.print(" | TEMP FAULT");
  else if (currentFault) Serial.print(" | CUR FAULT");
  else Serial.print(" | NORMAL");
  Serial.println();
}