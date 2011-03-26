/*
 * Whole-Body Control for Human-Centered Robotics http://www.me.utexas.edu/~hcrl/
 *
 * Copyright (c) 2011 University of Texas at Austin. All rights reserved.
 *
 * Author: Roland Philippsen
 *
 * BSD license:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of
 *    contributors to this software may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR THE CONTRIBUTORS TO THIS SOFTWARE BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <opspace/Parameter.hpp>
#include <fstream>

using namespace jspace;

namespace opspace {
  
  
  Parameter::
  Parameter(std::string const & name,
	    parameter_type_t type,
	    parameter_flags_t flags,
	    ParameterReflection const * checker)
    : name_(name),
      type_(type),
      flags_(flags),
      checker_(checker)
  {
    switch (type) {
    case PARAMETER_TYPE_VOID:
    case PARAMETER_TYPE_INTEGER:
    case PARAMETER_TYPE_STRING:
    case PARAMETER_TYPE_REAL:
    case PARAMETER_TYPE_VECTOR:
    case PARAMETER_TYPE_MATRIX:
      break;
    default:
      const_cast<parameter_type_t &>(type_) = PARAMETER_TYPE_VOID;
    }
  }
  
  
  Parameter::
  ~Parameter()
  {
  }
  
  
  int const * Parameter::
  getInteger() const
  {
    return 0;
  }
  
  
  std::string const * Parameter::
  getString() const
  {
    return 0;
  }


  double const * Parameter::
  getReal() const
  {
    return 0;
  }

  
  Vector const * Parameter::
  getVector() const
  {
    return 0;
  }

  
  Matrix const * Parameter::
  getMatrix() const
  {
    return 0;
  }
  
  
  Status Parameter::
  set(int value)
  {
    Status err(false, "type mismatch");
    return err;
  }
  
  
  Status Parameter::
  set(std::string const & value)
  {
    Status err(false, "type mismatch");
    return err;
  }
  
  
  Status Parameter::
  set(double value)
  {
    Status err(false, "type mismatch");
    return err;
  }
  
  
  Status Parameter::
  set(Vector const & value)
  {
    Status err(false, "type mismatch");
    return err;
  }
  
  
  Status Parameter::
  set(Matrix const & value)
  {
    Status err(false, "type mismatch");
    return err;
  }
  
  
  void Parameter::
  dump(std::ostream & os, std::string const & prefix) const
  {
    os << prefix << name_ << " : void\n";
  }
  
  
  IntegerParameter::
  IntegerParameter(std::string const & name,
		   parameter_flags_t flags,
		   ParameterReflection const * checker,
		   int * integer)
    : Parameter(name, PARAMETER_TYPE_INTEGER, flags, checker),
      integer_(integer)
  {
  }
  
  
  Status IntegerParameter::
  set(int integer)
  {
    Status st;
    if (checker_) {
      st = checker_->check(integer_, integer);
      if ( ! st) {
	return st;
      }
    }
    *integer_ = integer;
    return st;
  }
  
  
  void IntegerParameter::
  dump(std::ostream & os, std::string const & prefix) const
  {
    os << prefix << name_ << " : integer = " << *integer_ << "\n";
  }
  
  
  StringParameter::
  StringParameter(std::string const & name,
		  parameter_flags_t flags,
		  ParameterReflection const * checker,
		  std::string * instance)
    : Parameter(name, PARAMETER_TYPE_STRING, flags, checker),
      string_(instance)
  {
  }
  
  
  Status StringParameter::
  set(std::string const & value)
  {
    Status st;
    if (checker_) {
      st = checker_->check(string_, value);
      if ( ! st) {
	return st;
      }
    }
    *string_ = value;
    return st;
  }
  
  
  void StringParameter::
  dump(std::ostream & os, std::string const & prefix) const
  {
    os << prefix << name_ << " : string = " << *string_ << "\n";
  }


  RealParameter::
  RealParameter(std::string const & name,
		parameter_flags_t flags,
		ParameterReflection const * checker,
		double * real)
    : Parameter(name, PARAMETER_TYPE_REAL, flags, checker),
      real_(real)
  {
  }
  
  
  Status RealParameter::
  set(double real)
  {
    Status st;
    if (checker_) {
      st = checker_->check(real_, real);
      if ( ! st) {
	return st;
      }
    }
    *real_ = real;
    return st;
  }
  
  
  void RealParameter::
  dump(std::ostream & os, std::string const & prefix) const
  {
    os << prefix << name_ << " : real = " << *real_ << "\n";
  }


  VectorParameter::
  VectorParameter(std::string const & name,
		  parameter_flags_t flags,
		  ParameterReflection const * checker,
		  Vector * vector)
    : Parameter(name, PARAMETER_TYPE_VECTOR, flags, checker),
      vector_(vector)
  {
  }
  
  
  Status VectorParameter::
  set(Vector const & vector)
  {
    Status st;
    if (checker_) {
      st = checker_->check(vector_, vector);
      if ( ! st) {
	return st;
      }
    }
    *vector_ = vector;
    return st;
  }
  
  
  void VectorParameter::
  dump(std::ostream & os, std::string const & prefix) const
  {
    os << prefix << name_ << " : vector =\n"
       << prefix << "  " << pretty_string(*vector_) << "\n";
  }


  MatrixParameter::
  MatrixParameter(std::string const & name,
		  parameter_flags_t flags,
		  ParameterReflection const * checker,
		  Matrix * matrix)
    : Parameter(name, PARAMETER_TYPE_MATRIX, flags, checker),
      matrix_(matrix)
  {
  }
  
  
  Status MatrixParameter::
  set(Matrix const & matrix)
  {
    Status st;
    if (checker_) {
      st = checker_->check(matrix_, matrix);
      if ( ! st) {
	return st;
      }
    }
    *matrix_ = matrix;
    return st;
  }
  
  
  void MatrixParameter::
  dump(std::ostream & os, std::string const & prefix) const
  {
    os << prefix << name_ << " : matrix =\n"
       << pretty_string(*matrix_, prefix + "  ") << "\n";
  }
  

  ParameterReflection::
  ~ParameterReflection()
  {
    for (parameter_lookup_t::iterator ii(parameter_lookup_.begin());
	 ii != parameter_lookup_.end(); ++ii) {
      delete ii->second;
    }
  }


  Parameter * ParameterReflection::
  lookupParameter(std::string const & name)
  {
    parameter_lookup_t::iterator ii(parameter_lookup_.find(name));
    if (parameter_lookup_.end() == ii) {
      return 0;
    }
    return ii->second;
  }
  
  
  Parameter const * ParameterReflection::
  lookupParameter(std::string const & name) const
  {
    parameter_lookup_t::const_iterator ii(parameter_lookup_.find(name));
    if (parameter_lookup_.end() == ii) {
      return 0;
    }
    return ii->second;
  }
  
  
  Parameter * ParameterReflection::
  lookupParameter(std::string const & name, parameter_type_t type)
  {
    parameter_lookup_t::iterator ii(parameter_lookup_.find(name));
    if (parameter_lookup_.end() == ii) {
      return 0;
    }
    if (type != ii->second->type_) {
      return 0;	      // could maybe implement some sort of casting...
    }
    return ii->second;
  }
  
  
  Parameter const * ParameterReflection::
  lookupParameter(std::string const & name, parameter_type_t type) const
  {
    parameter_lookup_t::const_iterator ii(parameter_lookup_.find(name));
    if (parameter_lookup_.end() == ii) {
      return 0;
    }
    if (type != ii->second->type_) {
      return 0;	      // could maybe implement some sort of casting...
    }
    return ii->second;
  }
  
  
  Status ParameterReflection::
  check(int const * param, int value) const
  {
    Status ok;
    return ok;
  }
  
  
  Status ParameterReflection::
  check(std::string const * param, std::string const & value) const
  {
    Status ok;
    return ok;
  }


  Status ParameterReflection::
  check(double const * param, double value) const
  {
    Status ok;
    return ok;
  }

  
  Status ParameterReflection::
  check(Vector const * param, Vector const & value) const
  {
    Status ok;
    return ok;
  }
  
  
  Status ParameterReflection::
  check(Matrix const * param, Matrix const & value) const
  {
    Status ok; return ok;
  }
  
  
  void ParameterReflection::
  dump(std::ostream & os, std::string const & title, std::string const & prefix) const
  {
    if ( ! title.empty()) {
      os << title << "\n";
    }
    for (parameter_lookup_t::const_iterator ii(parameter_lookup_.begin());
	 ii != parameter_lookup_.end(); ++ii) {
      ii->second->dump(os, prefix + "    ");
    }
  }
  
  
  IntegerParameter * ParameterReflection::
  declareParameter(std::string const & name, int * integer, parameter_flags_t flags)
  {
    IntegerParameter * entry(new IntegerParameter(name, flags, this, integer));
    parameter_lookup_.insert(std::make_pair(name, entry));
    return entry;
  }
    
  
  StringParameter * ParameterReflection::
  declareParameter(std::string const & name, std::string * instance, parameter_flags_t flags)
  {
    StringParameter * entry(new StringParameter(name, flags, this, instance));
    parameter_lookup_.insert(std::make_pair(name, entry));
    return entry;
  }
  
  
  RealParameter * ParameterReflection::
  declareParameter(std::string const & name, double * real, parameter_flags_t flags)
  {
    RealParameter * entry(new RealParameter(name, flags, this, real));
    parameter_lookup_.insert(std::make_pair(name, entry));
    return entry;
  }
  
  
  VectorParameter * ParameterReflection::
  declareParameter(std::string const & name, Vector * vector, parameter_flags_t flags)
  {
    VectorParameter * entry(new VectorParameter(name, flags, this, vector));
    parameter_lookup_.insert(std::make_pair(name, entry));
    return entry;
  }
  
  
  MatrixParameter * ParameterReflection::
  declareParameter(std::string const & name, Matrix * matrix, parameter_flags_t flags)
  {
    MatrixParameter * entry(new MatrixParameter(name, flags, this, matrix));
    parameter_lookup_.insert(std::make_pair(name, entry));
    return entry;
  }
  
  
  template<typename parameter_t, typename storage_t>
  bool maybe_append(std::vector<ParameterLog::log_s<parameter_t, storage_t> > & collection,
		    Parameter const * parameter)
  {
    parameter_t const * pp(dynamic_cast<parameter_t const *>(parameter));
    if (pp) {
      if (pp->flags_ & PARAMETER_FLAG_NOLOG) {
	return true;
      }
      collection.push_back(ParameterLog::log_s<parameter_t, storage_t>(pp));
      return true;
    }
    return false;
  }
  
  
  ParameterLog::
  ParameterLog(std::string const & nn, parameter_lookup_t const & parameter_lookup)
    : name(nn)
  {
    for (parameter_lookup_t::const_iterator ii(parameter_lookup.begin());
	 ii != parameter_lookup.end(); ++ii) {
      if (maybe_append(intlog, ii->second)) {
	continue;
      }
      if (maybe_append(strlog, ii->second)) {
	continue;
      }
      if (maybe_append(reallog, ii->second)) {
	continue;
      }
      if (maybe_append(veclog, ii->second)) {
	continue;
      }
      if (maybe_append(mxlog, ii->second)) {
	continue;
      }
    }
  }
  
  
  void ParameterLog::
  update()
  {
    for (size_t ii(0); ii < intlog.size(); ++ii) {
      intlog[ii].log.push_back(*intlog[ii].parameter->getInteger());
    }
    for (size_t ii(0); ii < strlog.size(); ++ii) {
      strlog[ii].log.push_back(*strlog[ii].parameter->getString());
    }
    for (size_t ii(0); ii < reallog.size(); ++ii) {
      reallog[ii].log.push_back(*reallog[ii].parameter->getReal());
    }
    for (size_t ii(0); ii < veclog.size(); ++ii) {
      veclog[ii].log.push_back(*veclog[ii].parameter->getVector());
    }
    for (size_t ii(0); ii < mxlog.size(); ++ii) {
      mxlog[ii].log.push_back(*mxlog[ii].parameter->getMatrix());
    }
  }
  
  
  void ParameterLog::
  writeFiles(std::string const & prefix, std::ostream * progress) const
  {
    if (progress) {
      *progress << "writing parameter log: " << name << "\n";
    }
    
    if ( ! intlog.empty()) {
      if (progress) {
	*progress << "  integers:";
      }
      for (size_t ii(0); ii < intlog.size(); ++ii) {
	log_s<IntegerParameter, int> const & log(intlog[ii]);
	if ( ! log.log.empty()) {
	  if (progress) {
	    *progress << " " << log.parameter->name_ << "...";
	  }
	  std::string const fn(prefix + "-" + name + "-" + log.parameter->name_ + ".dump");
	  std::ofstream os(fn.c_str());
	  if (os) {
	    size_t const nn(log.log.size());
	    os << "# name: " << name << "\n"
	       << "# parameter: " << log.parameter->name_ << "\n"
	       << "# type: integer\n"
	       << "# size: " << nn << "\n";
	    for (size_t jj(0); jj < nn; ++jj) {
	      os << log.log[jj] << "\n";
	    }
	  }
	}
      }
      if (progress) {
	*progress << " DONE\n";
      }
    }
    
    if ( ! strlog.empty()) {
      if (progress) {
	*progress << "  strings:";
      }
      for (size_t ii(0); ii < strlog.size(); ++ii) {
	log_s<StringParameter, std::string> const & log(strlog[ii]);
	if ( ! log.log.empty()) {
	  if (progress) {
	    *progress << " " << log.parameter->name_ << "...";
	  }
	  std::string const fn(prefix + "-" + name + "-" + log.parameter->name_ + ".dump");
	  std::ofstream os(fn.c_str());
	  if (os) {
	    size_t const nn(log.log.size());
	    os << "# name: " << name << "\n"
	       << "# parameter: " << log.parameter->name_ << "\n"
	       << "# type: string\n"
	       << "# size: " << nn << "\n";
	    for (size_t jj(0); jj < nn; ++jj) {
	      os << log.log[jj] << "\n";
	    }
	  }
	}
      }
      if (progress) {
	*progress << " DONE\n";
      }
    }
    
    if ( ! reallog.empty()) {
      if (progress) {
	*progress << "  reals:";
      }
      for (size_t ii(0); ii < reallog.size(); ++ii) {
	log_s<RealParameter, double> const & log(reallog[ii]);
	if ( ! log.log.empty()) {
	  if (progress) {
	    *progress << " " << log.parameter->name_ << "...";
	  }
	  std::string const fn(prefix + "-" + name + "-" + log.parameter->name_ + ".dump");
	  std::ofstream os(fn.c_str());
	  if (os) {
	    size_t const nn(log.log.size());
	    os << "# name: " << name << "\n"
	       << "# parameter: " << log.parameter->name_ << "\n"
	       << "# type: real\n"
	       << "# size: " << nn << "\n";
	    for (size_t jj(0); jj < nn; ++jj) {
	      os << log.log[jj] << "\n";
	    }
	  }
	}
      }
      if (progress) {
	*progress << " DONE\n";
      }
    }
    
    if ( ! veclog.empty()) {
      if (progress) {
	*progress << "  vectors:";
      }
      for (size_t ii(0); ii < veclog.size(); ++ii) {
	log_s<VectorParameter, Vector> const & log(veclog[ii]);
	if ( ! log.log.empty()) {
	  if (progress) {
	    *progress << " " << log.parameter->name_ << "...";
	  }
	  std::string const fn(prefix + "-" + name + "-" + log.parameter->name_ + ".dump");
	  std::ofstream os(fn.c_str());
	  if (os) {
	    size_t const nn(log.log.size());
	    os << "# name: " << name << "\n"
	       << "# parameter: " << log.parameter->name_ << "\n"
	       << "# type: vector\n"
	       << "# size: " << nn << "\n";
	    for (size_t jj(0); jj < nn; ++jj) {
	      jspace::pretty_print(log.log[jj], os, "", "");
	    }
	  }
	}
      }
      if (progress) {
	*progress << " DONE\n";
      }
    }
    
    if ( ! mxlog.empty()) {
      if (progress) {
	*progress << "  matrices:";
      }
      for (size_t ii(0); ii < mxlog.size(); ++ii) {
	log_s<MatrixParameter, Matrix> const & log(mxlog[ii]);
	if ( ! log.log.empty()) {
	  if (progress) {
	    *progress << " " << log.parameter->name_ << "...";
	  }
	  std::string const fn(prefix + "-" + name + "-" + log.parameter->name_ + ".dump");
	  std::ofstream os(fn.c_str());
	  if (os) {
	    size_t const nn(log.log.size());
	    os << "# name: " << name << "\n"
	       << "# parameter: " << log.parameter->name_ << "\n"
	       << "# type: matrix\n"
	       << "# size: " << nn << "\n"
	       << "# line format: nrows ncols row_0 row_1 ...\n";
	    for (size_t jj(0); jj < nn; ++jj) {
	      Matrix const & mx(log.log[jj]);
	      os << mx.rows() << "  " << mx.cols();
	      for (int kk(0); kk < mx.rows(); ++kk) {
		os << "   ";
		for (int ll(0); ll < mx.cols(); ++ll) {
		  os << jspace::pretty_string(mx.coeff(kk, ll));
		}
	      }
	      os << "\n";
	    }
	  }
	}
      }
      if (progress) {
	*progress << " DONE\n";
      }
    }
    
  }
  
}
