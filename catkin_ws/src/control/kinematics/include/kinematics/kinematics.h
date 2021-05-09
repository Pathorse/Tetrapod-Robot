/*******************************************************************/
/*    AUTHOR: Paal Arthur S. Thorseth                              */
/*    ORGN:   Dept of Eng Cybernetics, NTNU Trondheim              */
/*    FILE:   kinematics.h                                         */
/*    DATE:   Feb 4, 2021                                          */
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

#pragma once

// C++ Standard Library
#include <cmath>

// ROS Package Libraries
#include <math_utils/angle_utils.h>

// Eigen
#include <Eigen/Core>

// Kindr
#include <kindr/Core>

using GeneralizedCoordinates = Eigen::Matrix<double, 18, 1>;
using FootstepPositions = Eigen::Matrix<Eigen::Vector3d, 4, 1>;
using JointSpaceVector = Eigen::Matrix<double, 12, 1>;
using Twist = Eigen::Matrix<double, 6, 1>;
using Vector3d = Eigen::Vector3d;
using TransMatrix = kindr::HomTransformMatrixD;

/// \brief A class for analytical Kinematics Solving
class Kinematics
{
    /// \brief Tetrapod leg enumerator
    public: enum LegType { frontLeft = 1, frontRight = 2, rearLeft = 3, rearRight = 4 };

    /// \brief Leg enumerator
    public: enum BodyType { base = 1, femur = 2, fibula = 3 };

    /// \brief Constructor
    public: Kinematics();

    /// \brief Destructor
    public: virtual ~Kinematics();

    /// \brief The SolveForwardKinematics function calculates
    /// the Forward Kinematics, i.e. maps joint angles in Joint Space
    /// to a coordinate point in Coordinate Space.
    /// \param[in] _q Generalized coordinates. containing the floating base
    /// and joint positions.
    /// \param[out] _f_pos Footstep positions generated from solving the forward
    /// kinematics.
    /// \return Evaluates true if an Forward Kinematics solution is found,
    /// and false if not.
    public: bool SolveForwardKinematics(const GeneralizedCoordinates &_q, 
                                        FootstepPositions &_f_pos);

    /// \brief The SolveInverseKinematics function calculates
    /// the Inverse Kinematics, i.e. maps a coordinate point 
    /// in the Coordinate Space to joint angles in the Joint Space.
    /// \param[in] _q_b Base Pose in the world frame.
    /// \param[in] _f_pos Footstep positions in the world frame.
    /// \param[out] _q_r Joint angles from the Inverse Kinematics solution.
    /// \return Evaluates true if an Inverse Kinematics solution is found,
    /// and false if not.
    public: bool SolveInverseKinematics(const Twist &_q_b, 
                                        const FootstepPositions &_f_pos, 
                                        JointSpaceVector &_q_r);

    /// \brief The SolveSingleLegForwardKinematics function calculates
    /// the Forward Kinematics for a single leg.
    /// \param[in] _h_pos Hip position in world frame.
    /// \param[in] _theta_hy Hip yaw angle.
    /// \param[in] _theta_hp Hip pitch angle.
    /// \param[in] _theta_kp Knee pitch angle.
    /// \return Returns end-effector position in Coordinate Space.
    public: Vector3d SolveSingleLegForwardKinematics(const Vector3d &_h_pos, 
                                                     const double &_theta_hy, 
                                                     const double &_theta_hp, 
                                                     const double &_theta_kp); 

    /// \brief The SolveSingeLegInverseKinematics function calculates
    /// the inverse kinematics for a single leg.
    /// \param[in] _offset Bool indicating whether the leg is roll
    /// offset to -90 or 90 degrees. True indicates roll offset of
    /// of 90 deg, false indicates roll offset of -90 degrees.
    /// \param[in] _h_pos Hip position in world frame.
    /// \param[in] _f_pos Foot position in world frame.
    /// \param[out] _joint_angles Joint angles from the Inverse Kinematics solution
    /// for a single-leg (sl).
    /// \return Evaluates true if an Inverse Kinematics solution is found, 
    /// and false if not.
    public: bool SolveSingleLegInverseKinematics(const bool &_offset, 
                                                 const Vector3d &_h_pos, 
                                                 const Vector3d &_f_pos, 
                                                 Vector3d &_joint_angles);

