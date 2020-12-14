#include "EasyNextionLibrary.h"

const int inPin =3;//can change
const float SHUNT_CURRENT =1000.00;//A
const float SHUNT_VOLTAGE =0.075*66*2;// mV
const float CORRECTION_FACTOR = -0.50;


const int ITERATION = 15; //can change (see video)
const float VOLTAGE_REFERENCE = 5.00;//1.1V
const int BIT_RESOLUTION =10;//and 12 for Due and MKR

const int NUM_BATT = 6; // Number of lead acid batteries in series
const int VBATT_CUTOFF_SINGLE = 11.6; // Lead acid 11.6v cutoff

const int MAX_VBATT_VOLTAGE = 76;
const int MIN_VBATT_VOLTAGE = VBATT_CUTOFF_SINGLE*NUM_BATT;


const byte PulsesPerRevolution = 1;  // Set how many pulses there are on each revolution. Default: 2.


// If the period between pulses is too high, or even if the pulses stopped, then we would get stuck showing the
// last value instead of a 0. Because of this we are going to set a limit for the maximum period allowed.
// If the period is above this value, the RPM will show as 0.
// The higher the set value, the longer lag/delay will have to sense that pulses stopped, but it will allow readings
// at very low RPM.
// Setting a low value is going to allow the detection of stop situations faster, but it will prevent having low RPM readings.
// The unit is in microseconds.
const unsigned long ZeroTimeout = 300000;  // For high response time, a good value would be 100000.
// For reading very low RPM, a good value would be 300000.


// Calibration for smoothing RPM:
const byte numReadings = 2;  // Number of samples for smoothing. The higher, the more smoothing, but it's going to
// react slower to changes. 1 = no smoothing. Default: 2.


/////////////
// Variables:
/////////////

volatile unsigned long LastTimeWeMeasured;  // Stores the last time we measured a pulse so we can calculate the period.
volatile unsigned long PeriodBetweenPulses = ZeroTimeout + 1000; // Stores the period between pulses in microseconds.
// It has a big number so it doesn't start with 0 which would be interpreted as a high frequency.
volatile unsigned long PeriodAverage = ZeroTimeout + 1000; // Stores the period between pulses in microseconds in total, if we are taking multiple pulses.
// It has a big number so it doesn't start with 0 which would be interpreted as a high frequency.
unsigned long FrequencyRaw;  // Calculated frequency, based on the period. This has a lot of extra decimals without the decimal point.
unsigned long FrequencyReal;  // Frequency without decimals.
unsigned long RPM;  // Raw RPM without any processing.
unsigned int PulseCounter = 1;  // Counts the amount of pulse readings we took so we can average multiple pulses before calculating the period.

unsigned long PeriodSum; // Stores the summation of all the periods to do the average.

unsigned long LastTimeCycleMeasure = LastTimeWeMeasured;  // Stores the last time we measure a pulse in that cycle.
// We need a variable with a value that is not going to be affected by the interrupt
// because we are going to do math and functions that are going to mess up if the values
// changes in the middle of the cycle.
unsigned long CurrentMicros = micros();  // Stores the micros in that cycle.
// We need a variable with a value that is not going to be affected by the interrupt
// because we are going to do math and functions that are going to mess up if the values
// changes in the middle of the cycle.

// We get the RPM by measuring the time between 2 or more pulses so the following will set how many pulses to
// take before calculating the RPM. 1 would be the minimum giving a result every pulse, which would feel very responsive
// even at very low speeds but also is going to be less accurate at higher speeds.
// With a value around 10 you will get a very accurate result at high speeds, but readings at lower speeds are going to be
// farther from eachother making it less "real time" at those speeds.
// There's a function that will set the value depending on the speed so this is done automatically.
unsigned int AmountOfReadings = 6;

unsigned int ZeroDebouncingExtra;  // Stores the extra value added to the ZeroTimeout to debounce it.
// The ZeroTimeout needs debouncing so when the value is close to the threshold it
// doesn't jump from 0 to the value. This extra value changes the threshold a little
// when we show a 0.

// Variables for smoothing tachometer:
unsigned long readings[numReadings];  // The input.
unsigned long readIndex;  // The index of the current reading.
unsigned long total;  // The running total.
unsigned long average;  // The RPM value after applying the smoothing.


