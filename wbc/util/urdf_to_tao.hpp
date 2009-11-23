/*
 * Stanford Whole-Body Control Framework http://stanford-wbc.sourceforge.net/
 *
 * Copyright (c) 1997-2009 Stanford University. All rights reserved.
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
   \file urdf_to_tao.hpp Convert a URDF robot model into a TAO tree.
   \author Roland Philippsen
*/

#ifndef WBC_URDF_TO_TAO_HPP
#define WBC_URDF_TO_TAO_HPP

#include <stdexcept>
#include <vector>

class taoNodeRoot;

namespace urdf {
  class Model;
}

namespace wbc {
  
  /**
     Convert the given URDF model to a TAO tree. Use the URDF link
     that has the name \c tao_root_name as the root of the TAO
     tree. That link will be considered as fixed by TAO.
     
     \note The conversion fuses all subtrees that are connected via
     fixed links, adding their inertias and repatriating their
     children to descend from the fused subtree.
     
     The optional \c tao_id_to_link_name_map parameter, if non-NULL,
     is filled in with the names of the URDF links that reside at
     given TAO node IDs, which are numbered from 0 onward. The TAO
     root node has an ID of -1 and is not included in this map
     (because you provided it as the \c tao_root_name anyway).
     
     \return The root of the freshly created TAO tree. An exception is
     thrown in case of errors. Typical errors are about invalid joint
     types. E.g. URDF has a notion of planar joint, which is lacking
     in TAO, and the joint axes in URDF can be arbitrary, whereas TAO
     is limited to the principal coordinate axes. These limitations of
     TAO might very well be removed in the future though.
   */
  taoNodeRoot * urdf_to_tao(urdf::Model const & urdf_model,
			    std::string const & tao_root_name,
			    std::vector<std::string> * tao_id_to_link_name_map) throw(std::runtime_error);
  
}

#endif // WBC_URDF_TO_TAO_HPP
