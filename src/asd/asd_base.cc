//
// BAGEL - Parallel electron correlation program.
// Filename: asd_base.cc
// Copyright (C) 2014 Toru Shiozaki
//
// Author: Shane Parker <shane.parker@u.northwestern.edu>
// Maintainer: Shiozaki Group
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

#include <src/asd/asd_base.h>
#include <src/smith/prim_op.h>

using namespace std;
using namespace bagel;

ASD_base::ASD_base(const shared_ptr<const PTree> input, shared_ptr<const Dimer> dimer) : dimer_(dimer)
{
  nstates_ = input->get<int>("nstates", 10);
  max_iter_ = input->get<int>("max_iter", 50);
  davidson_subspace_ = input->get<int>("davidson_subspace", 10);
  nguess_ = input->get<int>("nguess", 10*nstates_);
  dipoles_ = input->get<bool>("dipoles", false);
  thresh_ = input->get<double>("thresh", 1.0e-7);
  print_thresh_ = input->get<double>("print_thresh", 0.01);
  store_matrix_ = input->get<bool>("store_matrix", false);
  charge_ = input->get<int>("charge", 0);
  nspin_ = input->get<int>("spin", 0);

  shared_ptr<const PTree> model_input = input->get_child_optional("models");
  if (model_input) {
    shared_ptr<const PTree> pruning_input = model_input->get_child_optional("pruned");
    vector<int> pruned = (pruning_input ? model_input->get_vector<int>("pruned")
                                             : vector<int>());
    pruned.push_back(-1);

    for (auto& p : pruned) {
      vector<ModelBlock> this_model;
      shared_ptr<const PTree> sblock_input = model_input->get_child("subblocks");
      for (auto& b : *sblock_input) {
        array<int,2> charges = b->get_array<int, 2>("charges");
        array<int,2> spins = b->get_array<int, 2>("spins");
        const int nstates = b->get<int>("nstates");
        this_model.emplace_back( make_pair(spins[0],spins[1]), make_pair(charges[0],charges[1]), make_pair(p,p), nstates );
      }
      models_to_form_.emplace_back(move(this_model));
    }
  }

  Timer timer;

  shared_ptr<const Reference> dimerref = dimer_->sref();

  jop_ = make_shared<DimerJop>(dimerref, dimerref->nclosed(), dimerref->nclosed() + dimer_->active_refs().first->nact(), dimerref->nclosed() + dimerref->nact(), dimerref->coeff());
  cout << "  o computing integrals: " << timer.tick() << endl;

  energies_ = vector<double>(nstates_, 0.0);
}


shared_ptr<Matrix> ASD_base::compute_intra(const DimerSubspace_base& AB, shared_ptr<const DimerJop> jop, const double diag) const {
  auto out = make_shared<Matrix>(AB.dimerstates(), AB.dimerstates());

  const int nstatesA = AB.nstates<0>();
  const int nstatesB = AB.nstates<1>();

  // first H^{AA}_{AA}
  for(int stateA = 0; stateA < nstatesA; ++stateA) {
    for(int stateAp = 0; stateAp < stateA; ++stateAp) {
      const double value = AB.sigma<0>()->element(stateAp, stateA);
      for(int stateB = 0; stateB < nstatesB; ++stateB) {
        const int stateApB = AB.dimerindex(stateAp, stateB);
        const int stateAB = AB.dimerindex(stateA, stateB);
        (*out)(stateAB, stateApB) += value;
        (*out)(stateApB, stateAB) += value;
      }
    }
    const double value = AB.sigma<0>()->element(stateA, stateA);
    for(int stateB = 0; stateB < nstatesB; ++stateB) {
      const int stateAB = AB.dimerindex(stateA, stateB);
      (*out)(stateAB,stateAB) += value;
    }
  }

  // H^{BB}_{BB}
  for(int stateB = 0; stateB < nstatesB; ++stateB) {
    for(int stateBp = 0; stateBp < stateB; ++stateBp) {
      const double value = AB.sigma<1>()->element(stateBp, stateB);
      for(int stateA = 0; stateA < nstatesA; ++stateA) {
        const int stateAB = AB.dimerindex(stateA, stateB);
        const int stateABp = AB.dimerindex(stateA, stateBp);
        (*out)(stateAB, stateABp) += value;
        (*out)(stateABp, stateAB) += value;
      }
    }
    const double value = AB.sigma<1>()->element(stateB, stateB);
    for(int stateA = 0; stateA < nstatesA; ++stateA) {
      const int stateAB = AB.dimerindex(stateA, stateB);
      (*out)(stateAB,stateAB) += value;
    }
  }

  out->add_diag(diag);
  return out;
}



shared_ptr<Matrix> ASD_base::compute_diagonal_block_H(const DimerSubspace_base& subspace) const {
  const double core = dimer_->sref()->geom()->nuclear_repulsion() + jop_->core_energy();

  auto out = compute_intra(subspace, jop_, core);
  array<MonomerKey,4> keys {{ subspace.monomerkey<0>(), subspace.monomerkey<1>(), subspace.monomerkey<0>(), subspace.monomerkey<1>() }};
  *out += *compute_inter_2e_H(keys);

  return out;
}


//***************************************************************************************************************

tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_diagonal_block_RDM(const DimerSubspace_base& subspace) const {
// 1e is not considered (TODO maybe there is contribution?)
//***************************************************************************************************************

  array<MonomerKey,4> keys {{ subspace.monomerkey<0>(), subspace.monomerkey<1>(), subspace.monomerkey<0>(), subspace.monomerkey<1>() }};
  auto out = compute_inter_2e_RDM(keys, /*subspace diagonal*/true);

  return out;
}



shared_ptr<Matrix> ASD_base::compute_offdiagonal_1e_H(const array<MonomerKey,4>& keys, shared_ptr<const Matrix> hAB) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];

  Coupling term_type = coupling_type(keys);

  GammaSQ operatorA;
  GammaSQ operatorB;
  int neleA = Ap.nelea() + Ap.neleb();

  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());

  switch(term_type) {
    case Coupling::aET :
      operatorA = GammaSQ::CreateAlpha;
      operatorB = GammaSQ::AnnihilateAlpha;
      break;
    case Coupling::inv_aET :
      operatorA = GammaSQ::AnnihilateAlpha;
      operatorB = GammaSQ::CreateAlpha;
      --neleA;
      break;
    case Coupling::bET :
      operatorA = GammaSQ::CreateBeta;
      operatorB = GammaSQ::AnnihilateBeta;
      break;
    case Coupling::inv_bET :
      operatorA = GammaSQ::AnnihilateBeta;
      operatorB = GammaSQ::CreateBeta;
      --neleA;
      break;
    default :
      return out;
  }

  auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {operatorA});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {operatorB});
  Matrix tmp = gamma_A * (*hAB) ^ gamma_B;

  if ((neleA % 2) == 1) {
    // sort: (A,A',B,B') --> -1.0 * (A,B,A',B')
    SMITH::sort_indices<0,2,1,3,0,1,-1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  }
  else {
    // sort: (A,A',B,B') --> (A,B,A',B')
    SMITH::sort_indices<0,2,1,3,0,1,1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  }

  return out;
}


//***************************************************************************************************************

//tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
//ASD_base::compute_offdiagonal_1e_RDM(const array<MonomerKey,4>& keys, shared_ptr<const Matrix> hAB) const {
// Unsed function!
//***************************************************************************************************************
//assert(false);
//return make_tuple(nullptr,nullptr);
//}


// This term will couple off-diagonal blocks since it has no delta functions involved

shared_ptr<Matrix> ASD_base::compute_inter_2e_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];

  // alpha-alpha
  auto gamma_AA_alpha = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});
  auto gamma_BB_alpha = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});

  // beta-beta
  auto gamma_AA_beta = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});
  auto gamma_BB_beta = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});

  // build J and K matrices
  shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,1,0,1>();
  shared_ptr<const Matrix> Kmatrix = jop_->coulomb_matrix<0,1,1,0>();

  Matrix tmp((gamma_AA_alpha + gamma_AA_beta) * (*Jmatrix) ^ (gamma_BB_alpha + gamma_BB_beta));

  tmp -= gamma_AA_alpha * (*Kmatrix) ^ gamma_BB_alpha;
  tmp -= gamma_AA_beta * (*Kmatrix) ^ gamma_BB_beta;

  // sort: (A,A',B,B') --> (A,B,A',B') + block(A,B,A',B')
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  SMITH::sort_indices<0,2,1,3,0,1,1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  return out;
}


//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_inter_2e_RDM(const array<MonomerKey,4>& keys, const bool subdia) const {
//***************************************************************************************************************
  auto& B  = keys[1]; 
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out = make_shared<RDM<2>>(nactA+nactB);

  // alpha-alpha
  auto gamma_AA_alpha = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});
  auto gamma_BB_alpha = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});

  // beta-beta
  auto gamma_AA_beta = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});
  auto gamma_BB_beta = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});

  auto rdmAA = make_shared<Matrix>(gamma_AA_alpha % gamma_BB_alpha);
  auto rdmBB = make_shared<Matrix>(gamma_AA_beta  % gamma_BB_beta);

  auto rdmAB = make_shared<Matrix>(gamma_AA_alpha % gamma_BB_beta);
  auto rdmBA = make_shared<Matrix>(gamma_AA_beta  % gamma_BB_alpha);

  {// P(p,q',r',s) : p15
    auto rdmt = rdmAA->clone();
    SMITH::sort_indices<0,3,2,1, 0,1, -1,1>(rdmAA->data(), rdmt->data(), nactA, nactA, nactB, nactB); //aa
    SMITH::sort_indices<0,3,2,1, 1,1, -1,1>(rdmBB->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb
    if (!subdia) {
      SMITH::sort_indices<1,2,3,0, 1,1, -1,1>(rdmAA->data(), rdmt->data(), nactA, nactA, nactB, nactB); //aa of (N,M)
      SMITH::sort_indices<1,2,3,0, 1,1, -1,1>(rdmBB->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb of (N,M)
    }
    auto low = {    0, nactA, nactA,     0};
    auto up  = {nactA, nactT, nactT, nactA};
    auto outv = make_rwview(out->range().slice(low, up), out->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }

  {// d_pqr's' : p19
    auto rdmt = rdmAA->clone();
    SMITH::sort_indices<0,1,2,3, 0,1, 1,1>(rdmAA->data(), rdmt->data(), nactA, nactA, nactB, nactB); //aa
    SMITH::sort_indices<0,1,2,3, 1,1, 1,1>(rdmBB->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb
    SMITH::sort_indices<0,1,2,3, 1,1, 1,1>(rdmAB->data(), rdmt->data(), nactA, nactA, nactB, nactB); //aa bb
    SMITH::sort_indices<0,1,2,3, 1,1, 1,1>(rdmBA->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb aa
    if (!subdia) {
      SMITH::sort_indices<1,0,3,2, 1,1, 1,1>(rdmAA->data(), rdmt->data(), nactA, nactA, nactB, nactB); //aa of (N,M)
      SMITH::sort_indices<1,0,3,2, 1,1, 1,1>(rdmBB->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb of (N,M)
      SMITH::sort_indices<1,0,3,2, 1,1, 1,1>(rdmAB->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb of (N,M)
      SMITH::sort_indices<1,0,3,2, 1,1, 1,1>(rdmBA->data(), rdmt->data(), nactA, nactA, nactB, nactB); //bb of (N,M)
    }
    auto low = {    0,     0, nactA, nactA};
    auto up  = {nactA, nactA, nactT, nactT};
    auto outv = make_rwview(out->range().slice(low, up), out->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }

  return make_tuple(nullptr,out);
}



shared_ptr<Matrix> ASD_base::compute_aET_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];
  Matrix tmp(A.nstates()*Ap.nstates(), B.nstates()*Bp.nstates());

  // One-body aET
  {
    auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha});
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha});

    shared_ptr<const Matrix> Fmatrix = jop_->cross_mo1e();

    tmp += gamma_A * (*Fmatrix) ^ gamma_B;
  }

  //Two-body aET, type 1
  {
    auto gamma_A  = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha});
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});

    shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,1,1,1>();

    tmp -= gamma_A * (*Jmatrix) ^ (gamma_B1 + gamma_B2);
  }

  //Two-body aET, type 2
  {
    auto gamma_A1 = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});
    auto gamma_A2 = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha});

    shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,0,1,0>();

    tmp += (gamma_A1 + gamma_A2) * (*Jmatrix) ^ gamma_B;
  }

  const int neleA = Ap.nelea() + Ap.neleb();
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  if ((neleA % 2) == 1) {
    // sort: (A,A',B,B') --> -1.0 * (A,B,A',B')
    SMITH::sort_indices<0,2,1,3,0,1,-1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  } else {
    // sort: (A,A',B,B') --> (A,B,A',B')
    SMITH::sort_indices<0,2,1,3,0,1,1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  }
  return out;
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_aET_RDM(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************

  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out1 = make_shared<RDM<1>>(nactA+nactB);
  auto out2 = make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();


  //1RDM
  {
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha});
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha});
    
    auto rdm = make_shared<Matrix>(gamma_A % gamma_B);
    auto rdmt = rdm->clone();

    // P(p,q') : p10
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<0,1, 0,1, 1,1>(rdm->data(), rdmt->data(), nactA, nactB);
    rdmt->scale(fac);
 
    auto low = {    0, nactA};
    auto up  = {nactA, nactT};
    auto outv = make_rwview(out1->range().slice(low, up), out1->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }

  //2RDM
  {
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha});
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1);
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2);
    auto rdmt = rdm1->clone();

    // P(p,q',r',s') : p15
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<0,3,1,2, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactB, nactB, nactB);
    SMITH::sort_indices<0,2,1,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB);
    rdmt->scale(fac);

    auto low = {    0, nactA, nactA, nactA};
    auto up  = {nactA, nactT, nactT, nactT};
    auto outv = make_rwview(out2->range().slice(low, up), out2->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }
  {
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha});

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B);
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B);
    auto rdmt = rdm1->clone();

    //P(p,q',r,s) : p15
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<0,3,1,2, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB);
    SMITH::sort_indices<0,3,1,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB);
    rdmt->scale(fac);

    auto low = {    0, nactA,     0,     0};
    auto up  = {nactA, nactT, nactA, nactA};
    auto outv = make_rwview(out2->range().slice(low, up), out2->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }
  
  return make_tuple(out1,out2);
}



shared_ptr<Matrix> ASD_base::compute_bET_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];
  Matrix tmp(A.nstates()*Ap.nstates(), B.nstates()*Bp.nstates());

  // One-body bET
  {
    auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta});
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta});

    shared_ptr<const Matrix> Fmatrix = jop_->cross_mo1e();

    tmp += gamma_A * (*Fmatrix) ^ gamma_B;
  }


  //Two-body bET, type 1
  {
    auto gamma_A  = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta});
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha});
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});

    shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,1,1,1>();

    tmp -= gamma_A * (*Jmatrix) ^ (gamma_B1 + gamma_B2);
  }

  //Two-body aET, type 2
  {
    auto gamma_A1 = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});
    auto gamma_A2 = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta});

    shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,0,1,0>();

    tmp += (gamma_A1 + gamma_A2) * (*Jmatrix) ^ gamma_B;
  }

  const int neleA = Ap.nelea() + Ap.neleb();
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  if ((neleA % 2) == 1) {
    // sort: (A,A',B,B') --> -1.0 * (A,B,A',B')
    SMITH::sort_indices<0,2,1,3,0,1,-1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  }
  else {
    // sort: (A,A',B,B') --> (A,B,A',B')
    SMITH::sort_indices<0,2,1,3,0,1,1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());
  }

  return out;
}


//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_bET_RDM(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out1 = make_shared<RDM<1>>(nactA+nactB);
  auto out2 = make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();
  //RDM1
  {
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta});
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta});
    
    auto rdm = make_shared<Matrix>(gamma_A % gamma_B);
    auto rdmt = rdm->clone();
    
    // P(p,q') : p10
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<0,1, 0,1, 1,1>(rdm->data(), rdmt->data(), nactA, nactB);
    rdmt->scale(fac);
    
    auto low = {    0, nactA};
    auto up  = {nactA, nactT};
    auto outv = make_rwview(out1->range().slice(low, up), out1->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }

  //RDM2
  {
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta});
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha});
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1);
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2);
    auto rdmt = rdm1->clone();

    // P(p,q',r',s') : p15
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<0,2,1,3, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactB, nactB, nactB);
    SMITH::sort_indices<0,3,1,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB);
    rdmt->scale(fac);

    auto low = {    0, nactA, nactA, nactA};
    auto up  = {nactA, nactT, nactT, nactT};
    auto outv = make_rwview(out2->range().slice(low, up), out2->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }
  {
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta});

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B);
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B);
    auto rdmt = rdm1->clone();

    // P(p,q',r,s) : p15
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<0,3,1,2, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB);
    SMITH::sort_indices<0,3,1,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB);
    rdmt->scale(fac);

    auto low = {    0, nactA,     0,     0};
    auto up  = {nactA, nactT, nactA, nactA};
    auto outv = make_rwview(out2->range().slice(low, up), out2->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
  }

  return make_tuple(out1,out2);
}



shared_ptr<Matrix> ASD_base::compute_abFlip_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];

  auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta});

  shared_ptr<const Matrix> Kmatrix = jop_->coulomb_matrix<0,1,1,0>();

  Matrix tmp = gamma_A * (*Kmatrix) ^ gamma_B;

  // sort: (A,A',B,B') --> -1.0 * (A,B,A',B')
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  SMITH::sort_indices<0,2,1,3,0,1,-1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());

  return out;
}


//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_abFlip_RDM(const array<MonomerKey,4>& keys) const {
// if ab-flip, account ba-flip arising from (N,M)
// if(M,N) is ba-flip then (N,M) is ab-flip and this will include ba-flip of (M,N) too.
//***************************************************************************************************************
  auto& B = keys[1];
  auto& Bp = keys[3];

  assert(gammatensor_[0]->exist(keys[0], keys[2], {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}));

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;

  auto out = make_shared<RDM<2>>(nactA+nactB);

  auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta});

  auto rdm = make_shared<Matrix>(gamma_A % gamma_B);
  auto rdmt = rdm->clone();

  // P(p,q',r',s) : p4  ab-flip of (M,N)
  SMITH::sort_indices<0,3,2,1, 0,1, -1,1>(rdm->data(), rdmt->data(), nactA, nactA, nactB, nactB); //ab-flip
  SMITH::sort_indices<1,2,3,0, 1,1, -1,1>(rdm->data(), rdmt->data(), nactA, nactA, nactB, nactB); //ba-flip of (N,M) p15B

  auto low = {    0, nactA, nactA,     0};
  auto up  = {nactA, nactT, nactT, nactA};
  auto outv = make_rwview(out->range().slice(low, up), out->storage());
  assert(rdmt->size() == outv.size());
  copy(rdmt->begin(), rdmt->end(), outv.begin());

  return make_tuple(nullptr,out);
}



