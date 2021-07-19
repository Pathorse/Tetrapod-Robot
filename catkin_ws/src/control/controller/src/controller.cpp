#include "controller/controller.h"

void Controller::waitForReadyToProceed()
{
    ros::Rate rate(5);

    while(!ready_to_proceed)
    {
        ros::spinOnce();

        rate.sleep();
    }

    ready_to_proceed = false;
}

bool Controller::UpdateJointCommands()
{
    bool solved_fl, solved_fr, solved_rl, solved_rr;

    Eigen::Vector3d single_leg_joint_angles = Eigen::Vector3d::Zero();

    solved_fl = kinematics.SolveSingleLegInverseKinematics(false, Eigen::Vector3d::Zero(), fl_position_body, single_leg_joint_angles);

    if (solved_fl)
    {
        joint_angle_commands.block<3, 1>(0, 0) = single_leg_joint_angles;
    }
    
    solved_fr = kinematics.SolveSingleLegInverseKinematics(true, Eigen::Vector3d::Zero(), fr_position_body, single_leg_joint_angles);

    if (solved_fr)
    {
        joint_angle_commands.block<3, 1>(3, 0) = single_leg_joint_angles;
    }

    solved_rl = kinematics.SolveSingleLegInverseKinematics(true, Eigen::Vector3d::Zero(), rl_position_body, single_leg_joint_angles);

    if (solved_rl)
    {
        joint_angle_commands.block<3, 1>(6, 0) = single_leg_joint_angles;
    }

    solved_rr = kinematics.SolveSingleLegInverseKinematics(false, Eigen::Vector3d::Zero(), rr_position_body, single_leg_joint_angles);

    if (solved_rr)
    {
        joint_angle_commands.block<3, 1>(9, 0) = single_leg_joint_angles;
    }

    return solved_fl && solved_fr && solved_rl && solved_rr;
}

bool Controller::sendJointPositionCommands()
{
    // Check if the position commands are within the joint angle limit constraints
    if (this->kinematics.ValidateSolution(this->joint_angle_commands) == false)
    {
        // If they are violated report the problem and don't send a command to the motors
        ROS_WARN("Position setpoint violates joint limits. Command canceled.");

        return false;
    }
    else // If the joint angles are valid
    {
        // Create a joint state message
        sensor_msgs::JointState joint_state_msg;

        // Set the time of the joint state message
        joint_state_msg.header.stamp = ros::Time::now();

        // Create a float array for joint angle commands
        std_msgs::Float64MultiArray position_commands;

        // Put the joint angle reference vector into the position command message
        tf::matrixEigenToMsg(this->joint_angle_commands, position_commands);

        // Add the position commands to the joint state message
        joint_state_msg.position = position_commands.data;

        // Publish the message
        joint_command_publisher.publish(joint_state_msg);

        return true;
    }
}

void Controller::setInitialConfiguration()
{
    Eigen::Vector3d height_offset(0, 0, -0.25);

    fl_position_body = fl_offset + height_offset;
    
    fr_position_body = fr_offset + height_offset;
    
    rl_position_body = rl_offset + height_offset;
    
    rr_position_body = rr_offset + height_offset;

    UpdateJointCommands();

    sendJointPositionCommands();
}

//ROS related functions
void Controller::initROS()
{
    // Check if ROS has been initialized. If false, initialize ROS.
    if(!ros::isInitialized())
    {
        int argc = 0;
        char **argv = NULL;
        ros::init
        (
            argc,
            argv,
            node_name,
            ros::init_options::NoSigintHandler
        );
    }

    nodeHandle.reset(new ros::NodeHandle(node_name));

    // Initialize the ready to move subscriber
    ready_to_proceed_subscriber = nodeHandle->subscribe
    (
        "/ready_to_proceed",
        1, 
        &Controller::readyToProceedCallback,
        this
    );

    this->joint_state_subscriber = nodeHandle->subscribe
    (
        "/my_robot/joint_state",
        1,
        &Controller::jointStateCallback,
        this
    );

    this->twist_command_subscriber = nodeHandle->subscribe
    (
        "/twist_command",
        1,
        &Controller::TwistCommandCallback,
        this
    );

    this->twist_state_subscriber = nodeHandle->subscribe
    (
        "/twist_state",
        1,
        &Controller::TwistStateCallback,
        this
    );

    // Initialize the joint command publisher
    joint_command_publisher = nodeHandle->advertise<sensor_msgs::JointState>("/" + robot_name + "/joint_state_cmd", 1);

    // Initialize the joint state logger
    joint_state_logger = nodeHandle->advertise<sensor_msgs::JointState>("/log/joint_states", 1);

    // Initialize the joint command logger
    joint_command_logger = nodeHandle->advertise<sensor_msgs::JointState>("/log/joint_commands", 1);
}

void Controller::readyToProceedCallback(const std_msgs::Bool &msg)
{
    ready_to_proceed = msg.data;
}

void Controller::jointStateCallback(const sensor_msgs::JointStatePtr &msg)
{
    for(int i = 0; i < 12; i++)
    {
        // Store the jont angles
        this->joint_angles(i) = msg->position[i];

        // Store the joint velocities
        this->joint_velocities(i) = msg->velocity[i];

        // Store th joint torques
        this->joint_torques(i) = msg->effort[i];
    }
}

void Controller::TwistCommandCallback(const geometry_msgs::TwistConstPtr &_msg)
{
    this->lin_vel_x = _msg->linear.x;
    this->lin_vel_y = _msg->linear.y;
    this->ang_vel_z = _msg->angular.z; 
}

void Controller::TwistStateCallback(const geometry_msgs::TwistConstPtr &_msg)
{
    this->_lin_vel_x_estimated = _msg->linear.x;
    this->_lin_vel_y_estimated = _msg->linear.y;
    this->_ang_vel_z_estimated = _msg->angular.z;
}

void Controller::SetTwistCommand(double lin_vel_cmd_x, double lin_vel_cmd_y, double ang_vel_cmd_z)
{
    this->lin_vel_x = lin_vel_cmd_x;
    this->lin_vel_y = lin_vel_cmd_y;
    this->ang_vel_z = ang_vel_cmd_z;
}

void Controller::WriteToLog()
{
    // Creating a general joint state message

    sensor_msgs::JointState joint_state_msg;

    std_msgs::Float64MultiArray joint_state_position;
    std_msgs::Float64MultiArray joint_state_velocity;
    std_msgs::Float64MultiArray joint_state_torque;

    joint_state_msg.header.stamp = ros::Time::now();

    // Fill in data for the joint state message

    tf::matrixEigenToMsg(this->joint_angles, joint_state_position);
    tf::matrixEigenToMsg(this->joint_velocities, joint_state_velocity);
    tf::matrixEigenToMsg(this->joint_torques, joint_state_torque);

    joint_state_msg.position = joint_state_position.data;
    joint_state_msg.velocity = joint_state_velocity.data;
    joint_state_msg.effort = joint_state_torque.data;

    // Publish the joint state message

    joint_state_logger.publish(joint_state_msg);

    // Fill in data for the joint command message

    tf::matrixEigenToMsg(this->joint_angle_commands, joint_state_position);
    tf::matrixEigenToMsg(this->joint_velocity_commands, joint_state_velocity);
    tf::matrixEigenToMsg(this->joint_torque_commands, joint_state_torque);

    joint_state_msg.position = joint_state_position.data;
    joint_state_msg.velocity = joint_state_velocity.data;
    joint_state_msg.effort = joint_state_torque.data;

    // Publish the joint command message

    joint_command_logger.publish(joint_state_msg);
    
}