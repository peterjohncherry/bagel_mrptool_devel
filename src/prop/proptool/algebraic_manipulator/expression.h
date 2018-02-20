#ifndef __SRC_PROP_PROPTOOL_ALGEBRAICMANIPULATOR_EXPRESSION_H
#define __SRC_PROP_PROPTOOL_ALGEBRAICMANIPULATOR_EXPRESSION_H
#include <src/prop/proptool/proputils.h>
#include <src/prop/proptool/algebraic_manipulator/braket.h>
#include <src/prop/proptool/algebraic_manipulator/tensop.h>

template<typename DataType>
class Expression {

      public :
        std::string name_;   

        //List of terms, currently a list of BraKets...
        std::shared_ptr<std::vector< BraKet<DataType>>> braket_list_;

        //information about target states of the system
        std::shared_ptr<StatesInfo<DataType>> states_info_;

        // key : name of multitensor info
        // result : the info
        std::shared_ptr< std::map< std::string, std::shared_ptr< TensOp_Base > >> MT_map_;
        
        // key : name of block of contracted and uncontracted tensor/multitensor info objects
        // result : the info object
        std::shared_ptr< std::map< std::string, std::shared_ptr< CtrTensorPart_Base > >> CTP_map_;
        
        // key:  Name of a contracted tensor or contracted combintation fo tensors
        // result : List of operations which need to be performed to obtain it
        std::shared_ptr<std::map<std::string,  std::shared_ptr<std::vector< std::shared_ptr<CtrOp_base> >> >> ACompute_map_;
        
        // key : name of gammas
        // result : gamma_info, also contains sigma info,  includes lists of gammas which must be calculated first.
        std::shared_ptr<std::map<std::string, std::shared_ptr<GammaInfo> > > gamma_info_map_;
        
        // key: name of gamma (or sigma)
        // result :  name to a map containing the names of all A-tensors with which it must be contracted, and the relevant factors.
        std::shared_ptr<std::map<std::string, std::shared_ptr< std::map<std::string, AContribInfo > >>> G_to_A_map_; //TODO should be private
      
        // names of the range blocks of the original input tensors which are needed to compute this expression
        std::shared_ptr<std::set<std::string>> required_blocks_;

        Expression( std::shared_ptr<std::vector<BraKet<DataType>>> braket_list,
                    std::shared_ptr<StatesInfo<DataType>> states_info,
                    std::shared_ptr<std::map< std::string, std::shared_ptr<TensOp_Base>>>  MT_map,
                    std::shared_ptr<std::map< std::string, std::shared_ptr<CtrTensorPart_Base> >>            CTP_map,
                    std::shared_ptr<std::map< std::string, std::shared_ptr<std::vector<std::shared_ptr<CtrOp_base>> >>> ACompute_map,
                    std::shared_ptr<std::map< std::string, std::shared_ptr<GammaInfo> > >                         gamma_info_map );
        ~Expression(){};
        
        void get_gamma_Atensor_contraction_list();
        
        void necessary_tensor_blocks();
   
        std::string name() {return name_; }
        std::shared_ptr<std::vector< BraKet<DataType>>> braket_list(){ return  braket_list_;}
        std::shared_ptr<StatesInfo<DataType>> states_info(){ return  states_info_;}
        std::shared_ptr< std::map< std::string, std::shared_ptr< TensOp_Base >>> MT_map(){ return  MT_map_;}
        std::shared_ptr< std::map< std::string, std::shared_ptr< CtrTensorPart_Base > >> CTP_map(){ return  CTP_map_;}
        std::shared_ptr<std::map<std::string,  std::shared_ptr<std::vector< std::shared_ptr<CtrOp_base> >> >> ACompute_map(){ return  ACompute_map_;}
        std::shared_ptr<std::map<std::string, std::shared_ptr<GammaInfo> > > gamma_info_map(){ return  gamma_info_map_;}
        std::shared_ptr<std::map<std::string, std::shared_ptr< std::map<std::string, AContribInfo > >>> G_to_A_map(){ return  G_to_A_map_;} //TODO should be private
        std::shared_ptr<std::set<std::string>> required_blocks()  { return  required_blocks_; } 

};
#endif
