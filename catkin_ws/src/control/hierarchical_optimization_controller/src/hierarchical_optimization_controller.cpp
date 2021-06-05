/*******************************************************************/
/*    AUTHOR: Paal Arthur S. Thorseth                              */
/*    ORGN:   Dept of Eng Cybernetics, NTNU Trondheim              */
/*    FILE:   hierarchical_optimization_controller.cpp             */
/*    DATE:   May 15, 2021                                         */
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

#include <hierarchical_optimization_controller/hierarchical_optimization_controller.h>

// Constructor
HierarchicalOptimizationControl::HierarchicalOptimizationControl()
{
    this->InitRos();
    this->InitRosQueueThreads();

    this->genCoord.setZero();
    this->genVel.setZero();
    this->fPos.setZero();

    this->contactState[0] = 1;
    this->contactState[1] = 1;
    this->contactState[2] = 1;
    this->contactState[3] = 1;
}

// Destructor
HierarchicalOptimizationControl::~HierarchicalOptimizationControl()
{
    this->rosNode->shutdown();

    this->rosProcessQueue.clear();
    this->rosProcessQueue.disable();
    this->rosProcessQueueThread.join();

    this->rosPublishQueue.clear();
    this->rosPublishQueue.disable();
    this->rosPublishQueueThread.join();
}

// TODO remove this
void HierarchicalOptimizationControl::StaticTorqueTest()
{
    // Declare states
    Eigen::Matrix<double, 18, 1> q;
    Eigen::Matrix<double, 18, 1> u;
    Eigen::Matrix<Eigen::Vector3d, 4, 1> f_pos;

    q << 0,
         0,
         0.42,
         0,
         0,
         0,
         math_utils::degToRad(45),
         math_utils::degToRad(40),
         math_utils::degToRad(35),
         math_utils::degToRad(-45),
         math_utils::degToRad(-40),
         math_utils::degToRad(-35),
         math_utils::degToRad(135),
         math_utils::degToRad(-40),
         math_utils::degToRad(-35),
         math_utils::degToRad(-135),
         math_utils::degToRad(40),
         math_utils::degToRad(35);

    u << 0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0;

    kinematics.SolveForwardKinematics(q, f_pos);

    // Declare desired states
    Eigen::Vector3d desired_base_pos;
    Eigen::Matrix<Eigen::Vector3d, 4, 1> desired_f_pos;

    desired_base_pos << 0,
                        0,
                        0.40; 
    
    //desired_f_pos = f_pos;
    desired_f_pos = this->fPos;

    // Solve
    //Eigen::Matrix<double, 18, 1> desired_tau;
    Eigen::Matrix<double, 12, 1> desired_tau;


    ros::Rate loop_rate(10);

    while(this->rosNode->ok())
    {
        //desired_f_pos = f_pos;
        desired_f_pos = this->fPos;

        desired_base_pos = this->genCoord.topRows(3);

        desired_base_pos(2) = 0.40;

        desired_tau = this->HierarchicalOptimization(desired_base_pos,
                                                     desired_f_pos,
                                                     this->fPos,
                                                     this->genCoord,
                                                     this->genVel,
                                                     2);

        debug_utils::printBaseState(this->genCoord.topRows(6));
        debug_utils::printFootstepPositions(this->fPos);
        //debug_utils::printJointTorques(desired_tau.bottomRows(12));
        debug_utils::printJointTorques(desired_tau);

        //this->PublishTorqueMsg(desired_tau.bottomRows(12));
        this->PublishTorqueMsg(desired_tau);

        //break;

        loop_rate.sleep();
    }
}

