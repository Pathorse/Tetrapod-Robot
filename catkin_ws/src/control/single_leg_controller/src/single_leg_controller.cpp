#include "single_leg_controller/single_leg_controller.h"

SingleLegController::SingleLegController(double _publish_frequency)
{
    publish_frequency = _publish_frequency;

    swing_period = final_iteration/publish_frequency;

    for(int i = 0; i < 3; i++)
    {
        q(i) = uninitialized_state;
    }

    q_ref[0] = math_utils::HALF_PI;
    q_ref[1] = 0.0;
    q_ref[2] = 0.0;

    k_p_pos_hy = 1.0;
    k_i_pos_hy = 1.0;

    k_p_pos_hp = 1.0;
    k_i_pos_hp = 1.0;

    k_p_pos_kp = 1.0;
    k_i_pos_kp = 1.0;

    k_p_torque_hy = 100.0;
    k_i_torque_hy = 5.0;
    k_d_torque_hy = 20.0;

    k_p_torque_hp = 100.0;
    k_i_torque_hp = 5.0;
    k_d_torque_hp = 20.0;

    k_p_torque_kp = 100.0;
    k_i_torque_kp = 5.0;
    k_d_torque_kp = 20.0;

    K_p(0, 0) = k_p_torque_hy;
    K_p(1, 1) = k_p_torque_hp;
    K_p(2, 2) = k_p_torque_kp;

    K_d(0, 0) = k_d_torque_hy;
    K_d(1, 1) = k_d_torque_hp;
    K_d(2, 2) = k_d_torque_kp;

    /*
    K_p(0, 0) = 200.0;
    K_p(1, 1) = 200.0;
    K_p(2, 2) = 200.0;

    K_d(0, 0) = 40.0;
    K_d(1, 1) = 40.0;
    K_d(2, 2) = 40.0;
    */

    swing_start_time = - 2.0*swing_period;  

    for(int i = 0; i < 3; i++)
    {        
        joint_state_log_msg.position.push_back(CONTROL_IDLE);
        joint_state_log_msg.velocity.push_back(CONTROL_IDLE);
        joint_state_log_msg.effort.push_back(CONTROL_IDLE);

        joint_reference_log_msg.position.push_back(CONTROL_IDLE);
        joint_reference_log_msg.velocity.push_back(CONTROL_IDLE);
        joint_reference_log_msg.effort.push_back(CONTROL_IDLE);
    }
}

/*** ROS FUNCTIONS ***/

void SingleLegController::initROS()
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
            "single_leg_controller_node",
            ros::init_options::NoSigintHandler
        );
    }

    // WHY DO THIS?
    node_handle.reset(new ros::NodeHandle("single_leg_controller_node"));

    // Initialize the generalized coordinates subscriber
    generalized_coordinates_subscriber = node_handle->subscribe
    (
        "/my_robot/gen_coord", 
        100, 
        &SingleLegController::generalizedCoordinatesCallback,
        this
    );

    generalized_velocities_subscriber = node_handle->subscribe
    (
        "/my_robot/gen_vel",
        100,
        &SingleLegController::generalizedVelocitiesCallback,
        this
    );

    // Initialize the ready to move subscriber
    ready_to_proceed_subscriber = node_handle->subscribe
    (
        "/ready_to_proceed",
        1, 
        &SingleLegController::readyToProceedCallback,
        this
    );

    joint_setpoint_subscriber = node_handle->subscribe
    (
        "/controller/joint_setpoints",
        1,
        &SingleLegController::jointSetpointCallback,
        this
    ); 

    // Initialize the joint state publisher
    joint_state_publisher = node_handle->advertise<sensor_msgs::JointState>("/my_robot/joint_state_cmd", 10);

    motor_gain_publisher = node_handle->advertise<std_msgs::Float64MultiArray>("/motor/gains", 10);

    // Initialize the state logging publisher
    log_joint_states_publisher = node_handle->advertise<sensor_msgs::JointState>("/logging/joint_states", 10);

    // Initialize the reference logging publisher
    log_joint_references_publisher = node_handle->advertise<sensor_msgs::JointState>("/logging/joint_references", 10);
}

void SingleLegController::generalizedCoordinatesCallback(const std_msgs::Float64MultiArrayConstPtr &_msg)
{
    for(int i = 0; i < 3; i++)
    {
        q(i) = _msg->data[i + 6];
    }
}

void SingleLegController::generalizedVelocitiesCallback(const std_msgs::Float64MultiArrayConstPtr &_msg)
{
    for(int i = 0; i < 3; i++)
    {
        q_d(i) = _msg->data[i + 6];
    }
}

