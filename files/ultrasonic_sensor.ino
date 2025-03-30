const int echoPin = 33;
const int trigPin = 25;

unsigned long previousMillis = 0;
const unsigned long interval = 400;  // Interval between readings in milliseconds
bool plotterMode = true;  // Set this to true for plotting, false for regular serial output

void setup() {
  Serial.begin(115200);                  // Higher baud rate for ESP32
  pinMode(echoPin, INPUT);               
  pinMode(trigPin, OUTPUT);              
  
  if (!plotterMode) {   // Only print header if not in plotter mode
    Serial.println("Ultrasonic Distance Sensor");
    Serial.println("-------------------------");
  }
}

void loop() {
  float distance = readDistance();
  
  if (distance >= 0) {
    if (plotterMode) {
      Serial.println(distance);  // Just print the distance value for plotting
    } else {
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
    }
  }
  
  delay(400);  // Short delay for smoother plotting
}

// Function to read the sensor data and calculate the distance
float readDistance() {
  digitalWrite(trigPin, LOW);   
  delayMicroseconds(2);        
  
  digitalWrite(trigPin, HIGH);  
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);  

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    if (!plotterMode) Serial.println("Error: No pulse received");
    return -1;
  }
  
  float distance = duration * 0.034 / 2;

  if (distance > 400 || distance < 2) {  // Out of range handling
    return -1;
  }
  
  return distance;
}
