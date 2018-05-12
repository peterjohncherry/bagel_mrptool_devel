#include <bagel_config.h>
#include <map>
#include <src/prop/proptool/algebraic_manipulator/gamma_generator_orb_exc_deriv.h>

using namespace std;
using namespace WickUtils;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename DataType> 
void GammaGenerator_OrbExcDeriv<DataType>::add_gamma( const shared_ptr<Range_Block_Info> block_info, shared_ptr<vector<bool>> trans_aops  ) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  cout << "void GammaGenerator_OrbExcDeriv::add_gamma " << endl;

  block_idxs_ = vector<string>( std_ids_->size() );
  idxs_trans_ = block_info->idxs_trans();
  block_aops_ = trans_aops;
  block_aops_rngs_ = block_info->orig_rngs_ch(); // May be transformed by Bra or Ket.

  op_info_ = block_info->op_info();

  block_rngs_ = make_shared<vector<string>>( block_info->orig_rngs()->size() ) ; // After the below definition, this remains unchanged.
  vector<string>::iterator br_it = block_rngs_->begin();
  for ( vector<int>::iterator it_it = idxs_trans_->begin() ; it_it != idxs_trans_->end(); it_it++ , br_it++ )
    *br_it = (*(block_info->unique_block_->orig_rngs_))[*it_it];

  shared_ptr<vector<int>>  idxs_trans_inverse_ = block_info->idxs_trans_inverse();
  std_rngs_ = *(block_info->unique_block_->orig_rngs_);
  standard_order_  = *(block_info->idxs_trans());

  // TODO change; this will be problematic when we have multiple operators with the same name but different states.
  target_block_start_ = 0 ;
  target_block_end_ = 0 ;
  for ( vector<int>::iterator oo_it = block_info->op_info_->op_order_->begin() ; oo_it != block_info->op_info_->op_order_->end(); oo_it++ ) {
    shared_ptr<Range_Block_Info>& rb = (*(block_info->range_blocks()))[*oo_it];
    target_block_end_ += rb->num_idxs_; 
    if ( rb->op_info_->op_state_name_[0] == target_op_[0] ) {
      target_block_name_ = rb->name(); 
      target_block_size_ = rb->num_idxs_; 
      break; 
    }
    target_block_start_ += rb->num_idxs_; 
  }

  // TMP.
  std_rngs_target_op_free_ = vector<string>(block_idxs_.size() - target_block_size_ ); 
  std_idxs_target_op_free_ = vector<string>(block_idxs_.size() - target_block_size_ ); 
  block_pos_target_op_free_ = vector<bool>(block_idxs_.size(), true );
  { 
  std_name_target_op_free_ = "";
  shared_ptr<vector<shared_ptr<Range_Block_Info>>> range_blocks = block_info->range_blocks() ; 
  vector<string>::const_iterator si_it = std_ids_->begin();
  vector<string>::iterator sr_it = std_rngs_.begin();
  vector<string>::iterator sitof_it = std_idxs_target_op_free_.begin();
  vector<string>::iterator srtof_it = std_rngs_target_op_free_.begin();

  vector<bool>::iterator bptof_it = block_pos_target_op_free_.begin();
  for( vector<int>::iterator oo_it = op_info_->op_order_->begin(); oo_it != op_info_->op_order_->end(); oo_it++ ) {
    if ( (*range_blocks)[*oo_it]->full_op_name()[0] != target_op_[0] ){ 
      for ( int rr = 0  ; rr != (*range_blocks)[*oo_it]->num_idxs_; sr_it++, si_it++, srtof_it++, sitof_it++, rr++ ) {
        *sitof_it = *si_it;
        *srtof_it = *sr_it;
      }  
      std_name_target_op_free_ +=  (*range_blocks)[*oo_it]->op_state_name_;
    } else { 
      si_it += target_block_size_; 
      sr_it += target_block_size_;
      for( int qq = 0  ;qq != target_block_size_ ; qq++, bptof_it++ ) 
        *bptof_it = false;
    }
  }
  }
  {
  vector<int>::iterator it_it = idxs_trans_->begin();
  for ( vector<string>::iterator bi_it = block_idxs_.begin(); bi_it != block_idxs_.end(); bi_it++, it_it++ )
    *bi_it = (*std_ids_)[ *it_it ];
  }

  block_rngs_target_op_free_ = vector<string>(block_idxs_.size() - target_block_size_ ); 
  block_idxs_target_op_free_ = vector<string>(block_idxs_.size() - target_block_size_ ); 

  int kk = 0;
  for ( int ii = 0 ; ii != target_block_start_; ii++, kk++ ) {
    block_rngs_target_op_free_[kk] = (*block_rngs_)[ii];
    block_idxs_target_op_free_[kk] = block_idxs_[ii];
  }  

  for ( int ii = target_block_end_; ii != std_rngs_.size(); ii++, kk++ ){ 
    block_rngs_target_op_free_[kk] = (*block_rngs_)[ii];
    block_idxs_target_op_free_[kk] = block_idxs_[ii];
  }
   
