#ifndef motor_control_h
#define motor_control_h

#include <Eigen30.h>
#include "motor_message_generator.h"
#include "FlexCAN_T4.h"

const double GEAR_REDUCTION = 6.0;
const double ROTATION_DISTANCE = 60.0;
#define MOTOR_ADDRESS_OFFSET 0x140

// A list of the different motor commands
#define MOTOR_COMMAND_READ_PID_PARAMETERS 0x30
#define MOTOR_COMMAND_WRITE_PID_PARAMETERS_TO_RAM 0x31
#define MOTOR_COMMAND_WRITE_PID_PARAMETERS_TO_ROM 0x32
#define MOTOR_COMMAND_READ_ACCELERATION 0x33
#define MOTOR_COMMAND_WRITE_ACCELERATION_TO_RAM 0x34
#define MOTOR_COMMAND_READ_ENCODER_POSITION 0x90
#define MOTOR_COMMAND_WRITE_ENCODER_POSITION_OFFSET 0x91
#define MOTOR_COMMAND_WRITE_CURRENT_POSITION_TO_ROM 0x19
#define MOTOR_COMMAND_READ_MULTI_TURN_ANGLE 0x92
#define MOTOR_COMMAND_READ_SINGLE_CIRCLE_COMMAND 0x94
#define MOTOR_COMMAND_READ_MOTOR_STATUS_1_AND_ERROR_FLAG 0x9A
#define MOTOR_COMMAND_CLEAR_MOTOR_ERROR_FLAG 0x9B
#define MOTOR_COMMAND_READ_MOTOR_STATUS_2 0x9C
#define MOTOR_COMMAND_READ_MOTOR_STATUS_3 0x9D
#define MOTOR_COMMAND_MOTOR_OFF 0x80
#define MOTOR_COMMAND_MOTOR_STOP 0x81
#define MOTOR_COMMAND_MOTOR_RUNNING 0x88
#define MOTOR_COMMAND_TORQUE_CLOSED_LOOP 0xA1
#define MOTOR_COMMAND_SPEED_CLOSED_LOOP 0xA2
#define MOTOR_COMMAND_POSITION_CLOSED_LOOP_1 0xA3
#define MOTOR_COMMAND_POSITION_CLOSED_LOOP_2 0xA4
#define MOTOR_COMMAND_POSITION_CLOSED_LOOP_3 0xA5
#define MOTOR_COMMAND_POSITION_CLOSED_LOOP_4 0xA6

class MotorControl
{
public:

    /// \brief Class constructor for a MotorControl class.
    /// Motor ID and CAN port are set
    /// \param[in] _id ID of the motor [1 - 32]
    MotorControl(uint8_t _id);

    /// \brief Set the desired multiturn motor angle.
    /// \param[in] _angle Setpoint motor angle in radians
    void setPositionReference(double _angle);

    //void setPositionReferenceReply();

    /// \brief Set the desired motor speed.
    /// \param[in] _speed Setpoint motor speed in radians/second
    void setSpeedReference(double _speed);

    /// \brief Set the desired motor torque.
    /// \param[in] _torque Setpoint motor torque in Nm
    void setTorqueReference(double _torque);

    /// \brief Setting position, speed and torque all results in
    /// the same reply being sent from the motor.
    /// The states are stored in the private variables
    void replyControlCommand(unsigned char* _can_message);

    /// \brief Set the motor PID parameters and store them temporary in RAM
    /// \param[in] _PIDParameters Motor PID parameters 
    /// (kp_pos, ki_pos, kp_speed, ki_speed, kp_torque, ki_torque)   
    void writePIDParametersToRAM(Eigen::Matrix<double, 6, 1> _PID_parameters);

    /// \brief Set the motor PID parameters and store the values in memory 
    /// \param[in] _PIDParameters Motor PID parameters 
    /// (kp_pos, ki_pos, kp_speed, ki_speed, kp_torque, ki_torque)   
    void writePIDParametersToROM(Eigen::Matrix<double, 6, 1> _PID_parameters);

    /// \brief Stop the motor without turning it off
    void stopMotor();

    /// \brief Turn off the motor
    void turnOffMotor();

    /// \brief Read the motor PID parameters
    /// The private PID parameters are updated
    /// \return PID parameters as 6 dimensional vector
    /// (kp_pos, ki_pos, kp_speed, ki_speed, kp_torque, ki_torque)
    Eigen::Matrix<double, 6, 1> readPIDParameters();

    /// \brief Read the current motor position
    /// \return Motor position in radians
    double readCurrentPosition();

private:
    /// \brief Motor ID set through Serial configurator [1 - 32]
    uint8_t id;

    /// \brief Motor PID Parameters
    /// (kp_pos, ki_pos, kp_speed, ki_speed, kp_torque, ki_torque)
    Eigen::Matrix<double, 6, 1> PID_parameters;

    /// \brief Number of rotations of the built-in motor, 
    /// not the output-shaft
    double number_inner_motor_rotations;

    /// \brief Latest measured position of the shaft in radians
    double position;

    /// \brief Latest measured speed of the shaft in radians/second
    double speed;

    /// \brief Latest measured shaft torque in Nm
    double torque;

    /// \brief Latest measured temperature of the motor in degree Celsius
    double temperature;
    
    /// \brief CAN message for this motor.
    /// Only buf field is set for every message sent
    CAN_message_t can_message;

    /// \brief CAN port used for this motor.
    /// Port can be CAN1 or CAN2
    FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can_port;

    /// \brief Contains functions creating CAN messages 
    /// following the motor protocol
    MotorMessageGenerator motor_message_generator;



    // Motor specific parameters

    /// \brief The torque in Newton meter has to lie in the interval
    /// [-max_torque, max_torque]
    double max_torque = 30.0;

    /// \brief All torque values are scaled to the interval 
    /// [-max_torque_current, max_torque_current]
    int max_torque_current = 2000;

    /// \brief The 16 bit encoder measures the position 
    /// of the inner DC motor, not the shaft
    uint16_t max_encoder_value = 65536;
};

void delay_microseconds(double microsecond_delay);

#endif