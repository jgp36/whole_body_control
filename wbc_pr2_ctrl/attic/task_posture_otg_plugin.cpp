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
   \file task_posture_otg_plugin.cpp

   \brief Plugin that directly implements a WBC with task and
   nullspace-posture, and which uses the Reflexxes OTG library to
   limit accelerations in each task space separately.

   \author Roland Philippsen
*/

#include <wbc_pr2_ctrl/TaskPostureUI.h>
#include <wbc_pr2_ctrl/TaskPostureOTGDebug.h>
#include <pr2_controller_interface/controller.h>
#include <pluginlib/class_list_macros.h>
#include <jspace/tao_util.hpp>
#include <jspace/Model.hpp>
#include <wbc_urdf/Model.hpp>
#include <ros/console.h>
#include <tao/dynamics/taoDNode.h>
#include <Eigen/SVD>
#include <reflexxes_otg/TypeIOTG.h>

using namespace std;


namespace {
  // Need anonymous namespace, otherwise we get "interesting"
  // segfaults due to (I guess) the identically named but different
  // structs in other plugin sources.
  
  struct ctrl_to_ui_s {
    jspace::State state;
    jspace::Vector tau;
  };
  
  
  class OTGCursor
  {
  public:
    explicit OTGCursor(size_t ndof);
    
    int next(TypeIOTG & otg,
	     jspace::Vector const & maxvel,
	     jspace::Vector const & maxacc,
	     jspace::Vector const & goal);
    
    // good candidates for inlining!
    jspace::Vector & position();
    jspace::Vector const & position() const;
    jspace::Vector & velocity();
    jspace::Vector const & velocity() const;
    
    OTGCursor & operator = (OTGCursor const & rhs);
    
  protected:
    typedef Eigen::Matrix<bool, Eigen::Dynamic, 1> boolvec_t;
    
    boolvec_t selection_;
    jspace::Vector pos_clean_;
    jspace::Vector vel_clean_;
    jspace::Vector pos_dirty_;
    jspace::Vector vel_dirty_;
  };
  
  
  struct level_s {
    level_s & operator = (level_s const & rhs) {
      if (&rhs != this) {
	// Here, in this particular plugin, we know that otg and
	// cursor are shared between all our instances anyway, but
	// this will not be valid in all cases...
	goal = rhs.goal;
	maxvel = rhs.maxvel;
	maxacc = rhs.maxacc;
	kp = rhs.kp;
	kd = rhs.kd;
	goal_changed = rhs.goal_changed;
      }
      return *this;
    }
    
    boost::shared_ptr<TypeIOTG> otg;
    boost::shared_ptr<OTGCursor> cursor;
    jspace::Vector goal;
    jspace::Vector maxvel;
    jspace::Vector maxacc;
    jspace::Vector kp;
    jspace::Vector kd;
    mutable bool goal_changed;
  };
  
  
  struct ui_to_ctrl_s {
    ui_to_ctrl_s & operator = (ui_to_ctrl_s const & rhs) {
      if (&rhs != this) {
	end_effector = rhs.end_effector;
	control_point = rhs.control_point;
	for (size_t ii(0); ii < 2; ++ii) {
	  level[ii] = rhs.level[ii];
	}
      }
      return *this;
    }
    
    taoDNode const * end_effector;
    jspace::Vector control_point;
    
    level_s level[2];
  };

}


static bool stepTaskPosture(jspace::Model const & model,
			    ui_to_ctrl_s const & in,
			    ctrl_to_ui_s & out);


static size_t const NBUF(2);
static wbc_pr2_ctrl::TaskPostureOTGDebug dbg_msg;
static bool debug_output;

enum {
  TASK,
  POSTURE,
  NLEVELS
};


