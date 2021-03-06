#ifndef __SRC_PROP_PROPTOOL_TASKTRANSLATOR_SYSTEMCOMPUTER_H
#define __SRC_PROP_PROPTOOL_TASKTRANSLATOR_SYSTEMCOMPUTER_H

#include <src/smith/tensor.h>
#include <src/smith/multitensor.h>
#include <src/smith/indexrange.h>
#include <src/prop/proptool/integrals/moint_computer.h>
#include <src/prop/proptool/algebraic_manipulator/system_info.h>
#include <src/prop/proptool/task_translator/equation_computer.h>
#include <src/prop/proptool/task_translator/equation_computer_linearRM.h>

namespace bagel {
namespace  System_Computer { 
template<class DataType> 
class System_Computer {
  private: 
    // Tensor operators in MO basis and maps 
    std::shared_ptr<SMITH::Tensor_<DataType>> h1_; 
    std::shared_ptr<SMITH::Tensor_<DataType>> f1_; 
    std::shared_ptr<SMITH::Tensor_<DataType>> v2_; 

    std::shared_ptr<std::map< std::string, std::shared_ptr<SMITH::Tensor_<DataType>>>> civec_data_map_;
    std::shared_ptr<std::map< std::string, std::shared_ptr<SMITH::Tensor_<DataType>>>> sigma_data_map_;
    std::shared_ptr<std::map< std::string, std::shared_ptr<SMITH::Tensor_<DataType>>>> gamma_data_map_;
    std::shared_ptr<std::map< std::string, std::shared_ptr<SMITH::Tensor_<DataType>>>> tensop_data_map_;

    std::shared_ptr<System_Info<DataType>> system_info_;

#ifdef __PROPTOOL_RDM_UNIT_TEST
    RDM_Computer::RDM_Computer rdm_computer_;
#endif 

  public:
    System_Computer( std::shared_ptr<System_Info<DataType>> system_info ); 
   ~System_Computer(){};

    void build_equation_computer(std::string equation_name );
    void build_tensop( std::string tensop_name ) ;
 
    std::shared_ptr<std::map< std::string, std::shared_ptr<SMITH::IndexRange>>> range_conversion_map_ ;

    // TODO these should be protected 
    std::shared_ptr<MOInt_Computer<DataType>> moint_computer_;
    std::shared_ptr<Gamma_Computer::Gamma_Computer<DataType>> gamma_computer_;

};
}
}
#endif