shared_ptr<Matrix> ASD_base::compute_abET_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];

  auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});

  shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,0,1,1>();

  Matrix tmp = gamma_A * (*Jmatrix) ^ gamma_B;

  // sort: (A,A',B,B') --> -1.0 * (A,B,A',B')
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  SMITH::sort_indices<0,2,1,3,0,1,-1,1>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());

  return out;
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_abET_RDM(const array<MonomerKey,4>& keys) const {
// for (M,N)
// if inverse ab-ET / compute (N,M)
//***************************************************************************************************************
  auto& B = keys[1]; auto& Bp = keys[3];

  assert(gammatensor_[0]->exist(keys[0], keys[2], {GammaSQ::CreateAlpha, GammaSQ::CreateBeta}));

  auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});

  auto rdm  = make_shared<Matrix>(gamma_A % gamma_B);
  auto rdmt = rdm->clone();

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;

  // P(p,q',r,s') : p14B
  SMITH::sort_indices<0,2,1,3, 0,1, -1,1>(rdm->data(), rdmt->data(), nactA, nactA, nactB, nactB);
  SMITH::sort_indices<1,3,0,2, 1,1, -1,1>(rdm->data(), rdmt->data(), nactA, nactA, nactB, nactB);

  auto out = make_shared<RDM<2>>(nactA+nactB);
  auto low = {    0, nactA,     0, nactA};
  auto up  = {nactA, nactT, nactA, nactT};
  auto outv = make_rwview(out->range().slice(low, up), out->storage());
  assert(rdmt->size() == outv.size());
  copy(rdmt->begin(), rdmt->end(), outv.begin());

  return make_tuple(nullptr,out);
}



shared_ptr<Matrix> ASD_base::compute_aaET_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];
  auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});

  shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,0,1,1>();

  Matrix tmp = gamma_A * (*Jmatrix) ^ gamma_B;

  // sort: (A,A',B,B') --> -0.5 * (A,B,A',B')
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  SMITH::sort_indices<0,2,1,3,0,1,-1,2>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());

  return out;
}


//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_aaET_RDM(const array<MonomerKey,4>& keys) const {
//off-diagonal subspaces only!
// if(M,N) is inverse-aa-ET, swap M,N as (N,M) will be aa-ET and contribute to 2RDM
//***************************************************************************************************************
  auto& B = keys[1]; auto& Bp = keys[3];

  assert(gammatensor_[0]->exist(keys[0], keys[2], {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha}));

  auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});

  auto rdm  = make_shared<Matrix>(gamma_A % gamma_B);
  auto rdmt = rdm->clone();

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;

  // P(p,q',r,s') : p1B
  SMITH::sort_indices<0,3,1,2, 0,1, 1,1>(rdm->data(), rdmt->data(), nactA, nactA, nactB, nactB);

  auto out = make_shared<RDM<2>>(nactA+nactB);
  auto low = {    0, nactA,     0, nactA};
  auto up  = {nactA, nactT, nactA, nactT};
  auto outv = make_rwview(out->range().slice(low, up), out->storage());
  assert(rdmt->size() == outv.size());
  copy(rdmt->begin(), rdmt->end(), outv.begin());

  return make_tuple(nullptr,out);
}



shared_ptr<Matrix> ASD_base::compute_bbET_H(const array<MonomerKey,4>& keys) const {
  auto& A = keys[0]; auto& B = keys[1]; auto& Ap = keys[2]; auto& Bp = keys[3];
  auto gamma_A = gammatensor_[0]->get_block_as_matview(A, Ap, {GammaSQ::CreateBeta, GammaSQ::CreateBeta});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});

  shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,0,1,1>();

  Matrix tmp = gamma_A * (*Jmatrix) ^ gamma_B;

  // sort: (A,A',B,B') --> -0.5 * (A,B,A',B')
  auto out = make_shared<Matrix>(A.nstates()*B.nstates(), Ap.nstates()*Bp.nstates());
  SMITH::sort_indices<0,2,1,3,0,1,-1,2>(tmp.data(), out->data(), A.nstates(), Ap.nstates(), B.nstates(), Bp.nstates());

  return out;
}


//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> 
ASD_base::compute_bbET_RDM(const array<MonomerKey,4>& keys) const {
// cf. aaET
//***************************************************************************************************************
  auto& B = keys[1]; auto& Bp = keys[3];

  assert(gammatensor_[0]->exist(keys[0], keys[2], {GammaSQ::CreateBeta, GammaSQ::CreateBeta}));

  auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta});
  auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});

  auto rdm  = make_shared<Matrix>(gamma_A % gamma_B);
  auto rdmt = rdm->clone();

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;

  // P(p,q',r,s') : p14B
  SMITH::sort_indices<0,3,1,2, 0,1, 1,1>(rdm->data(), rdmt->data(), nactA, nactA, nactB, nactB);

  auto out = make_shared<RDM<2>>(nactA+nactB);
  auto low = {    0, nactA,     0, nactA};
  auto up  = {nactA, nactT, nactA, nactT};
  auto outv = make_rwview(out->range().slice(low, up), out->storage());
  assert(rdmt->size() == outv.size());
  copy(rdmt->begin(), rdmt->end(), outv.begin());

  return make_tuple(nullptr,out);
}


void ASD_base::print_hamiltonian(const string title, const int nstates) const {
  hamiltonian_->print(title, nstates);
}


void ASD_base::print_states(const Matrix& cc, const vector<double>& energies, const double thresh, const string title) const {
  const int nstates = cc.mdim();
  shared_ptr<Matrix> spn = spin_->apply(cc);
  cout << endl << " ===== " << title << " =====" << endl;
  for (int istate = 0; istate < nstates; ++istate) {
    cout << "   state  " << setw(3) << istate << ": "
         << setprecision(8) << setw(17) << fixed << energies.at(istate)
         << "   <S^2> = " << setw(4) << setprecision(4) << fixed << ddot_(dimerstates_, spn->element_ptr(0,istate), 1, cc.element_ptr(0,istate), 1) << endl;
    const double *eigendata = cc.element_ptr(0,istate);
    double printed = 0.0;
    for (auto& subspace : subspaces_base()) {
      const int nA = subspace.nstates<0>();
      const int nB = subspace.nstates<1>();
      for (int i = 0; i < nA; ++i) {
        for (int j = 0; j < nB; ++j, ++eigendata) {
          if ( (*eigendata)*(*eigendata) > thresh ) {
            cout << "      " << subspace.string(i,j) << setprecision(12) << setw(20) << *eigendata << endl;
            printed += (*eigendata)*(*eigendata);
          }
        }
      }
    }
    cout << "    total weight of printed elements: " << setprecision(12) << setw(20) << printed << endl << endl;
  }
}


void ASD_base::print_property(const string label, shared_ptr<const Matrix> property , const int nstates) const {
  const string indent("   ");
  const int nprint = min(nstates, property->ndim());

  cout << indent << " " << label << "    |0>";
  for (int istate = 1; istate < nprint; ++istate) cout << "         |" << istate << ">";
  cout << endl;
  for (int istate = 0; istate < nprint; ++istate) {
    cout << indent << "<" << istate << "|";
    for (int jstate = 0; jstate < nprint; ++jstate) {
      cout << setw(12) << setprecision(6) << property->element(jstate, istate);
    }
    cout << endl;
  }
  cout << endl;
}


void ASD_base::print(const double thresh) const {
  print_states(*adiabats_, energies_, thresh, "Adiabatic States");
  if (dipoles_) {for (auto& prop : properties_) print_property(prop.first, prop.second, nstates_); }
}

//***************************************************************************************************************
shared_ptr<Matrix>
ASD_base::couple_blocks_H(const DimerSubspace_base& AB, const DimerSubspace_base& ApBp) const {
//***************************************************************************************************************

  Coupling term_type = coupling_type(AB, ApBp);

  const DimerSubspace_base* space1 = &AB;
  const DimerSubspace_base* space2 = &ApBp;

  bool flip = (static_cast<int>(term_type) < 0);
  if (flip) {
    term_type = Coupling(-1*static_cast<int>(term_type));
    std::swap(space1,space2);
  }

  shared_ptr<Matrix> out;
  std::array<MonomerKey,4> keys {{space1->template monomerkey<0>(), space1->template monomerkey<1>(), space2->template monomerkey<0>(), space2->template monomerkey<1>()}};

  switch(term_type) {
    case Coupling::none :
      out = nullptr; break;
    case Coupling::diagonal :
      out = compute_inter_2e_H(keys); break;
    case Coupling::aET :
      out = compute_aET_H(keys); break;
    case Coupling::bET :
      out = compute_bET_H(keys); break;
    case Coupling::abFlip :
      out = compute_abFlip_H(keys); break;
    case Coupling::abET :
      out = compute_abET_H(keys); break;
    case Coupling::aaET :
      out = compute_aaET_H(keys); break;
    case Coupling::bbET :
      out = compute_bbET_H(keys); break;
    default :
      throw std::logic_error("Asking for a coupling type that has not been written.");
  }

  /* if we are computing the Hamiltonian and flip = true, then we tranpose the output (see above) */
//if (flip) out = out->transpose(); // same as below??
  if (flip) transpose_call(out);

  return out;
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>>
ASD_base::couple_blocks_RDM(const DimerSubspace_base& AB, const DimerSubspace_base& ApBp) const {
//***************************************************************************************************************

  Coupling term_type = coupling_type(AB, ApBp);

  const DimerSubspace_base* space1 = &AB;
  const DimerSubspace_base* space2 = &ApBp;

  bool flip = (static_cast<int>(term_type) < 0);
  if (flip) {
    term_type = Coupling(-1*static_cast<int>(term_type));
    std::swap(space1,space2);
  }
  
  tuple<shared_ptr<RDM<1>>,shared_ptr<RDM<2>>> out;
  std::array<MonomerKey,4> keys {{space1->template monomerkey<0>(), space1->template monomerkey<1>(), space2->template monomerkey<0>(), space2->template monomerkey<1>()}};

  switch(term_type) {
    case Coupling::none :
      out = make_tuple(nullptr,nullptr); break;
    case Coupling::diagonal :
      out = compute_inter_2e_RDM(keys, /*subspace diagonal*/false); break;
    case Coupling::aET :
      out = compute_aET_RDM(keys); break;
    case Coupling::bET :
      out = compute_bET_RDM(keys); break;
    case Coupling::abFlip :
      out = compute_abFlip_RDM(keys); break;
    case Coupling::abET :
      out = compute_abET_RDM(keys); break;
    case Coupling::aaET :
      out = compute_aaET_RDM(keys); break;
    case Coupling::bbET :
      out = compute_bbET_RDM(keys); break;
    default :
      throw std::logic_error("Asking for a coupling type that has not been written.");
  }
  
  return out;
}

//***************************************************************************************************************
void
ASD_base::debug_RDM() const {
//***************************************************************************************************************
  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;

  const int neleA = 2*(dimer_->isolated_refs().first->nclosed() - dimer_->active_refs().first->nclosed());
  const int neleB = 2*(dimer_->isolated_refs().first->nclosed() - dimer_->active_refs().first->nclosed());
  const int nelec = neleA+neleB;

  cout << "#of active electrons is A : " << neleA << endl;
  cout << "#of active electrons is B : " << neleB << endl;
  cout << "#of total active electrons: " << nelec << endl;

  auto rdm1A = std::make_shared<RDM<1>>(nactA);
  {
    auto low = {0,0};
    auto up  = {nactA,nactA};
    auto view = btas::make_view(onerdm_->range().slice(low,up), onerdm_->storage());
    copy(view.begin(), view.end(), rdm1A->begin());
  }
  auto rdm1B = std::make_shared<RDM<1>>(nactB);
  {
    auto low = {nactA,nactA};
    auto up  = {nactT,nactT};
    auto view = btas::make_view(onerdm_->range().slice(low,up), onerdm_->storage());
    copy(view.begin(), view.end(), rdm1B->begin());
  }

  auto rdm2A = std::make_shared<RDM<2>>(nactA);
  {
    auto low = {0,0,0,0};
    auto up  = {nactA,nactA,nactA,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage());
    copy(view.begin(), view.end(), rdm2A->begin());
  }
  auto rdm2B = std::make_shared<RDM<2>>(nactB);
  {
    auto low = {nactA,nactA,nactA,nactA};
    auto up  = {nactT,nactT,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage());
    copy(view.begin(), view.end(), rdm2B->begin());
  }

//auto rdm3A = std::make_shared<RDM<3>>(nactA);
//{
//  auto low = {0,0,0,0,0,0};
//  auto up  = {nactA,nactA,nactA,nactA,nactA,nactA};
//  auto view = btas::make_view(threerdm_->range().slice(low,up), threerdm_->storage());
//  copy(view.begin(), view.end(), rdm3A->begin());
//}
//auto rdm3B = std::make_shared<RDM<3>>(nactB);
//{
//  auto low = {nactA,nactA,nactA,nactA,nactA,nactA};
//  auto up  = {nactT,nactT,nactT,nactT,nactT,nactT};
//  auto view = btas::make_view(threerdm_->range().slice(low,up), threerdm_->storage());
//  copy(view.begin(), view.end(), rdm3B->begin());
//}

  //1RDM
  { //Monomer A
    double sum = 0.0;
    for (int i = 0; i != nactA; ++i) {
      sum += onerdm_->element(i,i);
    }
    std::cout << "1RDM(A)  Trace = " << sum << std::endl;
  }
  { //Monomer B
    double sum = 0.0;
    for (int i = nactA; i != nactT; ++i) {
      sum += onerdm_->element(i,i);
    }
    std::cout << "1RDM(B)  Trace = " << sum << std::endl;
  }
  { //Dimer AB
    double sum = 0.0;
    for (int i = 0; i != nactT; ++i) {
      sum += onerdm_->element(i,i);
    }
    std::cout << "1RDM(AB) Trace = " << sum << std::endl;
  }

  //2RDM check (A) Gamma_ij,kl = <0|E_ij,kl|0> = <0|(k1)'(i2)'(j2)(l1)|0>  1,2 = spin 
  //diagonal: i=j, k=l
  { //Monomer A
    double sum = 0.0;
    for (int i = 0; i != nactA; ++i)
    for (int j = 0; j != nactA; ++j) {
      sum += twordm_->element(i,i,j,j);
    }
    std::cout << "2RDM(A)  Trace = " << sum << std::endl;
  }
  { //Monomer B
    double sum = 0.0;
    for (int i = nactA; i != nactT; ++i)
    for (int j = nactA; j != nactT; ++j) {
      sum += twordm_->element(i,i,j,j);
    }
    std::cout << "2RDM(B)  Trace = " << sum << std::endl;
  }
  { //Dimer AB
    double sum = 0.0;
    for (int i = 0; i != nactT; ++i)
    for (int j = 0; j != nactT; ++j) {
      sum += twordm_->element(i,i,j,j);
    }
    std::cout << "2RDM(AB) Trace = " << sum << std::endl;
  }

  { //Gamma_ij,kk
    std::cout << "2RDM(A) Partial Trace Sum_k (i,j,k,k)" << std::endl;
    auto debug = std::make_shared<RDM<1>>(*rdm1A);
    for (int i = 0; i != nactA; ++i)
    for (int j = 0; j != nactA; ++j)
    for (int k = 0; k != nactA; ++k) {
      debug->element(i,j) -= 1.0/(neleA-1) * twordm_->element(i,j,k,k);
    }
    debug->print(1.0e-3);
  }
  { //Gamma_ij,kk
    std::cout << "2RDM(B) Partial Trace Sum_k (i,j,k,k)" << std::endl;
    auto debug = std::make_shared<RDM<1>>(*rdm1B);
    for (int i = nactA; i != nactT; ++i)
    for (int j = nactA; j != nactT; ++j)
    for (int k = nactA; k != nactT; ++k) {
      debug->element(i-nactA,j-nactA) -= 1.0/(neleB-1) * twordm_->element(i,j,k,k);
    }
    debug->print(1.0e-3);
  }
  { //Gamma_ij,kk
    std::cout << "2RDM(AB) Partial Trace Sum_k (i,j,k,k)" << std::endl;
    auto debug = std::make_shared<RDM<1>>(*onerdm_);
    for (int i = 0; i != nactT; ++i)
    for (int j = 0; j != nactT; ++j)
    for (int k = 0; k != nactT; ++k) {
      debug->element(i,j) -= 1.0/(nelec-1) * twordm_->element(i,j,k,k);
    }
    debug->print(1.0e-3);
  }


  //construct approx twordm
  cout << "Build approx 2RDM:" << endl;
  for (int i = 0; i != nactA; ++i)
  for (int j = nactA; j != nactT; ++j) {
    approx2rdm_->element(i,i,j,j) = onerdm_->element(i,i) * onerdm_->element(j,j);
    approx2rdm_->element(j,j,i,i) = approx2rdm_->element(i,i,j,j);
    approx2rdm_->element(i,j,j,i) = -0.5*(onerdm_->element(i,i) * onerdm_->element(j,j));// - onerdm_->element(i,i);
    approx2rdm_->element(j,i,i,j) = approx2rdm_->element(i,j,j,i); //-onerdm_->element(i,i) * onerdm_->element(j,j);// - onerdm_->element(j,j);
  }
  { //Dimer AB
    double sum = 0.0;
    for (int i = 0; i != nactT; ++i)
    for (int j = 0; j != nactT; ++j) {
      sum += approx2rdm_->element(i,i,j,j);
    }
    std::cout << "APPROX2RDM(AB) Trace = " << sum << std::endl;
  }
  { //difference
    auto debug = make_shared<RDM<2>>(*twordm_);
    for (int i = 0; i != nactT; ++i)
    for (int j = 0; j != nactT; ++j)
    for (int k = 0; k != nactT; ++k)
    for (int l = 0; l != nactT; ++l) {
      debug->element(i,j,k,l) -= approx2rdm_->element(i,j,k,l);
    }
    debug->print(1.0e-8);
    auto low = {0,0,0,0};
    auto up  = {nactT,nactT,nactT,nactT};
    auto view = btas::make_view(debug->range().slice(low,up), debug->storage());
    auto rdm2 = make_shared<Matrix>(nactT*nactT*nactT,nactT,1); 
    copy(view.begin(), view.end(), rdm2->begin());
    cout << "Norm of approx. 2RDM =" << rdm2->norm() << endl;
  }

  assert(false);

  //3RDM: Gamma_ij,kl,mn
  { //Trace A
    double sum = 0.0;
    for (int i = 0; i != nactA; ++i)
    for (int j = 0; j != nactA; ++j)
    for (int k = 0; k != nactA; ++k) {
      sum += threerdm_->element(i,i,j,j,k,k);
    }
    std::cout << "3RDM Trace (A)  = " << sum << std::endl;
  }
  { //Trace B
    double sum = 0.0;
    for (int i = nactA; i != nactT; ++i)
    for (int j = nactA; j != nactT; ++j)
    for (int k = nactA; k != nactT; ++k) {
      sum += threerdm_->element(i,i,j,j,k,k);
    }
    std::cout << "3RDM Trace (B)  = " << sum << std::endl;
  }
  { //Trace AB
    double sum = 0.0;
    for (int i = 0; i != nactT; ++i)
    for (int j = 0; j != nactT; ++j)
    for (int k = 0; k != nactT; ++k) {
      sum += threerdm_->element(i,i,j,j,k,k);
    }
    std::cout << "3RDM Trace (AB) = " << sum << std::endl;
  }

  { //Gamma_ij,kl,mm : p21
    std::cout << "3RDM(A) Partial Trace Sum_m (i,j,k,l,m,m)" << std::endl;
    auto debug = std::make_shared<RDM<2>>(*rdm2A);
    for (int i = 0; i != nactA; ++i)
    for (int j = 0; j != nactA; ++j)
    for (int k = 0; k != nactA; ++k) 
    for (int l = 0; l != nactA; ++l) 
    for (int m = 0; m != nactA; ++m) {
      debug->element(i,j,k,l) -= 1.0/(neleA-2) * threerdm_->element(i,j,k,l,m,m);
    }
    debug->print(1.0e-8);
  }
  { //Gamma_ij,kk,mm : p21
    std::cout << "3RDM(A) Partial Trace Sum_m (i,j,k,k,m,m)" << std::endl;
    auto debug = std::make_shared<RDM<1>>(*rdm1A);
    for (int i = 0; i != nactA; ++i)
    for (int j = 0; j != nactA; ++j)
    for (int k = 0; k != nactA; ++k) 
    for (int m = 0; m != nactA; ++m) {
      debug->element(i,j) -= 1.0/((neleA-2)*(neleA-1)) * threerdm_->element(i,j,k,k,m,m);
    }
    debug->print(1.0e-8);
  }

  { //Gamma_ij,kl,mm : p21
    std::cout << "3RDM(B) Partial Trace Sum_m (i,j,k,l,m,m)" << std::endl;
    auto debug = std::make_shared<RDM<2>>(*rdm2B);
    for (int i = nactA; i != nactT; ++i)
    for (int j = nactA; j != nactT; ++j)
    for (int k = nactA; k != nactT; ++k) 
    for (int l = nactA; l != nactT; ++l) 
    for (int m = nactA; m != nactT; ++m) {
      debug->element(i-nactA,j-nactA,k-nactA,l-nactA) -= 1.0/(neleA-2) * threerdm_->element(i,j,k,l,m,m);
    }
    debug->print(1.0e-8);
  }
  { //Gamma_ij,kk,mm : p21
    std::cout << "3RDM(B) Partial Trace Sum_m (i,j,k,k,m,m)" << std::endl;
    auto debug = std::make_shared<RDM<1>>(*rdm1B);
    for (int i = nactA; i != nactT; ++i)
    for (int j = nactA; j != nactT; ++j)
    for (int k = nactA; k != nactT; ++k) 
    for (int m = nactA; m != nactT; ++m) {
      debug->element(i-nactA,j-nactA) -= 1.0/((neleA-2)*(neleA-1)) * threerdm_->element(i,j,k,k,m,m);
    }
    debug->print(1.0e-8);
  }



  assert(false);
}

#if 0

  //4RDM check (A) Gamma_ij,kl,mn,op
  {
    double sum = 0.0;
    for (int i = 0; i != nactA; ++i)
    for (int j = 0; j != nactA; ++j)
    for (int k = 0; k != nactA; ++k)
    for (int l = 0; l != nactA; ++l) {
      sum += fourrdm_->element(i,i,j,j,k,k,l,l);
    }
    std::cout << "4RDM Trace = " << sum << std::endl;
  }



  // Checking 4RDM by comparing with 3RDM
  { 
    auto debug = std::make_shared<RDM<3>>(*threerdm_);
    std::cout << "4RDM debug test 1" << std::endl;
    for (int l = 0; l != nactA; ++l)
      for (int d = 0; d != nactA; ++d)
        for (int k = 0; k != nactA; ++k)
          for (int c = 0; c != nactA; ++c)
            for (int j = 0; j != nactA; ++j)
              for (int b = 0; b != nactA; ++b)
      for (int i = 0; i != nactA; ++i) {
        debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(i,i,b,j,c,k,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,i,i,c,k,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,i,i,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,d,l,i,i);
      }
    debug->print(1.0e-8);
  }
  { 
    auto debug = std::make_shared<RDM<3>>(*threerdm_);
    std::cout << "4RDM debug test 2" << std::endl;
    for (int l = 0; l != nactA; ++l)
      for (int d = 0; d != nactA; ++d)
        for (int k = 0; k != nactA; ++k)
          for (int c = 0; c != nactA; ++c)
            for (int j = 0; j != nactA; ++j)
              for (int b = 0; b != nactA; ++b)
      for (int i = 0; i != nactA; ++i) {
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(i,i,b,j,c,k,d,l);
        debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,i,i,c,k,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,i,i,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,d,l,i,i);
      }
    debug->print(1.0e-8);
  }
  { 
    auto debug = std::make_shared<RDM<3>>(*threerdm_);
    std::cout << "4RDM debug test 3" << std::endl;
    for (int l = 0; l != nactA; ++l)
      for (int d = 0; d != nactA; ++d)
        for (int k = 0; k != nactA; ++k)
          for (int c = 0; c != nactA; ++c)
            for (int j = 0; j != nactA; ++j)
              for (int b = 0; b != nactA; ++b)
      for (int i = 0; i != nactA; ++i) {
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(i,i,b,j,c,k,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,i,i,c,k,d,l);
        debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,i,i,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,d,l,i,i);
      }
    debug->print(1.0e-8);
  }
  { 
    auto debug = std::make_shared<RDM<3>>(*threerdm_);
    std::cout << "4RDM debug test 4" << std::endl;
    for (int l = 0; l != nactA; ++l)
      for (int d = 0; d != nactA; ++d)
        for (int k = 0; k != nactA; ++k)
          for (int c = 0; c != nactA; ++c)
            for (int j = 0; j != nactA; ++j)
              for (int b = 0; b != nactA; ++b)
      for (int i = 0; i != nactA; ++i) {
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(i,i,b,j,c,k,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,i,i,c,k,d,l);
  //    debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,i,i,d,l);
        debug->element(b,j,c,k,d,l) -= 1.0/(nelea+neleb-3) * fourrdm_->element(b,j,c,k,d,l,i,i);
      }
    debug->print(1.0e-8);
  }



  assert(false);

  std::cout << "Monomer A 4RDM print" << std::endl;
  for (int i = 0; i != nactA; ++ i)
  for (int j = 0; j != nactA; ++ j)
  for (int k = 0; k != nactA; ++ k)
  for (int l = 0; l != nactA; ++ l)
  for (int m = 0; m != nactA; ++ m)
  for (int n = 0; n != nactA; ++ n)
  for (int o = 0; o != nactA; ++ o)
  for (int p = 0; p != nactA; ++ p) {
    double elem = fourrdm_->element(i,j,k,l,m,n,o,p);
    elem = std::abs(elem);
    if(elem > 1.0e-8) std::cout << "RDM4(" << i << j << k << l << m << n << o << p << ") " << fourrdm_->element(i,j,k,l,m,n,o,p) << std::endl ;
  }
  assert(false);
