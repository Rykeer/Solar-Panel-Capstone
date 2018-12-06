// mainSolar.c ------------------------------------------------
// Created By:      Brad White
// Creation Date:   September 24, 2018
// Revision Date:   December 3, 2018
// Description:     Solar Panel Tracking System
//-------------------------------------------------------------
// so Preprocessor:--------------------------------------------
#include "mbed.h"
#include <stdio.h>
#include <stdlib.h>
#define ON          1    //On-off conditions 
#define OFF         0
#define PRESSED     0    //Home button status
#define NOTPRESSED  1
#define STLPRESSED  2
#define SENSORQ     8    //Array dimensions
#define SAMPLESIZE  10
#define SCALE       0.01 //Sensor scaling value
#define CW          1    //Motor Directions
#define CCW         2
#define STAY        0    //Holding Condition
#define LOW         0    
#define HIGH        1
#define PATTERNS    4    //Number of patterns to rotate motors
#define LOWLOW      0    //These are the coil conditions for turning the motors
#define LOWHIGH     1
#define HIGHHIGH    2
#define HIGHLOW     3
#define MAXPANELVOLTAGE 22.5
#define MAXPANELCURRENT 1.5
//serial communications with tera term
Serial pc(USBTX, USBRX);
//Switches
DigitalIn homeButton(p30);//Go home button
DigitalIn LS1(p11);//Home tilt switch 
DigitalIn LS2(p12);//Stop tilt switch
DigitalIn LS3(p13);//Home rotate switch
DigitalIn LS4(p14);//Stop rotate switch
//I2C serial clock and data pins
I2C i2c(p9,p10);
//LDR and Temperature sensors
AnalogIn rotateLDR(p20); //reads both rotate LDRs via BJT multiplexing
AnalogIn tiltLDR(p19);   //reads both tilt LDRs via BJT multiplexing
AnalogIn ReturnLDR(p18); //may be taken out
AnalogIn TempSensor(p17);//Reads the temperature at the back of the panel
//Panel inputs
AnalogIn Current(p15);//Reads from the op-amp amplifying the shunt resistor voltage
AnalogIn Voltage(p16);//Reads from the circuit attenuating the panel output voltage
//BJT Control. Used for multiplexing the rotate and tilt LDRs
DigitalOut BJT4 =(p5);//controls stop rotate LDR
DigitalOut BJT3 =(p6);//controls home rotate LDR
DigitalOut BJT2 =(p7);//controls stop tilt LDR
DigitalOut BJT1 =(p8);//controls home tilt LDR
//Motor 1 Tilt Control
DigitalOut E11(p26);//E-enable, 1 - Epin 1, 1 - motor 1  (XXX - E11)
DigitalOut M11(p28);//M-coil polarity, 1 - Mpin 1, motor 1
DigitalOut E21(p25);
DigitalOut M21(p27);
//Motor 2 Rotate Control
DigitalOut E12(p21);
DigitalOut M12(p22);
DigitalOut E22(p23);//E-enable, 2 - Epin 2, motor 2
DigitalOut M22(p24);//M-coil polarity, 2 - Mpin 2, motor 2
//LED indicators. On board LEDs to indicate motor direction
DigitalOut ccwTilt(LED1);
DigitalOut cwTilt(LED2);
DigitalOut ccwRotate(LED3);
DigitalOut cwRotate(LED4);
//Function Delarations
void initSystem(char *pHome, char *pSampCounter, int *pCounter);
void initMotors();
void initLCD();
void GoHome(char *pHome, int *pCounter);
void readButton(char *pStatus);
void readSensors(char *pCounter);
void calcAverage();
void printAverage(); // used for testing in tera term
void printLCD(char *pCounter);
void motorControl(char *pTilt, char *pRotate, int *pPatCounter);
void determineDirect(char *pD1, char *pD2);
void tilt(char Direction, int *pCounter);
void rotate(char Direction, int *pCounter);
//Global Variables
float sensorReadings[SENSORQ][SAMPLESIZE]; //holds readings for all sensors
float sensorAverage[SENSORQ];//holds average readings for all sensors
char patterns[PATTERNS] = {0,1,2,3};
char screenCmdArray[2] = {0x01,0x0C};// holds the blank screen and move cursor command
int coordinate[2];//holds the coordinate for the LDR system
const int screenAdress = 0x2A << 1; //Screen address bit shifted left for writing
// eo Preprocessor::---------------------------------------------------------
// so main:------------------------------------------------------------------
// Created By: Brad White
// Desc: Controls the main functionallity of the control system. Manages system 
// initialization, user input, sensor data, motor control, and LCD display.                   
int main()
{
char sampleCounter = 0; //keeps track of the array element to stay in bounds
int  patCounter1 = 0;
int  patCounter2 = 0;
char d1,d2 = STAY; //Direction variables that hold motor status
char buttonStatus = NOTPRESSED;//default status
char home;
initSystem(&home, &sampleCounter, &patCounter); //initialize system. Exits program if errors occur
while(1) 
    {  
    switch(buttonStatus)
        {
        case PRESSED:
            GoHome(&home, &patCounter);
            readButton(&buttonStatus);
            break;
        case NOTPRESSED:
            readSensors(&sampleCounter);
            calcAverage();
            //printAverage(); //uncomment to use tera term
            printLCD(&sampleCounter); // comment out when using tera term
            motorControl(&d1, &d2, &patCounter1, &patCounter2);
            readButton(&buttonStatus);
            break;
        }
    }
}// eo main::-----------------------------------------------------
// so motorControl:-----------------------------------------------
// Created By: Brad White
// Desc: Determines motor direction and executes movement. 
void motorControl(char *pTilt, char *pRotate, int *pPatCounter1, int *pPatCounter2)
{
determineDirect(pTilt, pRotate);
tilt(*pTilt, pPatCounter1);
rotate(*pRotate, pPatCounter2);
}//eo motorControl::----------------------------------------------
// so initSystem:-------------------------------------------------
// Created By: Brad White
// Desc: Initializes start position, samples system sensors, prepares LCD screen
void initSystem(char *pHome, char *pSampCounter, int *pCounter)
{
initMotors();
GoHome(pHome, pCounter);//Moves the panel home. Exits program on error
for (char counter = 0; counter < SAMPLESIZE; counter++)//fills sensor array
    {
    readSensors(pSampCounter);
    }
//initLCD();
}//eo initializeSystem::------------------------------------------

