//
// BAGEL - Parallel electron correlation program.
// Filename: simulationcell.cc
// Copyright (C) 2015 Toru Shiozaki
//
// Author: Hai-Anh Le <anh@u.northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// The BAGEL package is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the BAGEL package; see COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//


#include <src/periodic/simulationcell.h>

using namespace std;
using namespace bagel;

SimulationCell::SimulationCell(const shared_ptr<const Geometry> g, const int l) : geom_(g), ndim_(g->primitive_vectors().size()), lmax_(l) {
  primitive_vectors_.resize(ndim_);
  for (int i = 0; i != ndim_; ++i)
    primitive_vectors_[i] = g->primitive_vectors(i);

  init();
}


SimulationCell::SimulationCell(const shared_ptr<const Geometry> g, vector<array<double, 3>> pvec, const int l)
 : geom_(g), ndim_(pvec.size()), primitive_vectors_(pvec), lmax_(l) { init(); }


void SimulationCell::init() {

  vector<shared_ptr<const Atom>> atoms = geom_->atoms();
  radius_ = 0.0;
  for (auto& atom : atoms) {
    array<double, 3> r;
    r[0] = atom->position(0) - centre(0);
    r[1] = atom->position(1) - centre(1);
    r[2] = atom->position(2) - centre(2);
    const double rsq = r[0]*r[0] + r[1]*r[1] + r[2]*r[2];
    if (rsq > radius_)
      radius_ = rsq;
  }
  radius_ = sqrt(radius_);
  compute_multipoles();
}


void SimulationCell::compute_extent(const double thresh) { // extent at charge centre

  vector<shared_ptr<const Shell>> shells;
  for (auto& atom : geom_->atoms())
    shells.insert(shells.end(), atom->shells().begin(), atom->shells().end());

  extent_ = 0.0;
  for (auto& ish : shells)
    for (auto& jsh : shells) {
      const vector<double> exp0 = ish->exponents();
      const vector<double> exp1 = jsh->exponents();
      array<double, 3> AB;
      AB[0] = ish->position(0) - jsh->position(0);
      AB[1] = ish->position(1) - jsh->position(1);
      AB[2] = ish->position(2) - jsh->position(2);
      const double rsq = AB[0] * AB[0] + AB[1] * AB[1] + AB[2] * AB[2];
      const double lnthresh = log(thresh);

      for (auto& expi0 : exp0) {
        for (auto& expi1 : exp1) {
          const double cxp_inv = 1.0 / (expi0 + expi1);
          const double expi01 = expi0 * expi1;
          const double lda_kl = sqrt((- lnthresh - expi01 * rsq * cxp_inv + 0.75 * log(4.0 * expi01 / (pi__ * pi__))) * cxp_inv);

          array<double, 3> tmp;
          tmp[0] = (ish->position(0) * expi0 + jsh->position(0) * expi1) * cxp_inv - centre(0);
          tmp[1] = (ish->position(1) * expi0 + jsh->position(1) * expi1) * cxp_inv - centre(1);
          tmp[2] = (ish->position(2) * expi0 + jsh->position(2) * expi1) * cxp_inv - centre(2);

          const double extent0 = sqrt(tmp[0] * tmp[0] + tmp[1] * tmp[1] + tmp[2] * tmp[2]) + lda_kl;
          if (extent0 > extent_) extent_ = extent0;
        }
      }
    }
}


void SimulationCell::compute_multipoles() {

  const int nmultipole = (lmax_ + 1) * (lmax_ + 1);
  multipoles_.resize(nmultipole);
  vector<shared_ptr<ZMatrix>> multipoles(nmultipole);
  for (int i = 0; i != nmultipole; ++i)
    multipoles[i] = make_shared<ZMatrix>(nbasis(), nbasis());

  vector<shared_ptr<const Atom>> atoms = geom_->atoms();
  size_t ob0 = 0;
  for (auto& atom0 : atoms) {
    for (auto& b0 : atom0->shells()) {
      size_t ob1 = 0;
      for (auto& atom1 : atoms) {
        for (auto& b1 : atom1->shells()) {
          MultipoleBatch mpole(array<shared_ptr<const Shell>, 2>{{b1, b0}}, centre(), lmax_);
          mpole.compute();
          for (int i = 0; i != nmultipole; ++i)
            multipoles[i]->copy_block(ob1, ob0, b1->nbasis(), b0->nbasis(), mpole.data(i));

          ob1 += b1->nbasis();
        }
      }
      ob0 += b0->nbasis();
    }
  }

  for (int i = 0; i != nmultipole; ++i)
    multipoles_[i] = multipoles[i];
}


vector<shared_ptr<const ZMatrix>> SimulationCell::shift_multipoles(array<double, 3> r) const {

  LocalExpansion shift(r, multipoles_, lmax_);
  vector<shared_ptr<const ZMatrix>> out = shift.compute_shifted_multipoles();

  return out;
}
