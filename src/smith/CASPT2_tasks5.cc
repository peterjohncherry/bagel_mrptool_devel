//
// BAGEL - Parallel electron correlation program.
// Filename: CASPT2_tasks5.cc
// Copyright (C) 2014 Shiozaki group
//
// Author: Shiozaki group <shiozaki@northwestern.edu>
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

#include <bagel_config.h>
#ifdef COMPILE_SMITH

#include <src/smith/CASPT2_tasks5.h>

using namespace std;
using namespace bagel;
using namespace bagel::SMITH;
using namespace bagel::SMITH::CASPT2;

void Task200::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a2, c1, a4, c3") += (*ta1_)("x1, a2, c1, a4") * (*ta2_)("c3, x1");
}

void Task201::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("c3, x1") += (*ta1_)("x1, x0") * (*ta2_)("c3, x0");
}

void Task202::compute_() {
  (*ta0_)("c2, a3, x0, a1") += (*ta1_)("a3, c2, x0, a1");
}

void Task203::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("x1, a1") * (*ta2_)("a3, c2, x1, x0");
}

void Task204::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a3, c2, x1, x0") += (*ta1_)("x3, x2, x1, x0") * (*ta2_)("x3, a3, c2, x2");
}

void Task205::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x3, a3, c2, x2") += (*ta1_)("x3, a3, c2, x2") * (-1)
     + (*ta1_)("c2, a3, x3, x2") * 2;
}

void Task206::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("x1, a3") * (*ta2_)("a1, c2, x0, x1");
}

void Task207::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, c2, x0, x1") += (*ta1_)("x3, x0, x1, x2") * (*ta2_)("x3, a1, c2, x2");
}

void Task208::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, c2, x0, x1") += (*ta1_)("x3, x2, x1, x0") * (*ta2_)("c2, a1, x3, x2") * (-1);
}

void Task209::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("c2, a1") * (*ta2_)("a3, x0");
}

void Task210::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a3, x0") += (*ta1_)("x3, x0, x2, x1") * (*ta2_)("x3, a3, x2, x1") * (-1);
}

void Task211::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("c2, a3") * (*ta2_)("a1, x0");
}

void Task212::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, x0") += (*ta1_)("x3, x0, x2, x1") * (*ta2_)("x3, a1, x2, x1") * 2;
}

void Task213::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("c2, a3, c4, a1") * (*ta2_)("c4, x0");
}

void Task214::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("c4, x0") += (*ta1_)("x1, x0") * (*ta2_)("x1, c4") * (-4);
}

void Task215::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("c2, a1, c4, a3") * (*ta2_)("c4, x0");
}

void Task216::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("c4, x0") += (*ta1_)("x1, x0") * (*ta2_)("x1, c4") * 2;
}

void Task217::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("x3, x0") * (*ta2_)("x3, a3, c2, a1");
}

void Task218::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x3, a3, c2, a1") += (*ta1_)("x3, a3, c2, a1") * (-1)
     + (*ta1_)("x3, a1, c2, a3") * 2;
}

void Task219::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("x1, x0") * (*ta2_)("c2, x1, a3, a1");
}

void Task220::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a3, c2, a1") * e0_
     + (*ta1_)("x1, a1, c2, a3") * e0_ * (-2)
     + (*ta2_)("x1, a3, c2, a1") * (-1)
     + (*ta2_)("x1, a1, c2, a3") * 2;
}

void Task221::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a3, c4, a1") * (*ta2_)("c2, c4");
}

void Task222::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a1, c4, a3") * (*ta2_)("c2, c4") * (-2);
}

void Task223::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a4, c2, a3") * (*ta2_)("a4, a1") * 2;
}

void Task224::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a3, c2, a4") * (*ta2_)("a4, a1") * (-1);
}

void Task225::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a4, c2, a1") * (*ta2_)("a4, a3") * (-1);
}

void Task226::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c2, x1, a3, a1") += (*ta1_)("x1, a1, c2, a4") * (*ta2_)("a4, a3") * 2;
}

