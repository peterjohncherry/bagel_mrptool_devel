#include <bagel_config.h>
#include <src/prop/proptool/algebraic_manipulator/expression.h>
#include <src/prop/proptool/proputils.h>

#define __DEBUG_PROPTOOL_EXPRESSION
#ifdef __DEBUG_PROPTOOL_EXPRESSION
#include <src/prop/proptool/debugging_utils.h>
#endif
using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//TODO gamma_info_map should be generated outside and not fed in here. It constains _no_ Expression specific information.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
Expression<DataType>::Expression( shared_ptr<vector< shared_ptr<BraKet_Base>>> braket_list,
                                  shared_ptr<StatesInfo_Base> states_info,
                                  shared_ptr<map< string, shared_ptr<TensOp_Base>>>  MT_map,
                                  shared_ptr<map< string, shared_ptr<CtrTensorPart_Base> >>            CTP_map,
                                  shared_ptr<map< string, shared_ptr<vector<shared_ptr<CtrOp_base>> >>>     ACompute_map,
                                  shared_ptr<map< string, shared_ptr<GammaInfo_Base> > >                         gamma_info_map,
                                  string expression_type ):
                                  braket_list_(braket_list), states_info_(states_info), MT_map_(MT_map), CTP_map_(CTP_map),
                                  ACompute_map_(ACompute_map), gamma_info_map_(gamma_info_map), type_(expression_type) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_PROPTOOL_EXPRESSION
cout << "Expression<DataType>::Expression" << endl;
Debugging_Utils::print_names( *braket_list, "brakets in expression" ); cout << endl;
#endif //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //Note that this G_to_A_map_ is expression specific
  name_ = "";
  for ( shared_ptr<BraKet_Base>& bk : *braket_list_ ) {
    if ( bk->factor_.first != 0.0 )
      name_ += "(" + to_string( bk->factor_.first) ; 
    if ( bk->factor_.second != 0.0 ) {
      name_ += " ," + to_string( bk->factor_.second) +  ")" + bk->name() + " + ";
    } else { 
      name_ += ") + ";
    } 
  }
  name_.pop_back();
  name_.pop_back();

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Builds a list of the tensor blocks which are needed for evaluation of this expression.
// This effectively occurs in other functions, but is useful to have seperately here for allocation purposes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
void
Expression<DataType>::necessary_tensor_blocks( shared_ptr<map< string, shared_ptr< map<string, shared_ptr<AContribInfo_Base>> >>> G_to_A_map){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_PROPTOOL_EXPRESSION
cout << "Expression::necessary_tensor_blocks" << endl;
#endif ////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //loop through G_to_A_map ; get all A-tensors associated with a given gamma
  for (auto G2A_mapit = G_to_A_map->begin(); G2A_mapit != G_to_A_map->end(); G2A_mapit++) {
    
    auto A_map = G2A_mapit->second;
    for (auto A_map_it = A_map->begin(); A_map_it != A_map->end(); A_map_it++){
      string cmtp_name  = A_map_it->first;
      auto CMTP_loc = CTP_map_->find(cmtp_name);
      if ( CMTP_loc == CTP_map_->end())
        throw std::logic_error( cmtp_name + " is not yet in the map!! Generation of Gamma contributions probably has problems!! " ) ;

      //awkward, but this will avoid issues where members of CTP_vec are formed from multiple tensors
      shared_ptr<CtrTensorPart_Base> CMTP = CMTP_loc->second;
      shared_ptr<vector<string>> ranges = CMTP->full_id_ranges();
      shared_ptr<vector<string>> idxs = CMTP->full_idxs();
      int t_num = 0;
      int op_size = 1;
      char op_name = idxs->at(0)[0];
      for ( int ii = 1 ; ii != idxs->size() ; ii++ ) {
        if ( op_name == idxs->at(ii)[0] ){
          op_size+=1;
        } else {
          vector<string> ctp_idxs(op_size);
          vector<string> ctp_ranges(op_size);
          int pos = 0;
          for ( int jj = ii - op_size; jj != ii ; jj++, pos++ ) {
            ctp_idxs[pos] = idxs->at(pos);
            ctp_ranges[pos] = ranges->at(pos);
          }
          op_name = idxs->at(ii)[0];
          op_size = 1;
        }
      }
    }
  }
  return;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template class Expression<double>;
template class Expression<std::complex<double>>;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