#endif

//***************************************************************************************************************
void
ASD_base::debug_energy() const {
//***************************************************************************************************************

  //Energy calculation
  cout << "!@# Energy calculated from RDM:" << endl;
  const int nclosedA = dimer_->active_refs().first->nclosed();
  const int nclosedB = dimer_->active_refs().second->nclosed();

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;

  cout << "Number of closed orbitals: A(" << nclosedA << "), B(" << nclosedB << ")" << endl;
  cout << "Number of active orbitals: A(" << nactA << "), B(" << nactB << ")" << endl;

  shared_ptr<const Matrix> ha = jop_->monomer_jop<0>()->mo1e()->matrix(); //h_AA
  shared_ptr<const Matrix> hb = jop_->monomer_jop<1>()->mo1e()->matrix(); //h_BB
  //                                                    CSymMatrix -> Matrix conversion
  shared_ptr<const Matrix> hc = jop_->cross_mo1e(); //h_AB

  auto int1 = make_shared<Matrix>(nactT,nactT);
  int1->zero();
  int1->copy_block(0,0,ha->ndim(),ha->mdim(),ha);
  int1->copy_block(nactA,nactA,hb->ndim(),hb->mdim(),hb);
  int1->copy_block(0,nactA,hc->ndim(),hc->mdim(),hc);
  int1->copy_block(nactA,0,hc->mdim(),hc->ndim(),hc->transpose());
  int1->print("1e integral",nactT);

  auto rdm1 = onerdm_->rdm1_mat(0);
  rdm1->print("1RDM",nactT);

  double  e1 = ddot_(nactT*nactT, int1->element_ptr(0,0), 1, rdm1->element_ptr(0,0), 1);
  cout << "1E energy = " << e1 << endl;

  double e2 = 0.0;
  //AAAA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,0,0,0>();
    auto int2 = make_shared<Matrix>(nactA*nactA*nactA*nactA,1);
    SMITH::sort_indices<0,2,1,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactA, nactA); //conver to chemist not.

    auto low = {0,0,0,0};
    auto up  = {nactA,nactA,nactA,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_AAAA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactA,nactA,1); //empty d_AAAA (note: the dimension specification actually do not matter)
    copy(view.begin(), view.end(), rdm2->begin()); //d_AAAA filled
    cout << "2E energy (AAAA) = " << 0.5 * ddot_(nactA*nactA*nactA*nactA, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactA*nactA, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  //BBBB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,1,1,1>();
    auto int2 = make_shared<Matrix>(1,nactB*nactB*nactB*nactB);
    SMITH::sort_indices<0,2,1,3, 0,1, 1,1>(pint2->data(), int2->data(), nactB, nactB, nactB, nactB); //conver to chemist not.

    auto low = {nactA,nactA,nactA,nactA};
    auto up  = {nactT,nactT,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BBBB sector of d
    auto rdm2 = make_shared<Matrix>(1,nactB*nactB*nactB*nactB); //empty d_BBBB
    copy(view.begin(), view.end(), rdm2->begin()); //d_BBBB filled
    cout << "2E energy (BBBB) = " << 0.5 * ddot_(nactB*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactB*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  cout << "(3A,1B) part:" << endl;
  //AAAB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,0,0,1>(); // <pq|rs'> in (pqr,s') format
    auto int2 = make_shared<Matrix>(nactA*nactA*nactA*nactB,1);
    SMITH::sort_indices<0,2,1,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactA, nactB); //conver to chemist not.
    auto low = {    0,    0,    0,nactA};
    auto up  = {nactA,nactA,nactA,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_AAAB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactA*nactB,1); //empty d_AAAB
    copy(view.begin(), view.end(), rdm2->begin()); //d_AAAB filled
    cout << "2E energy (AAAB) = " << 0.5 * ddot_(nactA*nactA*nactA*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactA*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  //AABA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,1,0,0>(); // <pq'|rs> in (prs,q') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactA,1);
    SMITH::sort_indices<0,1,3,2, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactA, nactB); //conver to chemist not. [ij|k'l]

    auto low = {    0,    0,nactA,    0};
    auto up  = {nactA,nactA,nactT,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_AABA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactA,1); //empty d_AABA
    copy(view.begin(), view.end(), rdm2->begin()); //d_AABA filled
    cout << "2E energy (AABA) = " << 0.5 * ddot_(nactA*nactA*nactB*nactA, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactA*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  //BAAA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,0,0,0>(); // <p'q|rs> in (qrs,p') format 
    auto int2 = make_shared<Matrix>(nactB,nactA*nactA*nactA);
    SMITH::sort_indices<3,1,0,2, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactA, nactB); //conver to chemist not.

    auto low = {nactA,    0,    0,    0};
    auto up  = {nactT,nactA,nactA,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BAAA sector of d
    auto rdm2 = make_shared<Matrix>(nactB,nactA*nactA*nactA); //empty d_BAAA
    copy(view.begin(), view.end(), rdm2->begin()); //d_BAAA filled
    cout << "2E energy (BAAA) = " << 0.5 * ddot_(nactB*nactA*nactA*nactA, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactA*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  //ABAA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,0,1,0>(); // <pq|r's> in (pqs,r') format 
    auto int2 = make_shared<Matrix>(nactA*nactB*nactA*nactA,1);
    SMITH::sort_indices<0,3,1,2, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactA, nactB); //conver to chemist not. [pr'|qs]=[ij'|kl]

    auto low = {    0,nactA,    0,    0};
    auto up  = {nactA,nactT,nactA,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABAA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactB*nactA*nactA,1); //empty d_ABAA
    copy(view.begin(), view.end(), rdm2->begin()); //d_ABAA filled
    cout << "2E energy (ABAA) = " << 0.5 * ddot_(nactA*nactB*nactA*nactA, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactA*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }



  cout << "(1A,3B) part:" << endl;
  //ABBB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,1,1,1>(); // <pq'|r's'> in (p,q'r's') format 
    auto int2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1);
    SMITH::sort_indices<0,2,1,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactB, nactB, nactB); //conver to chemist not. [pr'|q's']=[ij'|k'l']

    auto low = {    0,nactA,nactA,nactA};
    auto up  = {nactA,nactT,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABBB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1); //empty d_ABBB
    copy(view.begin(), view.end(), rdm2->begin()); //d_ABBB filled
    cout << "2E energy (ABBB) = " << 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  //BABB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,1,0,1>(); // <p'q'|rs'> in (r,p'q's') format 
    auto int2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1);
    SMITH::sort_indices<1,0,2,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactB, nactB, nactB); //conver to chemist not. [p'r|q's']=[i'j|k'l']

    auto low = {nactA,    0,nactA,nactA};
    auto up  = {nactT,nactA,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BABB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1); //empty d_BABB
    copy(view.begin(), view.end(), rdm2->begin()); //d_ABBB filled
    cout << "2E energy (BABB) = " << 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  //BBAB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,0,1,1>(); // <p'q|r's'> in (q,p'r's') format 
    auto int2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1);
    SMITH::sort_indices<1,2,0,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactB, nactB, nactB); //conver to chemist not. [p'r'|qs']=[i'j'|kl']

    auto low = {nactA,nactA,    0,nactA};
    auto up  = {nactT,nactT,nactA,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BBAB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1); //empty d_BBAB
    copy(view.begin(), view.end(), rdm2->begin()); //d_ABBB filled
    cout << "2E energy (BBAB) = " << 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  //BBBA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,1,1,0>(); // <p'q'|r's> in (s,p'q'r') format 
    auto int2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1);
    SMITH::sort_indices<1,3,2,0, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactB, nactB, nactB); //conver to chemist not. [p'r'|q's]=[i'j'|k'l]

    auto low = {nactA,nactA,nactA,    0};
    auto up  = {nactT,nactT,nactT,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BBBA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactB*nactB*nactB,1); //empty d_BBBA
    copy(view.begin(), view.end(), rdm2->begin()); //d_BBBA filled
    cout << "2E energy (BBBA) = " << 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactB*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  cout << "(2A,2B) part:" << endl;
  //AABB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,1,0,1>(); // <pq'|rs'> in (pr,q's') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1);
    SMITH::sort_indices<0,1,2,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactB, nactB); //conver to chemist not. [pr|q's']=[ij|k'l']

    auto low = {    0,    0,nactA,nactA};
    auto up  = {nactA,nactA,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_AABB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_AABB
    copy(view.begin(), view.end(), rdm2->begin()); //d_AABB filled
    cout << "2E energy (AABB) = " << 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  //BBAA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,0,1,0>(); // <p'q|r's> in (qs,p'r') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1);
    SMITH::sort_indices<2,3,0,1, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactB, nactB); //conver to chemist not. [p'r'|qs]=[i'j'|kl]

    auto low = {nactA,nactA,    0,    0};
    auto up  = {nactT,nactT,nactA,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BBAA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_BBAA
    copy(view.begin(), view.end(), rdm2->begin()); //d_BBAA filled
    cout << "2E energy (BBAA) = " << 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  //ABAB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,0,1,1>(); // <pq|r's'> in (pq,r's') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1);
    SMITH::sort_indices<0,2,1,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactB, nactB); //conver to chemist not. [pr'|qs']=[ij'|kl']

    auto low = {    0,nactA,    0,nactA};
    auto up  = {nactA,nactT,nactA,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABAB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_ABAB
    copy(view.begin(), view.end(), rdm2->begin()); //d_ABAB filled
    cout << "2E energy (ABAB) = " << 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  //BABA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,1,0,0>(); // <p'q'|rs> in (rs,p'q') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1);
    SMITH::sort_indices<2,0,3,1, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactB, nactB); //conver to chemist not. [p'r|q's]=[i'j|k'l]

    auto low = {nactA,    0,nactA,    0};
    auto up  = {nactT,nactA,nactT,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BABA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_BABA
    copy(view.begin(), view.end(), rdm2->begin()); //d_BABA filled
    cout << "2E energy (BABA) = " << 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }

  //ABBA
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<0,1,1,0>(); // <pq'|r's> in (ps,q'r') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1);
    SMITH::sort_indices<0,3,2,1, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactB, nactB); //conver to chemist not. [pr'|q's]=[ij'|k'l]

    auto low = {    0,nactA,nactA,    0};
    auto up  = {nactA,nactT,nactT,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABBA sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_ABBA
    copy(view.begin(), view.end(), rdm2->begin()); //d_ABBA filled
    cout << "2E energy (ABBA) = " << 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  //BAAB
  {
    shared_ptr<const Matrix> pint2 = jop_->coulomb_matrix<1,0,0,1>(); // <p'q|rs'> in (qr,p's') format 
    auto int2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1);
    SMITH::sort_indices<2,1,0,3, 0,1, 1,1>(pint2->data(), int2->data(), nactA, nactA, nactB, nactB); //conver to chemist not. [p'r|qs']=[i'j|kl']

    auto low = {nactA,    0,    0,nactA};
    auto up  = {nactT,nactA,nactA,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_BAAB sector of d
    auto rdm2 = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_BAAB
    copy(view.begin(), view.end(), rdm2->begin()); //d_BAAB filled
    cout << "2E energy (BAAB) = " << 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1) << endl;
    e2 += 0.5 * ddot_(nactA*nactA*nactB*nactB, int2->element_ptr(0,0), 1, rdm2->element_ptr(0,0), 1);
  }
  
  //Energy print
  cout << "nuclear repulsion= " << dimer_->sref()->geom()->nuclear_repulsion() << endl;
  cout << "core energy      = " << jop_->core_energy() << endl;
  cout << "nuc + core energy= " << dimer_->sref()->geom()->nuclear_repulsion() + jop_->core_energy() << endl;
  cout << "1E energy = " <<  e1 << endl;
  cout << "2E energy = " <<  e2 << endl;
  cout << "Total energy = " << dimer_->sref()->geom()->nuclear_repulsion() + jop_->core_energy() + e1 + e2 << endl;
}

