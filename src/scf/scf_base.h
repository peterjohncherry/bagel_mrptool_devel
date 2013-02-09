//
// BAGEL - Parallel electron correlation program.
// Filename: scf_base.h
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
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

#ifndef __scf_scf_base_h
#define __scf_scf_base_h

#include <memory>
#include <string>
#include <map>
#include <src/wfn/geometry.h>
#include <src/scf/overlap.h>
#include <src/scf/hcore.h>
#include <src/scf/tildex.h>
#include <src/scf/fock.h>
#include <src/scf/coeff.h>
#include <src/wfn/reference.h>

namespace bagel {

class SCF_base {
  protected:
    const std::multimap<std::string, std::string> idata_;
    const std::shared_ptr<const Geometry> geom_;
    std::shared_ptr<TildeX> tildex_;
    std::shared_ptr<const Overlap> overlap_;
    std::shared_ptr<const Hcore> hcore_;
    std::shared_ptr<const Coeff> coeff_;

    int max_iter_;
    int diis_start_;
    int diis_size_;
    double thresh_overlap_;
    double thresh_scf_;

    std::vector<double> schwarz_;
    void init_schwarz();

    std::unique_ptr<double[]> eig_;
    double energy_;

    int nocc_;
    int noccB_;

    const std::string indent = "  ";

  public:
    SCF_base(const std::multimap<std::string, std::string>& idata_, const std::shared_ptr<const Geometry>,
             const std::shared_ptr<const Reference>, const bool need_schwarz = false);
    ~SCF_base() {};

    virtual void compute() = 0;

    const std::shared_ptr<const Geometry> geom() const { return geom_; };

    const std::shared_ptr<const Coeff> coeff() const { return coeff_; };
    void set_coeff(const std::shared_ptr<Coeff> o) { coeff_ = o; };

    const std::shared_ptr<const Hcore> hcore() const { return hcore_; };
    const std::vector<double>& schwarz() const { return schwarz_; };

    int nocc() const { return nocc_; };
    int noccB() const { return noccB_; };
    double energy() const { return energy_; };

    virtual std::shared_ptr<Reference> conv_to_ref() const = 0;

    double* eig() { return eig_.get(); };
};

}

#endif
