#!/usr/bin/python

import math

filename = "cvrr.cc"

ss = "\
//\n\
// BAGEL - Brilliantly Advanced General Electronic Structure Library\n\
// Filename: " + filename + "\n\
// Copyright (C) 2013 Toru Shiozaki\n\
//\n\
// Author: Ryan D. Reynolds <RyanDReynolds@u.northwestern.edu>\n\
// Maintainer: Shiozaki group\n\
//\n\
// This file is part of the BAGEL package.\n\
//\n\
// This program is free software: you can redistribute it and/or modify\n\
// it under the terms of the GNU General Public License as published by\n\
// the Free Software Foundation, either version 3 of the License, or\n\
// (at your option) any later version.\n\
//\n\
// This program is distributed in the hope that it will be useful,\n\
// but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
// GNU General Public License for more details.\n\
//\n\
// You should have received a copy of the GNU General Public License\n\
// along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\
//\n\
\n\
#include <src/integral/comprys/complexeribatch.h>\n\
#include <src/integral/rys/_vrr_drv.h>\n\
#include <complex>\n\
\n\
using namespace std;\n\
using namespace bagel;\n\
\n\
\n\
void ComplexERIBatch::perform_VRR() {\n\
  const int acsize = asize_ * csize_;\n\
  const int a = basisinfo_[0]->angular_number();\n\
  const int b = basisinfo_[1]->angular_number();\n\
  const int c = basisinfo_[2]->angular_number();\n\
  const int d = basisinfo_[3]->angular_number();\n\
  const int isize = (amax_+1) * (cmax_+1);\n\
  complex<double>* const workx = stack_->template get<complex<double>>(isize*rank_*3);\n\
  complex<double>* const worky = workx + isize*rank_;\n\
  complex<double>* const workz = worky + isize*rank_;\n\
  const int hashkey = (a << 24) + (b << 16) + (c << 8) + d;\n"

for a in range(0,8):
 for b in range(0,8):
  if a < b: continue
  for c in range(0,8):
   for d in range(0,8):
    if c < d: continue
    rank = int(math.ceil((a+b+c+d+1)*0.5-0.001))
    off = 1 << 8
    key = d+off*(c+off*(b+off*a))

    if a == 0 and c == 0:
     ss += "\
  switch (hashkey) {\n"
    if a == 7 or c == 7 or c == 7 or d == 7:
     ss += "\
#ifdef COMPILE_J_ORB\n"
    ss += "\
  case " + str(key) + " :\n\
    for (int j = 0; j != screening_size_; ++j) {\n\
      int ii = screening_[j];\n\
      vrr_driver<" + str(a) + "," + str(b) + "," + str(c) + "," +  str(d) + "," + str(rank) + ",complex<double>>(data_+ii*acsize, roots_+ii*rank_, weights_+ii*rank_, coeff_[ii],\n\
                    basisinfo_[0]->position(), basisinfo_[1]->position(), basisinfo_[2]->position(), basisinfo_[3]->position(),\n\
                    P_+ii*3, Q_+ii*3, xp_[ii], xq_[ii], amapping_, cmapping_, asize_, workx, worky, workz);\n\
    } break;\n"
    if a == 7 or c == 7 or c == 7 or d == 7:
     ss += "\
#endif\n"
ss += "\
  default :\n\
    assert(false);   // hashkey not found\n\
  }\n\
  stack_->release(rank_*isize*3, workx);\n\
\n\
}"

f = open(filename, "w")
f.write(ss)
