#include <bagel_config.h>
#include <map>
#include <src/prop/proptool/algebraic_manipulator/gamma_generator_redux.h>

#define __DEBUG_GAMMA_GENERATOR_REDUX

using namespace std;
using namespace WickUtils;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType> 
void GammaGeneratorRedux<DataType>::add_gamma( const shared_ptr<Range_Block_Info> block_info, shared_ptr<vector<bool>> trans_aops ) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_GAMMA_GENERATOR_REDUX
cout << "void GammaGeneratorRedux<DataType>::add_gamma " << endl;
#endif ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
  block_info_ = block_info;
  block_aops_ = trans_aops;
  block_aops_rngs_ = block_info->orig_rngs_ch();

  op_info_ = block_info_->op_info();
  idxs_trans_ = block_info_->idxs_trans();
  std_rngs_ = *(block_info_->unique_block_->orig_rngs_);
  standard_order_ = *(block_info_->idxs_trans());
  print_vector( standard_order_ , " standard_order" ); cout << "  "; cout.flush(); print_vector(*std_ids_, "std_ids_" ); cout << endl; 

  print_new_gamma_definition(); 

  cout << "ggr:ag 1 " << endl;
  shared_ptr<vector<int>> ids_pos = make_shared<vector<int>>( std_rngs_.size() );
  iota( ids_pos->begin(), ids_pos->end(), 0 );

  shared_ptr<vector<pair<int,int>>> deltas_pos = make_shared<vector<pair<int,int>>>(0);
  
  gamma_vec_ = make_shared<vector<shared_ptr<GammaIntermediate_Base>>>( 1 );
  cout << "ggr:ag 2 " << endl;
  gamma_vec_->at(0) = make_shared<GammaIntermediateRedux<DataType>>( ids_pos, deltas_pos, block_info_->factors() );
  cout << "ggr:ag 3 " << endl;

  final_gamma_vec_ = make_shared<vector<shared_ptr<GammaIntermediate_Base>>>(0);
  cout << "ggr:ag 4 " << endl;
 
  return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType> 
