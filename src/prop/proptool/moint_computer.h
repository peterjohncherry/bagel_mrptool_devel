#ifndef __SRC_PROPTOOL_MOINT_COMPUTER_H
#define __SRC_PROPTOOL_MOINT_COMPUTER_H


// Should presumably be expanded. Written like this to keep things seperated. 
// Should be done so all calls to members of info_ from K2ext and MOFock just refer
// to members of MO_integrator defined from info_. So K2ext and MOFock might work best
// as sub classes of MO_Integrator, but I think this might be a headache with the templates, 
// so I will leave it for now.

#include <stddef.h>
#include <memory>
#include <stdexcept>
#include <complex>
#include <src/prop/proptool/moint_init.h>
#include <src/smith/tensor.h>
#include <src/smith/indexrange.h>

namespace bagel {
  template<typename DataType>
  class MOInt_Computer {
    private :
      using MatType = typename std::conditional<std::is_same<DataType,double>::value,Matrix,ZMatrix>::type;

      using Tensor = typename std::conditional<std::is_same<DataType,double>::value,
                                               SMITH::Tensor_<double>,SMITH::Tensor_<std::complex<double>>>::type;

//    using IndexRange = class SMITH::IndexRange;
      std::shared_ptr<const MOInt_Init<DataType>> info_;
      std::shared_ptr<const MatType> coeffs_;
    public :
      MOInt_Computer( std::shared_ptr<const MOInt_Init<DataType>> r, std::shared_ptr<const MatType> c):
                      info_(r), coeffs_(c) {}
      ~MOInt_Computer(){};

      //note, this does not have the diagonal component
      std::shared_ptr<SMITH::Tensor_<DataType>> get_v2( const std::vector<SMITH::IndexRange>& blocks );
      std::shared_ptr<SMITH::Tensor_<DataType>> get_v2( const std::vector<std::string>& blocks );

      //is the core fock minus diagonal component from above
      std::shared_ptr<SMITH::Tensor_<DataType>> get_h1( const std::vector<std::string>& blocks );
      std::shared_ptr<SMITH::Tensor_<DataType>> get_h1( const std::vector<SMITH::IndexRange>& blocks );
  };
}
#endif
