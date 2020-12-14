# Arduino Dash
For my Electric BMW Conversion, I wanted a display that could keep track of all the vitals and statuses of the components throughout the car. This allows me to ensure safety during operation, and also add features that the car did not previously have. 

# Libraries
The files for the nextion display did not have the functions I needed, so I modified the source files to accomodate the features I wanted. 

# Setup
The project uses a 3.5" Nextion display that is controlled with an Arduino Mega. The Mega is a hub of all sorts of sensing material, and can calculate information like engine RPM's, current draw, main battery pack voltage, accessory battery voltage, and more. This is then fed via SoftwareSerial to the Nextion display where the corresponding boxes update.
