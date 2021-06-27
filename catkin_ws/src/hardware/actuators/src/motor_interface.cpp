/*******************************************************************/
/*    AUTHOR: Paal Arthur S. Thorseth                              */
/*    ORGN:   Dept of Eng Cybernetics, NTNU Trondheim              */
/*    FILE:   motor_interface.cpp                                  */
/*    DATE:   Apr 23, 2021                                         */
/*                                                                 */
/* Copyright (C) 2021 Paal Arthur S. Thorseth,                     */
/*                    Adrian B. Ghansah                            */
/*                                                                 */
/* This program is free software: you can redistribute it          */
/* and/or modify it under the terms of the GNU General             */
/* Public License as published by the Free Software Foundation,    */
/* either version 3 of the License, or (at your option) any        */
/* later version.                                                  */
/*                                                                 */
/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty     */
/* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         */
/* See the GNU General Public License for more details.            */
/*                                                                 */
/* You should have received a copy of the GNU General Public       */
/* License along with this program. If not, see                    */
/* <https://www.gnu.org/licenses/>.                                */
/*                                                                 */
/*******************************************************************/

#include <actuators/motor_interface.h>

// Constructor
MotorInterface::MotorInterface() :
    NUM_MOTORS { 12 }
{
    this->InitRos();
    //this->InitRosQueueThreads();

    this->num_motors_port_1 = MAX_NUM_MOTORS_PER_PORT;

    this->num_motors_port_2 = MAX_NUM_MOTORS_PER_PORT;

    this->serialInterface1.SetPort("/dev/ttyACM0");

    this->serialInterface2.SetPort("/dev/ttyACM1");

    this->serialInterface1.InitLibSerial();

    this->serialInterface2.InitLibSerial();
}

MotorInterface::MotorInterface(const int &_num_motors) :
    NUM_MOTORS { _num_motors }
{
    this->InitRos();
    //this->InitRosQueueThreads();


    // Check whether we should use one or two parts
    if(_num_motors <= this->MAX_NUM_MOTORS_PER_PORT)
    {
        this->num_motors_port_1 = _num_motors;

        this->serialInterface1.SetPort("/dev/ttyACM0");

        this->serialInterface1.SetNumberOfMotors(_num_motors);

        this->serialInterface1.InitLibSerial();
    }
    else
    {
        num_motors_port_1 = MAX_NUM_MOTORS_PER_PORT;
        
        num_motors_port_2 = _num_motors - num_motors_port_1;

        this->serialInterface1.SetPort("/dev/ttyACM0");

        this->serialInterface1.SetNumberOfMotors(num_motors_port_1);

        this->serialInterface2.SetPort("/dev/ttyACM1");

        this->serialInterface2.SetNumberOfMotors(num_motors_port_2);

        this->serialInterface1.InitLibSerial();

        this->serialInterface2.InitLibSerial();
    }


}

// Destructor 
MotorInterface::~MotorInterface() 
{
    this->rosNode->shutdown();
    /*
    this->rosProcessQueue.clear();
    this->rosProcessQueue.disable();
    this->rosProcessQueueThread.join();

    this->rosPublishQueue.clear();
    this->rosPublishQueue.disable();
    this->rosPublishQueueThread.join();
    */
}

void MotorInterface::SetJointPositions(const std::vector<double> &_pos)
{
    if (this->NUM_MOTORS <= MAX_NUM_MOTORS_PER_PORT)
    {
        this->serialInterface1.SendMessage(SerialCommunication::ControlMode::position, _pos);
    }
    else
    {
        std::vector<double> commands_port_1(_pos.begin(), _pos.begin() + this->num_motors_port_1);
        std::vector<double> commands_port_2(_pos.begin() + this->num_motors_port_1, _pos.end());

        serialInterface1.SendMessage(SerialCommunication::ControlMode::position, commands_port_1);
        serialInterface2.SendMessage(SerialCommunication::ControlMode::position, commands_port_2);
    }
}