void SingleLegController::readyToProceedCallback(const std_msgs::Bool &_msg)
{
    ready_to_proceed = _msg.data;
}

void SingleLegController::jointSetpointCallback(const std_msgs::Float64MultiArrayConstPtr &_msg)
{
    for(int i = 0; i < 3; i++)
    {
        q_ref(i) = _msg->data[i];
    }
}

/*** CONTROL FUNCTIONS ***/

Eigen::Matrix<double, 3, 1> SingleLegController::calculateSwingLegHeightTrajectory(double _percentage, double _period, double _max_swing_height, double _hip_height)
{
    double c = _max_swing_height;
    double a = c/pow(0.5, 4.0);
    double b = -2.0*a*pow(0.5, 2.0);

    Eigen::Matrix<double, 3, 1> trajectory;

    double x = _percentage - 0.5;

    trajectory(0) = a*pow(x, 4.0) + b*pow(x, 2.0) + c - _hip_height;

    trajectory(1) = (4.0*a*pow(x, 3.0) + 2.0*b*x)/_period;

    trajectory(2) = (12.0*a*pow(x, 2.0) + 2.0*b)/pow(_period, 2.0);

    return trajectory;
}

void SingleLegController::calculateSingleAxisTrajectory
(
    const double &_percentage, 
    const double &_period, 
    const double &_max_travel,
    double &_x, 
    double &_x_d, 
    double &_x_dd
)
{
    double a = 30.0*_max_travel;
    double b = -15.0*_max_travel;
    double c = 1.875*_max_travel;
    double d = -_max_travel*7.0/16.0;

    _x = 0.2*a*pow((_percentage - 0.5), 5.0) + b*pow((_percentage - 0.5), 3.0)/3.0 + c*_percentage + d;
    _x_d = (a*pow((_percentage - 0.5), 4.0) + b*pow((_percentage - 0.5), 2.0) + c)/_period;
    _x_dd = (4.0*a*pow((_percentage - 0.5), 3.0) + 2.0*b*(_percentage - 0.5))/(_period*_period);
}

void SingleLegController::updateSwingFootPositionTrajectory()
{
    double progress = current_iteration/final_iteration;

    double foot_dx;
    double foot_dy;
    double foot_vel_x;
    double foot_vel_y;
    double foot_acc_x;
    double foot_acc_y;

    calculateSingleAxisTrajectory(progress, swing_period, x_offset, foot_dx, foot_vel_x, foot_acc_x);
    calculateSingleAxisTrajectory(progress, swing_period, y_offset, foot_dy, foot_vel_y, foot_acc_y);
    Eigen::Matrix<double, 3, 1> z = calculateSwingLegHeightTrajectory(progress, swing_period, max_swing_height, hip_height);

    pos(0) = x_center - x_offset + foot_dx;
    pos(1) = y_center - y_offset + foot_dy;
    pos(2) = z(0);

    vel(0) = foot_vel_x;
    vel(1) = foot_vel_y;
    vel(2) = z(1);

    acc(0) = foot_acc_x;
    acc(1) = foot_acc_y;
    acc(2) = z(2);
}

void SingleLegController::updateStanceFootPositionTrajectory()
{
    double progress = current_iteration/final_iteration;

    double foot_dx;
    double foot_dy;
    double foot_vel_x;
    double foot_vel_y;
    double foot_acc_x;
    double foot_acc_y;

    calculateSingleAxisTrajectory(progress, swing_period, x_offset, foot_dx, foot_vel_x, foot_acc_x);
    calculateSingleAxisTrajectory(progress, swing_period, y_offset, foot_dy, foot_vel_y, foot_acc_y);

    pos(0) = x_center - foot_dx;
    pos(1) = y_center - foot_dy;
    pos(2) = - hip_height;

    if(current_iteration + 1 < final_iteration)
    {
        vel(0) = - foot_vel_x;
        vel(1) = - foot_vel_y;
        vel(2) = 0.0;

        acc(0) = - foot_acc_x;
        acc(1) = - foot_acc_y;
        acc(2) = 0.0;
    }
    else
    {
        vel(0) = 0.0;
        vel(1) = 0.0;
        vel(2) = 0.0;

        acc(0) = 0.0;
        acc(1) = 0.0;
        acc(2) = 0.0;
    }
}

