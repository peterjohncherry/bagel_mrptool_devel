#include <bagel_config.h>
#ifdef COMPILE_SMITH
#include <src/smith/wicktool/equation.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
Equation<DType>::Equation(std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::shared_ptr< TensOp<DType>>>>>> BraKet_list){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  T_map                 = make_shared<map< string, shared_ptr<TensOp<DType>>>>();
  CTP_map               = make_shared<map< string, shared_ptr<CtrTensorPart<DType>> >>();    
  CMTP_map              = make_shared<map< string, shared_ptr<CtrMultiTensorPart<DType>> >>(); 
  ACompute_map          = make_shared<map<string, shared_ptr<vector<shared_ptr<CtrOp_base>> > >>(); 
  CMTP_Eqn_Compute_List = make_shared<map< vector<string>, shared_ptr<vector<pair<shared_ptr<vector<string>>, pair<int,int> >>> >>();

  GammaMap              = make_shared< unordered_map<string, shared_ptr<GammaInfo> > >(); 
  G_to_A_map            = make_shared< unordered_map<string, shared_ptr< unordered_map<string, AContribInfo  >>>>(); 

  equation_build(BraKet_list);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class DType>
void Equation<DType>::Initialize(){
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  T_map  = make_shared<map< string, shared_ptr<TensOp<DType>>>>();
  CTP_map    = make_shared<map< string, shared_ptr<CtrTensorPart<DType>> >>();    
  CMTP_map   = make_shared<map< string, shared_ptr<CtrMultiTensorPart<DType>> >>(); 
  ACompute_map = make_shared<map<string, shared_ptr<vector<shared_ptr<CtrOp_base>> > >>(); 
  CMTP_Eqn_Compute_List = make_shared<map< vector<string>, shared_ptr<vector<pair<shared_ptr<vector<string>>, pair<int,int> >>> >>();
  G_to_A_map   = make_shared< unordered_map<string, shared_ptr< unordered_map<string, AContribInfo >>>>(); 
  GammaMap = make_shared< unordered_map<string, shared_ptr<GammaInfo> > >(); 

  return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
shared_ptr<TensOp<DType>> Equation<DType>::Build_TensOp( string op_name,
                                                         shared_ptr<DType> tensor_data, //needs to be templated, should be Bagel tensor
                                                         shared_ptr<vector<string>> op_idxs,
                                                         shared_ptr<vector<bool>> op_aops, 
                                                         shared_ptr<vector<vector<string>>> op_idx_ranges,
                                                         vector< tuple< shared_ptr<vector<string>>(*)(shared_ptr<vector<string>>),int,int >> Symmetry_Funcs,
                                                         vector<bool(*)(shared_ptr<vector<string>>)> Constraint_Funcs,
                                                         pair<double,double> factor, string Tsymmetry, bool hconj ) {
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "TensOp::BuildTensOp" <<   endl;

  if(hconj){
    reverse(op_idxs->begin(), op_idxs->end());  
    reverse(op_aops->begin(), op_aops->end());  
    reverse(op_idx_ranges->begin(), op_idx_ranges->end());  
    factor.second = -factor.second;// change this 
  }
  //NOTE: change to use proper factor
  int tmpfac =1;
  shared_ptr<TensOp<DType>>  New_Op = make_shared<TensOp<DType>>(op_name, Symmetry_Funcs, Constraint_Funcs);

  New_Op->data = tensor_data;
  New_Op->initialize(*op_idxs, *op_idx_ranges, *op_aops, tmpfac, Tsymmetry);
  New_Op->get_ctrs_tens_ranges();

  return New_Op;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void  Equation<DType>::equation_build(std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::shared_ptr< TensOp<DType> > >>>> BraKet_list){
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Into Equation" <<endl;

  for (auto BraKet_Tensors : *BraKet_list) 
    Build_BraKet( BraKet_Tensors );
  
  for (auto braket : BraKet_Terms)
    Get_CMTP_Compute_Terms();
  cout << "Leaving Equation" <<endl;

  return ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void Equation<DType>::Build_BraKet(shared_ptr<vector<shared_ptr<TensOp<DType>>>> Tens_vec ){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  auto New_BraKet = make_shared<BraKet<DType>>(G_to_A_map, GammaMap );
  New_BraKet->Sub_Ops = Tens_vec;
 
  New_BraKet->Build_TotalOp();
  New_BraKet->Build_Gamma_SpinFree(New_BraKet->Total_Op->aops, New_BraKet->Total_Op->idxs); 

  CMTP_map->insert(New_BraKet->Total_Op->CMTP_map->begin(), New_BraKet->Total_Op->CMTP_map->end());
  CTP_map->insert(New_BraKet->Total_Op->CTP_map->begin(), New_BraKet->Total_Op->CTP_map->end());
  BraKet_Terms.push_back(New_BraKet);   

  return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Adds terms associated with each gamma into the map
// Note this 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void Equation<DType>::Get_CMTP_Compute_Terms(){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Get_CMTP_Compute_Terms" << endl;  

  //loop through G_to_A_map ; get all A-tensors associated with a given gamma
  for (auto  G2A_mapit =G_to_A_map->begin(); G2A_mapit != G_to_A_map->end(); G2A_mapit++) {
    
    auto A_map = G2A_mapit->second;
    for (auto A_map_it = A_map->begin(); A_map_it != A_map->end(); A_map_it++){
      
      string CMTP_name  = A_map_it->first;

      shared_ptr<vector<shared_ptr<CtrOp_base>>>  ACompute_list; 
      if ( CMTP_map->find(CMTP_name) == CMTP_map->end())
        throw std::logic_error( CMTP_name + " is not yet in the map!! Generation of Gamma contributions probably has problems!! " ) ;

      auto ACompute_list_loc = ACompute_map->find(CMTP_name);
      if ( ACompute_list_loc != ACompute_map->end() ){
        cout << "Equation::Get_CMTP_Compute_Terms::already built compute list for " << CMTP_name << " during generation of earlier compute list" << endl;
        cout << CMTP_name << " has a compute list of length : "; cout.flush() ; cout << ACompute_map->at(CMTP_name)->size() << "  --- Still in if " << endl;
        continue;
      } else {  
        ACompute_list = make_shared<vector<shared_ptr<CtrOp_base> >>(0);
        CMTP_map->at(CMTP_name)->FullContract(CTP_map, ACompute_list, ACompute_map);
        ACompute_map->emplace(CMTP_name, ACompute_list);
        CMTP_map->at(CMTP_name)->got_compute_list =true; 
      }
  
      cout << CMTP_name << " has a compute list of length : "; cout.flush() ; cout << ACompute_map->at(CMTP_name)->size() << endl;

    }
    cout << "X" << endl;
  }
  cout << "leaving Get_CMTP_compute_Terms" << endl;
  return;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template class Equation<double>;
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif
