const long readInterval = 1000;

unsigned long lastReadMillis = 0; // Store the last time a reading was made

//TURBIDITY
int sensorPin = A1; // A0 for Arduino /
//PH
float calibration_value = 21.34 - 0.7;
int phval = 0; 
unsigned long int avgval; 
int buffer_arr[10],temp; 
float ph_act;
float sensorValue = 0;
float voltage = 0;
float turbidity = 0;

float volt = 0;

void setup() { 
  Serial.begin(115200);
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if it's time to read the temperature (every second)
  if (currentMillis - lastReadMillis >= readInterval) {
    lastReadMillis = currentMillis; 
    //Turbidity
    sensorValue = analogRead(sensorPin);
    voltage = sensorValue * (3.3 / 1024.0);
    
    // Calculate turbidity as a float, mapping from 0-202 to 100-0
    turbidity = map(sensorValue, 0, 205, 100, 0);
    turbidity = max(turbidity, 1); // Ensure turbidity does not drop below 0.1
    
    //ph
    for(int i=0;i<10;i++) 
      { 
      buffer_arr[i]=analogRead(A0);
      delay(30);
      }
    for(int i=0;i<9;i++)
      {
      for(int j=i+1;j<10;j++)
        {
          if(buffer_arr[i]>buffer_arr[j])
            {
            temp=buffer_arr[i];
            buffer_arr[i]=buffer_arr[j];
            buffer_arr[j]=temp;
            }
        }
    }  
    avgval = 0;
    for(int i=2;i<8;i++)
    avgval+=buffer_arr[i];
    volt=(float)avgval*5.0/1024/6; 
    ph_act = -5.70 * volt + calibration_value;
      String output = String(ph_act) + "," + String(turbidity);
  Serial.println(output); // Send data to ESP8266
    }

 
}