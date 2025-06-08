# Fuzzy_fire
Fire extinguisher using fuzzy inference system 

## Components used 

1) PIC16F877A
2) Two 4 pin flame sensors
3) Servo motor
4) oscillator 16MHZ and capacitors
5) Breadboard and jumper wires 

## overveiw
The two 4 pin flame sensors(Analog output) are connected to ADC port in PIC16F877A when there is an event of fire the analog output of flame sensors are fed into pic and the output throw is determined by water pump motor using fuzzy inference system. Based on maximum intensity of one of the flame sensors and a certain threshold values the servo motor sweeps back and forth towards that direction

## Converting .fis to C code 
To convert .fis to c code [qfisgen](https://in.mathworks.com/matlabcentral/fileexchange/117465-qfiscgen-fuzzy-c-code-generator-for-embedded-systems) a matlab tool is used which converts .fis to c code 



