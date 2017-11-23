 #ifndef __SRC_SMITH_EXPRESSION_INFO_H
 #define __SRC_SMITH_EXPRESSION_INFO_H
 #include <src/smith/wicktool/equation.h>
 #include <src/smith/tensor.h>
 #include <src/smith/wicktool/states_info.h>

using pint_vec = std::vector<std::pair<int,int>>;
using pstr_vec = std::vector<std::pair<std::string,std::string>>;

template<class DataType> 
class Expression_Info {
      private :
        bool spinfree_ = false;
        std::shared_ptr<StatesInfo<DataType>> TargetStates;

      public:

      Expression_Info(std::shared_ptr<StatesInfo<DataType>> TargetStates, bool spinfree);
      ~Expression_Info(){};

      //only makes sense to specify a and b electrons if non-rel
      // key :    Name of BraKet
      // result : Vector of TensOps corresponding to BraKet
      std::shared_ptr< std::map <std::string, 
                                 std::shared_ptr<std::vector<std::shared_ptr< TensOp<DataType>>>>>> BraKet_map;
  
      std::shared_ptr< std::map <std::string, std::shared_ptr<Equation<DataType>>>> expression_map;
    
      // key :    Name of contracted part of TensorOp.
      // result : Info for contracted part of TensorOp info.
      std::shared_ptr< std::map< std::string, std::shared_ptr< TensOp<DataType> > >> T_map    ;      

      // key :    Name of contracted part of TensorOp
      // result : Info for contracted part of TensorOp info
      std::shared_ptr< std::map< std::string, std::shared_ptr< CtrTensorPart<DataType> > >> CTP_map    ;      

      // key :    Name of contracted part of multitensorop
      // result : Info for contracted part of multitensorop info
      std::shared_ptr< std::map< std::string, std::shared_ptr< CtrMultiTensorPart<DataType> > >> CMTP_map   ;  


      void Build_BraKet(std::shared_ptr<std::vector<std::shared_ptr<TensOp<DataType>>>> Tens_vec  );
      
      std::shared_ptr<TensOp<DataType>> Build_TensOp( std::string op_name,
                                                      std::shared_ptr<DataType> tensor_data,
                                                      std::shared_ptr<std::vector<std::string>> op_idxs,
                                                      std::shared_ptr<std::vector<bool>> op_aops, 
                                                      std::shared_ptr<std::vector<std::vector<std::string>>> op_idx_ranges,
                                                      std::vector< std::tuple< std::shared_ptr<std::vector<std::string>>(*)(std::shared_ptr<std::vector<std::string>>),int,int >> Symmetry_Funcs,
                                                      std::vector<bool(*)(std::shared_ptr<std::vector<std::string>>)> Constraint_Funcs,
                                                      std::pair<double,double> factor, std::string Tsymmetry, bool hconj ) ;
     
      void Set_BraKet_Ops(std::shared_ptr<std::vector<std::string>> Op_names, std::string term_name ) ;
      void Build_Expression(std::shared_ptr<std::vector<std::string>> BraKet_names, std::string expression_name ) ;

      int nalpha(int state_num) { return TargetStates->nalpha( state_num ); };
      int nbeta(int state_num)  { return TargetStates->nbeta( state_num );  };
      int nact(int state_num)   { return TargetStates->nact( state_num );   };
      bool spinfree(){return spinfree_;}

      static std::string flip(std::string idx);
      static std::shared_ptr<std::vector<std::string>> ijkl_to_klij(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> ijkl_to_jilk(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> ijkl_to_lkji(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> ijkl_to_ijlk_block(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> ijkl_to_jikl_block(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> ijkl_to_jilk_block(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> bbbb_to_aaaa(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> bbaa_to_aaaa(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> aabb_to_aaaa(std::shared_ptr<std::vector<std::string>> invec) ;
      static std::shared_ptr<std::vector<std::string>> identity(std::shared_ptr<std::vector<std::string>> invec) ;
      static bool NotAllAct(std::shared_ptr<std::vector<std::string>> ranges);
      static bool always_true(std::shared_ptr<std::vector<std::string>> ranges);

      std::vector<std::tuple<std::shared_ptr<std::vector<std::string>>(*)(std::shared_ptr<std::vector<std::string>>),int,int >> set_2el_symmfuncs();
      std::vector<std::tuple<std::shared_ptr<std::vector<std::string>>(*)(std::shared_ptr<std::vector<std::string>>),int,int >> set_1el_symmfuncs();
      std::vector<std::tuple<std::shared_ptr<std::vector<std::string>>(*)(std::shared_ptr<std::vector<std::string>>),int,int >> identity_only();

      struct compare_string_length {
        bool operator()(const std::string& first, const std::string& second) {
            return first.size() > second.size();
        }
      };

};
#endif