//  cout << endl;
//  cout << "----------- gamma orb deriv def -------------------" << endl;
//  print_vector( std_rngs_ ,    " std_rngs_ "); cout <<endl;
//  print_vector(*idxs_trans_ ,  " idxs_trans_   "); cout << endl;
//  print_vector(*block_rngs_ ,  " block_rngs    "); cout <<endl;
//  print_vector( block_idxs_, "block_idxs" ); cout <<endl;
//  print_vector(block_rngs_target_op_free_ ,  " block_rngs_target_op_free_ "); cout << endl;
//  print_vector(block_idxs_target_op_free_ ,  " block_idxs_target_op_free_ "); cout <<endl;
//  print_vector(std_idxs_target_op_free_ ,    " std_idxs_target_op_free_   "); cout << endl;
//  print_vector(std_rngs_target_op_free_ ,    " std_rngs_target_op_free_   "); cout << endl;
//  cout << endl;

  int ii = 0 ;
  block_to_std_order_ = vector<int>(idxs_trans_->size());
  for ( vector<int>::iterator so_it = idxs_trans_->begin() ; so_it != idxs_trans_->end() ; ++so_it, ++ii ) 
    block_to_std_order_[*so_it] = ii;

  shared_ptr<vector<int>> ids_pos = make_shared<vector<int>>( std_rngs_.size() );
  iota( ids_pos->begin(), ids_pos->end(), 0 );

  shared_ptr<vector<pair<int,int>>> A_deltas_pos = make_shared<vector<pair<int,int>>>(0);
  shared_ptr<vector<pair<int,int>>> target_target_deltas_pos = make_shared<vector<pair<int,int>>>(0);
  shared_ptr<vector<pair<int,int>>> target_A_deltas_pos = make_shared<vector<pair<int,int>>>(0);
  
  pair< double, double >  factors = block_info->factors();
//  cout << "factors = (" << factors.first << "," << factors.second << ")" << endl;

  //TODO  Change to specialized class
  gamma_vec_ = make_shared<vector<shared_ptr<GammaIntermediate_Base >>>();
  gamma_vec_->push_back(make_shared<GammaIntermediate_OrbExcDeriv<DataType>>( ids_pos, A_deltas_pos, target_A_deltas_pos, target_target_deltas_pos, factors ));
  final_gamma_vec_ = make_shared<vector<shared_ptr<GammaIntermediate_Base >>>(0);
  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cannot pass shared_ptr to gammaintermediate, as push back can potentially result in the vector being moved,