// so initMotors:-------------------------------------------------
// Created By: Brad White
// Desc: Initializes the motors into a holding condition to begin
void initMotors()
{
E12 = ON; //Rotate Motor
E22 = ON;    
M12 = LOW; 
M22 = LOW;
//E11 = ON; //Tilt Motor
//E21 = ON;
//M11 = LOW;
//M21 = LOW;    
}//eo initMotors::------------------------------------------------
// so printAverage:-----------------------------------------------
// Created By: Brad White
// Desc: Used to communicate sensor data to tera term for testing purposes.
void printAverage()
{
pc.printf("\nREADINGS\n\r");
for(char x = 0; x <SENSORQ; x++)
    {
    pc.printf("SensAvg %d: %0.2f\n\r",x+1,sensorAverage[x]);
    }
}//eo printAverage:-----------------------------------------------
// so initLCD:----------------------------------------------------
// Created By: Said Smajic
// Desc: prepares the LCD screen for writing and sends the starting data
void initLCD()
{
char introMessage[20] = "Solar Tracking\n\r";
i2c.write(screenAdress, screenCmdArray, 2); //preps screen
wait(0.1); 
i2c.write(screenAdress, introMessage, sizeof(introMessage)); //sends intro
wait(0.1);
char introMessage2[20] = "Initializing\n\r";
i2c.write(screenAdress, introMessage2, sizeof(introMessage2)); //sends intro
}//eo initLCD::---------------------------------------------------
// so printLCD:---------------------------------------------------
// Created By: Said Smajic
// Desc: Prints the panel data and temp data to the screen.
void printLCD(char *pCounter)
{
char cmdArray[2] = {0x0C, 0x01};
char message0[20];
char stringValue0[10];

i2c.write(screenAdress, cmdArray, 2); 
wait(2);
sprintf(stringValue0, "%0.3f", sensorAverage[0]);
strcpy(message0, "Voltage(V): ");
strcat(message0, stringValue0);
strcat(message0, "\n\r");
i2c.write(screenAdress, message0, strlen(message0)); //sends temp
wait(1);

sprintf(stringValue0, "%0.3f", sensorAverage[1]);
strcpy(message0, "Current(A): ");
strcat(message0, stringValue0);
strcat(message0, "\n\r");
i2c.write(screenAdress, message0, strlen(message0));// sends current
wait(1);

sprintf(stringValue0, "%0.3f", sensorAverage[2]);
strcpy(message0, "Power(W): ");
strcat(message0, stringValue0);
strcat(message0, "\n\r");
i2c.write(screenAdress, message0, strlen(message0));// sends voltageresetArray[0];
wait(1);

sprintf(stringValue0, "%0.3f", sensorAverage[5]);
strcpy(message0, "Temp(C): ");
strcat(message0, stringValue0);
strcat(message0, "\0");
i2c.write(screenAdress, message0, strlen(message0));// sends Power
wait(1);
*pCounter += 1;
}