//***************************************************************************************************************
void
ASD_base::symmetrize_RDM() const {
//***************************************************************************************************************

  cout << "!@# Unsymmetrized 1RDM" << endl;
  onerdm_->print(1.0e-6);

  const int nactA = dimer_->active_refs().first->nact();
  const int nactB = dimer_->active_refs().second->nact();
  const int nactT = nactA + nactB;  

  //Symmetrize: D_AB (calculated) D_BA (uncalc.& symmetrized here)
  auto matBA = std::make_shared<Matrix>(nactB,nactA); //D_BA empty
  {
    auto low = {0, nactA};
    auto up  = {nactA, nactT};
    auto view = btas::make_view(onerdm_->range().slice(low,up), onerdm_->storage()); //D_AB sector of D (read ptr)
    auto matAB = std::make_shared<Matrix>(nactA,nactB); //D_AB empty
    std::copy(view.begin(), view.end(), matAB->begin()); //D_AB filled
    SMITH::sort_indices<1,0, 0,1, 1,1>(matAB->data(), matBA->data(), nactA, nactB); // transpose and fill D_BA
  }
  {
    auto low = {nactA, 0};
    auto up  = {nactT, nactA};
    auto outv = btas::make_rwview(onerdm_->range().slice(low,up), onerdm_->storage()); //D_BA sector of D (read & write ptr)
    std::copy(matBA->begin(), matBA->end(), outv.begin()); //copy D_BA -> D_BA sector of D
  }

  cout << "!@# Symmetrized 1RDM" << endl;
  onerdm_->print(1.0e-6);

  //Symmetrize: d(ABAA) note p18B, 19B
  {
    auto low = {0,nactA,0,0};
    auto up  = {nactA,nactT,nactA,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABAA sector of d
    auto inmat = make_shared<Matrix>(nactA*nactB,nactA*nactA); //empty d_ABAA
    copy(view.begin(), view.end(), inmat->begin()); //d_ABAA filled
    { //d(AAAB)
      auto outmat = make_shared<Matrix>(nactA*nactA,nactA*nactB); //empty d_AAAB
      SMITH::sort_indices<2,3,0,1, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactA, nactA); //reorder and fill d_AAAB
      auto low = {0,0,0,nactA};
      auto up  = {nactA,nactA,nactA,nactT};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_AAAB sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_AAAB into d_AAAB sector of d
    } 
    { //d(BAAA)
      auto outmat = make_shared<Matrix>(nactB*nactA,nactA*nactA); //empty d_BAAA
      SMITH::sort_indices<1,0,3,2, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactA, nactA); //reorder and fill d_BAAA
      auto low = {nactA,0,0,0};
      auto up  = {nactT,nactA,nactA,nactA};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BAAA sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BAAA into d_BAAA sector of d
    } 
    { //d(AABA)
      auto outmat = make_shared<Matrix>(nactA*nactA,nactB*nactA); //empty d_AABA
      SMITH::sort_indices<3,2,1,0, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactA, nactA); //reorder and fill d_AABA
      auto low = {0,0,nactA,0};
      auto up  = {nactA,nactA,nactT,nactA};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_AABA sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_AABA into d_AABA sector of d
    } 
  }
 
  //Symmetrize: d(ABBB) note p18B, 19B
  {
    auto low = {0,nactA,nactA,nactA};
    auto up  = {nactA,nactT,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABBB sector of d
    auto inmat = make_shared<Matrix>(nactA*nactB,nactB*nactB); //empty d_ABBB
    copy(view.begin(), view.end(), inmat->begin()); //d_ABBB filled
    { //d(BBAB)
      auto outmat = make_shared<Matrix>(nactB*nactB,nactA*nactB); //empty d_BBAB
      SMITH::sort_indices<2,3,0,1, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactB, nactB); //reorder and fill d_BBAB
      auto low = {nactA,nactA,0,nactA};
      auto up  = {nactT,nactT,nactA,nactT};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BBAB sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BBAB into d_BBAB sector of d
    } 
    { //d(BABB)
      auto outmat = make_shared<Matrix>(nactB*nactA,nactB*nactB); //empty d_BABB
      SMITH::sort_indices<1,0,3,2, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactB, nactB); //reorder and fill d_BABB
      auto low = {nactA,0,nactA,nactA};
      auto up  = {nactT,nactA,nactT,nactT};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BABB sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BABB into d_BABB sector of d
    } 
    { //d(BBBA)
      auto outmat = make_shared<Matrix>(nactB*nactB,nactB*nactA); //empty d_BBBA
      SMITH::sort_indices<3,2,1,0, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactB, nactB); //reorder and fill d_BBBA
      auto low = {nactA,nactA,nactA,0};
      auto up  = {nactT,nactT,nactT,nactA};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BBBA sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BBBA into d_BBBA sector of d
    } 
  }


  //Symmetrize: d(ABBA) note p19
  {
    auto low = {0,nactA,nactA,0};
    auto up  = {nactA,nactT,nactT,nactA};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABBA sector of d
    auto inmat = make_shared<Matrix>(nactA*nactB,nactB*nactA); //empty d_ABBA
    copy(view.begin(), view.end(), inmat->begin()); //d_ABBA filled
    { //d(BAAB)
      auto outmat = make_shared<Matrix>(nactB*nactA,nactA*nactB); //empty d_BAAB
      SMITH::sort_indices<2,3,0,1, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactB, nactA); //reorder and fill d_BAAB
      auto low = {nactA,0,0,nactA};
      auto up  = {nactT,nactA,nactA,nactT};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BAAB sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BAAB into d_BAAB sector of d
    } 
  }

  //Symmetrize: d(AABB) note 19
  { //d(AABB)
    auto low = {0,0,nactA,nactA};
    auto up  = {nactA,nactA,nactT,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_AABB sector of d
    auto inmat = make_shared<Matrix>(nactA*nactA*nactB*nactB,1); //empty d_AABB
    copy(view.begin(), view.end(), inmat->begin()); //d_AABB filled
    { //d(BBAA)
      auto outmat = make_shared<Matrix>(nactB*nactB*nactA*nactA,1); //empty d_BBAA
      SMITH::sort_indices<2,3,0,1, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactA, nactB, nactB); //reorder and fill d_BBAA
      auto low = {nactA,nactA,0,0};
      auto up  = {nactT,nactT,nactA,nactA};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BBAA sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BBAA into d_BBAA sector of d
    } 
  }

  //Symmetrize: d(ABAB) note p19
  {
    auto low = {0,nactA,0,nactA};
    auto up  = {nactA,nactT,nactA,nactT};
    auto view = btas::make_view(twordm_->range().slice(low,up), twordm_->storage()); //d_ABAB sector of d
    auto inmat = make_shared<Matrix>(nactA*nactB,nactA*nactB); //empty d_ABAB
    copy(view.begin(), view.end(), inmat->begin()); //d_ABAB filled
    { //d(BABA)
      auto outmat = make_shared<Matrix>(nactB*nactA,nactB*nactA); //empty d_BABA
      SMITH::sort_indices<1,0,3,2, 0,1, 1,1>(inmat->data(), outmat->data(), nactA, nactB, nactA, nactB); //reorder and fill d_BABA
      auto low = {nactA,0,nactA,0};
      auto up  = {nactT,nactA,nactT,nactA};
      auto outv = btas::make_rwview(twordm_->range().slice(low,up), twordm_->storage()); //d_BABA sector of d
      copy(outmat->begin(), outmat->end(), outv.begin()); //copy d_BBBA into d_BABA sector of d
    } 
  }

}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>>
ASD_base::couple_blocks_RDM34(const DimerSubspace_base& AB, const DimerSubspace_base& ApBp) const {
//***************************************************************************************************************
  cout << "couple_block_RDM34 enetered.." << endl;
  Coupling term_type = coupling_type_RDM34(AB, ApBp);

  const DimerSubspace_base* space1 = &AB;
  const DimerSubspace_base* space2 = &ApBp;

  bool flip = (static_cast<int>(term_type) < 0);
  if (flip) {
    term_type = Coupling(-1*static_cast<int>(term_type));
    std::swap(space1,space2);
  }
  
  tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> out;
  std::array<MonomerKey,4> keys {{space1->template monomerkey<0>(), space1->template monomerkey<1>(), space2->template monomerkey<0>(), space2->template monomerkey<1>()}};

  switch(term_type) {
    case Coupling::none :
      out = make_tuple(nullptr,nullptr); break;
    case Coupling::diagonal :
      out = compute_diag_RDM34(keys, /*subspace diagonal*/false); break;
    case Coupling::aET :
      out = compute_aET_RDM34(keys); break;
    case Coupling::bET :
      out = compute_bET_RDM34(keys); break;
    case Coupling::abFlip :
      out = compute_abFlip_RDM34(keys); break;
    case Coupling::abET :
      out = compute_abET_RDM34(keys); break;
    case Coupling::aaET :
      out = compute_aaET_RDM34(keys); break;
    case Coupling::bbET :
      out = compute_bbET_RDM34(keys); break;
//Below: RDM3 explicit
    case Coupling::aaaET :
      out = compute_aaaET_RDM34(keys); break;
    case Coupling::bbbET :
      out = compute_bbbET_RDM34(keys); break;
    case Coupling::aabET :
      out = compute_aabET_RDM34(keys); break;
    case Coupling::abbET :
      out = compute_abbET_RDM34(keys); break;
    case Coupling::aETflp :
      out = compute_aETFlip_RDM34(keys); break;
    case Coupling::bETflp :
      out = compute_bETFlip_RDM34(keys); break;
    default :
      throw std::logic_error("Asking for a coupling type that has not been written.");
  }
  
  return out;
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_aET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "aET_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 1: p26B
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // a'b'b'bb
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha}); // a
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B);
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B);
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B);
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    SMITH::sort_indices<1,4,2,3,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0,     0,     0,     0,     0, nactA};
    auto up  = {nactA, nactA, nactA, nactA, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  { //CASE 3', 3'': p27B, 28
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}); // a'b'b
    auto gamma_B1= gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'aa
    auto gamma_B2= gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // b'ab
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); 
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B2);
    auto rdm3 = make_shared<Matrix>(gamma_A1 % gamma_B2);
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B1);
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    { // E_a'i,bj',ck' 
      int fac = {neleA%2 == 0 ? -1 : 1};
      SMITH::sort_indices<3,2,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
      SMITH::sort_indices<3,2,1,5,0,4, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
      SMITH::sort_indices<3,2,0,4,1,5, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
      rdmt->scale(fac);
      cout << "rearranged" << endl; cout.flush();
      
      auto low = {nactA,     0,     0, nactA,     0, nactA};
      auto up  = {nactT, nactA, nactA, nactT, nactA, nactT};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
      copy(rdmt->begin(), rdmt->end(), outv.begin());
      cout << "copied" << endl; cout.flush();
    }
    { // E_a'i',bj',ck 
      int fac = {neleA%2 == 0 ? -1 : 1};
      SMITH::sort_indices<3,4,1,5,0,2, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); 
      SMITH::sort_indices<3,5,0,4,1,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); 
      SMITH::sort_indices<3,5,1,4,0,2, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
      SMITH::sort_indices<3,4,0,5,1,2, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
      rdmt->scale(fac);
      cout << "rearranged" << endl; cout.flush();
      
      auto low = {nactA, nactA,     0, nactA,     0,     0};
      auto up  = {nactT, nactT, nactA, nactT, nactA, nactA};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
      copy(rdmt->begin(), rdmt->end(), outv.begin());
      cout << "copied" << endl; cout.flush();
    }
  }

  { //CASE 5: p40B
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha}); //a'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'aaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'b'baa
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // b'b'bba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'|a'a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'|a'b'baa 
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // a'|b'b'bba
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|a'a'aaa
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|a'b'baa
    SMITH::sort_indices<1,4,2,3,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|b'a'aba
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|b'b'bba
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA, nactA, nactA, nactA,     0, nactA};
    auto up  = {nactT, nactT, nactT, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  //4RDM
  { 
    cout << "4RDM #1" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'a'aaa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta }); //a'b'a'a'aab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta }); //a'b'b'a'abb
    auto gamma_A4 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta }); //a'b'b'b'bbb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha}); // a
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // a'a'a'a'aaa|a
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // a'b'a'a'aab|a
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B); // a'b'b'a'abb|a
    auto rdm4 = make_shared<Matrix>(gamma_A4 % gamma_B); // a'b'b'b'bbb|a
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck,dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'a'a'a'aaa|a
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'b'a'a'aab|a
    SMITH::sort_indices<3,4,1,6,2,5,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'a'b'a'aba|a
    SMITH::sort_indices<2,5,1,6,3,4,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'a'a'b'baa|a
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'b'b'a'abb|a
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'b'a'b'bab|a
    SMITH::sort_indices<1,6,3,4,2,5,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'b'b'a'abb|a
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // a'b'b'b'bbb|a
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("l")) += *rdmt;
  }

  { 
    cout << "4RDM #2" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'aa 
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //a'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //a'b'b'bb
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'aa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  // b'ab


    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1);  // a'a'a'aa|a'aa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B1);  // a'b'a'ab|a'aa
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B1);  // a'b'b'bb|a'aa

    auto rdm4 = make_shared<Matrix>(gamma_A1 % gamma_B2);  // a'a'a'aa|b'ab
    auto rdm5 = make_shared<Matrix>(gamma_A2 % gamma_B2);  // a'b'a'ab|b'ab
    auto rdm6 = make_shared<Matrix>(gamma_A3 % gamma_B2);  // a'b'b'bb|b'ab

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',bj',ck,dl
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<5,6,2,7,1,3,0,4, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'a'aa|a'aa
    SMITH::sort_indices<5,6,2,7,1,4,0,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'a'ba|a'aa
    SMITH::sort_indices<5,6,2,7,0,3,1,4, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'a'ab|a'aa
    SMITH::sort_indices<5,6,0,7,2,3,1,4, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'a'aa|a'aa

    SMITH::sort_indices<5,7,2,6,1,3,1,4, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'a'aa|b'ba
    SMITH::sort_indices<5,7,2,6,1,4,0,3, 1,1, -1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'a'ba|a'aa
    SMITH::sort_indices<5,7,2,6,0,3,1,4, 1,1, -1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'a'ab|a'aa
    SMITH::sort_indices<5,7,0,6,2,3,1,4, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'a'aa|a'aa

    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("aij")) += *rdmt;
  }
  
  { 
    cout << "4RDM #3" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'aa 
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //a'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //a'b'b'bb

    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'aa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  // b'ab
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1);  // a'a'a'aa|a'aa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B1);  // a'b'a'ab|a'aa
    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B2);  // a'b'a'ab|b'ab
    auto rdm4 = make_shared<Matrix>(gamma_A3 % gamma_B2);  // a'b'b'bb|b'ab

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj,ck',dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l
    SMITH::sort_indices<5,3,2,4,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'a'aa|a'aa
    SMITH::sort_indices<5,3,1,4,2,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'b'ab|a'aa (b<->c)
    SMITH::sort_indices<5,4,2,3,1,7,0,6, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'a'ba|b'ba (i<->j, k<->l)
    SMITH::sort_indices<5,3,2,4,1,7,0,6, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'b'bb|b'ba (k<->l)
    SMITH::sort_indices<5,3,2,4,0,6,1,7, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'a'ba|b'ab (c<->d)
    SMITH::sort_indices<5,3,2,4,0,6,1,7, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'b'bb|b'ab (c<->d)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("akl")) += *rdmt;
  }

  { 
    cout << "4RDM #4" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});  //a'b'b

    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'aaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'b'baa
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); // b'b'bba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a'a|a'a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // a'a'a|a'b'baa
    auto rdm3 = make_shared<Matrix>(gamma_A1 % gamma_B3); // a'a'a|b'b'bba
 
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B1); // a'b'b|a'a'aaa
    auto rdm5 = make_shared<Matrix>(gamma_A2 % gamma_B2); // a'b'b|a'b'baa
    auto rdm6 = make_shared<Matrix>(gamma_A2 % gamma_B3); // a'b'b|b'b'bba

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck',dl
    int fac = {neleA%2 == 0 ? -1 : 1};
    //                  a i b j c k d l
    SMITH::sort_indices<4,5,3,6,1,7,0,2, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'a|a'a'aaa
    SMITH::sort_indices<4,5,3,6,1,7,0,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'a|a'b'baa 
    SMITH::sort_indices<3,6,4,5,1,7,0,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'a|b'a'aba (a<->b, i<->j)
    SMITH::sort_indices<4,5,3,6,1,7,0,2, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'a|b'b'bba

    SMITH::sort_indices<4,5,3,6,0,7,1,2, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'b|a'a'aaa (c<->d)
    SMITH::sort_indices<4,5,3,6,0,7,1,2, 1,1, -1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'b|a'b'baa (c<->d)
    SMITH::sort_indices<3,6,4,5,0,7,1,2, 1,1, -1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'b|b'a'aba (c<->d, a<->b, i<->j)
    SMITH::sort_indices<4,5,3,6,0,7,1,2, 1,1, -1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'b|b'b'bba (c<->d)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("baijk")) += *rdmt;
  }

  { 
    cout << "4RDM #5" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});  //a'b'b

    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'aaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'b'baa
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); // b'b'bba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a'a|a'a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // a'a'a|a'b'baa
 
    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B2); // a'b'b|a'b'baa
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B3); // a'b'b|b'b'bba

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,b'j',ck',dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'a|a'a'aaa
    SMITH::sort_indices<3,2,4,5,1,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'a|b'a'baa (a<->b)

    SMITH::sort_indices<4,2,3,6,1,5,0,7, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'b|a'b'aba (j<->k)
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'b|b'b'bba

    SMITH::sort_indices<4,2,3,6,0,7,1,5, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'b|a'b'aab (c<->d, l->j, k->l, j->k) (l moved left twice = +1)
    SMITH::sort_indices<4,2,3,5,0,7,1,6, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'b|b'b'bab (c<->d, k<->l)

    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("bajkl")) += *rdmt;
  }

  { 
    cout << "4RDM #6" << endl; cout.flush();
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha}); //a'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//a'a'a'aaaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//a'a'b'baaa
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//a'b'b'bbaa
    auto gamma_B4 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha});//b'b'b'bbba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'|a'a'a'aaaa
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'|a'a'b'baaa
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // a'|a'b'b'bbaa
    auto rdm4 = make_shared<Matrix>(gamma_A % gamma_B4); // a'|b'b'b'bbba
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',c'k',dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|a'a'a'aaaa
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|a'a'b'baaa
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|a'b'a'abaa (a<->b, i<->j)
    SMITH::sort_indices<1,5,3,6,2,4,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|b'a'a'aaba (a->c, b->a, c->b & i->j, j->k, k->i)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|a'b'b'bbaa
    SMITH::sort_indices<3,4,1,6,2,5,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|b'a'b'baba (b<->c, j<->k)
    SMITH::sort_indices<2,5,1,6,3,4,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|b'b'a'abba (c->a, b->c, a->b &  k->i, i->j, j->k)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // a'|b'b'b'bbba
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("cbaijkl")) += *rdmt;
  }

  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_bET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "bET_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 1: p26B
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // b'a'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // b'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // b'b'b'bb
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta}); // b
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B);
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B);
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B);
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    SMITH::sort_indices<1,4,2,3,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB);
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0,     0,     0,     0,     0, nactA};
    auto up  = {nactA, nactA, nactA, nactA, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  { //CASE 3', 3'': p27B, 28
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); // b'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}); // b'b'b
    auto gamma_B1= gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // a'ba
    auto gamma_B2= gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // b'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); //b'a'a * a'ba
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B2); //b'b'b * b'bb
    auto rdm3 = make_shared<Matrix>(gamma_A1 % gamma_B2); //b'a'a * b'bb
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B1); //b'b'b * a'ba
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    { // E_a'i,bj',ck' 
      int fac = {neleA%2 == 0 ? -1 : 1};
      SMITH::sort_indices<3,2,1,5,0,4, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // b'a'a|a'ab
      SMITH::sort_indices<3,2,0,4,1,5, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // a'b'a|a'ba
      SMITH::sort_indices<3,2,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // b'b'b|b'bb
      rdmt->scale(fac);
      cout << "rearranged" << endl; cout.flush();
      
      auto low = {nactA,     0,     0, nactA,     0, nactA};
      auto up  = {nactT, nactA, nactA, nactT, nactA, nactT};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
      copy(rdmt->begin(), rdmt->end(), outv.begin());
      cout << "copied" << endl; cout.flush();
    }
    { // E_a'i',bj',ck 
      int fac = {neleA%2 == 0 ? -1 : 1};
      SMITH::sort_indices<3,5,0,4,1,2, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); //a'b'a|a'ab
      SMITH::sort_indices<3,4,1,5,0,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); //b'b'b|b'bb
      SMITH::sort_indices<3,4,0,5,1,2, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); //a'b'a|b'bb
      SMITH::sort_indices<3,5,1,4,0,2, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); //b'b'b|a'ab
      rdmt->scale(fac);
      cout << "rearranged" << endl; cout.flush();
      
      auto low = {nactA, nactA,     0, nactA,     0,     0};
      auto up  = {nactT, nactT, nactA, nactT, nactA, nactA};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
      copy(rdmt->begin(), rdmt->end(), outv.begin());
      cout << "copied" << endl; cout.flush();
    }
  }
  { //CASE 5: p40B
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta}); //b'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'a'aab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'b'bab
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // b'b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'|a'a'aab
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'|a'b'bab 
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // b'|b'b'bbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|a'a'aaa
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|a'b'baa
    SMITH::sort_indices<1,4,2,3,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|b'a'aba
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB); // a'|b'b'bba
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA, nactA, nactA, nactA,     0, nactA};
    auto up  = {nactT, nactT, nactT, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  //4RDM 
  { //cf. aET "l"
    cout << "4RDM #1" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'a'a'a'aaa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta }); //b'b'a'a'aab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta }); //b'b'b'a'abb
    auto gamma_A4 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta }); //b'b'b'b'bbb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta}); // b
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // b'a'a'a'aaa|b
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'b'a'a'aab|b
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B); // b'b'b'a'abb|b
    auto rdm4 = make_shared<Matrix>(gamma_A4 % gamma_B); // b'b'b'b'bbb|b
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck,dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'a'a'a'aaa|b
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'b'a'a'aab|b
    SMITH::sort_indices<3,4,1,6,2,5,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'a'b'a'aba|b
    SMITH::sort_indices<2,5,1,6,3,4,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'a'a'b'baa|b
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'b'b'a'abb|b
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'b'a'b'bab|b
    SMITH::sort_indices<1,6,3,4,2,5,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'b'b'a'abb|b
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactA, nactB); // b'b'b'b'bbb|b
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("l")) += *rdmt;
  }

  { //cf aET "aij"
    cout << "4RDM #2" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'a'a'aa 
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //b'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'b'bb
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // a'ba
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});  // b'bb


    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1);  // b'a'a'aa|a'ba
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B1);  // b'b'a'ab|a'ba
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B1);  // b'b'b'bb|a'ba

    auto rdm4 = make_shared<Matrix>(gamma_A1 % gamma_B2);  // b'a'a'aa|b'bb
    auto rdm5 = make_shared<Matrix>(gamma_A2 % gamma_B2);  // b'b'a'ab|b'bb
    auto rdm6 = make_shared<Matrix>(gamma_A3 % gamma_B2);  // b'b'b'bb|b'bb

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',bj',ck,dl
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l                                                                                                     original order  (dcbkl|aij) => 56271304(=aibjckdl)
    SMITH::sort_indices<5,7,0,6,2,3,1,4, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'b'aa|a'ab: -(bdckl|aji) 
    SMITH::sort_indices<5,7,0,6,2,4,1,3, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'b'ba|a'ab:  (bdclk|aji)
    SMITH::sort_indices<5,7,1,6,2,3,0,4, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'b'ab|a'ab: -(dbckl|aji)
    SMITH::sort_indices<5,7,2,6,1,3,0,4, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'b'bb|a'ab: -(dcbkl|aji)

    SMITH::sort_indices<5,6,1,7,0,3,2,4, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'b'aa|b'bb:  (cbdkl|aij)
    SMITH::sort_indices<5,6,0,7,2,4,1,3, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'b'ba|b'bb:  (bdclk|aij)
    SMITH::sort_indices<5,6,1,7,2,3,0,4, 1,1, -1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'b'ab|b'bb: -(dbckl|aij)
    SMITH::sort_indices<5,6,2,7,1,3,0,4, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'b'bb|b'bb:  (dcbkl|aij)

    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("aij")) += *rdmt;
  }

  { 
    cout << "4RDM #3" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'a'a'aa 
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //b'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'b'bb

    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // a'ba
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});  // b'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1);  // b'a'a'aa|a'ba
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B1);  // b'b'a'ab|a'ba

    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B2);  // b'b'a'ab|b'bb
    auto rdm4 = make_shared<Matrix>(gamma_A3 % gamma_B2);  // b'b'b'bb|b'bb

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj,ck',dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l                                                                                                     original order  (dcbij|akl)
    SMITH::sort_indices<5,3,2,4,0,6,1,7, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'a'aa|a'ba: -(cdbij|akl) : c->d
    SMITH::sort_indices<5,3,1,4,0,6,2,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'b'b'ab|a'ba:  (cbdij|akl) : d->c->b
    SMITH::sort_indices<5,3,2,4,1,7,0,6, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'a'aa|a'ab: -(dcbij|alk) : k->l
    SMITH::sort_indices<5,3,1,4,2,7,0,6, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'a'b'ab|a'ab:  (dbcij|alk) : k->l, c->b
    SMITH::sort_indices<5,4,2,3,1,6,0,7, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'a'ba|b'bb: -(dcbji|akl) : j->i
    SMITH::sort_indices<5,3,2,4,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'b'bb|b'bb:  (dcbij|akl)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("akl")) += *rdmt;
  }

  { 
    cout << "4RDM #4" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //b'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});  //b'b'b

    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'a'aab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'b'bab
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta}); // b'b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // b'a'a|a'a'aab
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // b'a'a|a'b'bab
    auto rdm3 = make_shared<Matrix>(gamma_A1 % gamma_B3); // b'a'a|b'b'bbb
 
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B1); // b'b'b|a'a'aab
    auto rdm5 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'b'b|a'b'bab
    auto rdm6 = make_shared<Matrix>(gamma_A2 % gamma_B3); // b'b'b|b'b'bbb

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck',dl
    int fac = {neleA%2 == 0 ? -1 : 1};
    //                  a i b j c k d l                                                                                                     origianl order  (dcl|baijk)
    SMITH::sort_indices<4,5,3,6,0,7,1,2, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'a|a'a'aab: -(cdl|baijk) : d->c
    SMITH::sort_indices<4,5,3,6,0,7,1,2, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'a|a'b'bab: -(cdl|baijk) : d->c
    SMITH::sort_indices<3,6,4,5,0,7,1,2, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'a|b'a'abb: -(cdl|abjik) : d->c, b->a, i->j
    SMITH::sort_indices<4,5,3,6,1,7,0,2, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'a|b'b'bbb: -(cdl|baijk) : d->c

    SMITH::sort_indices<4,5,3,6,1,7,0,2, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'b|a'a'aab:  (dcl|baijk)
    SMITH::sort_indices<4,5,3,6,1,7,0,2, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'b|a'b'bab:  (dcl|baijk)
    SMITH::sort_indices<3,6,4,5,1,7,0,2, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'b|b'a'abb:  (dcl|abjik) : a->b, i->j
    SMITH::sort_indices<4,5,3,6,1,7,0,2, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'b|b'b'b'b:  (dcl|baijk)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("baijk")) += *rdmt;
  }

  { 
    cout << "4RDM #5" << endl; cout.flush();
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //b'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});  //b'b'b

    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'a'aab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'b'bab
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta}); // b'b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // b'a'a|a'a'aab
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // b'a'a|a'b'bab
 
    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'b'b|a'b'bab
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B3); // b'b'b|b'b'bbb

    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,b'j',ck',dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l                                                                                                     original order  (dci|bajkl)
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'a|a'a'aab   (dci|bajkl)
    SMITH::sort_indices<4,2,3,5,0,7,1,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'b'a|a'a'aba   (cdi|bajlk) : c->d, k->l
    SMITH::sort_indices<3,2,4,5,1,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'a|b'a'bab  -(dci|abjkl) : a->b
    SMITH::sort_indices<3,2,4,5,1,7,0,6, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'a'a|b'a'bba   (dci|abjlk) : a->b, k->l
    SMITH::sort_indices<4,2,3,6,1,5,0,7, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'b|a'b'abb  -(dci|abkjl) : j->k
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'b|b'b'bbb   (dci|abjkl)

    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("bajkl")) += *rdmt;
  }

  { 
    cout << "4RDM #6" << endl; cout.flush();
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta}); //b'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});//a'a'a'aaab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});//a'a'b'baab
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});//a'b'b'bbab
    auto gamma_B4 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});//b'b'b'bbbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'|a'a'a'aaab
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'|a'a'b'baab
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // b'|a'b'b'bbab
    auto rdm4 = make_shared<Matrix>(gamma_A % gamma_B4); // b'|b'b'b'bbbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',c'k',dl'
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k d l                                                                                                     original order  (d|cbaijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|a'a'a'aaab   (d|cbaijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|a'a'b'baab   (d|cbaijkl)
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|a'b'a'abab   (d|cabjikl) : a->b, i->j
    SMITH::sort_indices<1,6,3,4,2,5,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|b'a'a'aabb   (d|acbjkil) : a->b->c, i->j->k
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|a'b'b'bbab   (d|cbaijkl)
    SMITH::sort_indices<3,4,1,6,2,5,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|a'b'b'babb   (d|bcaikjl) : b->c, j->k
    SMITH::sort_indices<1,6,3,4,2,5,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|b'b'a'abbb   (d|acbjkil) : a->b->c, i->j->k
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactB, nactB, nactB, nactB, nactB, nactB, nactB); // b'|b'b'b'bbbb   (d|cbaijkl)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("cbaijkl")) += *rdmt;
  }
 
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_aaET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "aaET_RDM34" << endl; cout.flush();
//auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

