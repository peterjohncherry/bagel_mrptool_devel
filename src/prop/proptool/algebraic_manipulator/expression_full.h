#ifndef __SRC_PROP_PROPTOOL_ALGEBRAICMANIPULATOR_EXPRESSION_FULL_H
#define __SRC_PROP_PROPTOOL_ALGEBRAICMANIPULATOR_EXPRESSION_FULL_H
#include <src/prop/proptool/algebraic_manipulator/expression.h>

template<typename DataType>
class Expression_Full : public Expression<DataType>   {

   public :

     using Expression<DataType>::braket_list_;
     using Expression<DataType>::states_info_;
     using Expression<DataType>::name_;   
     using Expression<DataType>::type_;
     using Expression<DataType>::MT_map_;
     using Expression<DataType>::CTP_map_;
     using Expression<DataType>::ACompute_map_;
     using Expression<DataType>::gamma_info_map_;
     using Expression<DataType>::required_blocks_;

    // key: name of gamma (or sigma)
    // result :  name to a map containing the names of all A-tensors with which it must be contracted, and the relevant factors.
    std::shared_ptr<std::map<std::string, std::shared_ptr< std::map<std::string, std::shared_ptr<AContribInfo_Base>> >>> G_to_A_map_; //TODO should be private
     
     Expression_Full( std::shared_ptr<std::vector<std::shared_ptr<BraKet_Base>>> braket_list,
                      std::shared_ptr<StatesInfo_Base> states_info,
                      std::shared_ptr<std::map< std::string, std::shared_ptr<TensOp_Base>>>  MT_map,
                      std::shared_ptr<std::map< std::string, std::shared_ptr<CtrTensorPart_Base> >> CTP_map,
                      std::shared_ptr<std::map< std::string, std::shared_ptr<std::vector<std::shared_ptr<CtrOp_base>> >>> ACompute_map,
                      std::shared_ptr<std::map< std::string, std::shared_ptr<GammaInfo_Base> > > gamma_info_map,
                      std::string expression_type ) :
                      Expression<DataType>( braket_list, states_info, MT_map, CTP_map, ACompute_map, gamma_info_map, expression_type ){
                        G_to_A_map_ = std::make_shared<std::map< std::string, std::shared_ptr< std::map<std::string, std::shared_ptr<AContribInfo_Base>> >>>();
                      };

     ~Expression_Full(){};
             
     void generate_algebraic_task_list(); 
     void get_gamma_Atensor_contraction_list();

     std::shared_ptr<std::map<std::string, std::shared_ptr< std::map<std::string, std::shared_ptr<AContribInfo_Base>> >>> G_to_A_map(){ return G_to_A_map_; }  

};

#endif