// so readSensors:------------------------------------------
// Created By: Brad White
// Desc: Responsible for reading the LDRs, temp sensor, panel voltage and current
void readSensors(char *pCounter)
{
if(*pCounter >= SAMPLESIZE)
    {
    *pCounter = 0;
    }
BJT1 = ON;
BJT2 = OFF;
uint16_t reading1 = rotateLDR.read_u16(); // Rotate1 reading
float voltageRead1 = (reading1 / 65535.0) * 2.6;
sensorReadings[0][*pCounter] = voltageRead1 / SCALE;
BJT1 = OFF;
BJT2 = ON;
uint16_t reading2 = rotateLDR.read_u16(); // Rotate 2 reading
float voltageRead2 = (reading2 / 65535.0) * 2.6;
sensorReadings[1][*pCounter] = voltageRead2 / SCALE;
BJT1 = OFF;
BJT2 = OFF;
uint16_t reading3 = tiltLDR1.read_u16(); // Tilt Reading
float voltageRead3 = (reading3 / 65535.0) * 2.6;
sensorReadings[2][*pCounter] = voltageRead3 / SCALE;

uint16_t reading4 = tiltLDR2.read_u16(); // Tilt 2 Reading
float voltageRead4 = (reading4 / 65535.0) * 2.6;
sensorReadings[3][*pCounter] = voltageRead4 / SCALE;

uint16_t reading5 = voltage.read_u16(); //Voltage reading
float voltageRead5 = (reading5 / 65535.0) * 3.3;
sensorReadings[4][*pCounter] = (voltageRead5/3.3)*MAXPANELVOLTAGE;

uint16_t reading7 = Current.read_u16(); // Current Sensor
float voltageRead7 = (reading7 / 65535.0) * 3.3;
sensorReadings[5][*pCounter] = (voltageRead7/3.3)*MAXPANELCURRENT;

uint16_t reading6 = tempSensor.read_u16(); // TEMP Sensor
float voltageRead6 = ((reading6 / 65535.0)* 3) - 0.54;
sensorReadings[5][*pCounter] = (voltageRead6/0.009);
//*pCounter += 1;
}//eo printLCD::--------------------------------------------------
// so calcAverage:------------------------------------------------
// Created By: Brad White
// Desc: Iterates through the array of sensor data to calculate the averages. 
void calcAverage()
{
float sum;
for(char x = 0; x < SENSORQ; x++)
    {
    sum = 0;
    for (char i = 0; i < SAMPLESIZE; i++)
        {
        sum += sensorReadings[x][i];
        }
    sensorAverage[x] = sum / SAMPLESIZE;
    }
}//eo calcAverage::-----------------------------------------------
// so readButton:-------------------------------------------------
// Created By: Brad White
// Desc: Reads the home button. Stops the button from working continuously if held.
void readButton(char *pStatus)
{   
if(*pStatus == NOTPRESSED) 
    {
    if(homeButton == PRESSED)
        {
        *pStatus = PRESSED;  
        }
    }
else{
    if(homeButton == PRESSED)
        {
        *pStatus = STLPRESSED;  
        }
    else{
        *pStatus = NOTPRESSED;
        }
    }
}//eo readButton::-----------------------------------------------
// so GoHome:----------------------------------------------------
// Created By: Brad White
// Desc: Moves both the tilt and rotation motors home.
void GoHome(char *pHome, int *pCounter)
{
if(LS1 != PRESSED || LS3 != PRESSED)
    {
    *pHome = false;
    while(LS1 != PRESSED)
        { 
        tilt(CCW, pCounter);
        }
    while(LS3 != PRESSED)
        {
        rotate(CCW, pCounter);    
        } 
    if(LS1 != PRESSED || LS3 != PRESSED)
        {
        exit(1);
        }
    else{
        *pHome = true;
        }   
    }
else{
    *pHome = true;
    }
}//eo GoHome::----------------------------------------------
// so determineDirect:--------------------------------------
// Created By: Brad White
// Desc: Determines motor action based on the LDR averages
void determineDirect(char *pD1, char *pD2)
{
coordinate[0] = sensorAverage[0] - sensorAverage[2];
coordinate[1] = sensorAverage[1] - sensorAverage[3];
if(coordinate[0] <= -30 or coordinate[0] >= 30)
    {
    if((coordinate[0] * -1) < 0 )
        {
        *pD1 = CW;
        }
    else{
        *pD1 = CCW;
        }
    }
else{
    *pD1 = STAY;
    }
if(coordinate[1] <= -30 or coordinate[1] >=30)
    {
    if((coordinate[1]* -1) < 0 )
        {
        *pD2 = CCW;
        }
    else{
        *pD2 = CW;
        }   
    }
else{
    *pD2 = STAY;
    }
}//eo determineDirect::-------------------------------------
// so tilt:-------------------------------------------------
// Created By: Brad White
// Desc: Rotates the tilt motor unless limit switches have been reached
void tilt(char Direction, int *pCounter)
{
switch(Direction)
    {
    case STAY:
        ccwTilt = 0;
        cwTilt = 0;
        break;
    case CW:
        ccwTilt = 0;
        cwTilt = 1;
        if(LS2 != PRESSED)
            {
            *pCounter = *pCounter - 1;
            if(*pCounter < 0)
                {
                *pCounter = PATTERNS - 1;
                }
            char position = patterns[*pCounter];
            switch(position)
                {
                case LOWLOW:
                    M12 = LOW;
                    M22 = LOW;
                    break;
                case LOWHIGH:
                    M12 = LOW;
                    M22 = HIGH;
                    break;
                case HIGHHIGH:
                    M12 = HIGH;
                    M22 = HIGH;
                    break;
                case HIGHLOW:
                    M12 = HIGH;
                    M22 = LOW;
                    break;        
                }
            }
        break;
    case CCW:
        cwTilt = 0;
        ccwTilt = 1;
        if(LS1 != PRESSED)
            {
            *pCounter += 1;
            if(*pCounter > (PATTERNS - 1))
                {
                *pCounter = 0;
                }
            char position = patterns[*pCounter];
            switch(position)
                {
                case LOWLOW:
                    M12 = LOW;
                    M22 = LOW;
                    break;
                case LOWHIGH:
                    M12 = LOW;
                    M22 = HIGH;
                    break;
                case HIGHHIGH:
                    M12 = HIGH;
                    M22 = HIGH;
                    break;
                case HIGHLOW:
                    M12 = HIGH;
                    M22 = LOW;
                    break;        
                }
            }
        break;
    }
}// eo tilt::-----------------------------------------------
// so rotate:-----------------------------------------------
// Created By: Brad White
// Desc: Rotates the rotate motor unless limit switch has been reached
void rotate(char Direction, int *pCounter)
{ 
switch(Direction)
    {
    case STAY:
        ccwRotate = 0;
        cwRotate = 0;
        break;
    case CW:
        ccwRotate = 0;
        cwRotate = 1;
        if(LS4 != PRESSED)
            {
            *pCounter = *pCounter - 1;
            if(*pCounter < 0)
                {
                *pCounter = PATTERNS - 1;
                }
            char position = patterns[*pCounter];
            switch(position)
                {
                case LOWLOW:
                    M12 = LOW;
                    M22 = LOW;
                    break;
                case LOWHIGH:
                    M12 = LOW;
                    M22 = HIGH;
                    break;
                case HIGHHIGH:
                    M12 = HIGH;
                    M22 = HIGH;
                    break;
                case HIGHLOW:
                    M12 = HIGH;
                    M22 = LOW;
                    break;        
                }
            }
        break;
    case CCW:
        cwRotate = 0;
        ccwRotate = 1;
        if(LS3 != PRESSED)
            {
            *pCounter += 1;
            if(*pCounter > (PATTERNS - 1))
                {
                *pCounter = 0;
                }
            char position = patterns[*pCounter];
            switch(position)
                {
                case LOWLOW:
                    M12 = LOW;
                    M22 = LOW;
                    break;
                case LOWHIGH:
                    M12 = LOW;
                    M22 = HIGH;
                    break;
                case HIGHHIGH:
                    M12 = HIGH;
                    M22 = HIGH;
                    break;
                case HIGHLOW:
                    M12 = HIGH;
                    M22 = LOW;
                    break;        
                }
            }
        break;
    }
}//eo rotate::----------------------------------------------

// EO Script::------------------------------------------------------------------