//const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 2: p27
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}); // a'a'b'b
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // aa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // a'a'a'a|aa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // a'a'b'b|aa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj',ck' 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'a|aa
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'b|aa
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0,     0,     0, nactA,     0, nactA};
    auto up  = {nactA, nactA, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  { //CASE 4: p40B
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha}); //a'a'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'aaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // b'baa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'a'|a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'a'|b'baa 
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',bj',ck' 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // a'a'|a'aaa
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // a'a'|b'baa
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA, nactA,     0, nactA,     0, nactA};
    auto up  = {nactT, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  //4RDM
  { //p31
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); //a'a'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta}); //a'a'b'b'bb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // aa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // a'a'a'a'aa|aa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // a'a'b'a'ab|aa
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B); // a'a'b'b'bb|aa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbaij|kl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'a'aa|aa:  (dcbaij|kl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'a'ab|aa:  (dcbaij|kl)
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'b'ba|aa:  (dcabji|kl) : a->b, i->j
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'b'bb|aa:  (dcbaij|kl)
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("kl")) += *rdmt;
  }

  { //p34B
    cout << "4RDM #2" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});   //a'a'b'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'aaa 
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'baa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a'a'a|a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B2); // a'a'b'b|b'baa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbi|ajkl)
    SMITH::sort_indices<4,3,2,5,1,6,0,7, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'a'a|a'aaa:  (dcbi|ajkl)
    SMITH::sort_indices<4,3,2,5,1,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'b'b|b'baa:  (dcbi|ajkl)
    SMITH::sort_indices<4,3,1,6,2,5,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'a'b|b'aba:  (dbci|akjl) : b->c, j->k
    SMITH::sort_indices<4,3,0,7,2,5,1,6, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'a'b|b'aab:  (bdci|aklj) : b->c->d, j->k->l
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("ajkl")) += *rdmt;
  }

  { //p35
    cout << "4RDM #3" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});   //a'a'b'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'aaa 
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'baa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a'a'a|a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // a'a'a'a|b'baa
    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B1); // a'a'b'b|a'aaa
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B2); // a'a'b'b|b'baa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbl|aijk)
    SMITH::sort_indices<4,5,2,6,1,7,0,3, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'a'a|a'aaa:  (dcbl|aijk)
    SMITH::sort_indices<4,5,2,6,1,7,0,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'a'a|b'baa:  (dcbl|aijk)
    SMITH::sort_indices<4,5,1,6,0,7,2,3, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'a'b|a'aaa:  (cbdl|aijk) : d->c->b
    SMITH::sort_indices<4,5,1,6,0,7,2,3, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'a'b|b'baa:  (cbdl|aijk) : d->c->b
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("aijk")) += *rdmt;
  }
    
  { //p39
    cout << "4RDM #4" << endl;
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha}); //a'a'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//a'a'aaaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//a'b'baaa
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//b'b'bbaa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'a'|a'a'aaaa
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'a'|a'b'baaa
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // a'a'|b'b'bbaa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dc|baijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a'|a'a'aaaa:  (dc|baijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a'|a'b'baaa:  (dc|baijkl)
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a'|b'a'abaa:  (dc|abjikl) : b->a, j->i
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a'|b'b'bbaa:  (dc|baijkl)
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("baijkl")) += *rdmt;
  }
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_bbET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "bbET_RDM34" << endl; cout.flush();
//auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

