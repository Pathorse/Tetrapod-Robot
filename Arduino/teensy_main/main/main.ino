#include "motor_driver.h"
#include "motor_constants.h"
#include "teensy_can_ports.h"
#include "FlexCAN_T4.h"
#include "utilities.h"
#include "config_motor_driver.h"

// Serial communication
#include "teensy_serial.h"

// Number of motors per Teensy
const int NUMBER_OF_MOTORS = 6;

// Number of motors per CAN port
int NUMBER_OF_MOTORS_PER_PORT = 3;

// Loop rate
const int TIMEOUT = 0.00001;

// Object to communicate with the host PC over serial
TeensySerial serial_interface(NUMBER_OF_MOTORS);

// Variable deciding what kind of control to use
double control_command_type = 0.0;

// Array containing control inputs
double control_commands[NUMBER_OF_MOTORS];

// Arrays for motor states
double joint_positions[NUMBER_OF_MOTORS]; 
double joint_velocities[NUMBER_OF_MOTORS];
double joint_torques[NUMBER_OF_MOTORS];

// Create a vector of MotorControl elements, one for each motor
MotorControl* motors = new MotorControl[NUMBER_OF_MOTORS];

// A CAN message used to receive motor replies
CAN_message_t can_message;

// Variable to decide whether or not the motor gains have been set
bool motor_gains_set = false;

void setup() 
{
  // Set the baud rate for the serial communication
  //Serial.begin(500000);
  //while(!Serial);

  // Initialize CAN ports. 
  // DO NOT ATTEMPT TO SEND DATA OVER THE TEENSY CAN PORTS WITHOUT RUNNING CAN_PORT*.BEGIN()  
  can_port_1.begin();
  can_port_1.setBaudRate(MOTOR_BAUD_RATE);
  can_port_2.begin();
  can_port_2.setBaudRate(MOTOR_BAUD_RATE);
  //Serial.println("CAN PORTS SET");
  // Initialize the motor controllers
  for (int i = 0; i < NUMBER_OF_MOTORS; i++)
  {
    //Serial.println(i);
    // CAN port 1 should be used
    if(i < NUMBER_OF_MOTORS_PER_PORT)
    {
      motors[i] = MotorControl(i + 1, CAN_PORT_1, INITIAL_NUMBER_OF_MOTOR_ROTATIONS[i], POSITION_OFFSET[i]);
    }
    // CAN port 2 should be used
    else
    {
      motors[i] = MotorControl(i + 1, CAN_PORT_2, INITIAL_NUMBER_OF_MOTOR_ROTATIONS[i], POSITION_OFFSET[i]);
    }
    //Serial.println(i);
  }

  // Wait for the motor gains to be set
  uint8_t number_of_gains_per_motor = 6;
  //Serial.println("Motors initialized");
  // Try to set the PID gains of the motor
  while(motor_gains_set == false)
  {
    bool setting_gains_has_not_failed = true;

    for(int i = 0; i < NUMBER_OF_MOTORS; i++)
    {
      double k_p_pos = K_P_POS[i];
      double k_i_pos = K_I_POS[i];
      double k_p_vel = K_P_VEL[i];
      double k_i_vel = K_I_VEL[i];
      double k_p_torque = K_P_TOR[i];
      double k_i_torque = K_I_TOR[i];   
      
      if(!motors[i].writePIDParametersToRAM(k_p_pos, k_i_pos, k_p_vel, k_i_vel, k_p_torque, k_i_torque))
      {
        setting_gains_has_not_failed = false;
      }
    }

    motor_gains_set = setting_gains_has_not_failed;
    
    delay_microseconds(500000);
  }
  //Serial.println("Gains Set");
  // Empty the serial ports
  while(Serial.available())
  {
    Serial.read();
  }
}

void loop() 
{   
  // Empty the CAN buffers
  while(can_port_1.read(can_message));
  while(can_port_2.read(can_message));

  // Check if any new control commands have been received
  if(serial_interface.areControlCommandsAvailable())
  {
    // Update the control signals
    serial_interface.receiveControlCommands(control_command_type, control_commands);
    
    // Send control commands to all of the motors
    for(int i = 0; i < NUMBER_OF_MOTORS; i++)
    {
      if(control_command_type == 1.0)
      {
        motors[i].setPositionReference(control_commands[i]);
      }
      else if(control_command_type == 2.0)
      {
        motors[i].setSpeedReference(control_commands[i]);
      }
      else if(control_command_type == 3.0)
      {
        motors[i].setTorqueReference(control_commands[i]);
      }
      else
      {
        motors[i].requestMotorStatus();
      }
    } 
  }
  else
  {
    for(int i = 0; i < NUMBER_OF_MOTORS; i++)
    {
      motors[i].requestMotorStatus();
    }
  }

  // Wait for replies from the CAN ports
  delay_microseconds(3000); // Can be 3000
  
  // Go through all the replies and update the motor states
  while(can_port_1.read(can_message) || can_port_2.read(can_message))
  {
    // The motor IDs are [1 - 32]. The lower numbers are used first
    uint8_t id = can_message.id - MOTOR_ADDRESS_OFFSET;
    
    // Check if the incomming message belongs to any of the motors
    if((id <= NUMBER_OF_MOTORS) && (id >= 1))
    {
      // Update states of motor with id
      motors[id - 1].readMotorControlCommandReply(can_message.buf);
    }
    else
    {
      //Serial.println("ERROR: No motor ID corresponds to the incomming message");
    }
  }
 
  // Update the motor states using the replies
  for(int i = 0; i < NUMBER_OF_MOTORS; i++)
  {
    joint_positions[i] = motors[i].getPosition();
    joint_velocities[i] = motors[i].getVelocity();
    joint_torques[i] = motors[i].getTorque();
  }
  //for(int i = 0; i < NUMBER_OF_MOTORS; i++){Serial.print(motors[i].getPosition()); Serial.print("\t");}
  //Serial.println("");
  // Send joint states back to the host PC
  serial_interface.sendStates(joint_positions, joint_velocities, joint_torques);

  // Sleep
  delay(TIMEOUT);
}
