/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "casadi_map.hpp"
#include "../function/function_internal.hpp"
#include "../std_vector_tools.hpp"
#include "../mx/mx_tools.hpp"
#include "../matrix/matrix_tools.hpp"
#ifdef WITH_OPENMP
#include <omp.h>
#endif //WITH_OPENMP

using namespace std;

namespace casadi {

  Map::Map(const Function& fcn, const vector<vector<MX> >& arg) : fcn_(fcn) {
    // Number of calls
    n_ = arg.size();

    // Get all inputs
    int f_num_in = fcn_.nIn();
    vector<MX> all_arg;
    all_arg.reserve(n_ * f_num_in);
    for (vector<vector<MX> >::const_iterator j=arg.begin(); j!=arg.end(); ++j) {
      casadi_assert(j->size()==n_);
      for (int i=0; i<n_; ++i) {
        casadi_assert(j->at(i).shape()==fcn_.input(i).shape());
        // Insert sparsity projection nodes if needed
        all_arg.push_back(project(j->at(i), fcn_.input(i).sparsity()));
      }
    }
    casadi_assert(all_arg.size() == n_ * f_num_in);
    setDependencies(all_arg);
    setSparsity(Sparsity::scalar());
  }

  OmpMap::OmpMap(const Function& fcn, const vector<vector<MX> >& arg) : Map(fcn, arg) {
  }

  Map* Map::clone() const {
    return new Map(*this);
  }

  OmpMap* OmpMap::clone() const {
    return new OmpMap(*this);
  }

  std::string Map::print(const std::vector<std::string>& arg) const {
    stringstream ss;
    ss << fcn_.getOption("name") << ".map(";
    for (int i=0; i<ndep(); ++i) {
      if (i!=0) ss << ", ";
      ss << arg.at(i);
    }
    ss << ")";
    return ss.str();
  }

  void Map::evalD(const double** arg, double** res, int* iw, double* w) {
    int n_in = fcn_.nIn(), n_out = fcn_.nOut();
    const double** arg1 = arg+ndep();
    double** res1 = res+nout();
    for (int i=0; i<n_; ++i) {
      for (int j=0; j<n_in; ++j) arg1[j]=*arg++;
      for (int j=0; j<n_out; ++j) res1[j]=*res++;
      fcn_->evalD(arg1, res1, iw, w);
    }
  }

  void OmpMap::evalD(const double** arg, double** res, int* iw, double* w) {
#ifndef WITH_OPENMP
    // Not available, switching to serial mode
    Map::evalD(arg, res, iw, w);
#else // WITH_OPENMP
    size_t sz_arg, sz_res, sz_iw, sz_w;
    fcn_.sz_work(sz_arg, sz_res, sz_iw, sz_w);
#pragma omp parallel for
    for (int i=0; i<n_; ++i) {
      int n_in = fcn_.nIn(), n_out = fcn_.nOut();
      const double** arg_i = arg + n_in*n_ + sz_arg*i;
      copy(arg+i*n_in, arg+(i+1)*n_in, arg_i);
      double** res_i = res + n_out*n_ + sz_res*i;
      copy(res+i*n_out, res+(i+1)*n_out, res_i);
      int* iw_i = iw + i*sz_iw;
      double* w_i = w + i*sz_w;
      fcn_->evalD(arg_i, res_i, iw_i, w_i);
    }
#endif // WITH_OPENMP
  }

  void Map::spFwd(const bvec_t** arg, bvec_t** res, int* iw, bvec_t* w) {
    int n_in = fcn_.nIn(), n_out = fcn_.nOut();
    const bvec_t** arg1 = arg+ndep();
    bvec_t** res1 = res+nout();
    for (int i=0; i<n_; ++i) {
      for (int j=0; j<n_in; ++j) arg1[j]=*arg++;
      for (int j=0; j<n_out; ++j) res1[j]=*res++;
      fcn_->spFwd(arg1, res1, iw, w);
    }
  }

  void Map::spAdj(bvec_t** arg, bvec_t** res, int* iw, bvec_t* w) {
    int n_in = fcn_.nIn(), n_out = fcn_.nOut();
    bvec_t** arg1 = arg+ndep();
    bvec_t** res1 = res+nout();
    for (int i=0; i<n_; ++i) {
      for (int j=0; j<n_in; ++j) arg1[j]=*arg++;
      for (int j=0; j<n_out; ++j) res1[j]=*res++;
      fcn_->spAdj(arg1, res1, iw, w);
    }
  }

  int Map::nout() const {
    return n_ * fcn_.nOut();
  }

  const Sparsity& Map::sparsity(int oind) const {
    return fcn_.output(oind % fcn_.nOut()).sparsity();
  }

  const Function& Map::getFunction() const {
    return fcn_;
  }

  void Map::evalSX(const SXElement** arg, SXElement** res, int* iw, SXElement* w) {
    int n_in = fcn_.nIn(), n_out = fcn_.nOut();
    const SXElement** arg1 = arg+ndep();
    SXElement** res1 = res+nout();
    for (int i=0; i<n_; ++i) {
      for (int j=0; j<n_in; ++j) arg1[j]=*arg++;
      for (int j=0; j<n_out; ++j) res1[j]=*res++;
      fcn_->evalSX(arg1, res1, iw, w);
    }
  }