//const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 2: p27
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); // b'b'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}); // b'b'b'b
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // b'b'a'a|bb
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'b'b'b|bb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj',ck' 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'a|bb
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'b|bb
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0,     0,     0, nactA,     0, nactA};
    auto up  = {nactA, nactA, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  { //CASE 4: p40B
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta}); //b'b'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // a'abb
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'b'|a'abb
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'b'|b'bbb 
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',bj',ck' 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'b'|a'abb
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'b'|b'bbb
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA, nactA,     0, nactA,     0, nactA};
    auto up  = {nactT, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  { //p31
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'b'a'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); //b'b'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta}); //b'b'b'b'bb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // b'b'a'a'aa|bb
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'b'b'a'ab|bb
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B); // b'b'b'b'bb|bb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbaij|kl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'a'aa|bb:  (dcbaij|kl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'a'ab|bb:  (dcbaij|kl)
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'b'ba|bb:  (dcabji|kl) : a->b, i->j
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'b'bb|bb:  (dcbaij|kl)
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("kl")) += *rdmt;
  }

  { //p34B
    cout << "4RDM #2" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //b'b'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});   //b'b'b'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //a'abb 
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // b'b'a'a|a'abb
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'b'b'b|b'bbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbi|ajkl)
    SMITH::sort_indices<4,3,2,5,1,6,0,7, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'a'a|a'abb:  (dcbi|ajkl)
    SMITH::sort_indices<4,3,2,5,1,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'b'b|b'bbb:  (dcbi|ajkl)
    SMITH::sort_indices<4,3,1,6,2,5,0,7, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'b'a|a'bab:  (dbci|akjl) : b->c, k->j
    SMITH::sort_indices<4,3,1,6,0,7,2,5, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'b'a|a'bba:  (cbdi|aljk) : d->b->c, l->j->k
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("ajkl")) += *rdmt;
  }

  { //p35
    cout << "4RDM #3" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //b'b'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta});   //b'b'b'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //a'abb 
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // b'b'a'a|a'abb
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // b'b'a'a|b'bbb
    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B1); // b'b'b'b|a'abb
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'b'b'b|b'bbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbl|aijk)
    SMITH::sort_indices<4,5,1,6,0,7,2,3, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'b'a|a'abb:  (cbdl|aijk) : d->c->b
    SMITH::sort_indices<4,5,1,6,0,7,2,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'b'a|b'bbb:  (cbdl|aijk) : d->c->b
    SMITH::sort_indices<4,5,2,6,1,7,0,3, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'b'b|a'abb:  (dcbl|aijk) 
    SMITH::sort_indices<4,5,2,6,1,7,0,3, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'b'b|b'bbb:  (dcbl|aijk)
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("aijk")) += *rdmt;
  }

  { //p39
    cout << "4RDM #4" << endl;
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta}); //b'b'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //a'a'aabb
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //a'b'babb
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //b'b'bbbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'b'|a'a'aabb
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'b'|a'b'babb
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // b'b'|b'b'bbbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dc|baijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b'|a'a'aabb:  (dc|baijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b'|a'b'babb:  (dc|baijkl)
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b'|b'a'abbb:  (dc|abjikl) : b->a, j->i
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b'|b'b'bbbb:  (dc|baijkl)
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("baijkl")) += *rdmt;
  }

  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_abET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "abET_RDM34" << endl; cout.flush();
//auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

//const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 2: p27
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); // b'a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}); // b'a'b'b
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // ab
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // b'a'a'a|ab
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'a'b'b|ab
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj',ck' 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'a|ab
    SMITH::sort_indices<2,3,0,5,1,4, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'a|ba
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'b|ab
    SMITH::sort_indices<2,3,0,5,1,4, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'b|ba
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0,     0,     0, nactA,     0, nactA};
    auto up  = {nactA, nactA, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  { //CASE 4: p40B
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta}); //a'b'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // a'aba
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // b'bba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'b'|a'aba
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'b'|b'bba 
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',bj',ck' 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // a'b'|a'aba
    SMITH::sort_indices<2,3,0,5,1,4, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'a'|a'aab
    SMITH::sort_indices<2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // a'b'|b'bba
    SMITH::sort_indices<2,3,0,5,1,4, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'a'|b'bab
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA, nactA,     0, nactA,     0, nactA};
    auto up  = {nactT, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  //4RDM
  { //p31
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'b'a'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //a'b'b'a'ab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //a'b'b'b'bb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // ab
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // a'b'a'a'aa|ab
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // a'b'b'a'ab|ab
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B); // a'b'b'b'bb|ab
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai,bj,ck',dl'
    //                  a i b j c k d l                                                                                                     original order  (dcbaij|kl)
    SMITH::sort_indices<3,4,2,5,1,7,0,6, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'a'aa|ba:  (dcbaij|lk) : k->l
    SMITH::sort_indices<3,4,2,5,0,6,1,7, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'a'aa|ab:  (cdbaij|kl) : c->d
    SMITH::sort_indices<3,4,2,5,1,7,0,6, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'a'ab|ba:  (dcbaij|lk) : k->l
    SMITH::sort_indices<2,5,3,4,1,7,0,6, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'b'ba|ba:  (dcabji|lk) : k->l, i->j, a->b
    SMITH::sort_indices<3,4,2,5,0,6,1,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'a'ab|ab:  (cdbaij|kl) : c->d
    SMITH::sort_indices<2,5,3,4,0,6,1,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'b'ba|ab:  (cdabji|kl) : c->d, a->b, i->j
    SMITH::sort_indices<3,4,2,5,1,7,0,6, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'b'bb|ba:  (dcbaij|lk) : k->l
    SMITH::sort_indices<3,4,2,5,0,6,1,7, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'b'bb|ba:  (cdbaij|kl) : c->d
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("kl")) += *rdmt;
  }

  { //p34B, 35
    cout << "4RDM #3" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //b'a'a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}); //b'a'b'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); //a'aba 
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); //b'bba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // b'a'a'a|a'aba
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // b'a'a'a|b'bba
    auto rdm3 = make_shared<Matrix>(gamma_A2 % gamma_B1); // b'a'b'b|a'aba
    auto rdm4 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'a'b'b|b'bba
    cout << "full gammas" << endl; cout.flush();
    {
      auto rdmt = rdm1->clone();
      // E_a'i,bj',ck',dl' : sign(-1)
      //                  a i b j c k d l                                                                                                     original order  (dcbi|ajkl)
      SMITH::sort_indices<4,3,2,5,1,7,0,6, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'a'a|a'aab  -(dcbi|ajlk) : k->l
      SMITH::sort_indices<4,3,2,5,0,6,1,7, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'a'a|a'aba  -(cdbi|ajkl) : c->d
      SMITH::sort_indices<4,3,0,6,2,5,1,7, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'b'a|a'baa  -(bdci|akjl) : b->c->d, j->k
      SMITH::sort_indices<4,3,2,5,1,7,0,6, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'b'b|b'bab  -(dcbi|ajlk) : k->l
      SMITH::sort_indices<4,3,2,5,0,6,1,7, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'b'b|b'bba  -(cdbi|ajkl) : c->d
      SMITH::sort_indices<4,3,1,7,2,5,0,6, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'a'b|b'abb  -(dbci|aklj) : b->c, j->k->l
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("ajkl")) += *rdmt;
    }
    {
      auto rdmt = rdm1->clone();
      // E_a'i',bj',ck',dl : sign(-1)
      //                  a i b j c k d l                                                                                                     original order  (dcbl|aijk)
      SMITH::sort_indices<4,5,2,7,0,6,1,3, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'a'a|a'aab   (cdbl|aikj) : d->c, j->k
      SMITH::sort_indices<4,5,0,6,1,6,7,3, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'b'a|a'aba   (bcdl|aijk) : b->c->d
      SMITH::sort_indices<4,5,2,7,0,6,1,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'a'a|b'bab   (cdbl|aikj) : d->c, j->k
      SMITH::sort_indices<4,5,0,6,1,6,7,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'b'a|b'bba   (bcdl|aijk) : b->c->d
      SMITH::sort_indices<4,5,1,7,2,6,0,3, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'a'b|a'aab   (dbcl|aikj) : d->c, j->k
      SMITH::sort_indices<4,5,2,6,1,7,0,3, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'b'b|a'aba   (dcbl|aijk) 
      SMITH::sort_indices<4,5,1,7,2,6,0,3, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'a'b|b'bab   (dbcl|aikj) : c->b, j->k
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("aijk")) += *rdmt;
    }

  }

  { //p31
    cout << "4RDM #4" << endl;
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta} ); //a'b'
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha});//a'a'aaba
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha});//a'b'baba
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha});//b'b'bbba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'b'|a'a'aaba
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'b'|a'b'baba
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // a'b'|b'b'bbba
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i',b'j',ck',dl' : sign(+1)
    //                  a i b j c k d l                                                                                                     original order  (dc|baijkl)
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'b'|a'a'aaba   (dc|baijkl)
    SMITH::sort_indices<3,4,2,5,0,7,1,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'a'aaab   (cd|baijlk) : d->c, k->l
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'b'|a'b'baba   (dc|baijkl)
    SMITH::sort_indices<2,5,3,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'b'|b'a'abba   (dc|abjikl) : a->b. i->j
    SMITH::sort_indices<3,4,2,5,0,7,1,6, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'b'baab   (cd|baijlk) : d->c, k->l
    SMITH::sort_indices<2,5,3,4,0,7,1,6, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|b'a'abab   (cd|abjilk) : d->c, k->l, a->b. i->j
    SMITH::sort_indices<3,4,2,5,1,6,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'b'|b'b'bbba   (dc|baijkl) 
    SMITH::sort_indices<3,4,2,5,0,7,1,6, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|b'b'bbab   (cd|baijlk) : d->c, k->l
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("kl")) += *rdmt;
  }


  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_abFlip_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "abFlip_RDM34" << endl; cout.flush();
//auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

//const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 2': p27
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'b'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  // b'b'ab 
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta}); // a'b
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // a'b'aa|a'b
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'b'ab|a'b
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck 
    SMITH::sort_indices<4,2,1,5,0,3, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // a'b'aa|a'b
    SMITH::sort_indices<4,2,1,5,0,3, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'b'ab|a'b
    SMITH::sort_indices<5,1,2,4,3,0, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'a'bb|a'b (N,M) contribution p41
    SMITH::sort_indices<5,1,2,4,3,0, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); // b'b'ab|a'b (N,M) contribution p41
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA,     0,     0, nactA,     0,     0};
    auto up  = {nactT, nactA, nactA, nactT, nactA, nactA};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  { //CASE 4': p27
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}); // b'a
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // a'a'ab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta}); // b'a'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'a|a'a'ab
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'a|b'a'bb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,b'j',ck' 
    SMITH::sort_indices<3,1,2,4,0,5, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'a|a'a'ab
    SMITH::sort_indices<3,1,2,4,0,5, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'a|b'a'bb

    SMITH::sort_indices<5,0,4,3,1,2, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'a|a'a'ba (N,M) contribution p41
    SMITH::sort_indices<4,0,5,2,1,3, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); // b'a|a'b'bb (N,M) contribution p41
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA,     0, nactA, nactA,     0, nactA};
    auto up  = {nactT, nactA, nactT, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  //4RDM
  { //p32
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // b'a'a'aaa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  // b'b'a'aab
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  // b'b'b'abb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta}); //a'b
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // b'a'a'aaa|a'b
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'b'a'aab|a'b
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B); // b'b'b'abb|a'b
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj,ck,dl' : sign(+1)
    //                  a i b j c k d l                                                                                                     original order  (dcbijk|al)
    SMITH::sort_indices<6,3,2,4,1,5,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'aaa|a'b   (dcbijk|al)
    SMITH::sort_indices<6,3,2,4,1,5,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'aab|a'b   (dcbijk|al)
    SMITH::sort_indices<6,3,1,5,2,4,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'aba|a'b   (dbcikj|al) : c->b,  j->k
    SMITH::sort_indices<6,3,2,4,1,5,0,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'abb|a'b   (dcbijk|al)

    //                  a i b j c k d l                                                                                                     original order  (kjibcd|la) : (N,M) conbribution
    SMITH::sort_indices<7,0,3,2,4,1,5,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'aaa|a'b   (ikjbcd|la) : i->j->k
    SMITH::sort_indices<7,1,5,0,3,2,4,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'baa|a'b   (jikcdb|la) : k->j->i, b->c->d
    SMITH::sort_indices<7,1,3,2,5,0,4,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'aba|a'b   (kijbdc|la) : i->j, c->d
    SMITH::sort_indices<7,2,4,1,5,0,3,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bba|a'b   (kjidbc|la) : d->c->b

    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("al")) += *rdmt;
  }

  { //p41B
    cout << "4RDM #2" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'b'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});   // b'b'ab
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); //a'a'ab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});  //b'a'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'b'aa|a'a'ab
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'b'ab|b'a'bb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj,ck,dl' : sign(+1)
    //                  a i b j c k d l                                                                                                     original order  (dcij|bakl)
    SMITH::sort_indices<5,2,4,3,1,7,0,6, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'aa|a'a'ba  -(dcij|balk) : k->l
    SMITH::sort_indices<5,2,4,3,0,6,1,7, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'aa|a'a'ab  -(cdij|bakl) : d->c
    SMITH::sort_indices<5,2,4,3,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'ab|b'a'bb   (dcij|bakl)
    SMITH::sort_indices<4,3,5,2,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'ba|a'b'bb   (dcji|abkl) : i->j, b->a

    //                  a i b j c k d l                                                                                                     original order  (jicd|lkab) : (N,M) conbribution
    SMITH::sort_indices<7,1,6,0,2,5,3,4, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'aa|a'a'ba  -(jicd|lkba) : a->b
    SMITH::sort_indices<6,0,7,1,2,5,3,4, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'aa|a'a'ab  -(ijcd|lkab) : i->j
    SMITH::sort_indices<6,1,7,0,2,5,3,4, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'ab|b'a'bb   (jicd|lkab)
    SMITH::sort_indices<6,1,7,0,3,4,2,5, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'ba|a'b'bb   (jidc|klab) : c->d, l->k
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("bakl")) += *rdmt;
  }

  { //p39B
    cout << "4RDM #3" << endl;
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}); // b'a
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});//a'a'a'aab
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});//a'b'a'bab
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});//b'b'a'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'a'|a'a'a'aab
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'a'|a'b'a'bab
    auto rdm3 = make_shared<Matrix>(gamma_A % gamma_B3); // b'a'|b'b'a'bbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,b'j',c'k',dl' : sign(-1)
    //                  a i b j c k d l                                                                                                     original order  (di|cbajkl)
    SMITH::sort_indices<4,1,3,5,2,6,0,7, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'a'a'aab  (di|cbajkl)
    SMITH::sort_indices<4,1,3,5,2,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'b'a'bab  (di|cbajkl)
    SMITH::sort_indices<4,1,2,6,3,5,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|b'a'a'abb  (di|bcakjl) : c->b, j->k
    SMITH::sort_indices<4,1,3,5,2,6,0,7, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|b'b'a'bbb  (di|cbajkl)

    //                  a i b j c k d l                                                                                                     original order  (id|lkjabc) : (N,M) conbribution
    SMITH::sort_indices<7,0,5,4,7,3,1,2, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'a'a'baa  (id|lkjbca) : a->b->c
    SMITH::sort_indices<5,0,7,3,6,4,1,2, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'a'b'bba  (id|ljkacb) : k->j, b->c
    SMITH::sort_indices<5,0,6,4,7,3,1,2, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'b'a'bab  (id|lkjabc)
    SMITH::sort_indices<7,0,5,3,7,2,1,4, 1,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a'|a'b'b'bbb  (id|kjlabc) : l->k->j

    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("cbajkl")) += *rdmt;
  }
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_aaaET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "aaaET_RDM34" << endl; cout.flush();
  assert(false);
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 3: p26B
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha}); // a'a'a'
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // aaa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B); //a'a'a'|aaa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai',bj',ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0, nactA,     0, nactA,     0, nactA};
    auto up  = {nactA, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_bbbET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "bbbET_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 3: p26B
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta}); // b'b'b
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B); //b'b'b|bbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai',bj',ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k 
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB);
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0, nactA,     0, nactA,     0, nactA};
    auto up  = {nactA, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_aabET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "bbbET_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 3: p26B
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::CreateAlpha}); // a'b'a
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); // aba
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B); //a'b'a'|aba
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai',bj',ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k                                                                                    original order  (cba|ijk)
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // a'b'a'|aba:  (cba|ijk)  
    SMITH::sort_indices<2,3,0,5,1,4, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // b'a'a'|aab:  (bca|ikj) : c->b, k->j
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0, nactA,     0, nactA,     0, nactA};
    auto up  = {nactA, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_abbET_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "bbbET_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 3: p26B
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::CreateBeta}); // b'a'b
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta}); // bab
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B); // b'a'b'|bab
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_ai',bj',ck' 
    int fac = {neleA%2 == 0 ? 1 : -1};
    //                  a i b j c k                                                                                    original order  (cba|ijk)
    SMITH::sort_indices<2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // b'a'b'|bab:  (cba|ijk) 
    SMITH::sort_indices<2,3,0,5,1,4, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // a'b'b'|bba:  (bca|ikj) : c->b, k->j
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {    0, nactA,     0, nactA,     0, nactA};
    auto up  = {nactA, nactT, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_aETFlip_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "aETFlip_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 3': p27B
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta}); // a'a'b
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // b'aa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B); // a'a'b|b'aa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck' 
    int fac = {neleA%2 == 0 ? -1 : 1};
    SMITH::sort_indices<3,2,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // a'a'b|b'aa
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA,     0,     0, nactA,     0, nactA};
    auto up  = {nactT, nactA, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }
  
  //4RDM
  { //p33B
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}); //a'a'a'ba
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});  //a'a'b'bb
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha});//b'aa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // a'a'a'ba|b'aa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // a'a'b'bb|b'aa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    int fac = {neleA%2 == 0 ? 1 : -1};
    // E_a'i,bj,ck',dl' 
    //                  a i b j c k d l                                                                                                     original order  (dcbij|akl)
    SMITH::sort_indices<5,3,2,4,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'a'ba|b'aa   (dcbij|akl)
    SMITH::sort_indices<5,3,2,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // a'a'b'bb|b'aa   (dcbij|akl)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("akl")) += *rdmt;
  }

  { //p33B
    cout << "4RDM #2" << endl;
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta}); //a'a'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'b'aaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'b'baa
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // a'a'b|a'b'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // a'a'b|b'b'baa
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    int fac = {neleA%2 == 0 ? 1 : -1};
    // E_a'i,b'j',ck',dl' 
    //                  a i b j c k d l                                                                                                     original order  (dci|bajkl)
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'b|a'b'aaa   (dci|bajkl)
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // a'a'b|b'b'baa   (dci|bajkl)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("bajkl")) += *rdmt;
  }

  return make_tuple(out3,out4);
}