    /// \brief The GetPositionBaseToFootInB returns the position vector
    /// from base origin to end-effector (foot) position in body for
    /// a given leg.
    /// \param[in] _leg Leg type..
    /// \param[in] _q Generalized coordinates.
    /// \return Returns the position vector from base to foot.
    public: Eigen::Matrix<double, 3, 1> GetPositionBaseToFootInB(const LegType &_leg,
                                                                 const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetHipToFootTransform function returns the homogeneous
    /// transformation from the Hip frame to the Foot frame.
    /// \param[in] _offset Bool indicating whether roll offset
    /// should be set to -90 or 90 degrees. True indicates roll offset
    /// of 90 deg, false indicates roll offset of -90 degrees.
    /// \param[in] _theta_hy Hip yaw angle.
    /// \param[in] _theta_hp Hip pitch angle.
    /// \param[in] _theta_kp Knee pitch angle.
    /// \return Returns the kindr homogeneous transformation matrix from Hip to Foot.
    public: TransMatrix GetHipToFootTransform(const bool &_offset, 
                                              const double &_theta_hy, 
                                              const double &_theta_hp, 
                                              const double &_theta_kp);

    /// \brief The GetDhTransform function returns the Denavit-Hartenberg
    /// transformation from frame A to frame B.
    /// \param[in] _a Link length.
    /// \param[in] _alpha Link twist.
    /// \param[in] _d Link offset.
    /// \param[in] _theta Joint angle.
    /// \return Returns the kindr homogeneous transformation matrix from frame A to frame B.
    public: TransMatrix GetDhTransform(const double &_a, 
                                       const double &_alpha, 
                                       const double &_d, 
                                       const double &_theta);
                                    
    /// \brief The GetSingleLegTranslationJacobianInB function returns the 
    /// Jacobian matrix for end-effector linear velocities in body for the leg state.
    /// \param[in] _offset Bool indicating whether roll offset
    /// should be set to -90 or 90 degrees. True indicates roll offset
    /// of 90 deg, false indicates roll offset of -90 degrees.
    /// \param[in] _theta_hy Hip yaw angle.
    /// \param[in] _theta_hp Hip pitch angle.
    /// \param[in] _theta_kp Knee pitch angle.
    /// \return Returns the Jacobian Matrix relating end-effector velocities to joint velocities.
    public: Eigen::Matrix<double, 3, 3> GetSingleLegTranslationJacobianInB(const bool &_offset,
                                                                           const double &_theta_hy, 
                                                                           const double &_theta_hp, 
                                                                           const double &_theta_kp);

    /// \brief The GetSingleLegTranslationJacobianInB function returns the
    /// Jacobian matrix for end-effector linear velocities in body for the joint state.
    /// \param[in] _leg Leg type.
    /// \param[in] _q_r Joint coordinates.
    /// \return Returns the Jacobian Matrix relating end-effector velocities to joint velocities.
    public: Eigen::Matrix<double, 3, 12> GetSingleLegTranslationJacobianInB(const LegType &_leg, 
                                                                            const Eigen::Matrix<double, 12, 1> &_q_r);

    /// \brief The GetSingleLegLinkTranslationJacobianInB function returns the 
    /// Jacobian matrix relating the linear body velocities to joint rates for a leg link
    /// \param[in] _offset Bool indicating whether roll offset
    /// should be set to -90 or 90 degrees. True indicates roll offset
    /// of 90 deg, false indicates roll offset of -90 degrees.
    /// \param[in] _theta_hy Hip yaw angle.
    /// \param[in] _theta_hp Hip pitch angle.
    /// \param[in] _theta_kp Knee pitch angle.
    /// \param[in] _link_length_1 Distance from shoulder joint to next link point
    /// \param[in] _link_length_2 Distance from shoulder pitch joint to next link point
    /// \param[in] _link_length_3 Distance from knee pitch joint to next link point
    /// \return Returns the Jacobian Matrix relating the link CoM velocities to joint velocities.
    public: Eigen::Matrix<double, 3, 3> GetSingleLegLinkTranslationJacobianInB(const bool &_offset,
                                                                              const double &_theta_hy, 
                                                                              const double &_theta_hp, 
                                                                              const double &_theta_kp,
                                                                              const double &_link_length_1,
                                                                              const double &_link_length_2,
                                                                              const double &_link_length_3);

    /// \brief The GetSingleLegLinkTranslationJacobianInB function returns the translational
    /// Jacobian matrix for a leg link in body frame for the joint state.
    /// \param[in] _leg Leg Type
    /// \param[in] _q_r Joint coordinates
    /// \return Returns the Jacobian Matrix relating the leg link CoM velocities to joint velocities.
    public: Eigen::Matrix<double, 3, 12> GetSingleLegLinkTranslationJacobianInB(const LegType &_leg, 
                                                                               const Eigen::Matrix<double, 12, 1> &_q_r,
                                                                               const double &_length_link_1,
                                                                               const double &_length_link_2,
                                                                               const double &_length_link_3);

    /// \brief The GetTranslationJacobianInW function returns the
    /// spatial translation Jacobian mapping generalized velocities
    /// to operational space twist of the leg frame.
    /// \param[in] _leg Leg type.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns the translation Jacobian Matrix mapping from generalized coordinates to
    /// the operational space twist of the leg frame.
    public: Eigen::Matrix<double, 3, 18> GetTranslationJacobianInW(const LegType &_leg,
                                                                   const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetSingleLegRotationJacobianInB function returns the 
    /// Jacobian matrix for end-effector angular velocities in body for the leg state.
    /// \param[in] _offset Bool indicating whether roll offset
    /// should be set to -90 or 90 degrees. True indicates roll offset
    /// of 90 deg, false indicates roll offset of -90 degrees.
    /// \param[in] _theta_hy Hip yaw angle.
    /// \param[in] _theta_hp Hip pitch angle.
    /// \param[in] _theta_kp Knee pitch angle.
    /// \return Returns the Jacobian Matrix relating end-effector velocities to joint velocities.
    public: Eigen::Matrix<double, 3, 3> GetSingleLegRotationJacobianInB(const bool &_offset,
                                                                        const double &_theta_hy, 
                                                                        const double &_theta_hp, 
                                                                        const double &_theta_kp);

    /// \brief The GetSingleLegRotationJacobianInB function returns the
    /// Jacobian matrix for end-effector angular velocities in body for the joint state.
    /// \param[in] _leg Leg type.
    /// \param[in] _q_r Joint coordinates.
    /// \return Returns the Jacobian Matrix relating end-effector velocities to joint velocities.
    public: Eigen::Matrix<double, 3, 12> GetSingleLegRotationJacobianInB(const LegType &_leg, 
                                                                         const Eigen::Matrix<double, 12, 1> &_q_r);

    /// \brief The GetRotationJacobianInW function returns the
    /// spatial rotation Jacobian mapping generalized velocities
    /// to operational space twist of the leg frame.
    /// \param[in] _leg Leg type.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns the rotation Jacobian Matrix mapping from generalized coordinates to
    /// the operational space twist of the leg frame.
    public: Eigen::Matrix<double, 3, 18> GetRotationJacobianInW(const LegType &_leg,
                                                                const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetJacobianInW function returns the
    /// spatial Jacobian mapping generalized velocities
    /// to operational space twist of the leg frame.
    /// \param[in] _leg Leg type.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns the Jacobian Matrix mapping from generalized coordinates to
    /// the operational space twist of the leg frame.
    public: Eigen::Matrix<double, 6, 18> GetJacobianInW(const LegType &_leg,
                                                        const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetSingleBodyMassMatrix function returns the
    /// mass matrix for a single body in the system. 
    /// \param[in] _leg Leg type.
    /// \param[in] _body Body type.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns a single body mass matrix as specified by leg and body type.
    public: Eigen::Matrix<double, 18, 18> GetSingleBodyMassMatrix(const LegType &_leg,
                                                                  const BodyType &_body,
                                                                  const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetMassMatrix function returns the
    /// mass matrix for the floating base system.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns an orthogonal mass matrix (n_q x n_q) for the floating base system.
    public: Eigen::Matrix<double, 18, 18> GetMassMatrix(const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetSingleBodyCoriolisAndCentrifugalTerms function returns the
    /// coriolis and centrifugal terms for a single body in the system. 
    /// \param[in] _leg Leg type.
    /// \param[in] _body Body type.
    /// \param[in] _q Generalized coordinates.
    /// \param[in] _u Generalized velocities.
    /// \return Returns a single body coriolis and centrifugal terms as specified by leg and body type.
    public: Eigen::Matrix<double, 18, 1> GetSingleBodyCoriolisAndCentrifugalTerms(const LegType &_leg,
                                                                                  const BodyType &_body,
                                                                                  const Eigen::Matrix<double, 18, 1> &_q,
                                                                                  const Eigen::Matrix<double, 18, 1> &_u);

    /// \brief The GetCoriolisAndCentrifugalTerms function returns the
    /// coriolis and centrifugal terms for the floating base system.
    /// \param[in] _q Generalized coordinates.
    /// \param[in] _u Generalized velocities.
    /// \return Returns coriolis and centrifugal terms for the floating base system. 
    public: Eigen::Matrix<double, 18, 1> GetCoriolisAndCentrifugalTerms(const Eigen::Matrix<double, 18, 1> &_q,
                                                                        const Eigen::Matrix<double, 18, 1> &_u);

    /// \brief The GetSingleBodyGravitationalTerms function returns the
    /// gravitational terms for a single body in the system. 
    /// \param[in] _leg Leg type.
    /// \param[in] _body Body type.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns a single body gravitational terms as specified by leg and body type.
    public: Eigen::Matrix<double, 18, 1> GetSingleBodyGravitationalTerms(const LegType &_leg,
                                                                         const BodyType &_body,
                                                                         const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The GetGravitationalTerms function returns the
    /// gravitational terms for the floating base system.
    /// \param[in] _q Generalized coordinates.
    /// \return Returns gravitational terms for the floating base system. 
    public: Eigen::Matrix<double, 18, 1> GetGravitationalTerms(const Eigen::Matrix<double, 18, 1> &_q);

    /// \brief The ValidateSolution function evaluates whether
    /// a set of joint angles is within joint limits. 
    /// \param[in] _q_r Joint angles generated by the IK solver.
    /// \return Evaluates true if IK solution is valid, and false if not.
    private: bool ValidateSolution(const JointSpaceVector &_q_r);

    /// \brief Minimum joint limits.
    private: JointSpaceVector min_angles;

    /// \brief Maximum joint limits.
    private: JointSpaceVector max_angles;

    /// \brief Position vector relating the base coordinates to 
    /// the front left hip
    private: Vector3d positionBaseToFrontLeftInB;

    /// \brief Position vector relating the base coordinates to 
    /// the front right hip
    private: Vector3d positionBaseToFrontRightInB;

    /// \brief Position vector relating the base coordinates to 
    /// the rear left hip
    private: Vector3d positionBaseToRearLeftInB;

    /// \brief Position vector relating the base coordinates to 
    /// the rear right hip
    private: Vector3d positionBaseToRearRightInB;

    /// \brief Hip link length
    private: double L1;

    /// \brief Thigh (Femur) link length
    private: double L2;

    /// \brief Leg (Fibula & Tibia) link length
    private: double L3;

    /// \brief Shoulder yaw joint to shoulder CoM length
    private: double LC1;

    /// \brief Hip pitch joint to thigh CoM length
    private: double LC2;

    /// \brief Knee pitch joint to leg CoM length
    private: double LC3;

    /// \brief Front Left offset
    private: bool flOffset;

    /// \brief Front Right offset
    private: bool frOffset;

    /// \brief Rear Left offset
    private: bool rlOffset;

    /// \brief Rear Right offset
    private: bool rrOffset;
};