// Hierarchical Optimization
Eigen::Matrix<double, 12, 1> HierarchicalOptimizationControl::HierarchicalOptimization(const Eigen::Vector3d &_desired_base_pos,
                                                                                       const Eigen::Matrix<Eigen::Vector3d, 4, 1> &_desired_f_pos,
                                                                                       const Eigen::Matrix<Eigen::Vector3d, 4, 1> &_f_pos,
                                                                                       const Eigen::Matrix<double, 18, 1> &_q,
                                                                                       const Eigen::Matrix<double, 18, 1> &_u,
                                                                                       const int &_v)
{
    //*************************************************************************************
    // Declarations
    //*************************************************************************************

    Eigen::Matrix<double, 12, 1> desired_tau;    // 12x1 Reference joint torques

    Eigen::VectorXd x_opt;                       // Optimal solution x_opt = [dot_u_opt, F_c_opt]^T

    SolverType solver = SolverType::OSQP;        // Solver type

    // Tasks used to formulate the hierarchical optimization
    // problem
    Task t_eom;     // Task for the equations of motion
    Task t_cmc;     // Task for the contact motion constraint
    Task t_cftl;    // Task for the contact force and torque limits
    Task t_mt;      // Task for the motion tracking of the floating base and swing legs
    Task t_pt;      // Task for posture tracking, for a nominal posture
    Task t_cfm;     // Task for the contact force minimization

    std::vector<Task> tasks; // Set of tasks

    // Kinematic and dynamic matrices and terms used to
    // enforce constraints.
    Eigen::Matrix<double, 18, 18> M;                      // 18x18 Mass matrix
    Eigen::Matrix<double, 18, 1> b;                       // 18x1 Coriolis and centrifugal terms
    Eigen::Matrix<double, 18, 1> g;                       // 18x1 Gravitational terms
    Eigen::Matrix<double, Eigen::Dynamic, 18> J_c;        // (3*n_c)x18 Contact Jacobian

    // Matrices and terms used to enforce equations of motion 
    // for the floating base system dynamics (the first six
    // rows of the respective matrices and terms).
    Eigen::Matrix<double, 18, 18> M_fb;                    // 18x18 Floating base Mass matrix
    Eigen::Matrix<double, 18, 1> b_fb;                     // 18x1 Floating base Coriolis and centrifugal terms
    Eigen::Matrix<double, 18, 1> g_fb;                     // 18x1 Floating base Gravitational terms
    Eigen::Matrix<double, Eigen::Dynamic, 18> J_c_fb;      // (3*n_c)x18 Floating base Contact Jacobian

    // Matrices used to enforce contact motion constraints
    //Eigen::Matrix<double, Eigen::Dynamic, 18> J_c;         // (3*n_c)x18 Contact Jacobian
    Eigen::Matrix<double, Eigen::Dynamic, 18> dot_J_c;     // (3*n_c)x18 Time derivative of the Contact Jacobian

    // Matrices and parameters used to enfore motion tracking
    Eigen::Matrix<double, 3, 18> J_P_fb;                   // 3x18 Floating base Translation Jacobian
    Eigen::Matrix<double, 3, 18> J_P_fl;                   // 3x18 Front left foot Translation Jacobian
    Eigen::Matrix<double, 3, 18> J_P_fr;                   // 3x18 Front right foot Translation Jacobian
    Eigen::Matrix<double, 3, 18> J_P_rl;                   // 3x18 Rear left foot Translation Jacobian
    Eigen::Matrix<double, 3, 18> J_P_rr;                   // 3x18 Rear right foot Translation Jacobian

    Eigen::Matrix<double, 3, 18> dot_J_P_fb;               // 3x18 Time derivative of the Floating base Translation Jacobian
    Eigen::Matrix<double, 3, 18> dot_J_P_fl;               // 3x18 Time derivative of the Front left foot Translation Jacobian
    Eigen::Matrix<double, 3, 18> dot_J_P_fr;               // 3x18 Time derivative of the Front right foot Translation Jacobian
    Eigen::Matrix<double, 3, 18> dot_J_P_rl;               // 3x18 Time derivative of the Rear left foot Translation Jacobian
    Eigen::Matrix<double, 3, 18> dot_J_P_rr;               // 3x18 Time derivative of the Rear right foot Translation Jacobian

    // Terms used to enforce posture tracking
    Eigen::Matrix<double, 12, 1> q_nom;                    // 12x1 Nominal posture configuration

    // Matrices and terms used to enforce contact force
    // and torque limits for the joint dynamics (the last
    // 12 rows of the respective matrices and terms).
    Eigen::Matrix<double, 18, 18> M_r;                    // 18x18 Joint dynamics Mass matrix
    Eigen::Matrix<double, 18, 1> b_r;                     // 18x1 Joint dynamics Coriolis and centrifugal terms
    Eigen::Matrix<double, 18, 1> g_r;                     // 18x1 Joint dynamics Gravitational terms
    Eigen::Matrix<double, Eigen::Dynamic, 18> J_c_r;      // (3*n_c)x18 Joint dynamics Contact Jacobian

    Eigen::Matrix<double, 18, 1> tau_min;                 // 18x1 Minimum actuator torques
    Eigen::Matrix<double, 18, 1> tau_max;                 // 18x1 Maximum actuator torques

    std::vector<Kinematics::LegType> contact_legs; // Vector of contact points (legs)
    std::vector<Kinematics::LegType> swing_legs;   // Vector of swing legs

    //*************************************************************************************
    // Tuning parameters
    //*************************************************************************************

    // Motion tracking gains
    double k_p_fb = 300;      // Floating base proportional gain
    double k_p_fl = 100;      // Front left foot proportional gain
    double k_p_fr = 100;      // Front right foot proportional gain
    double k_p_rl = 100;      // Rear left foot proportional gain
    double k_p_rr = 100;      // Rear right foot proportional gain

    // Posture tracking gains
    double k_p_pt = 350;      // Posture tracking proportional gain

    //*************************************************************************************
    // Updates
    //*************************************************************************************

    // Fill contact and swing legs
    for (size_t i = 0; i < 4; i++)
    {
        // Contact State is assumed sorted by fl, fr, rl, rr
        if (contactState[i])
        {
            // fl = 1, fr = 2, rl = 3, rr = 4
            contact_legs.push_back(Kinematics::LegType(i + 1));
        }
        else
        {
            // fl = 1, fr = 2, rl = 3, rr = 4
            swing_legs.push_back(Kinematics::LegType(i + 1));
        }
    }

    // Number of contact and swing legs, respectively
    const unsigned int n_c = contact_legs.size();
    const unsigned int n_s = swing_legs.size();

    // Update state dimension x = [dot_u, F_c]^T
    const unsigned int state_dim = 18 + 3*n_c;

    // Resize task dimensions
    //t_eom.A_eq.resize(18, state_dim);
    //t_eom.b_eq.resize(18, 1);
    t_eom.A_eq.resize(6, state_dim);
    t_eom.b_eq.resize(6, 1);

    t_cmc.A_eq.resize(3*n_c, state_dim);
    t_cmc.b_eq.resize(3*n_c, 1);

    t_cftl.A_ineq.resize(36, state_dim);
    t_cftl.b_ineq.resize(36,1);

    t_mt.A_eq.resize(15, state_dim);
    t_mt.b_eq.resize(15, 1);

    t_pt.A_eq.resize(12, state_dim);
    t_pt.b_eq.resize(12, 1);

    t_cfm.A_eq.resize(3*n_c, state_dim);
    t_cfm.b_eq.resize(3*n_c, 1);

    // Resize dynamic matrices and terms
    x_opt.resize(state_dim, 1);
    J_c.resize(3*n_c, 18);
    J_c_fb.resize(3*n_c, 18);
    J_c_r.resize(3*n_c, 18);
    //J_c.resize(3*n_c, 18);
    dot_J_c.resize(3*n_c, 18);

    // Update matrices and terms
    M = kinematics.GetMassMatrix(_q);

    b = kinematics.GetCoriolisAndCentrifugalTerms(_q, _u);

    g = kinematics.GetGravitationalTerms(_q);

    J_c = kinematics.GetContactJacobianInW(contact_legs, _q);

    // Update matrices used by EOM constraints
    M_fb = kinematics.GetMassMatrix(_q);
    M_fb.bottomRows(12).setZero();

    b_fb = kinematics.GetCoriolisAndCentrifugalTerms(_q, _u);
    b_fb.bottomRows(12).setZero();

    g_fb = kinematics.GetGravitationalTerms(_q);
    g_fb.bottomRows(12).setZero();

    J_c_fb = kinematics.GetContactJacobianInW(contact_legs, _q);
    J_c_fb.rightCols(12).setZero();

    // Update matrices used by contact motion constraints
    //J_c = kinematics.GetContactJacobianInW(contact_legs, _q);
    dot_J_c = kinematics.GetContactJacobianInWDiff(contact_legs, _q, _u);

    // Update matrices used by contact force and torque limits constraint
    M_r = kinematics.GetMassMatrix(_q);
    M_r.topRows(6).setZero();

    b_r = kinematics.GetCoriolisAndCentrifugalTerms(_q, _u);
    b_r.topRows(6).setZero();

    g_r = kinematics.GetGravitationalTerms(_q);
    g_r.topRows(6).setZero();

    J_c_r = kinematics.GetContactJacobianInW(contact_legs, _q);
    J_c_r.leftCols(6).setZero();

    tau_max.topRows(6).setZero();
    tau_max.bottomRows(12).setConstant(40);
    tau_min.topRows(6).setZero();
    tau_min.bottomRows(12).setConstant(-40);

    // Update matrices used by motion tracking constraints
    J_P_fb = kinematics.GetTranslationJacobianInW(Kinematics::LegType::NONE,
                                                  Kinematics::BodyType::base,
                                                  _q);
    J_P_fl = kinematics.GetTranslationJacobianInW(Kinematics::LegType::frontLeft,
                                                  Kinematics::BodyType::foot,
                                                  _q);
    J_P_fr = kinematics.GetTranslationJacobianInW(Kinematics::LegType::frontRight,
                                                  Kinematics::BodyType::foot,
                                                  _q);
    J_P_rl = kinematics.GetTranslationJacobianInW(Kinematics::LegType::rearLeft,
                                                  Kinematics::BodyType::foot,
                                                  _q);
    J_P_rr = kinematics.GetTranslationJacobianInW(Kinematics::LegType::rearRight,
                                                  Kinematics::BodyType::foot,
                                                  _q);

    dot_J_P_fb = kinematics.GetTranslationJacobianInWDiff(Kinematics::LegType::NONE,
                                                          Kinematics::BodyType::base,
                                                          _q,
                                                          _u);
    dot_J_P_fl = kinematics.GetTranslationJacobianInWDiff(Kinematics::LegType::frontLeft,
                                                          Kinematics::BodyType::foot,
                                                          _q,
                                                          _u);
    dot_J_P_fr = kinematics.GetTranslationJacobianInWDiff(Kinematics::LegType::frontRight,
                                                          Kinematics::BodyType::foot,
                                                          _q,
                                                          _u);
    dot_J_P_rl = kinematics.GetTranslationJacobianInWDiff(Kinematics::LegType::rearLeft,
                                                          Kinematics::BodyType::foot,
                                                          _q,
                                                          _u);
    dot_J_P_rr = kinematics.GetTranslationJacobianInWDiff(Kinematics::LegType::rearRight,
                                                          Kinematics::BodyType::foot,
                                                          _q,
                                                          _u);
    // Update terms used by posture tracking constraints
    q_nom << math_utils::degToRad(45),
             math_utils::degToRad(40),
             math_utils::degToRad(35),
             math_utils::degToRad(-45),
             math_utils::degToRad(-40),
             math_utils::degToRad(-35),
             math_utils::degToRad(135),
             math_utils::degToRad(-40),
             math_utils::degToRad(-35),
             math_utils::degToRad(-135),
             math_utils::degToRad(40),
             math_utils::degToRad(35);

    // Update equations of motion task
    //t_eom.A_eq.leftCols(18) = M_fb;
    //t_eom.A_eq.rightCols(3*n_c) = - J_c_fb.transpose();
    //t_eom.b_eq = - (b_fb + g_fb);
    t_eom.A_eq.leftCols(18) = M.topRows(6);
    t_eom.A_eq.rightCols(3*n_c) = - J_c.transpose().topRows(6);
    t_eom.b_eq = - (b.topRows(6) + g.topRows(6));

    // Update contact force and torque limits task
        // Max torque limit
    t_cftl.A_ineq.topRows(18).leftCols(18) = M_r;
    t_cftl.A_ineq.topRows(18).rightCols(3*n_c) = - J_c_r.transpose();
    t_cftl.b_ineq.topRows(18) = tau_max - (b_r + g_r);
        // Min torque limit
    t_cftl.A_ineq.bottomRows(18).leftCols(18) = - M_r;
    t_cftl.A_ineq.bottomRows(18).rightCols(3*n_c) = J_c_r.transpose();
    t_cftl.b_ineq.bottomRows(18) = - tau_min + (b_r + g_r);

    // Update contact motion constraint task
    t_cmc.A_eq.leftCols(18) = J_c;
    t_cmc.A_eq.rightCols(3*n_c).setZero();
    t_cmc.b_eq = - dot_J_c * _u;

    // Update motion tracking task
        // Floating base
    t_mt.A_eq.block(0, 0, 3, state_dim).leftCols(18) = J_P_fb;
    t_mt.A_eq.block(0, 0, 3, state_dim).rightCols(3*n_c).setZero();
    t_mt.b_eq.block(0, 0, 3, 1) = - dot_J_P_fb * _u + k_p_fb * (_desired_base_pos - _q.topRows(3));

        // Front-left foot
    t_mt.A_eq.block(3, 0, 3, state_dim).leftCols(18) = J_P_fl;
    t_mt.A_eq.block(3, 0, 3, state_dim).rightCols(3*n_c).setZero();
    t_mt.b_eq.block(3, 0, 3, 1) = - dot_J_P_fl * _u + k_p_fl * (_desired_f_pos(0) - _f_pos(0));

        // Front-right foot
    t_mt.A_eq.block(6, 0, 3, state_dim).leftCols(18) = J_P_fr;
    t_mt.A_eq.block(6, 0, 3, state_dim).rightCols(3*n_c).setZero();
    t_mt.b_eq.block(6, 0, 3, 1) = - dot_J_P_fr * _u + k_p_fr * (_desired_f_pos(1) - _f_pos(1));

        // Rear-left foot
    t_mt.A_eq.block(9, 0, 3, state_dim).leftCols(18) = J_P_rl;
    t_mt.A_eq.block(9, 0, 3, state_dim).rightCols(3*n_c).setZero();
    t_mt.b_eq.block(9, 0, 3, 1) = - dot_J_P_rl * _u + k_p_rl * (_desired_f_pos(2) - _f_pos(2));

        // Rear-right foot
    t_mt.A_eq.block(12, 0, 3, state_dim).leftCols(18) = J_P_rr;
    t_mt.A_eq.block(12, 0, 3, state_dim).rightCols(3*n_c).setZero();
    t_mt.b_eq.block(12, 0, 3, 1) = - dot_J_P_rr * _u + k_p_rr * (_desired_f_pos(3) - _f_pos(3));

    // Update posture tracking task
    t_pt.A_eq.leftCols(6).setZero();
    t_pt.A_eq.block(0, 6, 12, 12).setIdentity();
    t_pt.A_eq.rightCols(3*n_c).setZero();
    t_pt.b_eq = k_p_pt * (q_nom - _q.bottomRows(12));

    // Update contact force minimization task
    t_cfm.A_eq.leftCols(18).setZero();
    t_cfm.A_eq.rightCols(3*n_c).setIdentity();
    t_cfm.b_eq.setZero();

    // Update task constraint setting
    t_eom.has_eq_constraint = true;
    t_cftl.has_ineq_constraint = true;
    t_cmc.has_eq_constraint = true;
    t_mt.has_eq_constraint = true;
    t_pt.has_eq_constraint = true;

    //*************************************************************************************
    // Hierarchical QP Optimization
    //*************************************************************************************

    // Add tasks in prioritized order
    //tasks.push_back(t_eom);
    //tasks.push_back(t_cftl);
    //tasks.push_back(t_cmc);
    //tasks.push_back(t_mt);
    tasks.push_back(t_pt);
    //tasks.push_back(t_cfm);

    Eigen::Matrix<Eigen::MatrixXd, 1, 1> A_ls;
    Eigen::Matrix<Eigen::VectorXd, 1, 1> b_ls;

    A_ls(0) = t_eom.A_eq;
    b_ls(0) = t_eom.b_eq;

    // Solve the hierarchical optimization problem
    x_opt = HierarchicalQPOptimization(state_dim, 
                                       tasks,
                                       solver, 
                                       _v);

    //x_opt = HierarchicalLeastSquareOptimization(A_ls,
    //                                            b_ls,
    //                                            _v);

    //*************************************************************************************
    // Compute reference joint torques
    //*************************************************************************************

    //desired_tau = M_r * x_opt.topRows(18) - J_c_r.transpose() * x_opt.bottomRows(3*n_c) + (b_r + g_r);
    desired_tau = M.bottomRows(12) * x_opt.topRows(18) - J_c.transpose().bottomRows(12) * x_opt.bottomRows(3*n_c) + b.bottomRows(12) + g.bottomRows(12);


    return desired_tau;
}

