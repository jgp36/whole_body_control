/*
 * Shared copyright notice and LGPLv3 license statement.
 *
 * Copyright (C) 2011 The Board of Trustees of The Leland Stanford Junior University. All rights reserved.
 * Copyright (C) 2011 University of Texas at Austin. All rights reserved.
 *
 * Authors: Roland Philippsen (Stanford) and Luis Sentis (UT Austin)
 *          http://cs.stanford.edu/group/manips/
 *          http://www.me.utexas.edu/~hcrl/
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

#ifndef UTA_OPSPACE_DYNAMIC_ACCURACY_TEST_HPP
#define UTA_OPSPACE_DYNAMIC_ACCURACY_TEST_HPP

#include <opspace/Skill.hpp>
#include <opspace/task_library.hpp>
#include <ros/ros.h>
#include <time.h>

namespace uta_opspace {
  
  using namespace opspace;
  
  
  class DynamicAccuracyTest
    : public Skill
  {
  public:
    DynamicAccuracyTest(std::string const & name);
    
    virtual Status init(Model const & model);
    virtual Status update(Model const & model);
    virtual task_table_t const * getTaskTable();
    virtual Status checkJStarSV(Task const * task, Vector const & sv);
    
    void dbg(std::ostream & os,
	     std::string const & title,
	     std::string const & prefix) const;
    
  protected:
    void updateCurGoal(ros::Time current_time);
    SelectedJointPostureTask * posture_task_;
    CartPosTask * goal_eepos_task_;
    task_table_t goal_task_table_;
  
    Parameter * eepos_goal_;
    Parameter * eevel_goal_;
    
    Vector cur_goal_;
    Vector center_position_;
    double radius_;
    double omega_;
    ros::Time start_time_;
    Vector offset_;
  };
  
}

#endif // UTA_OPSPACE_DYNAMIC_ACCURACY_TEST_HPP