void MotorInterface::SetJointVelocities(const std::vector<double> &_vel)
{
    if (this->NUM_MOTORS <= MAX_NUM_MOTORS_PER_PORT)
    {
        this->serialInterface1.SendMessage(SerialCommunication::ControlMode::velocity, _vel);
    }
    else
    {
        std::vector<double> commands_port_1(_vel.begin(), _vel.begin() + this->num_motors_port_1);
        std::vector<double> commands_port_2(_vel.begin() + this->num_motors_port_1, _vel.end());

        serialInterface1.SendMessage(SerialCommunication::ControlMode::velocity, commands_port_1);
        serialInterface2.SendMessage(SerialCommunication::ControlMode::velocity, commands_port_2);
    }
}

void MotorInterface::SetJointTorques(const std::vector<double> &_torque)
{
    if (this->NUM_MOTORS <= MAX_NUM_MOTORS_PER_PORT)
    {
        this->serialInterface1.SendMessage(SerialCommunication::ControlMode::torque, _torque);
    }
    else
    {
        std::vector<double> commands_port_1(_torque.begin(), _torque.begin() + this->num_motors_port_1);
        std::vector<double> commands_port_2(_torque.begin() + this->num_motors_port_1, _torque.end());

        serialInterface1.SendMessage(SerialCommunication::ControlMode::torque, commands_port_1);
        serialInterface2.SendMessage(SerialCommunication::ControlMode::torque, commands_port_2);
    }
}