#include <SoftwareSerial.h> //Include the library

SoftwareSerial display_serial(10, 11); // RX, TX
EasyNex nex(display_serial);
int min_rpm = 0;
int max_rpm = 8000;


void setup()  // Start of setup:
{  
  display_serial.begin(9600);
  nex.begin();
  Serial.begin(9600);  // Begin serial communication.
  pinMode(3,INPUT);
  //analogReference(INTERNAL1V1);
  attachInterrupt(digitalPinToInterrupt(2), &Pulse_Event, RISING);  // Enable interruption pin 2 when going from LOW to HIGH.

  delay(1000);  // We sometimes take several readings of the period to average. Since we don't have any readings
  // stored we need a high enough value in micros() so if divided is not going to give negative values.
  // The delay allows the micros() to be high enough for the first few cycles.


  write_to_screen("Vbatt.pco",0);
  write_to_screen("amps.pco",0);
  write_to_screen("Vacc.pco",0);
  write_to_screen("rpm.pco",0);
}

void loop() {
  
  // The following is going to store the two values that might change in the middle of the cycle.
  // We are going to do math and functions with those values and they can create glitches if they change in the
  // middle of the cycle.
  LastTimeCycleMeasure = LastTimeWeMeasured;  // Store the LastTimeWeMeasured in a variable.
  CurrentMicros = micros();  // Store the micros() in a variable.

  // CurrentMicros should always be higher than LastTimeWeMeasured, but in rare occasions that's not true.
  // I'm not sure why this happens, but my solution is to compare both and if CurrentMicros is lower than
  // LastTimeCycleMeasure I set it as the CurrentMicros.
  // The need of fixing this is that we later use this information to see if pulses stopped.
  if (CurrentMicros < LastTimeCycleMeasure)
  {
    LastTimeCycleMeasure = CurrentMicros;
  }

  // Calculate the frequency:
  FrequencyRaw = 10000000000 / PeriodAverage;  // Calculate the frequency using the period between pulses.

  // Detect if pulses stopped or frequency is too low, so we can show 0 Frequency:
  if (PeriodBetweenPulses > ZeroTimeout - ZeroDebouncingExtra || CurrentMicros - LastTimeCycleMeasure > ZeroTimeout - ZeroDebouncingExtra)
  { // If the pulses are too far apart that we reached the timeout for zero:
    FrequencyRaw = 0;  // Set frequency as 0.
    ZeroDebouncingExtra = 2000;  // Change the threshold a little so it doesn't bounce.
  }
  else
  {
    ZeroDebouncingExtra = 0;  // Reset the threshold to the normal value so it doesn't bounce.
  }
  FrequencyReal = FrequencyRaw / 10000;  // Get frequency without decimals.
  // This is not used to calculate RPM but we remove the decimals just in case
  // you want to print it.
  // Calculate the RPM:
  RPM = FrequencyRaw / PulsesPerRevolution * 60;  // Frequency divided by amount of pulses per revolution multiply by
  // 60 seconds to get minutes.
  RPM = RPM / 10000;  // Remove the decimals.
  // Smoothing RPM:
  total = total - readings[readIndex];  // Advance to the next position in the array.
  readings[readIndex] = RPM;  // Takes the value that we are going to smooth.
  total = total + readings[readIndex];  // Add the reading to the total.
  readIndex = readIndex + 1;  // Advance to the next position in the array.

  if (readIndex >= numReadings)  // If we're at the end of the array:
  {
    readIndex = 0;  // Reset array index.
  }

  // Calculate the average:
  average = total / numReadings;  // The average value it's the smoothed result.
  update_amps();
  update_vbatt_voltage();
  
  update_rpm();

}  // End of loop.

void update_amps(){
  float val = getCurrent();
  write_to_screen("amps.val",val*10.0); 
  if(val > SHUNT_CURRENT*0.6){
    write_to_screen("amps.bco",63448);
  }else {
    write_to_screen("amps.bco",2016);
  }
}

