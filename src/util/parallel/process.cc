//
// BAGEL - Parallel electron correlation program.
// Filename: process.cc
// Copyright (C) 2012 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
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

#include <cassert>
#include <src/util/parallel/process.h>
#include <src/util/parallel/mpi_interface.h>

using namespace std;
using namespace bagel;

Process::Process() : print_level_(3), muted_(false) {
  int rank;
#ifdef HAVE_MPI_H
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
  rank = 0;
#endif
  if (rank != 0) {
    cout_orig = cout.rdbuf();
    cout.rdbuf(ss_.rdbuf());
    muted_ = true;
  }
}

Process::~Process() {
  if (muted_)
    cout.rdbuf(cout_orig);
}

void Process::cout_on() {
  if (mpi__->rank() != 0) {
    assert(muted_);
    cout.rdbuf(cout_orig);
    muted_ = false;
  }
}


void Process::cout_off() {
  if (mpi__->rank() != 0) {
    assert(!muted_);
    cout.rdbuf(ss_.rdbuf());
    muted_ = true;
  }
}
