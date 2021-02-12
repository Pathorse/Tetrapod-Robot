// SEND EXAMPLE OF SERIAL CAN MODULE
// unsigned char send(unsigned long id, uchar ext, uchar rtrBit, uchar len, const uchar *buf);
// SUPPORT: joney.sui@longan-labs.cc
#include "motor_interface.h"

#include <Serial_CAN_Module.h>
//#include <SoftwareSerial.h>
MotorInterface motor_interface;
Serial_CAN can;

#define can_tx  0           // tx of serial can module connect to D2
#define can_rx  1           // rx of serial can module connect to D3

void setup()
{
    Serial.begin(9600);
    delay(1000);
    can.begin(can_tx, can_rx, 9600);      // tx, rx
    Serial.println("begin");
}

unsigned char dta[8] = {1, 2, 3, 4, 5, 6, 7, 8};
unsigned char m[8] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned char can_message[8];

// send(unsigned long id, byte ext, byte rtrBit, byte len, const byte *buf);
void loop()
{
    double timer = micros();
    motor_interface.writeAccelerationToRAM(can_message, 2864434431);
    can.send(0x55, 0, 0, 8, can_message);   // SEND TO ID:0X55
    Serial.print(micros() - timer); Serial.print("\t");
    for(int i=0; i<8; i++)
    {
        Serial.print("0x");
        Serial.print(can_message[i], HEX);
        Serial.print('\t');
    }
    Serial.println("");
    delay(3000);
}

// END FILE
