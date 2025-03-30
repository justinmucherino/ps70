class Capacitor {
  int readPin;
  int writePin;
  unsigned long previousMicros;

public:
  Capacitor(int pinA, int pinB) {
    readPin = pinA;
    writePin = pinB;
    previousMicros = 0;

    pinMode(writePin, OUTPUT);
  }

  long getReading() {
    previousMicros = micros();

    for (int count = 0; count < 1000; count++) {
      // Charge the capacitor
      digitalWrite(writePin, HIGH);
      delayMicroseconds(1);  // Allow a brief charge time

      // Read the analog value
      int reading = analogRead(readPin);

      // Check if the reading is above a threshold
      if (reading > 2000) break; // Adjust threshold based on your setup

      // Discharge the capacitor
      digitalWrite(writePin, LOW);
      delayMicroseconds(1);
    }

    return (micros() - previousMicros);
  }
};

Capacitor sensor(32,4);

void setup() {
  Serial.begin(9600);
  analogReadResolution(12);  // Set to full range (0 - 4095)
}

void loop() {
  long readings[100];
  for (int i = 0; i < 100; i++) {
    readings[i] = sensor.getReading();
    delay(10);
  }

  long sum = 0;
  for (int j = 0; j < 100; j++) {
    sum += readings[j];
  }

  Serial.println(sum / 100);
}