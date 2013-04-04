//
// BAGEL - Parallel electron correlation program.
// Filename: meh_diagonal.cc
// Copyright (C) 2013 Toru Shiozaki
//
// Author: Shane Parker <shane.parker@u.northwestern.edu>
// Maintainer: Shiozaki Group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and\/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
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

#include <tuple>

#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <vector>

#include <src/util/matrix.h>
#include <src/meh/meh.h>

using namespace std;
using namespace bagel;

shared_ptr<Matrix> MultiExcitonHamiltonian::compute_diagonal_block(DimerSubspace& subspace) {
  const int nstates = subspace.dimerstates();

  shared_ptr<Matrix> out(new Matrix(nstates, nstates));

  const double core = ref_->geom()->nuclear_repulsion() + jop_->core_energy();
  *out += *compute_diagonal_1e(subspace, jop_->mo1e_first(), jop_->mo1e_second(), core);

  *out += *compute_intra_2e(subspace);
  *out += *compute_inter_2e(subspace, subspace);

  return out;
}

shared_ptr<Matrix> MultiExcitonHamiltonian::compute_intra_2e(DimerSubspace& subspace) {

  const int nstatesA = subspace.nstates<0>();
  const int nstatesB = subspace.nstates<1>();

  const int nstates = subspace.dimerstates();

  const int nactA = nact_.first;
  const int nactB = nact_.second;

  shared_ptr<Matrix> out(new Matrix(nstates, nstates));

  // first H^{AA}_{AA}
  {
    shared_ptr<const Dvec> ccvecA = subspace.ci<0>();
    shared_ptr<const Determinants> detA = ccvecA->det();

    const int lenab = detA->lena() * detA->lenb();

    shared_ptr<Dvec> sigmavecAA = form_sigma_2e(ccvecA, jop_->mo2e_first());

    for(int stateA = 0; stateA < nstatesA; ++stateA) {
      for(int stateB = 0; stateB < nstatesB; ++stateB) {
        const int stateAB = subspace.dimerindex(stateA, stateB);
        const double *sdataA = sigmavecAA->data(stateA)->data();
        for(int stateAp = 0; stateAp < nstatesA; ++stateAp) {
          const int stateABp = subspace.dimerindex(stateAp, stateB);
          const double *cdataAp = ccvecA->data(stateAp)->data();
          double value = ddot_(lenab, sdataA, 1, cdataAp, 1);
          
          out->element(stateAB, stateABp) += value;
        }
      }
    }
  }
  
  // now do H^{BB}_{BB} case
  {
    shared_ptr<const Dvec> ccvecB = subspace.ci<1>();
    shared_ptr<const Determinants> detB = ccvecB->det();

    const int lenab = detB->lena() * detB->lenb();
    shared_ptr<Dvec> sigmavecBB = form_sigma_2e(ccvecB, jop_->mo2e_second());

    for(int stateA = 0; stateA < nstatesA; ++stateA) {
      for(int stateB = 0; stateB < nstatesB; ++stateB) {
        const int stateAB = subspace.dimerindex(stateA, stateB);
        const double *sdataB = sigmavecBB->data(stateB)->data();
        for(int stateBp = 0; stateBp < nstatesB; ++stateBp) {
          const int stateABp = subspace.dimerindex(stateA, stateBp);
          const double *cdataBp = ccvecB->data(stateBp)->data();
          double value = ddot_(lenab, sdataB, 1, cdataBp, 1);
          
          out->element(stateAB, stateABp) += value;
        }
      }
    }
  }

  return out;
}

// This term will couple off-diagonal blocks since it has no delta functions involved
shared_ptr<Matrix> MultiExcitonHamiltonian::compute_inter_2e(DimerSubspace& AB, DimerSubspace& ApBp) {
  shared_ptr<const Dvec> ccvecA = AB.ci<0>();
  shared_ptr<const Dvec> ccvecB = AB.ci<1>();
  shared_ptr<const Dvec> ccvecAp = ApBp.ci<0>();
  shared_ptr<const Dvec> ccvecBp = ApBp.ci<1>();

  const int nstatesA = AB.nstates<0>();
  const int nstatesB = AB.nstates<1>();
  const int nstates = nstatesA * nstatesB;

  const int nstatesAp = ApBp.nstates<0>();
  const int nstatesBp = ApBp.nstates<1>();
  const int nstatesp = nstatesAp * nstatesBp;

  // alpha-alpha
  shared_ptr<Quantization> alpha(new TwoBody<SQ::CreateAlpha,SQ::AnnihilateAlpha>());
  Matrix gamma_AA_alpha = *form_gamma(ccvecA, ccvecAp, alpha);
  Matrix gamma_BB_alpha = *form_gamma(ccvecB, ccvecBp, alpha);

  // beta-beta
  shared_ptr<Quantization> beta(new TwoBody<SQ::CreateBeta,SQ::AnnihilateBeta>());
  Matrix gamma_AA_beta = *form_gamma(ccvecA, ccvecAp, beta);
  Matrix gamma_BB_beta = *form_gamma(ccvecB, ccvecBp, beta);

  // build J and K matrices
  shared_ptr<Matrix> Jmatrix, Kmatrix;
  tie(Jmatrix, Kmatrix) = form_JKmatrices<0,1,0,1>();

  Matrix tmp(nstatesA*nstatesAp, nstatesB*nstatesBp);

  tmp += (gamma_AA_alpha + gamma_AA_beta) * (*Jmatrix) ^ (gamma_BB_alpha + gamma_BB_beta);

  tmp -= gamma_AA_alpha * (*Kmatrix) ^ gamma_BB_alpha;
  tmp -= gamma_AA_beta * (*Kmatrix) ^ gamma_BB_beta;

  shared_ptr<Matrix> out(new Matrix(nstates, nstatesp));
  reorder_matrix(tmp.data(), out->data(), nstatesA, nstatesAp, nstatesB, nstatesBp);

  return out;
}