//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_bETFlip_RDM34(const array<MonomerKey,4>& keys) const {
//***************************************************************************************************************
  cout << "bETFlip_RDM34" << endl; cout.flush();
  auto& Ap = keys[2];

  auto& B  = keys[1];
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr; //make_shared<RDM<2>>(nactA+nactB);

  const int neleA = Ap.nelea() + Ap.neleb();

  //3RDM 
  { //CASE 3': p27B
    auto gamma_A = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}); // b'b'a
    auto gamma_B = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); // a'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B); // b'b'a|a'bb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    // E_a'i,bj',ck' 
    int fac = {neleA%2 == 0 ? -1 : 1};
    SMITH::sort_indices<3,2,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB); // b'b'a|a'bb
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    auto low = {nactA,     0,     0, nactA,     0, nactA};
    auto up  = {nactT, nactA, nactA, nactT, nactA, nactT};
    auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    copy(rdmt->begin(), rdmt->end(), outv.begin());
    cout << "copied" << endl; cout.flush();
  }

  //4RDM
  { //p33B
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //b'b'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::CreateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //b'b'b'ab
    auto gamma_B  = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta});//a'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B); // b'b'a'aa|a'bb
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B); // b'b'b'ab|a'bb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    int fac = {neleA%2 == 0 ? 1 : -1};
    // E_a'i,bj,ck',dl' 
    //                  a i b j c k d l                                                                                                     original order  (dcbij|akl)
    SMITH::sort_indices<5,3,2,4,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'a'aa|a'bb   (dcbij|akl)
    SMITH::sort_indices<5,3,2,4,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactB, nactB, nactB); // b'b'b'ab|a'bb   (dcbij|akl)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("akl")) += *rdmt;
  }

  { //p33B
    cout << "4RDM #2" << endl;
    auto gamma_A  = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}); //b'b'a
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //a'a'abb
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}); //b'a'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A % gamma_B1); // b'b'a|a'a'abb
    auto rdm2 = make_shared<Matrix>(gamma_A % gamma_B2); // b'b'a|b'a'bbb
    auto rdmt = rdm1->clone();
    cout << "full gammas" << endl; cout.flush();

    int fac = {neleA%2 == 0 ? 1 : -1};
    // E_a'i,b'j',ck',dl' 
    //                  a i b j c k d l                                                                                                     original order  (dci|bajkl)
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'a|a'a'abb   (dci|bajkl)
    SMITH::sort_indices<4,2,3,5,1,6,0,7, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactB, nactB, nactB, nactB, nactB); // b'b'a|b'a'bbb   (dci|bajkl)
    rdmt->scale(fac);
    cout << "rearranged" << endl; cout.flush();

    *fourrdm_.at(string("bajkl")) += *rdmt;
  }
  
  return make_tuple(out3,out4);
}