// Hierarchical QP Optimization
Eigen::Matrix<double, Eigen::Dynamic, 1> HierarchicalOptimizationControl::HierarchicalQPOptimization(const int &_state_dim,
                                                                                                     const std::vector<Task> &_tasks,
                                                                                                     const SolverType &_solver,
                                                                                                     const int &_v)
{
    // Declarations
    Eigen::VectorXd x_opt(_state_dim);           // Optimal solution
    Eigen::MatrixXd N(_state_dim, _state_dim);   // Null-space projector
    Eigen::VectorXd z(_state_dim);               // Null-space vector
    Eigen::MatrixXd stacked_A_eq;                // Stacked equality matrices 
    Eigen::MatrixXd stacked_A_ineq;              // Stacked inequality matrices 
    Eigen::VectorXd stacked_b_ineq;              // Stacked inequality vectors 

    Eigen::MatrixXd Q;            // Quadratic program Q cost matrix
    Eigen::VectorXd c;            // Quadratic program c cost vector
    Eigen::MatrixXd E_ineq;       // Quadratic program E_ineq inequality constraint matrix 
    Eigen::VectorXd f_ineq;       // Quadratic program f_ineq inequality constraint vector
    Eigen::VectorXd xi;           // Quadratic program optimization variable, xi = [z, v]^T
    Eigen::VectorXd v;            // Quadratic program inequality constraint slack variable
    Eigen::VectorXd stacked_v;    // Stacked slack variables

    int rowsA_eq;   // Number of rows of current tasks A_eq
    int rowsA_ineq; // Number of rows of current tasks A_ineq
    int colsN;      // Number of cols of current null-space projector N
    int count = 0;  // Task counter

    // Initializations
    x_opt.setZero();
    N.setIdentity();

    // Iterate over the set of tasks 
    for (Task t : _tasks)
    {
        // Update count
        count += 1;

        // Update null-space dimension
        colsN = N.cols();

        // If task has a equality constraint
        if (t.has_eq_constraint)
        {
            // Update row count
            rowsA_eq = t.A_eq.rows(); 

            // Update stacked equality matrices
            Eigen::MatrixXd stacked_A_eq_tmp = stacked_A_eq;

            stacked_A_eq.resize(t.A_eq.rows() + stacked_A_eq_tmp.rows(), _state_dim); 
            stacked_A_eq << t.A_eq,
                            stacked_A_eq_tmp;
        }
        else
        {
            // Update row count
            rowsA_eq = 0;
        }

        // If task has a inequality constraint
        if (t.has_ineq_constraint)
        {
            // Update row count
            rowsA_ineq = t.A_ineq.rows();

            // Update stacked inequality matrices
            Eigen::MatrixXd stacked_A_ineq_tmp = stacked_A_ineq;

            stacked_A_ineq.resize(t.A_ineq.rows() + stacked_A_ineq_tmp.rows(), _state_dim); 
            stacked_A_ineq << t.A_ineq,
                              stacked_A_ineq_tmp;

            // Update stacked inequality vectors
            Eigen::VectorXd stacked_b_ineq_tmp = stacked_b_ineq;

            stacked_b_ineq.resize(t.b_ineq.rows() + stacked_b_ineq_tmp.rows(), 1);
            stacked_b_ineq << t.b_ineq,
                              stacked_b_ineq_tmp;
        }
        else
        {                
            // Update row count
            rowsA_ineq = 0;
        }

        // Update dimensions
        z.resize(colsN, 1);
        v.resize(rowsA_ineq, 1);
        xi.resize(colsN + rowsA_ineq, 1);

        // Update QP costs 
        Q.resize(colsN + rowsA_ineq, colsN + rowsA_ineq);
        c.resize(colsN + rowsA_ineq, 1);

        if (t.has_eq_constraint)
        {
            Q.topLeftCorner(colsN, colsN) = N.transpose() * t.A_eq.transpose() * t.A_eq * N;
            Q.topRightCorner(rowsA_ineq, colsN).setZero();
            Q.bottomLeftCorner(colsN, rowsA_ineq).setZero();
            Q.bottomRightCorner(rowsA_ineq, rowsA_ineq).setIdentity();

            c.topRows(colsN) = N.transpose() * t.A_eq.transpose() * (t.A_eq * x_opt - t.b_eq);
            c.bottomRows(rowsA_ineq).setZero();
        }
        else
        {
            Q.setZero();
            Q.bottomRightCorner(rowsA_ineq, rowsA_ineq).setIdentity();

            c.setZero();
        }

        // Update inequality constraints
        E_ineq.resize(stacked_A_ineq.rows() + rowsA_ineq, colsN + rowsA_ineq);
        f_ineq.resize(stacked_A_ineq.rows() + rowsA_ineq, 1);

        E_ineq.setZero();

        E_ineq.topLeftCorner(stacked_A_ineq.rows(), colsN) = stacked_A_ineq * N;
        E_ineq.topRightCorner(rowsA_ineq, rowsA_ineq) = - Eigen::MatrixXd::Identity(rowsA_ineq, rowsA_ineq);

        E_ineq.bottomRightCorner(rowsA_ineq, rowsA_ineq) = - Eigen::MatrixXd::Identity(rowsA_ineq, rowsA_ineq);

        f_ineq.topRows(stacked_b_ineq.rows()) = stacked_b_ineq - stacked_A_ineq * x_opt;
        f_ineq.segment(rowsA_ineq, stacked_A_ineq.rows() - rowsA_ineq) += stacked_v; 
        f_ineq.bottomRows(rowsA_ineq).setZero();

        // Print w.r.t verbosity level
        if (_v > 1)
        {
            ROS_INFO_STREAM("----------------------" << "Task: " << count << "---------------------- \n" );
            ROS_INFO_STREAM("Q: \n" << Q << "\n");
            ROS_INFO_STREAM("eigenvalues(Q): \n" << Q.eigenvalues() << "\n");
            ROS_INFO_STREAM("c: \n" << c << "\n");
            ROS_INFO_STREAM("E_ineq: \n" << E_ineq << "\n");
            ROS_INFO_STREAM("f_ineq: \n" << f_ineq << "\n");
        }
        if (_v > 2)
        {
            ROS_INFO_STREAM("N: \n" << N << "\n");
            ROS_INFO_STREAM("A_eq: \n" << t.A_eq << "\n");
            ROS_INFO_STREAM("b_eq: \n" << t.b_eq << "\n");
            ROS_INFO_STREAM("stacked_A_eq: \n" << stacked_A_eq << "\n");
            ROS_INFO_STREAM("stacked_A_ineq: \n" << stacked_A_ineq << "\n");
            ROS_INFO_STREAM("stacked_b_ineq: \n" << stacked_b_ineq << "\n");
        }

        // Solve QP
        if (!SolveQP(Q, c, E_ineq, f_ineq, xi, _solver,_v))
        {
            ROS_ERROR_STREAM("[HierarchicalOptimizationControl::HierarchicalQPOptimization] Failed to solve QP at task " << count << "!");
            
            break;
        }

        // Get solutions
        z = xi.topRows(colsN);
        v = xi.bottomRows(rowsA_ineq);

        // Update optimal state for the hierarchical optimization
        x_opt = x_opt + N * z;

        // Update the null-space projector if a equality constraint
        // is present.
        if (t.has_eq_constraint)
        {
            N = math_utils::SVDNullSpaceProjector(stacked_A_eq);
        }

        // Update stacked slack variables if a inequality constraint
        // is present.
        if (t.has_ineq_constraint)
        {
            Eigen::VectorXd stacked_v_tmp = stacked_v;

            stacked_v.resize(v.rows() + stacked_v_tmp.rows(), 1);
            stacked_v << v,
                         stacked_v_tmp;
        }

        // Print w.r.t verbosity level
        if (_v > 0 )
        {
            ROS_INFO_STREAM("Solution at task " << count << " is: \n" << x_opt);
        }

        // Terminate if null-space is zero (i.e. only the trivial solution exist)
        if (N.isZero())
        {
            if (_v > 0)
            {
                ROS_INFO_STREAM("Terminating at task " << count << " due to the null-space being zero. \n");
            }
            break;
        }

    }

    return x_opt;
}

