//
// Newint - Parallel electron correlation program.
// Filename: libint.h
// Copyright (C) 2012 Toru Shiozaki
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


#ifdef LIBINT_INTERFACE

#ifndef __SRC_RYSINT_LIBINT_H
#define __SRC_RYSINT_LIBINT_H

#include <src/rysint/rysint.h>

class Libint : public RysInt {
  protected:
    void root_weight(int){};
    void compute_ssss(double){};

  public:
    Libint(const std::array<std::shared_ptr<const Shell>,4>&);
    ~Libint() {};

    void compute() {};

};

#endif

#endif