int test = 0;
void update_vbatt_voltage(){

  float val = analogRead(4)*(MAX_VBATT_VOLTAGE/1023.0);
  write_to_screen("Vbatt.val",val*10);
  if(val > MAX_VBATT_VOLTAGE*0.8){
    // Voltage is within a good range, display green
    write_to_screen("Vbatt.bco",2016);
  }else if(val < MAX_VBATT_VOLTAGE*0.8 && val > MIN_VBATT_VOLTAGE * 1.2){
    // Voltage is within a good range, display green
    write_to_screen("Vbatt.bco",65504);
  }else{
    write_to_screen("Vbatt.bco", 63488); 
  }

}

void update_rpm(){
  write_to_screen("rpm.val",average);
  if(average > max_rpm*0.8){
    write_to_screen("rpm.bco",63448);
  }else {
    write_to_screen("rpm.bco",2016);
  }
}


void write_to_screen(String loc, int val){
  /*display_serial.print(loc+"=");
  display_serial.print(val);
  display_serial.write(0xff);
  display_serial.write(0xff);
  display_serial.write(0xff);
  */
  nex.writeNum(loc,val);
}

void Pulse_Event()  // The interrupt runs this to calculate the period between pulses:
{

  PeriodBetweenPulses = micros() - LastTimeWeMeasured;  // Current "micros" minus the old "micros" when the last pulse happens.
  // This will result with the period (microseconds) between both pulses.
  // The way is made, the overflow of the "micros" is not going to cause any issue.

  LastTimeWeMeasured = micros();  // Stores the current micros so the next time we have a pulse we would have something to compare with.
  if (PulseCounter >= AmountOfReadings) // If counter for amount of readings reach the set limit:
  {
    PeriodAverage = PeriodSum / AmountOfReadings;  // Calculate the final period dividing the sum of all readings by the
    // amount of readings to get the average.
    PulseCounter = 1;  // Reset the counter to start over. The reset value is 1 because its the minimum setting allowed (1 reading).
    PeriodSum = PeriodBetweenPulses;  // Reset PeriodSum to start a new averaging operation.


    // Change the amount of readings depending on the period between pulses.
    // To be very responsive, ideally we should read every pulse. The problem is that at higher speeds the period gets
    // too low decreasing the accuracy. To get more accurate readings at higher speeds we should get multiple pulses and
    // average the period, but if we do that at lower speeds then we would have readings too far apart (laggy or sluggish).
    // To have both advantages at different speeds, we will change the amount of readings depending on the period between pulses.
    // Remap period to the amount of readings:
    int RemapedAmountOfReadings = map(PeriodBetweenPulses, 40000, 5000, 1, 10);  // Remap the period range to the reading range.
    // 1st value is what are we going to remap. In this case is the PeriodBetweenPulses.
    // 2nd value is the period value when we are going to have only 1 reading. The higher it is, the lower RPM has to be to reach 1 reading.
    // 3rd value is the period value when we are going to have 10 readings. The higher it is, the lower RPM has to be to reach 10 readings.
    // 4th and 5th values are the amount of readings range.
    RemapedAmountOfReadings = constrain(RemapedAmountOfReadings, 1, 10);  // Constrain the value so it doesn't go below or above the limits.
    AmountOfReadings = RemapedAmountOfReadings;  // Set amount of readings as the remaped value.
  }
  else
  {
    PulseCounter++;  // Increase the counter for amount of readings by 1.
    PeriodSum = PeriodSum + PeriodBetweenPulses;  // Add the periods so later we can average.
  }
}  // End of Pulse_Event.

float getCurrent()
{
 //robojax.com 50A Shunt Current Measurement for Arduino

    float averageSensorValue =0;
    int sensorValue ;
    float voltage, current;

    for(int i=0; i< ITERATION; i++)
    {   
      sensorValue = analogRead(inPin);
      if(sensorValue >1&& sensorValue < 1000)
      {
        voltage = (sensorValue) * (VOLTAGE_REFERENCE /  (pow(2,BIT_RESOLUTION)-1)); 
        current  = voltage * (SHUNT_CURRENT /SHUNT_VOLTAGE )  ;
        if(i !=0){
          averageSensorValue += current+CORRECTION_FACTOR;
        }
      }else{
        break;
      }
    }  
    averageSensorValue /=(ITERATION-1);
    return   averageSensorValue;
}//getCurrent()