// Hierarchical Least-Square Optimization
Eigen::Matrix<double, Eigen::Dynamic, 1> HierarchicalOptimizationControl::HierarchicalLeastSquareOptimization(const Eigen::Matrix<Eigen::MatrixXd, Eigen::Dynamic, 1> &_A,
                                                                                                              const Eigen::Matrix<Eigen::Matrix<double, Eigen::Dynamic, 1>, Eigen::Dynamic, 1> &_b,
                                                                                                              const int &_v)
{
    const auto rowsA = _A.rows();
    const auto rowsb = _b.rows();

    // Validate number of equations
    if( rowsA != rowsb )
    {
        ROS_ERROR("[HierarchicalOptimizationControl::HierarchicalLeastSquareOptimization] _A and _b does not contain equal amount of elements!");

        std::abort();
    }

    const auto state_dim = _A(0).cols();

    // Validate state and equation dimensions
    for (size_t i = 0; i < rowsA; i++)
    {
        if (_A(i).cols() != state_dim || _A(i).rows() != _b(i).rows() )
        {
            ROS_ERROR_STREAM("[HierarchicalOptimizationControl::HierarchicalLeastSquareOptimization] State or equation dimensions failed at index: i = !" << i);

            std::abort();
        }
    }

    // Declarations
    Eigen::Matrix<double, Eigen::Dynamic, 1> x_opt;                 // Optimal solution
    Eigen::Matrix<double, Eigen::Dynamic, 1> x_i;                   // Optimal solution for the at hand projected QP
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> N;        // Null-space projector
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> AN;       // Matrix multiplication of A and N (A*N)
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> pinvAN;   // Pseudo-inverse of A*N
    Eigen::MatrixXd A_stacked;                                      // Stacked matrix of A_i matrices

    // Setup
    x_opt.resize(state_dim, 1);
    N.resize(state_dim, state_dim);

    // Initialize
    x_opt.setZero();
    N.setIdentity();

    // Loop
    for (size_t i = 0; i < rowsA; i++)
    {
        // Find Pseudoinverse
        AN = _A(i) * N;
        kindr::pseudoInverse(AN, pinvAN);
    
        // Find least squares optimal state for the at hand QP
        x_i = pinvAN * (_b(i) - _A(i) * x_opt);

        // Update optimal state for the hierarchical optimization
        x_opt = x_opt + N * x_i;

        // Find new null-space projector
        Eigen::MatrixXd A_stacked_tmp = A_stacked;

        A_stacked.resize(A_stacked_tmp.rows() + _A(i).rows(), state_dim);
        A_stacked << A_stacked_tmp,
                     _A(i);

        N = math_utils::SVDNullSpaceProjector(A_stacked);

        // Print w.r.t verbosity level
        if (_v == 1)
        {
            ROS_INFO_STREAM("Solution at iteration " << i << " is: \n" << x_opt);
        }

    }

    return x_opt;
}