void SingleLegController::updateJointReferences()
{
    Eigen::Matrix<double, 3, 1> zero = Eigen::Matrix<double, 3, 1>::Zero();
    if(!kinematics.SolveSingleLegInverseKinematics(false, zero, pos, q_ref))
    {
        ROS_WARN("[updateJointReferences] Failed to solve inverse kinematics");
    }

    Eigen::Matrix<double, 3, 3> J_s = kinematics.GetTranslationJacobianInB(Kinematics::LegType::frontLeft, Kinematics::BodyType::foot, q(0), q(1), q(2));
    Eigen::Matrix<double, 3, 3> J_s_d = kinematics.GetTranslationJacobianInBDiff(Kinematics::LegType::frontLeft, Kinematics::BodyType::foot, q(0), q(1), q(2), q_d(0), q_d(1), q_d(2));

    q_d_ref = J_s.inverse()*vel;

    q_dd_ref = J_s.inverse()*(acc - J_s_d*vel);
}

void SingleLegController::updateJointTorques
(
    const Eigen::Matrix<double, 3, 1> &_q_ref,
    const Eigen::Matrix<double, 3, 1> &_q_d_ref,
    const Eigen::Matrix<double, 3, 1> &_q_dd_ref,
    const Eigen::Matrix<double, 3, 1> &_q,
    const Eigen::Matrix<double, 3, 1> &_q_d
)
{
    Eigen::Matrix<double, 3, 1> normalized_joint_torques = _q_dd_ref - K_p*(_q - _q_ref) - K_d*(_q_d - _q_d_ref);

    Eigen::Matrix<double, 3, 3> M = kinematics.GetSingleLegMassMatrix(_q);

    Eigen::Matrix<double, 3, 1> b = kinematics.GetSingleLegCoriolisAndCentrifugalTerms(_q, _q_d);

    Eigen::Matrix<double, 3, 1> g = kinematics.GetSingleLegGravitationalTerms(_q);

    /*
    ROS_INFO("y_ref: %f\t y_d_ref: %f\t y_dd_ref: %f\t y: %f\t y_d: %f", _q_ref(1), _q_d_ref(1), _q_dd_ref(1), _q(1), _q_d(1));

    ROS_INFO(
        "Mass matrix:\n%f\t%f\t%f\t\n%f\t%f\t%f\t\n%f\t%f\t%f\t\n", 
        M(0, 0), M(0, 1), M(0, 2), M(1, 0), M(1, 1), M(1, 2), M(2, 0), M(2, 1), M(2, 2));


    ROS_INFO("z1: %f\tz2: %f\tz3: %f", normalized_joint_torques(0), normalized_joint_torques(1), normalized_joint_torques(2));
    ROS_INFO("b1: %f\tb2: %f\tb3: %f", b(0), b(1), b(2));
    ROS_INFO("g1: %f\tg2: %f\tg3: %f", g(0), g(1), g(2));
    */

    tau = b + g + M*normalized_joint_torques;
}

void SingleLegController::updateJointTorques()
{
    updateJointTorques(q_ref, q_d_ref, q_dd_ref, q, q_d);
}

void SingleLegController::sendTorqueCommand()
{
    std_msgs::Float64MultiArray torque_msg;

    tf::matrixEigenToMsg(tau, torque_msg);

    sensor_msgs::JointState joint_state_msg;

    joint_state_msg.header.stamp = ros::Time::now();

    joint_state_msg.effort = torque_msg.data;

    joint_state_publisher.publish(joint_state_msg);
}

void SingleLegController::sendPositionCommand()
{
    bool cancel_position_command = false;

    // Check if the desired reference setpoints violates the joint angle constraints
    if((q_ref[0] < math_utils::degToRad(15)) || (q_ref[0] > math_utils::degToRad(165)))
    {
        cancel_position_command = true;
    }
    else if((q_ref[1] < math_utils::degToRad(-45)) || (q_ref[1] > math_utils::degToRad(45)))
    {
        cancel_position_command = true;
    }
    else if((q_ref[2] < math_utils::degToRad(-110)) || (q_ref[2] > math_utils::degToRad(110)))
    {
        cancel_position_command = true;
    }

    
    if (cancel_position_command == true)
    {
        // If the constraints were violated, cancel the command and report the error
        ROS_WARN("Position setpoint violates joint limits. Command canceled.");
    }
    else
    {
        // Create a joint position command and send it to the motors
        std_msgs::Float64MultiArray position_msg;

        tf::matrixEigenToMsg(q_ref, position_msg);

        sensor_msgs::JointState joint_state_msg;

        joint_state_msg.header.stamp = ros::Time::now();

        joint_state_msg.position = position_msg.data;

        joint_state_publisher.publish(joint_state_msg);
    }
}

