//
// Newint - Parallel electron correlation program.
// Filename: eribatch.cc
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the Newint package (to be renamed).
//
// The Newint package is free software; you can redistribute it and\/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The Newint package is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the Newint package; see COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//

#define PITWOHALF 17.493418327624862
#define PIMHALF 0.564189583547756
#define SQRTPI2 0.886226925452758013649083741671

#include <cmath>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <src/rysint/eribatch.h>
#include <src/rysint/inline.h>
#include <src/rysint/macros.h>
#include <src/stackmem.h>

using namespace std;

typedef std::shared_ptr<Shell> RefShell;

// This object lives in main.cc
extern StackMem* stack;


ERIBatch::ERIBatch(const vector<RefShell> _info, const double max_density, const double dummy, const bool dum)
:  RysInt(_info) { 
  vrr_ = static_cast<shared_ptr<VRRListBase> >(dynamic_cast<VRRListBase*>(new VRRList()));

  const double integral_thresh = (max_density != 0.0) ? (PRIM_SCREEN_THRESH / max_density) : 0.0;

  // determins if we want to swap shells
  set_swap_info(true);

  // stores AB and CD
  set_ab_cd();

  // set primsize_ and contsize_, as well as relevant members
  set_prim_contsizes();

  // sets angular info
  int asize_final, csize_final, asize_final_sph, csize_final_sph;
  tie(asize_final, csize_final, asize_final_sph, csize_final_sph) = set_angular_info();

  
  const unsigned int size_start = asize_ * csize_ * primsize_; 
  const unsigned int size_intermediate = asize_final * csize_ * contsize_;
  const unsigned int size_intermediate2 = asize_final_sph * csize_final * contsize_;
  size_final_ = asize_final_sph * csize_final_sph * contsize_;
  size_alloc_ = max(size_start, max(size_intermediate, size_intermediate2));
  data_ = stack->get(size_alloc_);
  data2_ = NULL;

  allocate_arrays(primsize_);

  compute_ssss(integral_thresh);

  root_weight(primsize_);

}


ERIBatch::~ERIBatch() {
}