void Task227::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a3, c2, x0, a1") += (*ta1_)("c2, x1") * (*ta2_)("a1, a3, x0, x1");
}

void Task228::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, a3, x0, x1") += (*ta1_)("x3, x0, x2, x1") * (*ta2_)("x3, a1, x2, a3") * (-2);
}

void Task229::compute_() {
  (*ta0_)("x1, a2, x0, a1") += (*ta1_)("a1, x0, x1, a2") + (*ta1_)("a2, x1, x0, a1");
}

void Task230::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("a1, x0, x1, a2") += (*ta1_)("x2, a2") * (*ta2_)("a1, x0, x2, x1");
}

void Task231::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, x0, x2, x1") += (*ta1_)("x5, x0, x4, x3, x2, x1") * (*ta2_)("x5, a1, x4, x3");
}

void Task232::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, x0, x1, a2") += (*ta1_)("x3, x0, x2, x1") * (*ta2_)("x2, x3, a1, a2");
}

void Task233::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x2, x3, a1, a2") += (*ta1_)("x3, a1, c3, a2") * (*ta2_)("x2, c3") * (-1);
}

void Task234::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x2, x3, a1, a2") += (*ta1_)("x3, a1, x2, a3") * (*ta2_)("a3, a2") * 2;
}

void Task235::compute_() {
  (*ta0_)("x1, a2, x0, a1") += (*ta1_)("a1, a2, x0, x1");
}

void Task236::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, a2, x0, x1") += (*ta1_)("x5, x0, x4, x1") * (*ta2_)("x5, a1, x4, a2") * 2;
}

void Task237::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("a1, a2, x0, x1") += (*ta1_)("x3, x0, x2, x1") * (*ta2_)("x3, a1, x2, a2");
}

void Task238::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x3, a1, x2, a2") += (*ta1_)("x3, a1, x2, a2") * e0_ * (-2)
     + (*ta2_)("x3, a1, x2, a2");
}

void Task239::compute_() {
  (*ta0_)("c3, a4, c1, a2") += (*ta1_)("c1, a4, c3, a2");
}

void Task240::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("c1, a4, c3, a2") += (*ta1_)("c1, a4, c3, a2") * (-2)
     + (*ta1_)("c1, a2, c3, a4") * 4;
}

void Task241::compute_() {
  ta1_->init();
  target_ += (*ta1_)("x0, x3, x1, x2").dot((*ta2_)("x1, x0, x3, x2")).get();
}

void Task242::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x1, x0, x3, x2") += (*ta1_)("c1, x3, c2, x2") * (*ta2_)("c1, x0, c2, x1");
}

void Task243::compute_() {
  target_ += (*ta1_)("x0, x1, c1, x2").dot((*ta2_)("c1, x2, x1, x0")).get();
}

void Task244::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("c1, x2, x1, x0") += (*ta1_)("x2, x5, x4, x3, x1, x0") * (*ta2_)("c1, x5, x4, x3") * 0.5;
}

void Task245::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  ta1_->init();
  (*ta0_)("c1, x2, x1, x0") += (*ta1_)("x5, x4, x2, x3, x1, x0") * (*ta2_)("x5, x4, c1, x3") * 0.5;
}

void Task246::compute_() {
  ta1_->init();
  target_ += (*ta1_)("x0, x1").dot((*ta2_)("x0, x1")).get();
}

void Task247::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x0, x1") += (*ta1_)("c3, x1, c1, a2") * (*ta2_)("c1, a2, c3, x0") * 2;
}

void Task248::compute_() {
  if (!ta0_->initialized())
    ta0_->fill_local(0.0);
  (*ta0_)("x0, x1") += (*ta1_)("c1, x1, c3, a2") * (*ta2_)("c1, a2, c3, x0") * (-1);
}

void Task249::compute_() {
  ta1_->init();
  target_ += (*ta1_)("x3, x2, x1, x0").dot((*ta2_)("x1, x0, x3, x2")).get();
}

#endif