//***************************************************************************************************************
tuple<shared_ptr<RDM<3>>,shared_ptr<RDM<4>>> 
ASD_base::compute_diag_RDM34(const array<MonomerKey,4>& keys, const bool subdia) const {
//***************************************************************************************************************
  cout << "DIAG_RDM34" << endl; cout.flush();
  auto& B  = keys[1]; 
  auto& Bp = keys[3];

  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();
  const int nactT = nactA+nactB;
  auto out3 = make_shared<RDM<3>>(nactA+nactB);
  auto out4 = nullptr;

  { //CASE 2'' & 2': p27
    auto gamma_A_aaaa = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'aa
    auto gamma_A_abba = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); //a'b'ba
    auto gamma_A_bbbb = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'bb
    auto gamma_B_aa = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); // a'a
    auto gamma_B_bb = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});  // b'b

    auto rdmA_a  = make_shared<Matrix>(gamma_A_aaaa % gamma_B_aa); //a'a'aa|a'a
    auto rdmA_ab = make_shared<Matrix>(gamma_A_abba % gamma_B_aa); //a'b'ba|a'a
    auto rdmA_b  = make_shared<Matrix>(gamma_A_bbbb % gamma_B_aa); //b'b'bb|a'a
    auto rdmB_a  = make_shared<Matrix>(gamma_A_aaaa % gamma_B_bb); //a'a'aa|b'b
    auto rdmB_ab = make_shared<Matrix>(gamma_A_abba % gamma_B_bb); //a'b'ba|b'b
    auto rdmB_b  = make_shared<Matrix>(gamma_A_bbbb % gamma_B_bb); //b'b'bb|b'b
    
    {
      auto rdmt = rdmA_a->clone();
      // E_a'i',bj,ck
      SMITH::sort_indices<4,5,1,2,0,3, 0,1,  1,1>(rdmA_a->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //a'a'aa|a'a
      SMITH::sort_indices<4,5,1,2,0,3, 1,1,  1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'b'ba|a'a
      SMITH::sort_indices<4,5,0,3,1,2, 1,1,  1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'a'ab|a'a
      SMITH::sort_indices<4,5,1,2,0,3, 1,1,  1,1>(rdmA_b->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //b'b'bb|a'a
      
      SMITH::sort_indices<4,5,1,2,0,3, 1,1,  1,1>(rdmB_a->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //a'a'aa|b'b
      SMITH::sort_indices<4,5,1,2,0,3, 1,1,  1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'b'ba|b'b
      SMITH::sort_indices<4,5,0,3,1,2, 1,1,  1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'a'ab|b'b
      SMITH::sort_indices<4,5,1,2,0,3, 1,1,  1,1>(rdmB_b->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //b'b'bb|b'b
      
      if (!subdia) { // <M|op|N> contribution k'j'bc|i'a
        SMITH::sort_indices<5,4,2,1,3,0, 1,1,  1,1>(rdmA_a->data(),  rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'a'aa|a'a
        SMITH::sort_indices<5,4,2,1,3,0, 1,1,  1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'b'ba|a'a
        SMITH::sort_indices<5,4,3,0,2,1, 1,1,  1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'a'ab|a'a
        SMITH::sort_indices<5,4,2,1,3,0, 1,1,  1,1>(rdmA_b->data(),  rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'b'bb|a'a

        SMITH::sort_indices<5,4,2,1,3,0, 1,1,  1,1>(rdmB_a->data(),  rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'a'aa|b'b
        SMITH::sort_indices<5,4,2,1,3,0, 1,1,  1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'b'ba|b'b
        SMITH::sort_indices<5,4,3,0,2,1, 1,1,  1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'a'ab|b'b
        SMITH::sort_indices<5,4,2,1,3,0, 1,1,  1,1>(rdmB_b->data(),  rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'b'bb|b'b
      }
      
      auto low = {nactA, nactA,     0,     0,     0,     0};
      auto up  = {nactT, nactT, nactA, nactA, nactA, nactA};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
      copy(rdmt->begin(), rdmt->end(), outv.begin());
    }
    {
      auto rdmt = rdmA_a->clone();
      // E_a'i,bj',ck
      SMITH::sort_indices<4,2,1,5,0,3, 0,1, -1,1>(rdmA_a->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //a'a'aa|a'a
      SMITH::sort_indices<4,3,0,5,1,2, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'a'ab|a'a
      
      SMITH::sort_indices<4,2,1,5,0,3, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'b'ba|b'b
      SMITH::sort_indices<4,2,1,5,0,3, 1,1, -1,1>(rdmB_b->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //b'b'bb|b'b
      
      if (!subdia) { // <M|op|N> contribution k'i'bc|j'a
        SMITH::sort_indices<5,1,2,4,3,0, 1,1, -1,1>(rdmA_a->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //a'a'aa|a'a
        SMITH::sort_indices<5,0,3,4,2,1, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //b'a'ab|a'a
        
        SMITH::sort_indices<5,1,2,4,3,0, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB); //a'b'ba|b'b
        SMITH::sort_indices<5,1,2,4,3,0, 1,1, -1,1>(rdmB_b->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB);  //b'b'bb|b'b
      }
      
      auto low = {nactA,     0,     0, nactA,     0,     0};
      auto up  = {nactT, nactA, nactA, nactT, nactA, nactA};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    }
  }

  { //CASE 4'' & 4': p27
    auto gamma_A_aa = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}); //a'a
    auto gamma_A_bb = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta});  //b'b
    auto gamma_B_aaaa = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); // a'a'aa
    auto gamma_B_abba = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); // a'b'ba
    auto gamma_B_bbbb = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  // b'b'bb

    auto rdmA_a  = make_shared<Matrix>(gamma_A_aa % gamma_B_aaaa); //a'a|a'a'aa
    auto rdmA_ab = make_shared<Matrix>(gamma_A_aa % gamma_B_abba); //a'a|a'b'ba
    auto rdmA_b  = make_shared<Matrix>(gamma_A_aa % gamma_B_bbbb); //a'a|b'b'bb
    auto rdmB_a  = make_shared<Matrix>(gamma_A_bb % gamma_B_aaaa); //b'b|a'a'aa
    auto rdmB_ab = make_shared<Matrix>(gamma_A_bb % gamma_B_abba); //b'b|a'b'ba
    auto rdmB_b  = make_shared<Matrix>(gamma_A_bb % gamma_B_bbbb); //b'b|b'b'bb
    
    {
      auto rdmt = rdmA_a->clone();
      // E_a'i',b'j',ck
      SMITH::sort_indices<3,4,2,5,0,1, 0,1, -1,1>(rdmA_a->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'a'aa
      SMITH::sort_indices<3,4,2,5,0,1, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'b'ba
      SMITH::sort_indices<2,5,3,4,0,1, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|b'a'ab
      SMITH::sort_indices<3,4,2,5,0,1, 1,1, -1,1>(rdmA_b->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|b'b'bb

      SMITH::sort_indices<3,4,2,5,0,1, 1,1, -1,1>(rdmB_a->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|a'a'aa
      SMITH::sort_indices<3,4,2,5,0,1, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|a'b'ba
      SMITH::sort_indices<2,5,3,4,0,1, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'a'ab
      SMITH::sort_indices<3,4,2,5,0,1, 1,1, -1,1>(rdmB_b->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'b'bb
      
      if (!subdia) {
        SMITH::sort_indices<4,3,5,2,1,0, 1,1, -1,1>(rdmA_a->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'a'aa
        SMITH::sort_indices<4,3,5,2,1,0, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'b'ba
        SMITH::sort_indices<5,2,4,3,1,0, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|b'a'ab
        SMITH::sort_indices<4,3,5,2,1,0, 1,1, -1,1>(rdmA_b->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|b'b'bb
        
        SMITH::sort_indices<4,3,5,2,1,0, 1,1, -1,1>(rdmB_a->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|a'a'aa
        SMITH::sort_indices<4,3,5,2,1,0, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|a'b'ba
        SMITH::sort_indices<5,2,4,3,1,0, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'a'ab
        SMITH::sort_indices<4,3,5,2,1,0, 1,1, -1,1>(rdmB_b->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'b'bb
      }
      
      auto low = {nactA, nactA, nactA, nactA,     0,     0};
      auto up  = {nactT, nactT, nactT, nactT, nactA, nactA};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
      copy(rdmt->begin(), rdmt->end(), outv.begin());
    }
    {
      auto rdmt = rdmA_a->clone();
      // E_a'i,b'j',ck'
      SMITH::sort_indices<3,1,2,4,0,5, 0,1,  1,1>(rdmA_a->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'a'aa
      SMITH::sort_indices<2,1,3,4,0,5, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|b'a'ba
                                                                                                                                   
      SMITH::sort_indices<3,1,2,5,0,4, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|a'b'ab
      SMITH::sort_indices<3,1,2,4,0,5, 1,1,  1,1>(rdmB_b->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'b'bb
      
      if (!subdia) {
        SMITH::sort_indices<4,0,5,3,1,2, 1,1,  1,1>(rdmA_a->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'a'aa
        SMITH::sort_indices<5,0,4,3,1,2, 1,1, -1,1>(rdmA_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //a'a|a'b'ab
                                                                                                                                     
        SMITH::sort_indices<4,0,5,2,0,3, 1,1, -1,1>(rdmB_ab->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'a'ba
        SMITH::sort_indices<4,0,5,3,1,2, 1,1,  1,1>(rdmB_b->data(),  rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB); //b'b|b'b'bb
      }
      
      auto low = {nactA, nactA,     0, nactA,     0, nactA};
      auto up  = {nactT, nactT, nactA, nactT, nactA, nactT};
      auto outv = make_rwview(out3->range().slice(low, up), out3->storage());
    }
  }

  //4RDM
  { //p31B
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'aaa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); //a'b'a'aba
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //b'a'b'bab
    auto gamma_A4 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'b'bbb
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});//a'a
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta}); //b'b
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a'a'aaa|a'a
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B1); // a'b'a'aba|a'a
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B1); // b'a'b'bab|a'a
    auto rdm4 = make_shared<Matrix>(gamma_A4 % gamma_B1); // b'b'b'bbb|a'a

    auto rdm5 = make_shared<Matrix>(gamma_A1 % gamma_B2); // a'a'a'aaa|b'b
    auto rdm6 = make_shared<Matrix>(gamma_A2 % gamma_B2); // a'b'a'aba|b'b
    auto rdm7 = make_shared<Matrix>(gamma_A3 % gamma_B2); // b'a'b'bab|b'b
    auto rdm8 = make_shared<Matrix>(gamma_A4 % gamma_B2); // b'b'b'bbb|b'b
    cout << "full gammas" << endl; cout.flush();

    {
      auto rdmt = rdm1->clone();
      // E_a'i',bj,ck,dl sign(+1)
      //                  a i b j c k d l                                                                                                     original order  (dcbjkl|ai)
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'aaa|a'a   (dcbjkl|ai)
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'aba|a'a   (dcbjkl|ai)
      SMITH::sort_indices<6,7,1,4,2,3,0,5, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'baa|a'a   (dbckjl|ai) : c->b, j->k
      SMITH::sort_indices<6,7,2,3,0,5,1,4, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'aab|a'a   (cdbjlk|ai) : d->c, k->l
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'bab|a'a   (dcbjkl|ai)
      SMITH::sort_indices<6,7,1,4,2,3,0,5, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'abb|a'a   (dbckjl|ai) : c->b, j->k
      SMITH::sort_indices<6,7,2,3,0,5,1,4, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'bba|a'a   (cdbjlk|ai) : d->c, k->l
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bbb|a'a   (dcbjkl|ai)
      
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'aaa|b'b   (dcbjkl|ai)
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'aba|b'b   (dcbjkl|ai)
      SMITH::sort_indices<6,7,1,4,2,3,0,5, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'baa|b'b   (dbckjl|ai) : c->b, j->k
      SMITH::sort_indices<6,7,2,3,0,5,1,4, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'aab|b'b   (cdbjlk|ai) : d->c, k->l
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'bab|b'b   (dcbjkl|ai)
      SMITH::sort_indices<6,7,1,4,2,3,0,5, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'abb|b'b   (dbckjl|ai) : c->b, j->k
      SMITH::sort_indices<6,7,2,3,0,5,1,4, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'bba|b'b   (cdbjlk|ai) : d->c, k->l
      SMITH::sort_indices<6,7,2,3,1,4,0,5, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bbb|b'b   (dcbjkl|ai)
      if (!subdia) { 
        //                  a i b j c k d l                                                                                                     original order  (lkjbcd|ia)
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'aaa|a'a   (lkjbcd|ia)
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'aba|a'a   (lkjbcd|ia)
        SMITH::sort_indices<7,6,4,1,3,2,5,0, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'baa|a'a   (ljkcbd|ia) : k->j, b->c
        SMITH::sort_indices<7,6,3,2,5,0,4,1, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'aab|a'a   (kljbdc|ia) : l->k, c->d
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'bab|a'a   (lkjbcd|ia)
        SMITH::sort_indices<7,6,4,1,3,2,5,0, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'abb|a'a   (ljkcbd|ia) : k->j, b->c
        SMITH::sort_indices<7,6,3,2,5,0,4,1, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'bba|a'a   (lkjbcd|ia) : l->k, c->d
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bbb|a'a   (lkjbcd|ia)

        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'aaa|b'b   (lkjbcd|ia)
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'aba|b'b   (lkjbcd|ia)
        SMITH::sort_indices<7,6,4,1,3,2,5,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'baa|b'b   (ljkcbd|ia) : k->j, b->c
        SMITH::sort_indices<7,6,3,2,5,0,4,1, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'aab|b'b   (kljbdc|ia) : l->k, c->d
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'bab|b'b   (lkjbcd|ia)
        SMITH::sort_indices<7,6,4,1,3,2,5,0, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'abb|b'b   (ljkcbd|ia) : k->j, b->c
        SMITH::sort_indices<7,6,3,2,5,0,4,1, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'bba|b'b   (lkjbcd|ia) : l->k, c->d
        SMITH::sort_indices<7,6,3,2,4,1,5,0, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bbb|b'b   (lkjbcd|ia)
      }
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("ai")) += *rdmt;
    }

    {
      auto rdmt = rdm1->clone();
      // E_a'i,bj,ck,dl' sign(+1)
      //                  a i b j c k d l                                                                                                     original order  (dcbijk|al)
      SMITH::sort_indices<6,3,2,4,1,5,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'aaa|a'a   (dcbijk|al)
      SMITH::sort_indices<6,3,2,5,1,4,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'aab|a'a  -(dcbikj|al) : j->k
      SMITH::sort_indices<6,3,1,4,2,5,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'aba|a'a  -(dbcijk|al) : c->b
      SMITH::sort_indices<6,4,2,3,0,5,1,7, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'abb|a'a   (cdbjik|al) : c->d, j->i

      SMITH::sort_indices<6,4,2,3,0,5,1,7, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'baa|b'b   (dcbijk|al) : c->d, j->i
      SMITH::sort_indices<6,3,1,4,2,5,0,7, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'bab|b'b  -(dbcijk|al) : c->b
      SMITH::sort_indices<6,3,2,5,1,4,0,7, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'bba|b'b   (dcbikj|al) : j->k
      SMITH::sort_indices<6,3,2,4,1,5,0,7, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bbb|b'b   (dcbijk|al)
      if (!subdia) { 
        //                  a i b j c k d l                                                                                                     original order  (kjibcd|la)
        SMITH::sort_indices<7,2,3,1,4,0,5,6, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'a'aaa|a'a   (kjibcd|la)
        SMITH::sort_indices<7,2,4,1,3,0,5,6, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'a'baa|a'a   (kjicbd|la) : c->b
        SMITH::sort_indices<7,2,3,0,4,1,5,6, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'a'aba|a'a   (jkibcd|la) : j->k
        SMITH::sort_indices<7,1,3,2,5,0,4,6, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'a'bba|a'a   (kijbdc|la) : j->i, c->d

        SMITH::sort_indices<7,1,3,2,5,0,4,6, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'b'aab|b'b   (kijbdc|la) : j->i, c->d
        SMITH::sort_indices<7,2,3,0,4,1,5,6, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'b'bab|b'b   (jkibcd|la) : j->k
        SMITH::sort_indices<7,2,4,1,3,0,5,6, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'b'abb|b'b   (kjicbd|la) : c->b
        SMITH::sort_indices<7,2,3,1,4,0,5,6, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'b'bbb|b'b   (kjibcd|la)
      }
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("al")) += *rdmt;
    }
  }

  { //p36, 41
    cout << "4RDM #2" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'aa
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); //a'b'ba
    auto gamma_A3 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'bb
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'aa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); //a'b'ba
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'bb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a'aa|a'a'aa
    auto rdm2 = make_shared<Matrix>(gamma_A2 % gamma_B1); // a'b'ba|a'a'aa
    auto rdm3 = make_shared<Matrix>(gamma_A3 % gamma_B1); // b'b'bb|a'a'aa

    auto rdm4 = make_shared<Matrix>(gamma_A1 % gamma_B2); // a'a'aa|a'b'ba
    auto rdm5 = make_shared<Matrix>(gamma_A2 % gamma_B2); // a'b'ba|a'b'ba
    auto rdm6 = make_shared<Matrix>(gamma_A3 % gamma_B2); // b'b'bb|a'b'ba

    auto rdm7 = make_shared<Matrix>(gamma_A1 % gamma_B3); // a'a'aa|b'b'bb
    auto rdm8 = make_shared<Matrix>(gamma_A2 % gamma_B3); // a'b'ba|b'b'bb
    auto rdm9 = make_shared<Matrix>(gamma_A3 % gamma_B3); // b'b'bb|b'b'bb
    cout << "full gammas" << endl; cout.flush();

    {
      auto rdmt = rdm1->clone();
      // E_a'i',b'j',ck,dl sign(+1)
      //                  a i b j c k d l                                                                                                     original order  (dckl|baij)
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|a'a'aa   (dckl|baij)
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|a'a'aa   (dckl|baij)
      SMITH::sort_indices<5,6,4,7,0,3,1,2, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|a'a'aa   (cdlk|baij) : c->d, k->l
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|a'a'aa   (dckl|baij)

      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|a'b'ba   (dckl|baij)
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|a'b'ba   (dckl|baij)
      SMITH::sort_indices<5,6,4,7,0,3,1,2, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|a'b'ba   (cdlk|baij) : c->d, k->l
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|a'b'ba   (dckl|baij)

      SMITH::sort_indices<4,7,5,6,1,2,0,3, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|b'a'ab   (dckl|baij) : a->b, i->j
      SMITH::sort_indices<4,7,5,6,1,2,0,3, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|b'a'ab   (dckl|baij) : a->b ,i->j
      SMITH::sort_indices<4,7,5,6,0,3,1,2, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|b'a'ab   (cdlk|baij) : c->d, k->l, a->b, i->j
      SMITH::sort_indices<4,7,5,6,1,2,0,3, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|b'a'ab   (dckl|baij) : a->b, i->j

      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|a'a'aa   (dckl|baij)
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|a'a'aa   (dckl|baij)
      SMITH::sort_indices<5,6,4,7,0,3,1,2, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|a'a'aa   (cdlk|baij) : c->d, k->l
      SMITH::sort_indices<5,6,4,7,1,2,0,3, 1,1,  1,1>(rdm9->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|a'a'aa   (dckl|baij)
      if (!subdia) { 
        //                  a i b j c k d l                                                                                                     original order  (lkcd|jiab)
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|a'a'aa   (lkcd|jiab)
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|a'a'aa   (lkcd|jiab) 
        SMITH::sort_indices<6,5,7,4,3,0,2,1, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|a'a'aa   (kldc|jiab) : k->l, c->d
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|a'a'aa   (lkcd|jiab)

        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|a'b'ba   (lkcd|jiab)
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|a'b'ba   (lkcd|jiab) 
        SMITH::sort_indices<6,5,7,4,3,0,2,1, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|a'b'ba   (kldc|jiab) : k->l, c->d
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|a'b'ba   (lkcd|jiab)

        SMITH::sort_indices<7,4,6,5,2,1,3,0, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|b'a'ab   (lkcd|ijba) : i->j, a->b
        SMITH::sort_indices<7,4,6,5,2,1,3,0, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|b'a'ab   (lkcd|ijba) : i->j, a->b
        SMITH::sort_indices<7,4,6,5,3,0,2,1, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|b'a'ab   (kldc|ijba) : k->l, c->d, i->j, a->b
        SMITH::sort_indices<7,4,6,5,2,1,3,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|b'a'ab   (lkcd|ijba) : i->j, a->b

        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'a'aa|b'b'bb   (lkcd|jiab)
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // a'b'ba|b'b'bb   (lkcd|jiab) 
        SMITH::sort_indices<6,5,7,4,3,0,2,1, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'a'ab|b'b'bb   (kldc|jiab) : k->l, c->d
        SMITH::sort_indices<6,5,7,4,2,1,3,0, 1,1,  1,1>(rdm9->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactB, nactB, nactB, nactB); // b'b'bb|b'b'bb   (lkcd|jiab)
      }
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("baij")) += *rdmt;
    }

    { //p36B, 41B
      auto rdmt = rdm1->clone();
      // E_a'i,b'j,ck',dl' sign(+1)
      //                  a i b j c k d l                                                                                                     original order  (dcij|bakl)
      SMITH::sort_indices<5,2,4,3,1,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'aa|a'a'aa   (dcij|bakl)
      SMITH::sort_indices<5,2,4,3,1,6,0,7, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'ba|a'b'ba   (dcij|bakl)
      SMITH::sort_indices<4,3,5,2,0,7,1,6, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'ab|b'a'ab   (cdji|ablk) : c->d, i->j, a->b, k->l
      SMITH::sort_indices<4,3,5,2,1,6,0,7, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'ab|b'a'ba   (dcji|abkl) : i->j, b->a
      SMITH::sort_indices<5,2,4,3,0,7,1,6, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'ba|a'b'ab   (cdij|balk) : d->c, k->l
      SMITH::sort_indices<5,2,4,3,1,6,0,7, 1,1,  1,1>(rdm9->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'bb|b'b'bb   (dcij|bakl)
      if (!subdia) { 
        //                  a i b j c k d l                                                                                                     original order  (jicd|lkab)
        SMITH::sort_indices<6,1,7,0,2,5,3,4, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'a'aa|a'a'aa   (jicd|lkab)
        SMITH::sort_indices<6,1,7,0,2,5,3,4, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'ba|a'b'ba   (jicd|lkab)
        SMITH::sort_indices<7,0,6,1,3,4,2,5, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'ab|b'a'ab   (ijdc|klba) : i->j, c->d, k->l, a->b
        SMITH::sort_indices<6,1,7,0,3,4,2,5, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // a'b'ab|b'a'ba   (jidc|klab) : c->d, k->l
        SMITH::sort_indices<7,0,6,1,2,5,3,4, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'a'ba|a'b'ab   (ijcd|lkba) : i->j, a->b
        SMITH::sort_indices<6,1,7,0,2,5,3,4, 1,1,  1,1>(rdm9->data(), rdmt->data(), nactA, nactA, nactA, nactA, nactA, nactA, nactB, nactB); // b'b'bb|b'b'bb   (jicd|lkab)
      }
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("bakl")) += *rdmt;
    }
  }

  { //p39
    cout << "4RDM #1" << endl;
    auto gamma_A1 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha});//a'a
    auto gamma_A2 = worktensor_->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta}); //b'b
    auto gamma_B1 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}); //a'a'a'aaa
    auto gamma_B2 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha}); //a'b'a'aba
    auto gamma_B3 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateAlpha, GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateBeta});  //b'a'b'bab
    auto gamma_B4 = gammatensor_[1]->get_block_as_matview(B, Bp, {GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::CreateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta,  GammaSQ::AnnihilateBeta});  //b'b'b'bbb
    cout << "partial gammas" << endl; cout.flush();

    auto rdm1 = make_shared<Matrix>(gamma_A1 % gamma_B1); // a'a|a'a'a'aaa
    auto rdm2 = make_shared<Matrix>(gamma_A1 % gamma_B2); // a'a|a'b'a'aba
    auto rdm3 = make_shared<Matrix>(gamma_A1 % gamma_B3); // a'a|b'a'b'bab
    auto rdm4 = make_shared<Matrix>(gamma_A1 % gamma_B4); // a'a|b'b'b'bbb
                                                                 
    auto rdm5 = make_shared<Matrix>(gamma_A2 % gamma_B1); // b'b|a'a'a'aaa
    auto rdm6 = make_shared<Matrix>(gamma_A2 % gamma_B2); // b'b|a'b'a'aba
    auto rdm7 = make_shared<Matrix>(gamma_A2 % gamma_B3); // b'b|b'a'b'bab
    auto rdm8 = make_shared<Matrix>(gamma_A2 % gamma_B4); // b'b|b'b'b'bbb
    cout << "full gammas" << endl; cout.flush();

    {
      auto rdmt = rdm1->clone();
      // E_a'i',bj,ck,dl sign(-1)
      //                  a i b j c k d l                                                                                                     original order  (dl|cbaijk)
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 0,1, -1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'a'aaa   (dl|cbaijk)
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'a'aba   (dl|cbaijk)
      SMITH::sort_indices<3,6,4,5,2,7,0,1, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'b'baa   (dl|cabjik) : b->a, j->i
      SMITH::sort_indices<4,5,2,7,3,6,0,1, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'a'a'aab   (dl|bcaikj) : b->c, j->k
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'a'b'bab   (dl|cbaijk)
      SMITH::sort_indices<3,6,4,5,2,7,0,1, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'b'a'abb   (dl|cabjik) : b->a, j->i
      SMITH::sort_indices<4,5,2,7,3,6,0,1, 1,1, -1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'b'bba   (dl|bcaikj) : b->c, j->k
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'b'b'bbb   (dl|cbaijk)

      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'a'a'aaa   (dl|cbaijk)
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'b'a'aba   (dl|cbaijk)
      SMITH::sort_indices<3,6,4,5,2,7,0,1, 1,1, -1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'a'b'baa   (dl|cabjik) : b->a, j->i
      SMITH::sort_indices<4,5,2,7,3,6,0,1, 1,1, -1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'a'aab   (dl|bcaikj) : b->c, j->k
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'b'bab   (dl|cbaijk)
      SMITH::sort_indices<3,6,4,5,2,7,0,1, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'a'abb   (dl|cabjik) : b->a, j->i
      SMITH::sort_indices<4,5,2,7,3,6,0,1, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'b'b'bba   (dl|bcaikj) : b->c, j->k
      SMITH::sort_indices<4,5,3,6,2,7,0,1, 1,1, -1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'b'bbb   (dl|cbaijk)
      if (!subdia) { 
        //                  a i b j c k d l                                                                                                     original order  (ld|kjiabc)
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'a'aaa   (ld|kjiabc)
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'a'aba   (ld|kjiabc)
        SMITH::sort_indices<6,3,5,4,7,2,1,0, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'b'baa   (ld|kijbac) : j->i, b->a
        SMITH::sort_indices<5,4,7,2,6,3,1,0, 1,1,  1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'a'a'aab   (ld|jkiacb) : j->k, b->c
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'a'b'bab   (ld|kjiabc)
        SMITH::sort_indices<6,3,5,4,7,2,1,0, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'b'a'abb   (ld|kijbac) : j->i, b->a
        SMITH::sort_indices<5,4,7,2,6,3,1,0, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'b'bba   (ld|jkiacb) : j->k, b->c
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm4->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'b'b'bbb   (ld|kjiabc)

        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm5->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'a'a'aaa   (ld|kjiabc)
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'b'a'aba   (ld|kjiabc)
        SMITH::sort_indices<6,3,5,4,7,2,1,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'a'b'baa   (ld|kijbac) : j->i, b->a
        SMITH::sort_indices<5,4,7,2,6,3,1,0, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'a'aab   (ld|jkiacb) : j->k, b->c
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'b'bab   (ld|kjiabc)
        SMITH::sort_indices<6,3,5,4,7,2,1,0, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'a'abb   (ld|kijbac) : j->i, b->a
        SMITH::sort_indices<5,4,7,2,6,3,1,0, 1,1,  1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'b'b'bba   (ld|jkiacb) : j->k, b->c
        SMITH::sort_indices<5,4,6,3,7,2,1,0, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'b'bbb   (ld|kjiabc)
      }
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("cbaijk")) += *rdmt;
    }

    {
      auto rdmt = rdm1->clone();
      // E_a'i,bj,ck,dl' sign(-1)
      //                  a i b j c k d l                                                                                                     original order  (di|cbajkl)
      SMITH::sort_indices<4,1,3,5,2,6,0,7, 0,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'a'aaa   (di|cbajkl)
      SMITH::sort_indices<4,1,3,6,2,5,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'a'baa  -(di|cbakjl) : k->j
      SMITH::sort_indices<4,1,2,5,3,6,0,7, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'a'a'aba  -(di|bcajkl) : c->b
      SMITH::sort_indices<3,1,4,5,2,7,0,6, 1,1,  1,1>(rdm3->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|b'b'a'bba   (di|cabjlk) : a->b, l->k
      SMITH::sort_indices<3,1,4,5,2,7,0,6, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'a|a'a'b'aab   (di|cabjlk) : a->b, l->k
      SMITH::sort_indices<4,1,3,6,2,5,0,7, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'b'abb  -(di|cbakjl) : k->j
      SMITH::sort_indices<4,1,2,5,3,6,0,7, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|a'b'b'bab  -(di|bcajkl) : c->b
      SMITH::sort_indices<4,1,3,5,2,6,0,7, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'b'bbb   (di|cbajkl)
      if (!subdia) { 
        //                  a i b j c k d l                                                                                                     original order  (id|lkjabc)
        SMITH::sort_indices<5,0,6,4,7,3,1,2, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'a'aaa:  (id|lkjabc)
        SMITH::sort_indices<5,0,6,3,7,4,1,2, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'a'b'aba: -(id|ljkabc) : j->k
        SMITH::sort_indices<5,0,7,4,6,3,1,2, 1,1, -1,1>(rdm2->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'a'aab: -(id|lkjacb) : c->b
        SMITH::sort_indices<6,0,5,4,7,2,1,3, 1,1,  1,1>(rdm1->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // a'a|a'b'b'abb:  (id|kljbac) : l->k, a->b

        SMITH::sort_indices<6,0,5,4,7,2,1,3, 1,1,  1,1>(rdm6->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'a'baa:  (id|kljbac) : l->k, a->b
        SMITH::sort_indices<5,0,7,4,6,3,1,2, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'a'b'bba: -(id|lkjacb) : c->b
        SMITH::sort_indices<5,0,6,3,7,4,1,2, 1,1, -1,1>(rdm7->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'a'bab: -(id|ljkabc) : j->k
        SMITH::sort_indices<5,0,6,4,7,3,1,2, 1,1,  1,1>(rdm8->data(), rdmt->data(), nactA, nactA, nactB, nactB, nactB, nactB, nactB, nactB); // b'b|b'b'b'bbb:  (id|lkjabc)
      }
      cout << "rearranged" << endl; cout.flush();
      
      *fourrdm_.at(string("cbajkl")) += *rdmt;
    }
  }
  return make_tuple(out3,out4);
}

//***************************************************************************************************************
void
ASD_base::initialize_4RDM() {
//***************************************************************************************************************
  cout << "Initialize 4RDM" << endl;
  const int nactA = dimer_->embedded_refs().first->nact();
  const int nactB = dimer_->embedded_refs().second->nact();

  //E_ai,bj,ck,dl = sum d+c+b+a+ ijkl
  //#of B indices = 0
  fourrdm_.emplace(string("monomerA"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA*nactA*nactA*nactA, 1)); //monomer A
  //# = 1
  fourrdm_.emplace(string("l"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA*nactA*nactA, nactB )); // l
  //# = 2
  fourrdm_.emplace(string("kl"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA*nactA, nactB*nactB )); // kl
  fourrdm_.emplace(string("al"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA*nactA, nactB*nactB )); // al
  fourrdm_.emplace(string("ai"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA*nactA, nactB*nactB )); // ai
  //# = 3
  fourrdm_.emplace(string("jkl"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA, nactB*nactB*nactB )); // jkl
  fourrdm_.emplace(string("akl"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA, nactB*nactB*nactB )); // akl
  fourrdm_.emplace(string("aij"), make_shared<Matrix>(nactA*nactA*nactA*nactA*nactA, nactB*nactB*nactB )); // aij
  //# = 4
  fourrdm_.emplace(string("ijkl"), make_shared<Matrix>(nactA*nactA*nactA*nactA, nactB*nactB*nactB*nactB )); // ijkl
  fourrdm_.emplace(string("ajkl"), make_shared<Matrix>(nactA*nactA*nactA*nactA, nactB*nactB*nactB*nactB )); // ajkl
  fourrdm_.emplace(string("aijk"), make_shared<Matrix>(nactA*nactA*nactA*nactA, nactB*nactB*nactB*nactB )); // aijk
  fourrdm_.emplace(string("bakl"), make_shared<Matrix>(nactA*nactA*nactA*nactA, nactB*nactB*nactB*nactB )); // bakl
  fourrdm_.emplace(string("baij"), make_shared<Matrix>(nactA*nactA*nactA*nactA, nactB*nactB*nactB*nactB )); // baij
  //# = 5
  fourrdm_.emplace(string("aijkl"), make_shared<Matrix>(nactA*nactA*nactA, nactB*nactB*nactB*nactB*nactB )); // aijkl
  fourrdm_.emplace(string("bajkl"), make_shared<Matrix>(nactA*nactA*nactA, nactB*nactB*nactB*nactB*nactB )); // bajkl
  fourrdm_.emplace(string("baijk"), make_shared<Matrix>(nactA*nactA*nactA, nactB*nactB*nactB*nactB*nactB )); // baijk
  //# = 6
  fourrdm_.emplace(string("baijkl"), make_shared<Matrix>(nactA*nactA, nactB*nactB*nactB*nactB*nactB*nactB )); // baijkl
  fourrdm_.emplace(string("cbajkl"), make_shared<Matrix>(nactA*nactA, nactB*nactB*nactB*nactB*nactB*nactB )); // cbajkl
  fourrdm_.emplace(string("cbaijk"), make_shared<Matrix>(nactA*nactA, nactB*nactB*nactB*nactB*nactB*nactB )); // cbaijk
  //# = 7
  fourrdm_.emplace(string("cbaijkl"), make_shared<Matrix>(nactA, nactB*nactB*nactB*nactB*nactB*nactB*nactB )); // cbaijkl
  //# = 8
  fourrdm_.emplace(string("monomerB"), make_shared<Matrix>(1, nactB*nactB*nactB*nactB*nactB*nactB*nactB*nactB )); // monomer B

  cout << "# of nonredundnat Dimer 4RDM = " << fourrdm_.size() << endl;

}