// Solve a Quadratic Program
bool HierarchicalOptimizationControl::SolveQP(const Eigen::MatrixXd &_Q,
                                              const Eigen::VectorXd &_c,
                                              const Eigen::MatrixXd &_E_ineq,
                                              const Eigen::VectorXd &_f_ineq,
                                              Eigen::VectorXd &_sol,
                                              const SolverType &_solver,
                                              const int &_v)
{
    // Create a negative infinite lower bound
    Eigen::VectorXd lb;

    lb.resize(_f_ineq.rows(), 1);

    lb.setConstant(-math_utils::INF);

    return this->SolveQP(_Q, _c, _E_ineq, lb, _f_ineq, _sol, _solver, _v);
}

// Solve a Quadratic Program
bool HierarchicalOptimizationControl::SolveQP(const Eigen::MatrixXd &_Q,
                                              const Eigen::VectorXd &_c,
                                              const Eigen::MatrixXd &_A,
                                              const Eigen::VectorXd &_lb,
                                              const Eigen::VectorXd &_ub,
                                              Eigen::VectorXd &_sol,
                                              const SolverType &_solver,
                                              const int &_v)
{
    // Create an empty Mathematical Program
    drake::solvers::MathematicalProgram prog;

    // Create an empty Mathematical Program Result
    drake::solvers::MathematicalProgramResult result;

    // Add decision variables
    auto x = prog.NewContinuousVariables(_Q.cols(), "x");

    // Add quadratic cost
    // TODO change false to true to speed up
    prog.AddQuadraticCost(_Q, _c, x, true);

    // Add inequality constraints
    if (!_A.isZero())
    {
        if (_v > 2)
        {
            ROS_INFO_STREAM("Linear ineq, A: \n" << _A << "\n");
            ROS_INFO_STREAM("Linear ineq, lb: \n" << _lb << "\n");
            ROS_INFO_STREAM("Linear ineq, ub: \n" << _ub << "\n");
        }

        prog.AddLinearConstraint(_A, _lb, _ub, x);
    }

    // Solve the program
    switch (_solver)
    {
        case OSQP:
        {
            auto solver = drake::solvers::OsqpSolver();
            
            auto start = std::chrono::steady_clock::now();

            result = solver.Solve(prog);

            auto end = std::chrono::steady_clock::now();

            std::chrono::duration<double, std::micro> diff = end - start;

            ROS_INFO_STREAM("OSQP run-time: " << diff.count() << " microseconds. \n");

            auto details = result.get_solver_details<drake::solvers::OsqpSolver>();
            ROS_INFO_STREAM("Solver details: run_time: " << details.run_time << "\n");

            break;
        }
        case ECQP:
        {
            auto solver = drake::solvers::EqualityConstrainedQPSolver();
            result = solver.Solve(prog);

            break;
        }
        case CLP:
        {
            auto solver = drake::solvers::ClpSolver();

            auto start = std::chrono::steady_clock::now();

            result = solver.Solve(prog);

            auto end = std::chrono::steady_clock::now();

            std::chrono::duration<double, std::micro> diff = end - start;

            ROS_INFO_STREAM("CLP run-time: " << diff.count() << " microseconds. \n");

            break;
        }
        case SCS:
        {
            auto solver = drake::solvers::ScsSolver();
            result = solver.Solve(prog);

            break;
        }
        case SNOPT:
        {
            auto solver = drake::solvers::SnoptSolver();

            auto start = std::chrono::steady_clock::now();

            result = solver.Solve(prog);

            auto end = std::chrono::steady_clock::now();

            std::chrono::duration<double, std::micro> diff = end - start;

            ROS_INFO_STREAM("SNOPT run-time: " << diff.count() << " microseconds. \n");

            auto details = result.get_solver_details<drake::solvers::SnoptSolver>();
            ROS_INFO_STREAM("Solver details: info: " << details.info << "\n");

            break;
        }
        case UNSPECIFIED:
        {
            result = Solve(prog);

            break;
        }
        default:
        {
            ROS_WARN("[HierarchicalOptimizationControl::SolveQP] Could not determine solver type!");

            std::abort();
        }
    }

    if (!result.is_success())
    {
        ROS_WARN("[HierarchicalOptimizationControl::SolveQP] Failed to solve QP!");

        if (_v > 0)
        {
            //auto details = result.get_solver_details<drake::solvers::SnoptSolver>();

            ROS_INFO_STREAM("Solver id: " << result.get_solver_id() << "\n");
            ROS_INFO_STREAM("Solver result: " << result.get_solution_result() << "\n");
            ROS_INFO_STREAM("Solution result: " << result.GetSolution(x) << "\n");
            ROS_INFO_STREAM("Solution result: " << result.GetSolution(x) << "\n");
            //ROS_INFO_STREAM("Solver detals: status_val: " << details.status_val << "\n");
            //ROS_INFO_STREAM("Solver details: run_time: " << details.run_time);
            //ROS_INFO_STREAM("Solver details: info: " << details.info << "\n");
        }

        _sol.setZero();

        return false;
    }

    // Set solution
    _sol = result.GetSolution(x);


    // TODO add verbosity prints


    return true;
}