  void Map::evalMX(const std::vector<MX>& arg, std::vector<MX>& res) {
    // Collect arguments
    int f_num_in = fcn_.nIn();
    vector<vector<MX> > v(n_);
    vector<MX>::const_iterator arg_it = arg.begin();
    for (int i=0; i<n_; ++i) {
      v[i] = vector<MX>(arg_it, arg_it+f_num_in);
      arg_it += f_num_in;
    }

    // Call in parallel
    v = fcn_.map(v, parallelization());

    // Get results
    int f_num_out = fcn_.nOut();
    vector<MX>::iterator res_it = res.begin();
    for (int i=0; i<n_; ++i) {
      copy(v[i].begin(), v[i].end(), res_it);
      res_it += f_num_out;
    }
  }

  void Map::evalFwd(const std::vector<std::vector<MX> >& fseed,
                     std::vector<std::vector<MX> >& fsens) {
    // Derivative function
    int nfwd = fsens.size();
    Function dfcn = fcn_.derForward(nfwd);

    // Nondifferentiated inputs and outputs
    vector<MX> arg(ndep());
    for (int i=0; i<arg.size(); ++i) arg[i] = dep(i);
    vector<MX> res(nout());
    for (int i=0; i<res.size(); ++i) res[i] = getOutput(i);

    // Collect arguments
    vector<vector<MX> > v(n_);
    for (int i=0; i<n_; ++i) {
      v[i].insert(v[i].end(), arg.begin(), arg.end());
      v[i].insert(v[i].end(), res.begin(), res.end());
      v[i].insert(v[i].end(), fseed[i].begin(), fseed[i].end());
    }

    // Call the cached function
    fsens = dfcn.map(v, parallelization());
  }

  void Map::evalAdj(const std::vector<std::vector<MX> >& aseed,
                     std::vector<std::vector<MX> >& asens) {
    // Derivative function
    int nadj = asens.size();
    Function dfcn = fcn_.derReverse(nadj);

    // Nondifferentiated inputs and outputs
    vector<MX> arg(ndep());
    for (int i=0; i<arg.size(); ++i) arg[i] = dep(i);
    vector<MX> res(nout());
    for (int i=0; i<res.size(); ++i) res[i] = getOutput(i);

    // Collect arguments
    vector<vector<MX> > v(n_);
    for (int i=0; i<n_; ++i) {
      v[i].insert(v[i].end(), arg.begin(), arg.end());
      v[i].insert(v[i].end(), res.begin(), res.end());
      v[i].insert(v[i].end(), aseed[i].begin(), aseed[i].end());
    }

    // Call the cached function
    v = dfcn.map(v, parallelization());
    for (int i=0; i<v.size(); ++i) {
      for (int j=0; j<v[i].size(); ++j) {
        asens[i][j] += v[i][j];
      }
    }
  }

  void Map::deepCopyMembers(map<SharedObjectNode*, SharedObject>& already_copied) {
    MXNode::deepCopyMembers(already_copied);
    fcn_ = deepcopy(fcn_, already_copied);
  }

  size_t Map::sz_arg() const {
    return ndep() + fcn_.sz_arg();
  }

  size_t Map::sz_res() const {
    return nout() + fcn_.sz_res();
  }

  size_t Map::sz_iw() const {
    return fcn_.sz_iw();
  }

  size_t Map::sz_w() const {
    return fcn_.sz_w();
  }

  size_t OmpMap::sz_arg() const {
    return ndep() + fcn_.sz_arg()*n_;
  }

  size_t OmpMap::sz_res() const {
    return nout() + fcn_.sz_res()*n_;
  }

  size_t OmpMap::sz_iw() const {
    return fcn_.sz_iw()*n_;
  }

  size_t OmpMap::sz_w() const {
    return fcn_.sz_w()*n_;
  }

  std::vector<std::vector<MX> >
  Map::create(const Function& fcn, const std::vector<std::vector<MX> > &arg,
              const std::string& parallelization) {
    int n = arg.size();
    std::vector<std::vector<MX> > ret(n);
    if (parallelization.compare("expand")==0) {
      // Bypass the Map, call the original function n times
      for (int i=0; i<n; ++i) {
        const_cast<Function&>(fcn)->call(arg[i], ret[i], false, false);
      }
    } else {
      // Get type of parallelization
      bool omp;
      if (parallelization.compare("openmp")==0) {
        omp = true;
      } else if (parallelization.compare("serial")==0) {
        omp = false;
      } else {
        casadi_error("Unsupported parallelization \"" << parallelization
                     << "\": Available options are expand|serial|openmp");
      }

      // Call the map
      std::vector<MX> v;
      if (omp) {
        v = MX::createMultipleOutput(new OmpMap(fcn, arg));
      } else {
        v = MX::createMultipleOutput(new Map(fcn, arg));
      }

      // Collect outputs
      std::vector<MX>::const_iterator v_it = v.begin();
      int n_out = fcn.nOut();
      for (int i=0; i<n; ++i) {
        ret[i] = std::vector<MX>(v_it, v_it+n_out);
        v_it += n_out;
      }
      casadi_assert(v_it==v.end());
    }
    return ret;
  }


} // namespace casadi