class TaskPostureOTGPlugin
  : public pr2_controller_interface::Controller
{
public:
  TaskPostureOTGPlugin();
  virtual ~TaskPostureOTGPlugin();
  
  virtual void update(void);
  virtual bool init(pr2_mechanism_model::RobotState * robot, ros::NodeHandle &nn);
  
  bool uiCallback(wbc_pr2_ctrl::TaskPostureUI::Request & request,
		  wbc_pr2_ctrl::TaskPostureUI::Response & response);
  
  std::vector<pr2_mechanism_model::JointState *> controlled_joint_;
  jspace::ros::Model ros_model_;
  size_t ndof_;
  jspace::Model model_;
  int tick_;
  
  size_t ctrl_to_ui_tick_;
  ctrl_to_ui_s ctrl_to_ui_data_[NBUF];
  
  size_t ui_to_ctrl_tick_;
  ui_to_ctrl_s ui_to_ctrl_data_[NBUF];
  
  ros::ServiceServer ui_server_;
  ros::Publisher dbg_pub_;
  
  std::string ui_dbg_msg_;
};


TaskPostureOTGPlugin::
TaskPostureOTGPlugin()
  : ros_model_("/wbc_pr2_ctrl/"),
    ndof_(0),
    tick_(0),
    ctrl_to_ui_tick_(1),	// start at one because we need tick-1
    ui_to_ctrl_tick_(1)		// start at one because we need tick-1
{
}


TaskPostureOTGPlugin::
~TaskPostureOTGPlugin()
{
}


static inline size_t clean(size_t tick)
{
  return tick % NBUF;
}


static inline size_t dirty(size_t tick)
{
  return (tick - 1) % NBUF;
}


void TaskPostureOTGPlugin::
update(void)
{
  ctrl_to_ui_s & out(ctrl_to_ui_data_[dirty(ctrl_to_ui_tick_)]);
  ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
  
  //////////////////////////////////////////////////
  // update state
  
  ros::WallTime const wall_now(ros::WallTime::now());
  out.state.time_sec_ = wall_now.sec;
  out.state.time_usec_ = wall_now.nsec / 1000;
  
  for (size_t ii(0); ii < ndof_; ++ii) {
    out.state.position_[ii] = controlled_joint_[ii]->position_;
    out.state.velocity_[ii] = controlled_joint_[ii]->velocity_;
    out.state.force_[ii] = controlled_joint_[ii]->measured_effort_;
  }
  model_.update(out.state);
  
  //////////////////////////////////////////////////
  // compute control torques
  
  bool const ok(stepTaskPosture(model_, in, out));
  if (debug_output) {
    cerr << "++++++++++++++++++++++++++++++++++++++++++++++++++\n"
	 << "UI callback debug:\n" << ui_dbg_msg_;
  }
  
  //////////////////////////////////////////////////
  // send torques to motors
  
  if (ok) {
    dbg_pub_.publish(dbg_msg);
    for (size_t ii(0); ii < ndof_; ++ii) {
      controlled_joint_[ii]->commanded_effort_ = out.tau[ii];
    }
  }
  else {
    for (size_t ii(0); ii < ndof_; ++ii) {
      out.tau[ii] = 0;
      controlled_joint_[ii]->commanded_effort_ = 0;
    }
  }
  
  // I guess ctrl_to_ui_tick is synonymous with just tick... also note
  // that ui_to_ctrl_tick gets updated in uiCallback()
  ++ctrl_to_ui_tick_;
  ++tick_;
}


