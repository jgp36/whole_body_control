/*
 * PR2 controller plugin for Stanford-WBC http://stanford-wbc.sourceforge.net/
 *
 * Copyright (c) 2010 Stanford University. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 */

/**
   \file wbc_plugin.cpp
   \brief Plugin that directly implements a whole-body controller.
   \author Roland Philippsen
*/

#include <pr2_controller_interface/controller.h>
#include <pluginlib/class_list_macros.h>
#include <jspace/tao_util.hpp>
#include <jspace/Model.hpp>
#include <wbc_urdf/Model.hpp>
#include <ros/console.h>
#include <tao/dynamics/taoDNode.h>
#include <Eigen/SVD>

using namespace std;


static bool stepTaskPosture(jspace::Model const & model,
			    taoDNode const * end_effector,
			    jspace::Vector const & local_control_point,
			    jspace::Vector const & task_goal,
			    jspace::Vector const & task_kp,
			    jspace::Vector const & task_kd,
			    jspace::Vector const & posture_goal,
			    jspace::Vector const & posture_kp,
			    jspace::Vector const & posture_kd,
			    jspace::Vector & tau);


class WBCPlugin
  : public pr2_controller_interface::Controller
{
public:
  WBCPlugin();
  virtual ~WBCPlugin();
  
  virtual void update(void);
  virtual bool init(pr2_mechanism_model::RobotState * robot, ros::NodeHandle &nn);
  
  std::vector<pr2_mechanism_model::JointState *> controlled_joint_;
  jspace::ros::Model ros_model_;
  size_t ndof_;
  jspace::Model model_;
  taoDNode const * end_effector_;
  jspace::State state_;
  jspace::Vector tau_;
  int tick_;
};


WBCPlugin::
WBCPlugin()
  : ros_model_("/wbc_pr2_ctrl/"),
    ndof_(0),
    tick_(0)
{
}


WBCPlugin::
~WBCPlugin()
{
}


void WBCPlugin::
update(void)
{  
  //////////////////////////////////////////////////
  // update state
  
  ros::WallTime const wall_now(ros::WallTime::now());
  state_.time_sec_ = wall_now.sec;
  state_.time_usec_ = wall_now.nsec / 1000;
  
  for (size_t ii(0); ii < ndof_; ++ii) {
    state_.position_[ii] = controlled_joint_[ii]->position_;
    state_.velocity_[ii] = controlled_joint_[ii]->velocity_;
    state_.force_[ii] = controlled_joint_[ii]->measured_effort_;
  }
  model_.update(state_);
  
  //////////////////////////////////////////////////
  // compute control torques
  
  jspace::Vector local_control_point(3);
  local_control_point << 0.0 , 0.1 , 0.0;
  jspace::Vector task_goal(3);
  task_goal << 0.2 , 0.2 , 0.2;
  jspace::Vector task_kp(3);
  task_kp = 20.0 * jspace::Vector::Ones(3);
  jspace::Vector task_kd(3);
  task_kd = 1.0 * jspace::Vector::Ones(3);
  jspace::Vector posture_goal;
  posture_goal = 20.0 * M_PI / 180.0 * jspace::Vector::Ones(ndof_);
  jspace::Vector posture_kp;
  posture_kp = 20.0 * jspace::Vector::Ones(ndof_);
  jspace::Vector posture_kd;
  posture_kd = 1.0 * jspace::Vector::Ones(ndof_);

  bool const ok(stepTaskPosture(model_,
				end_effector_, local_control_point,
				task_goal, task_kp, task_kd,
				posture_goal, posture_kp, posture_kd,
				tau_));
  
  //////////////////////////////////////////////////
  // send torques to motors
  
  if (ok) {
    for (size_t ii(0); ii < ndof_; ++ii) {
      controlled_joint_[ii]->commanded_effort_ = tau_[ii];
    }
  }
  else {
    for (size_t ii(0); ii < ndof_; ++ii) {
      controlled_joint_[ii]->commanded_effort_ = 0;
    }
  }
  
  ++tick_;
}


bool WBCPlugin::
init(pr2_mechanism_model::RobotState * robot, ros::NodeHandle & nn)
{
  try {
    
    ROS_INFO ("creating TAO tree from URDF");
    static size_t const n_tao_roots(1);
    ros_model_.initFromURDF(nn, robot->model_->robot_model_, n_tao_roots);
    
    ROS_INFO ("retrieving controlled joints");
    controlled_joint_.clear();	// paranoid
    for (size_t ii(0); ii < ros_model_.tao_trees_[0]->info.size(); ++ii) {
      pr2_mechanism_model::JointState * joint(robot->getJointState(ros_model_.tao_trees_[0]->info[ii].joint_name));
      if ( ! joint) { // "never" happens because this is where the joint names come from in the first place...
	throw runtime_error("weird, no joint called `" + ros_model_.tao_trees_[0]->info[ii].joint_name
			    + "' in the pr2_mechanism_model???");
      }
      controlled_joint_.push_back(joint);
    }
    ndof_ = controlled_joint_.size();
    
    ROS_INFO ("creating jspace model from TAO");
    {
      std::ostringstream msg;
      if (0 != model_.init(ros_model_.tao_trees_[0], 0, &msg)) {
	throw std::runtime_error("jspace::Model::init() failed: " + msg.str());
      }
    }
    if (model_.getNDOF() != ndof_) {
      std::ostringstream msg;
      msg << "weird, jspace::Model::getNDOF() says "
	  << model_.getNDOF() << " but we have " << ndof_ << "DOF";
      throw std::runtime_error(msg.str());
    }
    end_effector_ = model_.getNodeByName("l_wrist_roll_link");
    if ( ! end_effector_) {
      throw std::runtime_error("no l_wrist_roll_link in model (MAKE THIS RUNTIME CONFIGURABLE)");
    }
    
    ROS_INFO ("marking gravity-compensated joints");
    std::vector<std::string>::const_iterator
      gclink(ros_model_.gravity_compensated_links_.begin());
    for (/**/; gclink != ros_model_.gravity_compensated_links_.end(); ++gclink) {
      taoDNode const * node(model_.getNodeByName(*gclink));
      if ( ! node) {
	throw std::runtime_error("gravity-compensated link " + *gclink
				 + " is not part of the jspace::Model");
      }
      int const id(node->getID());
      model_.disableGravityCompensation(id, true);
      ROS_INFO ("disabled gravity compensation for link %s (ID %d)", gclink->c_str(), id);
    }
    
    state_.init(ndof_, ndof_, ndof_);
    tau_ = jspace::Vector::Zero(ndof_);
    tick_ = 0;
    
    ROS_INFO ("wbc_plugin ready to rock!");
  }
  catch (std::exception const & ee) {
    ROS_ERROR ("WBCPlugin::init(): EXCEPTION: %s", ee.what());
    return false;
  }
  
  return true;
}