void GammaGeneratorRedux<DataType>::add_Acontrib_to_map( int kk, string bra_name, string ket_name ){  // e.g. ++++----
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_GAMMA_GENERATOR_REDUX
cout << "void GammaGeneratorRedux<DataType>::add_Acontrib_to_map" << endl;
#endif ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  shared_ptr<GammaIntermediate_Base> gint = gamma_vec_->at(kk);

  shared_ptr<vector<pair<int,int>>> deltas_pos     = gint->deltas_pos_;
  shared_ptr<vector<int>> ids_pos                  = gint->ids_pos_;

  vector<int> standardized_ids_pos( ids_pos->size() ); 
  {
  vector<int>::iterator si_it = standardized_ids_pos.begin();
  for ( vector<int>::iterator ip_it = ids_pos->begin(); ip_it != ids_pos->end(); ip_it++, si_it++ ) 
    *si_it = standard_order_[*ip_it];
  } 

  //TODO should standardize deltas pos when building gamma intermediate so as to avoid repeated transformation
  vector<pair<int,int>> idxs_deltas_pos(deltas_pos->size());
  vector<pair<int,int>>::iterator  idp_it = idxs_deltas_pos.begin();
  for ( vector<pair<int,int>>::iterator dp_it = deltas_pos->begin(); dp_it != deltas_pos->end(); dp_it++, idp_it++ ) 
    *idp_it = make_pair( (*idxs_trans_)[dp_it->first], (*idxs_trans_)[dp_it->second]);

  string Aname_alt = get_ctp_name( op_info_->op_state_name_canonical(), *std_ids_,  std_rngs_, idxs_deltas_pos );
  cout << "Aname_alt = " << Aname_alt << endl;
  
  if ( total_op_->CTP_map()->find(Aname_alt) == total_op_->CTP_map()->end() ) 
    total_op_->enter_cmtps_into_map(idxs_deltas_pos, std_rngs_, op_info_ );
  
  string Gname_alt = get_gamma_name(chrvec_to_strvec(*block_aops_rngs_), *block_aops_, *ids_pos, bra_name, ket_name );

  if ( G_to_A_map->find( Gname_alt ) == G_to_A_map->end() )
    G_to_A_map->emplace( Gname_alt, make_shared<map<string, shared_ptr<AContribInfo_Base>>>() );

  //TODO do this reordering w.r.t. standardized orders DQ : Is this ok? 
  vector<int> Aid_order_new = get_Aid_order( standardized_ids_pos );
  pair<double,double> new_fac = bk_factor_; 
  pair_fac_mult( gint->factors_, new_fac );

  auto AInfo_loc =  G_to_A_map->at( Gname_alt )->find(Aname_alt);
  if ( AInfo_loc == G_to_A_map->at( Gname_alt )->end() ) {
    auto AInfo = make_shared<AContribInfo_Full<DataType>>( Aname_alt, Aid_order_new, new_fac );
    G_to_A_map->at( Gname_alt )->emplace(Aname_alt, AInfo) ;

  } else {
    shared_ptr<AContribInfo_Base> AInfo = AInfo_loc->second;
    for ( int qq = 0 ; qq != AInfo->id_orders().size(); qq++ ) {
      if( Aid_order_new == AInfo->id_order(qq) ){
        AInfo->combine_factors( qq, new_fac );
        AInfo->remaining_uses_ += 1;
        AInfo->total_uses_ += 1;
        break;

      } else if ( qq == AInfo->id_orders().size()-1) {
        AInfo->add_id_order(Aid_order_new);
        AInfo->add_factor(new_fac);
      }
    }
  }

  Gamma_map->emplace( Gname_alt, make_shared<GammaInfo<DataType>>( target_states_->civec_info(bra_name), target_states_->civec_info(ket_name),
                                                                   block_aops_, make_shared<const vector<string>>(chrvec_to_strvec(*block_aops_rngs_)),
                                                                   ids_pos, Gamma_map) );
  return;
}
////////////////////////////////////////////////////////////////////////
template<typename DataType> 
void GammaGeneratorRedux<DataType>::block_trans_test( int kk ){
////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_GAMMA_GENERATOR_REDUX
cout << "GammaGeneratorRedux<DataType>::block_trans_test " << endl;
#endif /////////////////////////////////////////////////////////////////

  auto gint =  gamma_vec_->at(kk) ; 
  auto deltas_pos = gint->deltas_pos_;

  vector<pair<int,int>> idxs_deltas_pos(deltas_pos->size());
  vector<pair<int,int>>::iterator  idp_it = idxs_deltas_pos.begin();
  for ( vector<pair<int,int>>::iterator dp_it = deltas_pos->begin(); dp_it != deltas_pos->end(); dp_it++, idp_it++ ) 
    *idp_it = make_pair( (*idxs_trans_)[dp_it->first], (*idxs_trans_)[dp_it->second]);

  block_idxs_ = vector<string>( std_ids_->size() );
  {
  vector<int>::iterator it_it = block_info_->idxs_trans()->begin();
  for ( vector<string>::iterator bi_it = block_idxs_.begin(); bi_it != block_idxs_.end(); bi_it++, it_it++ ) 
    *bi_it = (*std_ids_)[ *it_it ];
  }

  print_vector( block_idxs_, " block_idxs " ); cout << endl;
  print_vector( *std_ids_,  " std_ids_   " ); cout << endl;

  print_pair_vector( *(gint->deltas_pos_) , "gint->deltas_pos_" ); cout.flush();
  cout << "  : [ "; for ( auto ctr : *(gint->deltas_pos_) ) { cout << "(" << block_idxs_[ctr.first] <<","<<  block_idxs_[ctr.second] << ")"; cout.flush(); }  cout << "]" << endl;

  print_pair_vector( idxs_deltas_pos , "idxs_deltas_pos_" );  
  cout << "  : [ "; for ( auto ctr : idxs_deltas_pos ) { cout << "(" << (*std_ids_)[ctr.first] <<","<<  (*std_ids_)[ctr.second] << ")"; cout.flush(); }  cout << "]" << endl;
  cout << "  : [ "; for ( auto ctr : idxs_deltas_pos ) { cout << "(" << (std_rngs_)[ctr.first] <<","<<  (std_rngs_)[ctr.second] << ")"; cout.flush(); }  cout << "]" << endl;

  return;
}
/////////////////////////////////////////////////////////////////////////////////////
template<typename DataType> 
void GammaGeneratorRedux<DataType>::print_new_gamma_definition() {
/////////////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_GAMMA_GENERATOR_REDUX
cout << "GammaGeneratorRedux<DataType>::print_new_gamma_block_definition" << endl;
#endif //////////////////////////////////////////////////////////////////////////////

  cout << endl;
  cout << "--------------- gamma def -------------------" << endl;
  print_vector( *(block_info_->unique_block_->orig_rngs())  ,        " unique_block_      "); cout <<endl;
  print_vector( standard_order_ ,  " range_reordering   "); cout << endl;
  print_vector(*(block_info_->orig_rngs()) ,      " orig_rngs          "); cout <<endl;
  cout << endl;

  return;
}
//////////////////////////////////////////////////////////////////////////
template class GammaGeneratorRedux<double>;
template class GammaGeneratorRedux<std::complex<double>>;
/////////////////////////////////////////////////////////////////////////