void SingleLegController::updateJointSetpoints()
{
    updateJointReferences();
    q_d_ref = Eigen::Matrix<double, 3, 1>::Zero();
    q_dd_ref = Eigen::Matrix<double, 3, 1>::Zero();
}


bool SingleLegController::moveJointsToSetpoints()
{
    ros::Rate command_send_rate(120.0);

    bool is_joint_target_reached = false;

    while(!is_joint_target_reached)
    {
        /*
        if(isTargetPositionReached()) // 5 degrees for each joint + speed
        {
            //is_joint_target_reached = true;

            ROS_WARN("GOAL REACHED");
        }
        else
        {
            ros::spinOnce();

            updateJointTorques();

            sendTorqueCommand();

            command_send_rate.sleep();
        }
        */
        ros::spinOnce();

        updateJointTorques();

        printTorques();

        sendTorqueCommand();

        command_send_rate.sleep();
    }

    return true;
}

bool SingleLegController::moveJointsToCenter()
{
    ros::Rate command_send_rate(publish_frequency);

    swing_percentage = 0.0;

    updateStanceFootPositionTrajectory();

    updateJointReferences();

    q_d_ref = Eigen::Matrix<double, 3, 1>::Zero();

    q_dd_ref = Eigen::Matrix<double, 3, 1>::Zero();

    bool is_joint_target_reached = false;

    while(!is_joint_target_reached)
    {
        if(isTargetPositionReached() && readyToProceed())
        {
            is_joint_target_reached = true;
            ROS_INFO("Print Target Reached");
            command_send_rate.sleep();
        }
        else
        {
            ros::spinOnce();

            updateJointTorques();

            sendTorqueCommand();

            command_send_rate.sleep();
        }
    }

    return true;
}

bool SingleLegController::updateSimpleFootTrajectory()
{
    double foot_dx;
    double foot_vel_x;
    double foot_acc_x;

    calculateSingleAxisTrajectory(current_iteration/final_iteration, swing_period, x_offset, foot_dx, foot_vel_x, foot_acc_x);

    pos(0) = x_center - foot_dx;
    pos(1) = y_center;
    pos(2) = - hip_height;

    if(current_iteration + 1 < final_iteration)
    {
        vel(0) = - foot_vel_x;
        vel(1) = 0.0;
        vel(2) = 0.0;

        acc(0) = - foot_acc_x;
        acc(1) = 0.0;
        acc(2) = 0.0;
    }
    else
    {
        vel(0) = 0.0;
        vel(1) = 0.0;
        vel(2) = 0.0;

        acc(0) = 0.0;
        acc(1) = 0.0;
        acc(2) = 0.0;
    }

    return true;
}

void SingleLegController::increaseIterator()
{
    current_iteration++;
    if(current_iteration >= final_iteration)
    {
        current_iteration = final_iteration;
    }
}

void SingleLegController::updateState()
{
    current_iteration++;

    if(current_iteration >= final_iteration)
    {
        current_iteration = 0.0;

        if(state != State::stance)
        {
            state = State::stance;
        }
        else
        {
            state = State::swing;
        }
    }
    
}

void SingleLegController::updateFootReference()
{
    if(state == State::swing)
    {
        ROS_INFO("SWING");
        updateSwingFootPositionTrajectory();
    }
    else
    {
        ROS_INFO("STANCE");
        updateStanceFootPositionTrajectory();
    }
}


/*** HELPER FUNCTIONS ***/

void SingleLegController::checkForNewMessages()
{
    ros::spinOnce();
}

bool SingleLegController::isTargetPositionReached()
{
    Eigen::Matrix<double, 3, 1> joint_error = q - q_ref;
    double q_speed = q_d.transpose()*q_d;
    ROS_INFO("q: %f, %f, %f\t q_ref: %f, %f, %f\t Error q_d: %f", q(0), q(1), q(2), q_ref(0), q_ref(1), q_ref(2), q_speed);
    if((joint_error.transpose()*joint_error < POSITION_CONVERGENCE_CRITERIA) && (q_speed < 0.050))
    {
        ROS_INFO("Target Reached");
        return true;
    }
    else
    {
        return false;
    }
}

bool SingleLegController::isJointVelocitySmall()
{
    if(q_d.transpose()*q_d < 0.010)
    {
        return true;
    }
    else
    {
        ROS_INFO("Velocity is too large");
        return false;
    }
}

bool SingleLegController::initialStateReceived()
{
    if(q(0) == uninitialized_state)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void SingleLegController::printTorques()
{
    ROS_INFO("T1: %f\tT2: %f\tT3: %f", tau(0), tau(1), tau(2));
}

void SingleLegController::printSpatialTrajectories()
{
    ROS_INFO("Pos: %f, %f, %f\tVel: %f, %f, %f\tAcc: %f, %f, %f", pos(0), pos(1), pos(2), vel(0), vel(1), vel(2), acc(0), acc(1), acc(2));
}

void SingleLegController::printJointTrajectories()
{
    ROS_INFO("q_ref: %f, %f, %f\tq_d_ref: %f, %f, %f\tq_dd_ref: %f, %f, %f", q_ref(0), q_ref(1), q_ref(2), q_d_ref(0), q_d_ref(1), q_d_ref(2), q_dd_ref(0), q_dd_ref(1), q_dd_ref(2));
}

void SingleLegController::printPercentage()
{
    ROS_INFO("Progress: %f", swing_percentage);
}

void SingleLegController::printAllStates()
{
    ROS_INFO("I: %.0f\tP: %.3f, %.3f, %.3f\tV: %.3f, %.3f, %.3f\tA: %.3f, %.3f, %.3f\tq_r: %.3f, %.3f, %.3f\tq_d_r: %.3f, %.3f, %.3f\tq_dd_r: %.3f, %.3f, %.3f\tq: %.3f, %.3f, %.3f\tq_d: %.3f, %.3f, %.3f\tT:  %.3f, %.3f, %.3f",
    current_iteration,
    pos(0), pos(1), pos(2), vel(0), vel(1), vel(2), acc(0), acc(1), acc(2),
    q_ref(0), q_ref(1), q_ref(2), q_d_ref(0), q_d_ref(1), q_d_ref(2), q_dd_ref(0), q_dd_ref(1), q_dd_ref(2),
    q(0), q(1), q(2), q_d(0), q_d(1), q_d(2),
    tau(0), tau(1), tau(2)
    );
}

void SingleLegController::writeToLog()
{
    // Fill the log messages with the latest data
    
    for(int i = 0; i < 3; i++)
    {
        joint_state_log_msg.position[i] = q(i);
        joint_state_log_msg.velocity[i] = q_d(i);
        joint_state_log_msg.effort[i] = tau(i);

        joint_reference_log_msg.position[i] = q_ref(i);
        joint_reference_log_msg.velocity[i] = q_d_ref(i);
        joint_reference_log_msg.effort[i] = q_dd_ref(i);
    }
    
    // Update the time stamps
    joint_state_log_msg.header.stamp = ros::Time::now();
    joint_reference_log_msg.header.stamp = ros::Time::now();

    log_joint_states_publisher.publish(joint_state_log_msg);
    log_joint_references_publisher.publish(joint_reference_log_msg);
}

void SingleLegController::setMotorGains()
{
    ros::Rate set_gains_rate(0.4);

    gains_set = false;

    std_msgs::Float64MultiArray motor_gain_msg;

    motor_gain_msg.data.push_back(k_p_pos_hy);
    motor_gain_msg.data.push_back(k_i_pos_hy);
    motor_gain_msg.data.push_back(0.0);
    motor_gain_msg.data.push_back(0.0);
    motor_gain_msg.data.push_back(k_p_torque_hy);
    motor_gain_msg.data.push_back(k_i_torque_hy);

    motor_gain_msg.data.push_back(k_p_pos_hp);
    motor_gain_msg.data.push_back(k_i_pos_hp);
    motor_gain_msg.data.push_back(0.0);
    motor_gain_msg.data.push_back(0.0);
    motor_gain_msg.data.push_back(k_p_torque_hp);
    motor_gain_msg.data.push_back(k_i_torque_hp);

    motor_gain_msg.data.push_back(k_p_pos_kp);
    motor_gain_msg.data.push_back(k_i_pos_kp);
    motor_gain_msg.data.push_back(0.0);
    motor_gain_msg.data.push_back(0.0);
    motor_gain_msg.data.push_back(k_p_torque_kp);
    motor_gain_msg.data.push_back(k_i_torque_kp);

    while(!gains_set)
    {
        motor_gain_publisher.publish(motor_gain_msg);

        ROS_INFO("Waiting for gains set confirmation message");

        set_gains_rate.sleep();

        ros::spinOnce();
    }

    ROS_INFO("Gains set sucessfully");
}