// Solve a Quadratic Program
bool HierarchicalOptimizationControl::SolveQP(const Eigen::MatrixXd &_Q,
                                              const Eigen::VectorXd &_c,
                                              const Eigen::MatrixXd &_E_eq,
                                              const Eigen::VectorXd &_f_eq,
                                              const Eigen::MatrixXd &_E_ineq,
                                              const Eigen::VectorXd &_f_ineq,
                                              Eigen::VectorXd &_sol,
                                              const SolverType &_solver,
                                              const int &_v)
{
    // Create an empty Mathematical Program
    drake::solvers::MathematicalProgram prog;

    // Create an empty Mathematical Program Result
    drake::solvers::MathematicalProgramResult result;

    // Add decision variables
    auto x = prog.NewContinuousVariables(_Q.cols(), "x");

    // Add quadratic cost
    prog.AddQuadraticCost(_Q, _c, x, true);

    // Add equality constraints
    if (!_E_eq.isZero())
    {
        prog.AddLinearEqualityConstraint(_E_eq, _f_eq, x);
    }

    // Add inequality constraints
    if (!_E_ineq.isZero())
    {
        prog.AddLinearConstraint((_E_ineq * x).array() <= _f_ineq.array());
    }

    // Solve the program
    switch (_solver)
    {
        case OSQP:
        {
            auto solver = drake::solvers::OsqpSolver();
            result = solver.Solve(prog);
            break;
        }
        case ECQP:
        {
            auto solver = drake::solvers::EqualityConstrainedQPSolver();
            result = solver.Solve(prog);
            break;
        }
        case CLP:
        {
            auto solver = drake::solvers::ClpSolver();
            result = solver.Solve(prog);
            break;
        }
        case SCS:
        {
            auto solver = drake::solvers::ScsSolver();
            result = solver.Solve(prog);
            break;
        }
        case SNOPT:
        {
            auto solver = drake::solvers::SnoptSolver();
            result = solver.Solve(prog);
            break;
        }
        case UNSPECIFIED:
        {
            result = Solve(prog);
            break;
        }
        default:
        {
            ROS_WARN("[HierarchicalOptimizationControl::SolveQP] Could not determine solver type!");

            std::abort();
        }
    }

    if (!result.is_success())
    {
        ROS_WARN("[HierarchicalOptimizationControl::SolveQP] Failed to solve QP!");

        _sol.setZero();

        return false;
    }

    // Set solution
    _sol = result.GetSolution(x);


    // TODO add verbosity prints


    return true;
}

