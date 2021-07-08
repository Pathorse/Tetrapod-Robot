#include "Arduino.h"

#include "teensy_serial.h"

const int NUMBER_OF_MOTORS = 6;

TeensySerial serial(NUMBER_OF_MOTORS);

double control_command_type;

double control_commands[NUMBER_OF_MOTORS];

double joint_positions[NUMBER_OF_MOTORS];

double joint_velocities[NUMBER_OF_MOTORS];

double joint_torques[NUMBER_OF_MOTORS];

void setup() 
{
  //Serial.begin(9600);
  //while(!Serial);
  //Serial.println("Start");
  //delay(1000);
}

/*
void loop()
{
  for(int i = 0; i < NUMBER_OF_MOTORS; i++)
  {
    joint_positions[i] = 2.0;
    joint_velocities[i] = 4.0;
    joint_torques[i] = 6.0;
  }
  
  //serial.sendStates(joint_positions, joint_velocities, joint_torques);

  unsigned char buff[24];

  double data[3] = { 1.44, 3.22, 10.34 };

  for (int i = 0; i < 24; i++)
  {
    buff[i] = ((char*)data)[i];
  }

  Serial.write(buff, 24);

  delay(1000);
}*/

void loop() 
{
  
  if(serial.areControlCommandsAvailable())
  {
    //Serial.println("New Data");
    serial.receiveControlCommands(control_command_type, control_commands);
    //Serial.println("Printint New Data");
    //Serial.println(control_command_type);
    
    for(int i = 0; i < NUMBER_OF_MOTORS; i++)
    {
      
      joint_positions[i] = control_commands[i]*2.0;
      joint_velocities[i] = control_commands[i]*4.0;
      joint_torques[i] = control_commands[i]*6.0;  
      //Serial.print(control_commands[i]); Serial.print("\t");
    }

    
    //Serial.println("");

    serial.sendStates(joint_positions, joint_velocities, joint_torques);
  }
  else
  {
    //Serial.println("No data");
  }
  delay(0.1);

}

/*
void loop()
{
  for(int i = 0; i < NUMBER_OF_MOTORS; i++)
  {
    joint_positions[i] = 1.9;
    joint_velocities[i] = 3.5;
    joint_torques[i] = 6.71;
  }
  serial.sendStates(joint_positions, joint_velocities, joint_torques);
  //Serial.println("CLEAR");
  //serial.printTxBuffer();
  delay(1000);

  //while(true);
}*/