bool TaskPostureOTGPlugin::
init(pr2_mechanism_model::RobotState * robot, ros::NodeHandle & nn)
{
  try {
    
    string debug_output_string;
    if ( ! nn.getParam("debug_output", debug_output_string)) {
      debug_output = false;
      ROS_INFO ("debug_output disabled (use `on' to enable it)");
    }
    else {
      if (("on" == debug_output_string) ||
	  ("true" == debug_output_string) ||
	  ("enabled" == debug_output_string) ||
	  ("TRUE" == debug_output_string)) {
	debug_output = true;
	ROS_INFO ("debug_output enabled");
      }
    }
    
    ROS_INFO ("creating TAO tree from URDF");
    static size_t const n_tao_roots(1);
    ros_model_.initFromURDF(nn, robot->model_->robot_model_, n_tao_roots);
    
    ROS_INFO ("retrieving controlled joints");
    controlled_joint_.clear();	// paranoid
    for (size_t ii(0); ii < ros_model_.tao_trees_[0]->info.size(); ++ii) {
      pr2_mechanism_model::JointState *
	joint(robot->getJointState(ros_model_.tao_trees_[0]->info[ii].joint_name));
      if ( ! joint) {
	// "never" happens because this is where the joint names come from in the first place...
	throw runtime_error("weird, no joint called `"
			    + ros_model_.tao_trees_[0]->info[ii].joint_name
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
    
    string end_effector_name;
    if ( ! nn.getParam("end_effector_name", end_effector_name)) {
      end_effector_name = "l_wrist_roll_link";
    }
    ROS_INFO ("end_effector_name `%s'", end_effector_name.c_str());
    
    taoDNode * ee(model_.getNodeByName(end_effector_name));
    if ( ! ee) {
      throw std::runtime_error("end effector `" + end_effector_name + "' not in model");
    }
    
    ROS_INFO ("initialising shared instances of task-internal data");
    
    ui_to_ctrl_data_[0].level[TASK].otg.reset(new TypeIOTG(3, 1e-3));
    ui_to_ctrl_data_[0].level[TASK].cursor.reset(new OTGCursor(3));
    ui_to_ctrl_data_[0].level[POSTURE].otg.reset(new TypeIOTG(ndof_, 1e-3));
    ui_to_ctrl_data_[0].level[POSTURE].cursor.reset(new OTGCursor(ndof_));
    for (size_t ii(1); ii < NBUF; ++ii) {
      for (size_t jj(0); jj < NLEVELS; ++jj) {
	ui_to_ctrl_data_[ii].level[jj].otg = ui_to_ctrl_data_[0].level[jj].otg;
	ui_to_ctrl_data_[ii].level[jj].cursor = ui_to_ctrl_data_[0].level[jj].cursor;
      }
    }
    
    ROS_INFO ("initialising double-buffered instances of task-internal data");
    
    for (size_t ii(0); ii < NBUF; ++ii) {
      
      ui_to_ctrl_data_[ii].end_effector = ee;
      ui_to_ctrl_data_[ii].control_point = jspace::Vector::Zero(3);
      
      ui_to_ctrl_data_[ii].level[TASK].goal =     0.6 * jspace::Vector::Ones(3);
      ui_to_ctrl_data_[ii].level[TASK].maxvel =   0.3 * jspace::Vector::Ones(3);
      ui_to_ctrl_data_[ii].level[TASK].maxacc =   0.6 * jspace::Vector::Ones(3);
      ui_to_ctrl_data_[ii].level[TASK].kp =     100.0 * jspace::Vector::Ones(3);
      ui_to_ctrl_data_[ii].level[TASK].kd =      20.0 * jspace::Vector::Ones(3);
      
      ui_to_ctrl_data_[ii].level[POSTURE].goal =  20.0 * M_PI / 180.0 * jspace::Vector::Ones(ndof_);
      ui_to_ctrl_data_[ii].level[POSTURE].maxvel =       M_PI         * jspace::Vector::Ones(ndof_);
      ui_to_ctrl_data_[ii].level[POSTURE].maxacc = 2.0 * M_PI         * jspace::Vector::Ones(ndof_);
      ui_to_ctrl_data_[ii].level[POSTURE].kp =   100.0 * jspace::Vector::Ones(ndof_);
      ui_to_ctrl_data_[ii].level[POSTURE].kd =    20.0 * jspace::Vector::Ones(ndof_);
      
      for (size_t jj(0); jj < NLEVELS; ++jj) {
	// The first time around, the trajectories will need to get
	// initialized, just as if a goal had just been set.
	ui_to_ctrl_data_[ii].level[jj].goal_changed = true;
      }
      
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
    
    for (size_t ii(0); ii < NBUF; ++ii) {
      ctrl_to_ui_data_[ii].state.init(ndof_, ndof_, ndof_);
      ctrl_to_ui_data_[ii].tau = jspace::Vector::Zero(ndof_);
    }
    tick_ = 0;
    
    ROS_INFO ("wbc_plugin ready to rock!");
  }
  catch (std::exception const & ee) {
    ROS_ERROR ("TaskPostureOTGPlugin::init(): EXCEPTION: %s", ee.what());
    return false;
  }
  
  ui_server_ = nn.advertiseService("/wbc_pr2_ctrl/tp_ui", &TaskPostureOTGPlugin::uiCallback, this);
  dbg_pub_ = nn.advertise<wbc_pr2_ctrl::TaskPostureOTGDebug>("/wbc_pr2_ctr/tp_dbg", 100);
  dbg_msg.task.pos_act.assign(3, 0.0);
  dbg_msg.task.vel_act.assign(3, 0.0);
  dbg_msg.task.pos_trj.assign(3, 0.0);
  dbg_msg.task.vel_trj.assign(3, 0.0);
  dbg_msg.task.pos_end.assign(3, 0.0);
  dbg_msg.posture.pos_act.assign(ndof_, 0.0);
  dbg_msg.posture.vel_act.assign(ndof_, 0.0);
  dbg_msg.posture.pos_trj.assign(ndof_, 0.0);
  dbg_msg.posture.vel_trj.assign(ndof_, 0.0);
  dbg_msg.posture.pos_end.assign(ndof_, 0.0);
    
  return true;
}


bool TaskPostureOTGPlugin::
uiCallback(wbc_pr2_ctrl::TaskPostureUI::Request & request,
	   wbc_pr2_ctrl::TaskPostureUI::Response & response)
{
  std::ostringstream dbg_os;
  
  response.ok = true;
  
  switch (request.mode) {
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_CONTROL_POINT:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_CONTROL_POINT\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (3 == request.value.size()) {
	  jspace::convert(request.value, out.control_point);
	  jspace::pretty_print(out.control_point, dbg_os, "  control_point", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid control point dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_TASK_GOAL:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_TASK_GOAL\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (3 == request.value.size()) {
	  jspace::convert(request.value, out.level[TASK].goal);
	  out.level[TASK].goal_changed = true;
	  jspace::pretty_print(out.level[TASK].goal, dbg_os, "  task_goal", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid task goal dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_TASK_KP:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_TASK_KP\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (3 == request.value.size()) {
	  jspace::convert(request.value, out.level[TASK].kp);
	  jspace::pretty_print(out.level[TASK].kp, dbg_os, "  task_kp", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid task kp dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }

  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_TASK_KD:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_TASK_KD\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (3 == request.value.size()) {
	  jspace::convert(request.value, out.level[TASK].kd);
	  jspace::pretty_print(out.level[TASK].kd, dbg_os, "  task_kd", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid task kd dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_POSTURE_GOAL:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_POSTURE_GOAL\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (ndof_ == request.value.size()) {
	  jspace::convert(request.value, out.level[POSTURE].goal);
	  out.level[POSTURE].goal_changed = true;
	  jspace::pretty_print(out.level[POSTURE].goal, dbg_os, "  posture_goal", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid posture goal dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_POSTURE_KP:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_POSTURE_KP\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (ndof_ == request.value.size()) {
	  jspace::convert(request.value, out.level[POSTURE].kp);
	  jspace::pretty_print(out.level[POSTURE].kp, dbg_os, "  posture_kp", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid posture kp dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }

  case wbc_pr2_ctrl::TaskPostureUI::Request::SET_POSTURE_KD:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = SET_POSTURE_KD\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      ui_to_ctrl_s & out(ui_to_ctrl_data_[dirty(ui_to_ctrl_tick_)]);
      out = in;
      if ( ! request.value.empty()) {
	if (ndof_ == request.value.size()) {
	  jspace::convert(request.value, out.level[POSTURE].kd);
	  jspace::pretty_print(out.level[POSTURE].kd, dbg_os, "  posture_kd", "    ");
	}
	else {
	  response.ok = false;
	  response.errstr = "invalid posture kd dimension";
	}
      }
      ++ui_to_ctrl_tick_;
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_CONTROL_POINT:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_CONTROL_POINT\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.control_point, response.value);
      jspace::pretty_print(in.control_point, dbg_os, "  control_point", "    ");
      for (size_t ii(0); ii < 3; ++ii) {
	response.lower_bound.push_back(-1); // should not hardcode this... ah well.
	response.upper_bound.push_back(1);
	response.unit.push_back("m");
      }
      response.name.push_back("ctrl pt x");
      response.name.push_back("ctrl pt y");
      response.name.push_back("ctrl pt z");
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_TASK_GOAL:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_TASK_GOAL\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.level[TASK].goal, response.value);
      jspace::pretty_print(in.level[TASK].goal, dbg_os, "  task_goal", "    ");
      for (size_t ii(0); ii < 3; ++ii) {
	response.lower_bound.push_back(-2); // should not hardcode this... ah well.
	response.upper_bound.push_back(2);
	response.unit.push_back("m");
      }
      response.name.push_back("EE pos x");
      response.name.push_back("EE pos y");
      response.name.push_back("EE pos z");
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_TASK_KP:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_TASK_KP\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.level[TASK].kp, response.value);
      jspace::pretty_print(in.level[TASK].kp, dbg_os, "  task_kp", "    ");
      for (size_t ii(0); ii < 3; ++ii) {
	response.lower_bound.push_back(0);
	response.upper_bound.push_back(1000); // should not hardcode this... ah well.
      }
      response.name.push_back("kp x");
      response.name.push_back("kp y");
      response.name.push_back("kp z");
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_TASK_KD:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_TASK_KD\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.level[TASK].kd, response.value);
      jspace::pretty_print(in.level[TASK].kd, dbg_os, "  task_kd", "    ");
      for (size_t ii(0); ii < 3; ++ii) {
	response.lower_bound.push_back(0);
	response.upper_bound.push_back(63); // should not hardcode this... ah well.
      }
      response.name.push_back("kd x");
      response.name.push_back("kd y");
      response.name.push_back("kd z");
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_POSTURE_GOAL:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_POSTURE_GOAL\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.level[POSTURE].goal, response.value);
      jspace::pretty_print(in.level[POSTURE].goal, dbg_os, "  posture_goal", "    ");
      for (size_t ii(0); ii < ndof_; ++ii) {
	response.lower_bound.push_back(-2 * M_PI); // should not hardcode this... ah well.
	response.upper_bound.push_back(2 * M_PI);
	response.unit.push_back("rad");
	ostringstream msg;
	msg << "joint pos " << ii;
	response.name.push_back(msg.str());
      }
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_POSTURE_KP:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_POSTURE_KP\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.level[POSTURE].kp, response.value);
      jspace::pretty_print(in.level[POSTURE].kp, dbg_os, "  posture_kp", "    ");
      for (size_t ii(0); ii < ndof_; ++ii) {
	response.lower_bound.push_back(0);
	response.upper_bound.push_back(1000); // should not hardcode this... ah well.
	ostringstream msg;
	msg << "joint kp " << ii;
	response.name.push_back(msg.str());
      }
      break;
    }
    
  case wbc_pr2_ctrl::TaskPostureUI::Request::GET_POSTURE_KD:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = GET_POSTURE_KD\n";
      ui_to_ctrl_s const & in(ui_to_ctrl_data_[clean(ui_to_ctrl_tick_)]);
      jspace::convert(in.level[POSTURE].kd, response.value);
      jspace::pretty_print(in.level[POSTURE].kd, dbg_os, "  posture_kd", "    ");
      for (size_t ii(0); ii < ndof_; ++ii) {
	response.lower_bound.push_back(0);
	response.upper_bound.push_back(63); // should not hardcode this... ah well.
	ostringstream msg;
	msg << "joint kd " << ii;
	response.name.push_back(msg.str());
      }
      break;
    }
    
  default:
    {
      dbg_os << "  request.mode = " << (int) request.mode << " = INVALID\n";
      ostringstream msg;
      msg << "invalid mode: " << (int) request.mode;
      response.ok = false;
      response.errstr = msg.str();
      break;
    }
  }
  
  if ( ! response.ok) {
    dbg_os << "  ERROR = " << response.errstr << "\n";
  }
  else {
    dbg_os << "  success\n";
  }
  ui_dbg_msg_ = dbg_os.str();
  
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
		     ui_to_ctrl_s const & in,
		     ctrl_to_ui_s & out)
{
  //////////////////////////////////////////////////
  // sanity checks
  
  if (in.control_point.rows() != 3) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid control_point dimension %d",
	       in.control_point.rows());
    return false;
  }
  if (in.level[TASK].goal.rows() != 3) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid task_goal dimension %d",
	       in.level[TASK].goal.rows());
    return false;
  }
  if (in.level[TASK].maxvel.rows() != 3) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid task_maxvel dimension %d",
  	       in.level[TASK].maxvel.rows());
    return false;
  }
  if (in.level[TASK].maxacc.rows() != 3) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid task_maxacc dimension %d",
  	       in.level[TASK].maxacc.rows());
    return false;
  }
  if (in.level[TASK].kp.rows() != 3) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid task_kp dimension %d",
	       in.level[TASK].kp.rows());
    return false;
  }
  if (in.level[TASK].kd.rows() != 3) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid task_kd dimension %d",
	       in.level[TASK].kd.rows());
    return false;
  }
  
  size_t const ndof(model.getNDOF());
  if (in.level[POSTURE].goal.rows() != ndof) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid posture_goal dimension %d",
	       in.level[POSTURE].goal.rows());
    return false;
  }
  if (in.level[POSTURE].maxvel.rows() != ndof) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid posture_maxvel dimension %d",
  	       in.level[POSTURE].maxvel.rows());
    return false;
  }
  if (in.level[POSTURE].maxacc.rows() != ndof) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid posture_maxacc dimension %d",
  	       in.level[POSTURE].maxacc.rows());
    return false;
  }
  if (in.level[POSTURE].kp.rows() != ndof) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid posture_kp dimension %d",
	       in.level[POSTURE].kp.rows());
    return false;
  }
  if (in.level[POSTURE].kd.rows() != ndof) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): invalid posture_kd dimension %d",
	       in.level[POSTURE].kd.rows());
    return false;
  }

  //////////////////////////////////////////////////
  // task
  
  jspace::Transform eepos;
  model.computeGlobalFrame(in.end_effector,
			   in.control_point[0],
			   in.control_point[1],
			   in.control_point[2],
			   eepos);
  
  jspace::Matrix Jfull;
  model.computeJacobian(in.end_effector,
			eepos.translation()[0],
			eepos.translation()[1],
			eepos.translation()[2],
			Jfull);
  jspace::Matrix Jx(Jfull.block(0, 0, 3, ndof));
  jspace::Matrix invA;
  model.getInverseMassInertia(invA);
  jspace::Matrix invLambda(Jx * invA * Jx.transpose());
  jspace::Matrix Lambda;
  pseudoInverse(invLambda, 1e-3, Lambda);
  
  // use online trajectory generator for acceleration-bounded control
  jspace::Vector curpos(eepos.translation());
  jspace::Vector curvel(Jx * model.getState().velocity_);
  if (in.level[TASK].goal_changed) {
    in.level[TASK].cursor->position() = curpos;
    in.level[TASK].cursor->velocity() = curvel;
    in.level[TASK].goal_changed = false;
  }
  int otg_result(in.level[TASK].cursor->next(*in.level[TASK].otg,
					     in.level[TASK].maxvel,
					     in.level[TASK].maxacc,
					     in.level[TASK].goal));
  if (0 > otg_result) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): OTG returned failure code %d for task",
	       otg_result);
    return false;
  }
  jspace::Vector poserror(curpos - in.level[TASK].cursor->position());
  jspace::Vector velerror(curvel - in.level[TASK].cursor->velocity());
  jspace::Vector tau_task(Jx.transpose() * (-Lambda)
			  * (   in.level[TASK].kp.cwise() * poserror
			      + in.level[TASK].kd.cwise() * velerror));
  
  // debugging...
  
  jspace::convert(curpos, dbg_msg.task.pos_act);
  jspace::convert(curvel, dbg_msg.task.vel_act);
  jspace::convert(in.level[TASK].cursor->position(), dbg_msg.task.pos_trj);
  jspace::convert(in.level[TASK].cursor->velocity(), dbg_msg.task.vel_trj);
  jspace::convert(in.level[TASK].goal, dbg_msg.task.pos_end);
  
  if (debug_output) {
    cerr << "==================================================\n";
    jspace::pretty_print(in.level[TASK].goal, cerr, "task_goal", "  ");
    jspace::pretty_print(curpos, cerr, "curpos", "  ");
    jspace::pretty_print(curvel, cerr, "curvel", "  ");
    jspace::pretty_print(in.level[TASK].cursor->position(), cerr, "otg_task_pos", "  ");
    jspace::pretty_print(in.level[TASK].cursor->velocity(), cerr, "otg_task_vel", "  ");
    jspace::pretty_print(poserror, cerr, "poserror", "  ");
    jspace::pretty_print(velerror, cerr, "velerror", "  ");
    jspace::pretty_print(in.level[TASK].kp, cerr, "task_kp", "  ");
    jspace::pretty_print(in.level[TASK].kd, cerr, "task_kd", "  ");
    jspace::pretty_print(Jx, cerr, "Jx", "  ");
    jspace::pretty_print(Lambda, cerr, "Lambda", "  ");
    cerr << "--------------------------------------------------\n";
    jspace::pretty_print(tau_task, cerr, "tau_task", "  ");
  }
  
  //////////////////////////////////////////////////
  // posture
  
  jspace::Matrix Jbar(invA * Jx.transpose() * Lambda);
  jspace::Matrix nullspace(jspace::Matrix::Identity(ndof, ndof) - Jbar * Jx);
  jspace::Matrix invLambda_p(nullspace * invA);
  jspace::Matrix Lambda_p;
  pseudoInverse(invLambda_p, 1e-3, Lambda_p);
  
  // use online trajectory generator for acceleration-bounded control
  if (in.level[POSTURE].goal_changed) {
    in.level[POSTURE].cursor->position() = model.getState().position_;
    in.level[POSTURE].cursor->velocity() = model.getState().velocity_;
    in.level[POSTURE].goal_changed = false;
  }
  otg_result = in.level[POSTURE].cursor->next(*in.level[POSTURE].otg,
					      in.level[POSTURE].maxvel,
					      in.level[POSTURE].maxacc,
					      in.level[POSTURE].goal);
  if (0 > otg_result) {
    ROS_ERROR ("TaskPostureOTGPlugin::stepTaskPosture(): OTG returned failure code %d for posture",
	       otg_result);
    return false;
  }
  jspace::Vector posture_poserror(model.getState().position_ - in.level[POSTURE].cursor->position());
  jspace::Vector posture_velerror(model.getState().velocity_ - in.level[POSTURE].cursor->velocity());
  jspace::Vector tau_posture(nullspace.transpose() * (-Lambda_p)
			     * (  in.level[POSTURE].kp.cwise() * posture_poserror
				+ in.level[POSTURE].kd.cwise() * posture_velerror));
  
  // debugging...
  
  jspace::convert(model.getState().position_, dbg_msg.posture.pos_act);
  jspace::convert(model.getState().velocity_, dbg_msg.posture.vel_act);
  jspace::convert(in.level[POSTURE].cursor->position(), dbg_msg.posture.pos_trj);
  jspace::convert(in.level[POSTURE].cursor->velocity(), dbg_msg.posture.vel_trj);
  jspace::convert(in.level[POSTURE].goal, dbg_msg.posture.pos_end);
  
  if (debug_output) {
    cerr << "--------------------------------------------------\n";
    jspace::pretty_print(in.level[POSTURE].goal, cerr, "posture_goal", "  ");
    jspace::pretty_print(model.getState().position_, cerr, "posture curpos", "  ");
    jspace::pretty_print(model.getState().velocity_, cerr, "posture curvel", "  ");
    jspace::pretty_print(in.level[POSTURE].cursor->position(), cerr, "otg_posture_pos", "  ");
    jspace::pretty_print(in.level[POSTURE].cursor->velocity(), cerr, "otg_posture_vel", "  ");
    jspace::pretty_print(posture_poserror, cerr, "posture_poserror", "  ");
    jspace::pretty_print(posture_velerror, cerr, "posture_velerror", "  ");
    jspace::pretty_print(in.level[POSTURE].kp, cerr, "posture_kp", "  ");
    jspace::pretty_print(in.level[POSTURE].kd, cerr, "posture_kd", "  ");
    jspace::pretty_print(nullspace, cerr, "nullspace", "  ");
    jspace::pretty_print(Lambda_p, cerr, "Lambda_p", "  ");
    cerr << "--------------------------------------------------\n";
    jspace::pretty_print(tau_posture, cerr, "tau_posture", "  ");
  }
  
  //////////////////////////////////////////////////
  // sum it up...
  
  jspace::Vector gg;
  model.getGravity(gg);
  out.tau = tau_task + tau_posture + gg;
  
  if (debug_output) {
    cerr << "--------------------------------------------------\n";
    jspace::pretty_print(gg, cerr, "gravity", "  ");
    jspace::pretty_print(out.tau, cerr, "tau", "  ");
  }
}