// Publish function for ROS Joint State Torque messages
void HierarchicalOptimizationControl::PublishTorqueMsg(const Eigen::Matrix<double, 12, 1> &_desired_tau)
{
    // Declare msg
    sensor_msgs::JointState joint_state_msg;    

    // Set dimension
    joint_state_msg.effort.resize(12);

    // Front-left 
    joint_state_msg.effort[0] = _desired_tau(0);
    joint_state_msg.effort[1] = _desired_tau(1);
    joint_state_msg.effort[2] = _desired_tau(2);

    // Front right
    joint_state_msg.effort[3] = _desired_tau(3);
    joint_state_msg.effort[4] = _desired_tau(4);
    joint_state_msg.effort[5] = _desired_tau(5);

    // Rear left
    joint_state_msg.effort[6] = _desired_tau(6);
    joint_state_msg.effort[7] = _desired_tau(7);
    joint_state_msg.effort[8] = _desired_tau(8);

    // Rear right
    joint_state_msg.effort[9] = _desired_tau(9);
    joint_state_msg.effort[10] = _desired_tau(10);
    joint_state_msg.effort[11] = _desired_tau(11);

    // Set timestamp
    joint_state_msg.header.stamp = ros::Time::now();

    // Publish
    this->jointStatePub.publish(joint_state_msg);
}

