#include <bagel_config.h>
#include <src/prop/proptool/algebraic_manipulator/expression_full.h>
#include <src/prop/proptool/proputils.h>

using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Adds terms associated with each gamma (as determined by BraKet) into the map
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
void Expression_Full<DataType>::generate_algebraic_task_list(){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_PROPTOOL_EXPRESSION_FULL
cout << "void Expression_Full<DataType>::generate_algebraic_task_list()" << endl;
#endif ////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
  required_blocks_ = make_shared<set<shared_ptr<Range_Block_Info>>>();
  cout << "braket_list_->size() = "; cout.flush(); cout << braket_list_->size() << endl;
  for ( shared_ptr<BraKet_Base>& braket : *braket_list_ )
    braket->generate_gamma_Atensor_contractions( MT_map_, G_to_A_map_, gamma_info_map_, states_info_, required_blocks_, CTP_map_ );
 
  this->get_gamma_Atensor_contraction_list();

  return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
void 
Expression_Full<DataType>::get_gamma_Atensor_contraction_list(){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_PROPTOOL_EXPRESSION_FULL
cout << "Expression_Full::get_gamma_Atensor_contraction_list" << endl;
#endif ////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //loop through G_to_A_map ; get all A-tensors associated with a given gamma
  for (auto G2A_mapit = G_to_A_map_->begin(); G2A_mapit != G_to_A_map_->end(); G2A_mapit++) {
    auto A_map = G2A_mapit->second;
    for (auto A_map_it = A_map->begin(); A_map_it != A_map->end(); A_map_it++){
      string cmtp_name  = A_map_it->first;
      if ( CTP_map_->find(cmtp_name) == CTP_map_->end()){
        throw std::logic_error( cmtp_name + " is not yet in the map!! Generation of Gamma contributions probably has problems!! " ) ;
      }
      auto ACompute_list_loc = ACompute_map_->find(cmtp_name);
      if ( ACompute_list_loc != ACompute_map_->end() ){
        continue;
      } else {
        shared_ptr<vector<shared_ptr<CtrOp_base>>> ACompute_list = make_shared<vector<shared_ptr<CtrOp_base> >>(0);
        CTP_map_->at(cmtp_name)->build_contraction_sequence(CTP_map_, ACompute_list, ACompute_map_);
        ACompute_map_->emplace(cmtp_name, ACompute_list);
        CTP_map_->at(cmtp_name)->got_compute_list( true );
      }
    }
  }
  return;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template class Expression_Full<double>;
template class Expression_Full<std::complex<double>>;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
