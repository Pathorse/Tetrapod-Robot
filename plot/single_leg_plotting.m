close all
clear
clc

timestamp = "2021-05-24-13-35-45";

number_of_motors = 3;

%% Extract ROSbag data
state_bag = rosbag("states_" + timestamp + ".bag");
reference_bag = rosbag("references_" + timestamp + ".bag");

%% Initialize state vectors 
state_time = get_time_states(state_bag);
joint_pos_state = get_joint_position_states(state_bag, number_of_motors);
joint_vel_state = get_joint_velocity_states(state_bag, number_of_motors);
joint_torques = get_joint_effort_states(state_bag, number_of_motors);

reference_time = get_time_states(reference_bag);
joint_pos_reference = get_joint_position_states(reference_bag, number_of_motors);
joint_vel_reference = get_joint_velocity_states(reference_bag, number_of_motors);
joint_acc_reference = get_joint_effort_states(reference_bag, number_of_motors);

%% Remove time offset from the measurements so that time zero is when the first message of one time is received
time_offset = 0;

if(state_time(1, 1) < reference_time(1, 1))
    time_offset = state_time(1, 1);
else 
    time_offset = reference_time(1, 1);
end

state_time = state_time' - time_offset;

reference_time = reference_time' - time_offset;

%% Calculate foot position and velocities

N = length(state_time);

foot_pos_state = zeros(N, 3);

foot_pos_ref = zeros(N, 3);

foot_vel_state = zeros(N, 3);

foot_vel_ref = zeros(N, 3);

for i = 1:N
    foot_pos_state(i, :) = CalculateFootPosition(joint_pos_state(i, :));
    foot_pos_ref(i, :) = CalculateFootPosition(joint_pos_reference(i, :));
    
    foot_vel_state(i, :) = CalculateFootVelocity(joint_pos_state(i, :), joint_vel_state(i, :));
    foot_vel_ref(i, :) = CalculateFootVelocity(joint_pos_reference(i, :), joint_vel_reference(i, :));
end

%% Convert from rad to deg
joint_pos_state = joint_pos_state*180.0/pi;
joint_vel_state = joint_vel_state*180.0/pi;

joint_pos_reference = joint_pos_reference*180.0/pi;
joint_vel_reference = joint_vel_reference*180.0/pi;
joint_acc_reference = joint_acc_reference*180.0/pi;

%% Plot results

% Joint positions
figure(1)

subplot(3, 1, 1)
hold on 
grid on
plot(reference_time, joint_pos_reference(:, 1));
plot(state_time, joint_pos_state(:, 1));
legend("\theta_{hy, ref}", "\theta_{hy}");
%xlabel("time [s]");
%ylabel("angle [deg]");
hold off

subplot(3, 1, 2)
hold on 
grid on
plot(reference_time, joint_pos_reference(:, 2));
plot(state_time, joint_pos_state(:, 2));
legend("\theta_{hp, ref}", "\theta_{hp}");
%xlabel("time [s]");
ylabel("angle [deg]");
hold off

subplot(3, 1, 3)
hold on 
grid on
plot(reference_time, joint_pos_reference(:, 3));
plot(state_time, joint_pos_state(:, 3));
legend("\theta_{kp, ref}", "\theta_{kp}");
xlabel("time [s]");
%ylabel("angle [deg]");
hold off

% Joint velocities
figure(2)

subplot(3, 1, 1)
hold on 
grid on
plot(reference_time, joint_vel_reference(:, 1));
plot(state_time, joint_vel_state(:, 1));
legend("\omega_{hy, ref}", "\omega_{hy}");
%xlabel("time [s]");
%ylabel("angular rate [deg/s]");
hold off

subplot(3, 1, 2)
hold on 
grid on
plot(reference_time, joint_vel_reference(:, 2));
plot(state_time, joint_vel_state(:, 2)');
legend("\omega_{hp, ref}", "\omega_{hp}");
%xlabel("time [s]");
ylabel("angular rate [deg/s]");
hold off

subplot(3, 1, 3)
hold on 
grid on
plot(reference_time, joint_vel_reference(:, 3));
plot(state_time, joint_vel_state(:, 3)');
legend("\omega_{kp, ref}", "\omega_{kp}");
xlabel("time [s]");
%ylabel("angular rate [deg/s]");
hold off

% Joint Torques
figure(3)
hold on
grid on
plot(state_time, joint_torques(:, 1));
plot(state_time, joint_torques(:, 2));
plot(state_time, joint_torques(:, 3));
legend("\tau_{hy}", "\tau_{hp}", "\tau_{kp}");
xlabel("time [s]");
ylabel("torque [Nm]");
hold off

% Foot position 
figure(4)

subplot(3, 1, 1)
hold on 
grid on
plot(reference_time, foot_pos_ref(:, 1));
plot(state_time, foot_pos_state(:, 1));
legend("x_{ref}", "x");
hold off

subplot(3, 1, 2)
hold on 
grid on
plot(reference_time, foot_pos_ref(:, 2));
plot(state_time, foot_pos_state(:, 2));
legend("y_{ref}", "y");
ylabel("Pos [m]");
hold off

subplot(3, 1, 3)
hold on 
grid on
plot(reference_time, foot_pos_ref(:, 3));
plot(state_time, foot_pos_state(:, 3));
legend("z_{ref}", "z");
xlabel("time [s]");
hold off

% Foot velocity 
figure(5)

subplot(3, 1, 1)
hold on 
grid on
plot(reference_time, foot_vel_ref(:, 1));
plot(state_time, foot_vel_state(:, 1));
legend("x_{d, ref}", "x_d");
hold off

subplot(3, 1, 2)
hold on 
grid on
plot(reference_time, foot_vel_ref(:, 2));
plot(state_time, foot_vel_state(:, 2));
legend("y_{d, ref}", "y_d");
ylabel("Vel [m/s]");
hold off

subplot(3, 1, 3)
hold on 
grid on
plot(reference_time, foot_vel_ref(:, 3));
plot(state_time, foot_vel_state(:, 3));
legend("z_{d, ref}", "z_d");
xlabel("time [s]");
hold off

% Foot position 3D plot

figure(6)
plot3(foot_pos_ref(:, 1), foot_pos_ref(:, 2), foot_pos_ref(:, 3));
hold on
grid on
plot3(foot_pos_state(:, 1), foot_pos_state(:, 2), foot_pos_state(:, 3));
axis([-0.5 .5 -0.5 0.5 -0.5 0.5]);
legend("p(t)_{ref}", "p(t)");
xlabel("x [m]");
ylabel("y [m]");
zlabel("z [m]");
hold off