// which messes up the pointer inside the shared_ptr.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename DataType> 
void GammaGenerator_OrbExcDeriv<DataType>::swap( int ii, int jj, int kk ){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  cout << "GammaGenerator_OrbExcDeriv<DataType>::swap ii = " << ii << " jj = " << jj << " kk = " << kk << endl;

  shared_ptr<GammaIntermediate_Base> gint =  gamma_vec_->at(kk);

  // only need one buffer really, but this looks clearer
  int j_pos = (*((gint)->ids_pos_))[jj];
  int i_pos = (*((gint)->ids_pos_))[ii];

  (*((gint)->ids_pos_))[ii] = j_pos;
  (*((gint)->ids_pos_))[jj] = i_pos;

  if ( ( (*block_aops_rngs_)[ j_pos ] == (*block_aops_rngs_)[ i_pos ]) &&( (*block_aops_)[ i_pos ] != (*block_aops_)[ j_pos ]) ){

      i_pos = standard_order_[i_pos ]; 
      j_pos = standard_order_[j_pos ]; 

      shared_ptr<vector<int>> new_ids_pos = make_shared<vector<int>>( gint->ids_pos_->size()-2);
      vector<int>::iterator nip_it = new_ids_pos->begin();
      vector<int>::iterator gip_it = gint->ids_pos_->begin();
      for( int qq = 0 ; qq != gint->ids_pos_->size() ; qq++, gip_it++) {
        if( (qq != ii) && (qq != jj) ){
         *nip_it = *gip_it;
          ++nip_it;
        }
      }

      shared_ptr<pint_vec> new_target_target_deltas_pos = make_shared<pint_vec>( *(gint->target_target_deltas_pos()) );
      shared_ptr<pint_vec> new_target_A_deltas_pos = make_shared<pint_vec>( *(gint->target_A_deltas_pos()) );
      shared_ptr<pint_vec> new_A_A_deltas_pos = make_shared<pint_vec>( *(gint->deltas_pos_) );

      //Make it so T is first index, if all or no T, make it so creation op is first index
      if ( (j_pos >= target_block_start_) && ( j_pos < target_block_end_ )  ) {

        if ( (i_pos >= target_block_start_) &&  ( i_pos < target_block_end_ )  ) {
          pair<int,int> new_delta = (*block_aops_)[ j_pos ] ? make_pair( j_pos, i_pos ) : make_pair( i_pos , j_pos);
	  new_target_target_deltas_pos->push_back(new_delta);
        } else {
          pair<int,int> new_delta = make_pair( j_pos, i_pos );
	  new_target_A_deltas_pos->push_back(new_delta);
        }

      } else if ( i_pos >= target_block_start_ && i_pos < target_block_end_ ) {
        pair<int,int> new_delta = make_pair( i_pos, j_pos );
        new_target_A_deltas_pos->push_back(new_delta);

      } else {  
        pair<int,int> new_delta = (*block_aops_)[ j_pos ] ? make_pair( j_pos, i_pos ) : make_pair( i_pos , j_pos);
        new_A_A_deltas_pos->push_back(new_delta);
      } 
  
    shared_ptr<GammaIntermediate_OrbExcDeriv<DataType>> new_gamma = 
         make_shared<GammaIntermediate_OrbExcDeriv<DataType>>( new_ids_pos, new_A_A_deltas_pos, new_target_A_deltas_pos, new_target_target_deltas_pos, (gint)->factors_ );

    gamma_vec_->push_back(new_gamma);

  }
  //Note that these are factors for the real and imaginary part, they are _not_ the real and imaginary part of the factor
  gint->factors_.first*-1.0;
  gint->factors_.second*-1.0;

  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType> 
void GammaGenerator_OrbExcDeriv<DataType>::add_Acontrib_to_map( int kk, string bra_name, string ket_name ){  // e.g. ++++----
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "GammaGenerator_OrbExcDeriv<DataType>::add_Acontrib_to_map" << endl;

  shared_ptr<GammaIntermediate_Base> gint = (*gamma_vec_)[kk];

  //get full gamma ids pos and A_contrib_ids
  vector<int> gamma_ids_pos = *(gint->ids_pos_);
  
  string Gname_alt = get_gamma_name( chrvec_to_strvec(*block_aops_rngs_), *block_aops_, gamma_ids_pos, bra_name, ket_name );

  cout << "target_block_start_ = " << target_block_start_ << "   target_block_end_ = " << target_block_end_;
  cout << "Gname_alt = " << Gname_alt << endl; 
  if ( Gamma_map->find(Gname_alt) == Gamma_map->end() ) {
    Gamma_map->emplace( Gname_alt, make_shared<GammaInfo<DataType>>( target_states_->civec_info(bra_name), target_states_->civec_info(ket_name),
                                                                     block_aops_, make_shared<const vector<string>>(chrvec_to_strvec(*block_aops_rngs_)),
                                                                     gint->ids_pos_, Gamma_map) );
  }

  {
  vector<int> gamma_ids_pos_buff = gamma_ids_pos;
  vector<int>::iterator gipb_it = gamma_ids_pos_buff.begin(); 
  for ( vector<int>::iterator gip_it = gamma_ids_pos.begin(); gip_it != gamma_ids_pos.end() ; gip_it++, gipb_it++ ){
    *gip_it = standard_order_[*gipb_it];  
  }
  }
 
  vector<int> A_gamma_contraction_pos(0);
  vector<int> T_gamma_pos(0);
  vector<int> T_pos(0);
  // Get A indexes which must be contracted with gamma indexes
  // Get T indexes which are not contracted with A, and are basically gamma indexes
  for( vector<int>::iterator gip_it = gamma_ids_pos.begin(); gip_it != gamma_ids_pos.end() ; gip_it++ ){
    if ( ( *gip_it < target_block_start_) ||  ( *gip_it >= target_block_end_ )  ){ 
      A_gamma_contraction_pos.push_back(*gip_it);
    } else {
      T_gamma_pos.push_back(*gip_it);
      T_pos.push_back(*gip_it);
    }
  }

  // Get A indexes which are not contracted with gamma; these are T indexes.
  vector<int> A_T_pos(0);
  if ( gint->target_A_deltas_pos()->size() != 0 ) {
    for ( pair<int,int>&  ctr : *(gint->target_A_deltas_pos()) ) { 
      A_T_pos.push_back(ctr.second);
      T_pos.push_back(ctr.first);
    }
  }
  // Might have
  // Tw Tx Ty Tz  = < | ax ay ai al | J > Ai A(j/w) A(k/z) Al   
  //
  // Sort A to contract i and l with gamma
  // Tw Tx Ty Tz  = < | ax ay | J > A'(j/w) A'(k/z)       : [ ijkl -> iljk ]
  //
  // Sort remaining indexes so they match up with T order
  // Tw Tx Ty Tz  = < | ax ay | J > A'(j/w) A'(k/z)       : [ xyjk ->jxyk ] or [ xywz -> wxyz ]
  //
  // The latter case corresponds to this reordering (not we get x and y in the first loop, and j/w and k/z in the second
  vector<int> A_gamma_to_T_index_order = get_Aid_order ( T_pos );

  // Get positions where T is contracted with itself ( I think this should be disposed of, and we should have T_^{S} and T^{D} seperate )
  if ( gint->target_target_deltas_pos()->size() != 0 ) {
    for ( pair<int,int>&  ctr : *(gint->target_target_deltas_pos()) ) { 
      T_pos.push_back(ctr.first);
      T_pos.push_back(ctr.second);
    }
  }

  vector<int> A_ids_pos( A_gamma_contraction_pos.size() + A_T_pos.size() );
  copy ( A_gamma_contraction_pos.begin(), A_gamma_contraction_pos.end(), A_ids_pos.begin());
  copy ( A_T_pos.begin(), A_T_pos.end(), A_ids_pos.begin() + A_gamma_contraction_pos.size());

  print_vector( gamma_ids_pos,             "Gamma_ids_pos             " ); cout << endl;
  print_vector( A_gamma_contraction_pos,   "A_gamma_contraction_pos   " ); cout << endl;
  print_vector( A_T_pos,                   "A_T_pos                   " ); cout << endl;
  print_vector( A_ids_pos,                 "A_ids_pos                 " ); cout << endl;
  print_vector( T_pos,                     "T_pos                     " ); cout << endl;
  print_vector( A_gamma_to_T_index_order,  "A_gamma_to_T_index_order  " ); cout << endl; 
   
  //This is the name of the CTP we need to contract (perhaps partially) with gamma
  string Aname_alt = get_ctp_name( std_name_target_op_free_, std_idxs_target_op_free_, std_rngs_target_op_free_, *(gint->deltas_pos_) );
   
  if ( total_op_->CTP_map()->find(Aname_alt) == total_op_->CTP_map()->end() )
    total_op_->enter_cmtps_into_map( *(gint->deltas_pos_), std_rngs_, op_info_ );

  cout << "target_block_name_ = " << target_block_name_ << endl;
  
  shared_ptr<map<string, shared_ptr<map<string, shared_ptr<AContribInfo_Base>>>>> G_to_A_map;
  auto map_loc = block_G_to_A_map_->find(target_block_name_);
  if( map_loc == block_G_to_A_map_->end() ) {
     G_to_A_map = make_shared<map<string, shared_ptr<map<string, shared_ptr<AContribInfo_Base>>>>>() ;
     block_G_to_A_map_->emplace( target_block_name_,  G_to_A_map );
  } else {  
    G_to_A_map = map_loc->second;
  } 

  //Reorder the A_indexes which get contracted with the gamma
  vector<int> Aid_order_new = get_Aid_order( A_gamma_contraction_pos ); 
  pair<double,double> new_fac = bk_factor_; 
  pair_fac_mult( gint->factors_, new_fac );

  // Tested (a bit) to here

  { 
  if ( G_to_A_map->find( Gname_alt ) == G_to_A_map->end() ){
    auto AInfo_map = make_shared<map<string, shared_ptr<AContribInfo_Base>>>();
    G_to_A_map->emplace( Gname_alt, AInfo_map );
  }
  }

  auto AInfo_loc =  G_to_A_map->at( Gname_alt )->find(Aname_alt);
  if ( AInfo_loc == G_to_A_map->at( Gname_alt )->end() ) {
     // TODO add new definition of AContrb_Info for orbexcderiv
    auto AInfo = make_shared<AContribInfo_Full<DataType>>( Aname_alt, Aid_order_new, new_fac );
    G_to_A_map->at( Gname_alt )->emplace( Aname_alt, AInfo );
  } else {
    shared_ptr<AContribInfo_Base> AInfo = AInfo_loc->second;
    // REDO THIS
    // Merge using A_gamma_to_T_index_order first, and then A_id_order second
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
  

  cout << " Leaving GammaGenerator_OrbExcDeriv<DataType>::add_Acontrib_to_map" << endl;
}
///////////////////////////////////////////////////////////////////////////////////////
// Brute approach to getting inverse, keep for now to check
///////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
vector<int> GammaGenerator_OrbExcDeriv<DataType>::get_standard_op_id_order ( const vector<int>& ids_pos ) {
///////////////////////////////////////////////////////////////////////////////////////
  cout << "GammaGenerator_OrbExcDeriv::get_standard_op_id_order " << endl;

  vector<int> new_ids_pos(ids_pos.size());

  vector<int> tmp_pos(ids_pos.size());
  iota(tmp_pos.begin(), tmp_pos.end(), 0);
  vector<int>& standard_order_ref = standard_order_;
  sort(tmp_pos.begin(), tmp_pos.end(), [&ids_pos, standard_order_ref](int i1, int i2){return (bool)( standard_order_ref[i1] < standard_order_ref[i2] ); });

  vector<int> reordering(ids_pos.size());
  iota(reordering.begin(), reordering.end(), 0);
  sort(reordering.begin(), reordering.end(), [&tmp_pos](int i1, int i2){return (bool)( tmp_pos[i1] < tmp_pos[i2] ); });

  return reordering;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
vector<int> GammaGenerator_OrbExcDeriv<DataType>::sort_arg1_wrt_arg2(const vector<int> &ids_pos, const vector<int>& standard_order ) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "GammaGenerator_OrbExcDeriv::sort_art1_wrt_arg2" << endl;

  vector<int> pos(ids_pos.size());
  iota(pos.begin(), pos.end(), 0);
  sort(pos.begin(), pos.end(), [&ids_pos, &standard_order](int i1, int i2){return (bool)( standard_order[i1] < standard_order[i2] ); });

  return pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType> 
void GammaGenerator_OrbExcDeriv<DataType>::transformation_tester( int kk  ){  // e.g. ++++----
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "GammaGenerator_OrbExcDeriv<DataType>::transformation_tester" << endl;

  shared_ptr<GammaIntermediate_Base> gint = gamma_vec_->at(kk);
  shared_ptr<vector<pair<int,int>>> deltas_pos = gint->deltas_pos_;

  cout << endl << endl << endl;
  cout << "----------- gamma orb deriv Contribs -------------------" << endl;
  print_vector( *(gint->ids_pos_) ,                         " ids_pos     _            "); cout <<endl;
  print_pair_vector( *( gint->deltas_pos_),                 " A_A_deltas_pos           "); cout << endl;
  print_pair_vector( *( gint->target_target_deltas_pos()),  " target_target_deltas_pos "); cout << endl;
  print_pair_vector( *( gint->target_A_deltas_pos()),       " target_A_deltas_pos      "); cout << endl;
  cout << endl;

  if ( gint->A_A_deltas_pos()->size() != 0 ) { 

    vector<pair<string,string>> A_A_deltas_rngs;
    for ( pair<int,int>& ctr :  *(gint->deltas_pos_) )
      A_A_deltas_rngs.push_back(make_pair((*block_rngs_)[ctr.first],(*block_rngs_)[ctr.second])) ;
    
    vector<pair<string,string>> A_A_deltas_idxs;
    for ( pair<int,int>& ctr :  *(gint->deltas_pos_) ) 
      A_A_deltas_idxs.push_back(make_pair(block_idxs_[ctr.first] , block_idxs_[ctr.second] )) ;


    vector<pair<int,int>> A_A_deltas_pos( gint->deltas_pos_->size() ); 
    vector<pair<int,int>>::iterator aadp_it =  A_A_deltas_pos.begin(); 
    for ( auto gaadp_it = gint->deltas_pos_->begin(); gaadp_it != gint->deltas_pos_->end(); aadp_it++, gaadp_it++ ){
       (*aadp_it).first  =  (*idxs_trans_)[ (*gaadp_it).first  ];
       (*aadp_it).second =  (*idxs_trans_)[ (*gaadp_it).second ];
    }

    vector<pair<string,string>> A_A_deltas_rngs_trans;
    for ( pair<int,int>& ctr :  A_A_deltas_pos )
      A_A_deltas_rngs_trans.push_back(make_pair(std_rngs_[ctr.first] ,std_rngs_[ctr.second])) ;
    
    vector<pair<string,string>> A_A_deltas_idxs_trans;
    for ( pair<int,int>& ctr :  A_A_deltas_pos ) 
      A_A_deltas_idxs_trans.push_back(make_pair((*std_ids_)[ctr.first] , (*std_ids_)[ctr.second] )) ;

    print_pair_vector( A_A_deltas_idxs_trans , "A_A_deltas_idxs_trans " ); 
    print_pair_vector( A_A_deltas_idxs , "   A_A_deltas_idxs " ); 

    assert( A_A_deltas_idxs_trans == A_A_deltas_idxs ); 

  } 

  if ( gint->target_target_deltas_pos()->size() != 0 ) { 

    vector<pair<string,string>> T_T_deltas_rngs;
    for ( pair<int,int>& ctr :  *(gint->target_target_deltas_pos()) )
       T_T_deltas_rngs.push_back(make_pair( std_rngs_[ctr.first] , std_rngs_[ctr.second] ) );
    
    vector<pair<string,string>> T_T_deltas_idxs;
    for ( pair<int,int>& ctr :  *(gint->target_target_deltas_pos()) )
       T_T_deltas_idxs.push_back(make_pair( (*std_ids_)[ctr.first] , (*std_ids_)[ctr.second] ) );

    for ( pair<int,int>& ctr :  *(gint->target_target_deltas_pos()) ){
       ctr.first  =  (*idxs_trans_)[ctr.first];
       ctr.second =  (*idxs_trans_)[ctr.second];
    }

    vector<pair<string,string>> T_T_deltas_trans_rngs;
    for ( pair<int,int>& ctr :  *(gint->target_target_deltas_pos()) )
       T_T_deltas_trans_rngs.push_back(make_pair( std_rngs_[ctr.first] , std_rngs_[ctr.second] ) );
    
    vector<pair<string,string>> T_T_deltas_trans_idxs;
    for ( pair<int,int>& ctr :  *(gint->target_target_deltas_pos()) )
       T_T_deltas_trans_idxs.push_back(make_pair( (*std_ids_)[ctr.first] , (*std_ids_)[ctr.second] ) );
  
    print_pair_vector( T_T_deltas_trans_idxs , "T_T_deltas_trans_idxs " ); 
    print_pair_vector( T_T_deltas_idxs , "   T_T_deltas_idxs " ); 

    assert( T_T_deltas_trans_idxs == T_T_deltas_idxs ) ;
  }

  if ( gint->target_A_deltas_pos()->size() != 0 ) { 

 
    vector<pair<string,string>> T_A_deltas_rngs;
    for ( pair<int,int>& ctr :  *(gint->target_A_deltas_pos()) )
       T_A_deltas_rngs.push_back(make_pair( std_rngs_[ctr.first] , std_rngs_[ctr.second] ) );
    
    vector<pair<string,string>> T_A_deltas_idxs;
    for ( pair<int,int>& ctr :  *(gint->target_A_deltas_pos()) )
       T_A_deltas_idxs.push_back(make_pair( (*std_ids_)[ctr.first] , (*std_ids_)[ctr.second] ) );

    for ( pair<int,int>& ctr :  *(gint->target_A_deltas_pos()) ){
      ctr.first  =  (*idxs_trans_)[ctr.first];
      ctr.second =  (*idxs_trans_)[ctr.second];
    }

    vector<pair<string,string>> T_A_deltas_trans_rngs;
    for ( pair<int,int>& ctr :  *(gint->target_A_deltas_pos()) )
       T_A_deltas_trans_rngs.push_back(make_pair( std_rngs_[ctr.first] , std_rngs_[ctr.second] ) );
    
    vector<pair<string,string>> T_A_deltas_trans_idxs;
    for ( pair<int,int>& ctr :  *(gint->target_A_deltas_pos()) )
       T_A_deltas_trans_idxs.push_back(make_pair( (*std_ids_)[ctr.first] , (*std_ids_)[ctr.second] ) );

    print_pair_vector( T_A_deltas_trans_idxs , "T_A_deltas_trans_idxs " ); 
    print_pair_vector( T_A_deltas_idxs , "   T_A_deltas_idxs " ); 

    assert( T_A_deltas_trans_idxs == T_A_deltas_idxs )  ;
 
  } 
  cout << endl << endl;
  return;
}
////////////////////////////////////////////////////////////////////////////
template class GammaGenerator_OrbExcDeriv<double>;
template class GammaGenerator_OrbExcDeriv<std::complex<double>>;
/////////////////////////////////////////////////////////////////////////