namespace {  
  
  
  OTGCursor::
  OTGCursor(size_t ndof)
  {
    pos_clean_ = jspace::Vector::Zero(ndof);
    vel_clean_ = jspace::Vector::Zero(ndof);
    pos_dirty_ = jspace::Vector::Zero(ndof);
    vel_dirty_ = jspace::Vector::Zero(ndof);
    selection_.resize(ndof);
    for (size_t ii(0); ii < ndof; ++ii) {
      selection_[ii] = true;
    }
  }
  
  
  static void dump_selection_vector(std::ostream & os, bool const * selection, size_t len)
  {
    for (size_t ii(0); ii < len; ++ii) {
      if (ii) {
	os << "    ";
      }
      if (selection[ii]) {
	os << " true";
      }
      else {
	os << "false";
      }
    }
  }
  
  
  int OTGCursor::
  next(TypeIOTG & otg,
       jspace::Vector const & maxvel,
       jspace::Vector const & maxacc,
       jspace::Vector const & goal)
  {
    int const otg_result(otg.GetNextMotionState_Position(pos_clean_.data(),
							 vel_clean_.data(),
							 maxvel.data(),
							 maxacc.data(),
							 goal.data(),
							 selection_.data(),
							 pos_dirty_.data(),
							 vel_dirty_.data()));
    
    if (debug_output) {
      cerr << "++++++++++++++++++++++++++++++++++++++++++++++++++\n"
	   << "OTGCursor::next():\n";
      jspace::pretty_print(maxvel, cerr, "  maxvel", "    ");
      jspace::pretty_print(maxacc, cerr, "  maxacc", "    ");
      jspace::pretty_print(goal, cerr, "  goal", "    ");
      cerr << "  selection\n    ";
      dump_selection_vector(cerr, selection_.data(), selection_.rows());
      cerr << "\n";
      jspace::pretty_print(pos_clean_, cerr, "  pos_clean", "    ");
      jspace::pretty_print(pos_dirty_, cerr, "  pos_dirty", "    ");
      jspace::pretty_print(vel_clean_, cerr, "  vel_clean", "    ");
      jspace::pretty_print(vel_dirty_, cerr, "  vel_dirty", "    ");
      if (0 <= otg_result) {
	cerr << "  success\n";
      }
      else {
	cerr << "  ERROR: " << otg_result << "\n";
      }
    }
    
    if (0 <= otg_result) {
      pos_clean_ = pos_dirty_;
      vel_clean_ = vel_dirty_;
    }
    
    return otg_result;
  }
  
  
  jspace::Vector & OTGCursor::
  position()
  {
    return pos_clean_;
  }
  
  
  jspace::Vector const & OTGCursor::
  position() const
  {
    return pos_clean_;
  }

  
  jspace::Vector & OTGCursor::
  velocity()
  {
    return vel_clean_;
  }


  jspace::Vector const & OTGCursor::
  velocity() const
  {
    return vel_clean_;
  }
  
  
  OTGCursor & OTGCursor::
  operator = (OTGCursor const & rhs)
  {
    if (&rhs != this) {
      selection_ = rhs.selection_;
      pos_clean_ = rhs.pos_clean_;
      vel_clean_ = rhs.vel_clean_;
    }
    return *this;
  }
  
}


PLUGINLIB_REGISTER_CLASS (TaskPostureOTGPlugin, TaskPostureOTGPlugin, pr2_controller_interface::Controller)