void MotorInterface::ProcessSerialMessages()
{
    if(serialInterface1.IsNewDataAvailable())
    {
        Eigen::Matrix<Eigen::VectorXd, 3, 1> joint_states = serialInterface1.ReceiveMessage();

        

        std_msgs::Float64MultiArray joint_state_positions;
        std_msgs::Float64MultiArray joint_state_velocities;
        std_msgs::Float64MultiArray joint_state_torques;

        tf::matrixEigenToMsg(joint_states(0), joint_state_positions);
        tf::matrixEigenToMsg(joint_states(1), joint_state_velocities);
        tf::matrixEigenToMsg(joint_states(2), joint_state_torques);
        
        sensor_msgs::JointState joint_state_msg;

        joint_state_msg.position = joint_state_positions.data;
        joint_state_msg.velocity = joint_state_velocities.data;
        joint_state_msg.effort = joint_state_torques.data;

        joint_state_msg.header.stamp = ros::Time::now();

        jointStatePublisher.publish(joint_state_msg);
    }
}
/*
// Publish function for ROS Joint State Torque messages
void MotorInterface::PublishJointStateMsg()
{
    // Declare msg
    sensor_msgs::JointState joint_state_msg;    

    // Set dimension
    joint_state_msg.effort.resize(12);

    // Front-left 
    //joint_state_msg.effort[0] = _desired_tau(0);
    //joint_state_msg.effort[1] = _desired_tau(1);
    //joint_state_msg.effort[2] = _desired_tau(2);

    //// Front right
    //joint_state_msg.effort[3] = _desired_tau(3);
    //joint_state_msg.effort[4] = _desired_tau(4);
    //joint_state_msg.effort[5] = _desired_tau(5);

    //// Rear left
    //joint_state_msg.effort[6] = _desired_tau(6);
    //joint_state_msg.effort[7] = _desired_tau(7);
    //joint_state_msg.effort[8] = _desired_tau(8);

    //// Rear right
    //joint_state_msg.effort[9] = _desired_tau(9);
    //joint_state_msg.effort[10] = _desired_tau(10);
    //joint_state_msg.effort[11] = _desired_tau(11);

    // Set timestamp
    joint_state_msg.header.stamp = ros::Time::now();

    // Publish
    this->jointStatePub.publish(joint_state_msg);
}
*/
// Callback for ROS Contact State messages
void MotorInterface::OnJointStateCmdMsg(const sensor_msgs::JointStateConstPtr &_msg)
{
    if ((!_msg->position.empty()) && (_msg->position.size() == this->NUM_MOTORS))
    {
        this->SetJointPositions(_msg->position);
    } 

    else if ((!_msg->velocity.empty()) && (_msg->velocity.size() == this->NUM_MOTORS))
    {
        this->SetJointVelocities(_msg->velocity);
    }

    else if ((!_msg->effort.empty()) && (_msg->effort.size() == this->NUM_MOTORS))
    {
        this->SetJointTorques(_msg->effort);
    }
    else
	{
        ROS_ERROR("[MotorInterface::OnJointStateCmdMsg] Message failed to match specifications!");
    }
}
/*
// Setup thread to process messages
void MotorInterface::SerialProcessQueueThread()
{
    ros::Rate loop_rate(1000);

    while (this->rosNode->ok())
    {

        loop_rate.sleep();
    }
}
*//*
// Setup thread to publish messages
void MotorInterface::SerialPublishQueueThread()
{
    ros::Rate loop_rate(1000);

    while (this->rosNode->ok())
    {

        loop_rate.sleep();
    }
}
*//*
// Setup thread to process messages
void MotorInterface::RosProcessQueueThread()
{
    static const double timeout = 0.01;
    while (this->rosNode->ok())
    {
        this->rosProcessQueue.callAvailable(ros::WallDuration(timeout));
    }
}
*//*
// Setup thread to publish messages
void MotorInterface::RosPublishQueueThread()
{
    static const double timeout = 0.01;
    while (this->rosNode->ok())
    {
        this->rosPublishQueue.callAvailable(ros::WallDuration(timeout));
    }
}
*/
// Initialize ROS
void MotorInterface::InitRos()
{
    if (!ros::isInitialized())
    {
        int argc = 0;
        char **argv = NULL;
        ros::init(
            argc,
            argv,
            "motor_interface_node",
            ros::init_options::NoSigintHandler
        );
    }

    this->rosNode.reset(new ros::NodeHandle("motor_interface_node"));
    /*
    ros::SubscribeOptions joint_state_cmd_so = 
        ros::SubscribeOptions::create<sensor_msgs::JointState>(
            "/my_robot/data",
            1,
            boost::bind(&MotorInterface::OnJointStateCmdMsg, this, _1),
            ros::VoidPtr(),
            &this->rosProcessQueue
            );


    ros::AdvertiseOptions joint_state_ao =
        ros::AdvertiseOptions::create<sensor_msgs::JointState>(
            "/motor/data",
            1,
            ros::SubscriberStatusCallback(),
            ros::SubscriberStatusCallback(),
            ros::VoidPtr(),
            &this->rosPublishQueue
        );

    this->jointStateCmdSub = this->rosNode->subscribe(joint_state_cmd_so);

    this->jointStatePub = this->rosNode->advertise(joint_state_ao);
    */
    this->jointStatePublisher = this->rosNode->advertise<sensor_msgs::JointState>("/motor/states", 1);

    this->jointCommandSubscriber = this->rosNode->subscribe
    (
        "/my_robot/joint_state_cmd",
        1,
        &MotorInterface::OnJointStateCmdMsg,
        this
    );
}
/*
// Initialize ROS Publish and Process Queue Threads
void MotorInterface::InitRosQueueThreads()
{
    this->serialPublishQueueThread = std::thread(
        std::bind(&MotorInterface::SerialPublishQueueThread, this)
    );

    this->serialProcessQueueThread = std::thread(
        std::bind(&MotorInterface::SerialProcessQueueThread, this)
    );

    this->rosPublishQueueThread = std::thread(
        std::bind(&MotorInterface::RosPublishQueueThread, this)
    );

    this->rosProcessQueueThread = std::thread(
        std::bind(&MotorInterface::RosProcessQueueThread, this)
    );
}
*/