static void pseudoInverse(jspace::Matrix const & matrix,
			  double sigmaThreshold,
			  jspace::Matrix & invMatrix)
{
  Eigen::SVD<jspace::Matrix> svd(matrix);
  // not sure if we need to svd.sort()... probably not
  int const nrows(svd.singularValues().rows());
  jspace::Matrix invS;
  invS = jspace::Matrix::Zero(nrows, nrows);
  for (int ii(0); ii < nrows; ++ii) {
    if (svd.singularValues().coeff(ii) > sigmaThreshold) {
      invS.coeffRef(ii, ii) = 1.0 / svd.singularValues().coeff(ii);
    }
  }
  invMatrix = svd.matrixU() * invS * svd.matrixU().transpose();
}


bool stepTaskPosture(jspace::Model const & model,
		     taoDNode const * end_effector,
		     jspace::Vector const & local_control_point,
		     jspace::Vector const & task_goal,
		     jspace::Vector const & task_kp,
		     jspace::Vector const & task_kd,
		     jspace::Vector const & posture_goal,
		     jspace::Vector const & posture_kp,
		     jspace::Vector const & posture_kd,
		     jspace::Vector & tau)
{
  //////////////////////////////////////////////////
  // sanity checks
  
  if (local_control_point.rows() != 3) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid local_control_point dimension %d",
	       local_control_point.rows());
    return false;
  }
  if (task_goal.rows() != 3) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid task_goal dimension %d",
	       task_goal.rows());
    return false;
  }
  if (task_kp.rows() != 3) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid task_kp dimension %d",
	       task_kp.rows());
    return false;
  }
  if (task_kd.rows() != 3) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid task_kd dimension %d",
	       task_kd.rows());
    return false;
  }
  
  size_t const ndof(model.getNDOF());
  if (posture_goal.rows() != ndof) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid posture_goal dimension %d",
	       posture_goal.rows());
    return false;
  }
  if (posture_kp.rows() != ndof) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid posture_kp dimension %d",
	       posture_kp.rows());
    return false;
  }
  if (posture_kd.rows() != ndof) {
    ROS_ERROR ("WBCPlugin::stepTaskPosture(): invalid posture_kd dimension %d",
	       posture_kd.rows());
    return false;
  }
  
  //////////////////////////////////////////////////
  // task
  
  jspace::Transform eepos;
  model.computeGlobalFrame(end_effector,
			   local_control_point[0],
			   local_control_point[1],
			   local_control_point[2],
			   eepos);
  
  jspace::Matrix Jfull;
  model.computeJacobian(end_effector,
			eepos.translation()[0],
			eepos.translation()[1],
			eepos.translation()[2],
			Jfull);
  jspace::Matrix Jx(Jfull.block(0, 0, 3, 7));
  jspace::Matrix invA;
  model.getInverseMassInertia(invA);
  jspace::Matrix invLambda(Jx * invA * Jx.transpose());
  jspace::Matrix Lambda;
  pseudoInverse(invLambda, 1e-3, Lambda);
  
  jspace::Vector poserror(eepos.translation() - task_goal);
  jspace::Vector tau_task(Jx.transpose() * (-Lambda)
			  * (   task_kp.cwise() * poserror
			      + task_kd.cwise() * Jx * model.getState().velocity_));
  
  //////////////////////////////////////////////////
  // posture
  
  jspace::Matrix Jbar(invA * Jx.transpose() * Lambda);
  jspace::Matrix nullspace(jspace::Matrix::Identity(7, 7) - Jbar * Jx);
  jspace::Matrix invLambda_p(nullspace * invA);
  jspace::Matrix Lambda_p;
  pseudoInverse(invLambda_p, 1e-3, Lambda_p);
  
  jspace::Vector tau_posture(nullspace.transpose() * (-Lambda_p)
			     * (  posture_kp.cwise() * (model.getState().position_ - posture_goal)
				+ posture_kd.cwise() *  model.getState().velocity_));
  
  //////////////////////////////////////////////////
  // sum it up...
  
  jspace::Vector gg;
  model.getGravity(gg);
  tau = tau_task + tau_posture + gg;
}


PLUGINLIB_REGISTER_CLASS (WBCPlugin, WBCPlugin, pr2_controller_interface::Controller)