// Callback for ROS Generalized Coordinates messages
void HierarchicalOptimizationControl::OnGenCoordMsg(const std_msgs::Float64MultiArrayConstPtr &_msg)
{
    std::vector<double> data = _msg->data;

    this->genCoord = Eigen::Map<Eigen::MatrixXd>(data.data(), 18, 1);

    //debug_utils::printGeneralizedCoordinates(this->genCoord);

    if (!this->kinematics.SolveForwardKinematics(genCoord, fPos))
    {
        ROS_ERROR("[HierarchicalOptimizationControl::OnGenCoordMsg] Could not solve forward kinematics!");
    }

    //debug_utils::printFootstepPositions(this->fPos);
}

// Callback for ROS Generalized Velocities messages
void HierarchicalOptimizationControl::OnGenVelMsg(const std_msgs::Float64MultiArrayConstPtr &_msg)
{
    std::vector<double> data = _msg->data;

    this->genVel = Eigen::Map<Eigen::MatrixXd>(data.data(), 18, 1); 

}

// Callback for ROS Contact State messages
void HierarchicalOptimizationControl::OnContactStateMsg(const std_msgs::Int8MultiArrayConstPtr &_msg)
{
    if (!_msg->data.size() == 4)
    {
        ROS_ERROR("[HierarchicalOptimizationControl::OnContactStateMsg] Received contact message with wrong size!");
    }
    else
    {
        this->contactState[0] = _msg->data[0];
        this->contactState[1] = _msg->data[1];
        this->contactState[2] = _msg->data[2];
        this->contactState[3] = _msg->data[3];
    }
}

// Setup thread to process messages
void HierarchicalOptimizationControl::ProcessQueueThread()
{
    static const double timeout = 0.01;
    while (this->rosNode->ok())
    {
        this->rosProcessQueue.callAvailable(ros::WallDuration(timeout));
    }
}

// Setup thread to publish messages
void HierarchicalOptimizationControl::PublishQueueThread()
{
    static const double timeout = 0.01;
    while (this->rosNode->ok())
    {
        this->rosPublishQueue.callAvailable(ros::WallDuration(timeout));
    }
}

// Initialize ROS
void HierarchicalOptimizationControl::InitRos()
{
    if (!ros::isInitialized())
    {
        int argc = 0;
        char **argv = NULL;
        ros::init(
            argc,
            argv,
            "hierarchical_optimization_control_node",
            ros::init_options::NoSigintHandler
        );
    }

    this->rosNode.reset(new ros::NodeHandle("hierarchical_optimization_control_node"));

    ros::SubscribeOptions gen_coord_so = 
        ros::SubscribeOptions::create<std_msgs::Float64MultiArray>(
            "/my_robot/gen_coord",
            1,
            boost::bind(&HierarchicalOptimizationControl::OnGenCoordMsg, this, _1),
            ros::VoidPtr(),
            &this->rosProcessQueue
            );

    ros::SubscribeOptions gen_vel_so = 
        ros::SubscribeOptions::create<std_msgs::Float64MultiArray>(
            "/my_robot/gen_vel",
            1,
            boost::bind(&HierarchicalOptimizationControl::OnGenVelMsg, this, _1),
            ros::VoidPtr(),
            &this->rosProcessQueue
            );

    ros::SubscribeOptions contact_state_so = 
        ros::SubscribeOptions::create<std_msgs::Int8MultiArray>(
            "/my_robot/contact_state",
            1,
            boost::bind(&HierarchicalOptimizationControl::OnContactStateMsg, this, _1),
            ros::VoidPtr(),
            &this->rosProcessQueue
            );

    ros::AdvertiseOptions joint_state_ao =
        ros::AdvertiseOptions::create<sensor_msgs::JointState>(
            "/my_robot/joint_state_cmd",
            1,
            ros::SubscriberStatusCallback(),
            ros::SubscriberStatusCallback(),
            ros::VoidPtr(),
            &this->rosPublishQueue
        );

    this->genCoordSub = this->rosNode->subscribe(gen_coord_so);

    this->genVelSub = this->rosNode->subscribe(gen_vel_so);

    this->contactStateSub = this->rosNode->subscribe(contact_state_so);

    this->jointStatePub = this->rosNode->advertise(joint_state_ao);
}

// Initialize ROS Publish and Process Queue Threads
void HierarchicalOptimizationControl::InitRosQueueThreads()
{
    this->rosPublishQueueThread = std::thread(
        std::bind(&HierarchicalOptimizationControl::PublishQueueThread, this)
    );

    this->rosProcessQueueThread = std::thread(
        std::bind(&HierarchicalOptimizationControl::ProcessQueueThread, this)
    );
}