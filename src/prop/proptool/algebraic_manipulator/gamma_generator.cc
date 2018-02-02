#include <bagel_config.h>
#include <map>
#include <src/prop/proptool/algebraic_manipulator/gamma_generator.h>

using namespace std; 
using namespace WickUtils;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//TODO extend for rel case (Bra and Ket can vary,  prev gammas and 
// prev_Bra_info should be constructed from idxs_pos, full_idxs_ranges, full_aops and Ket. 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
GammaInfo::GammaInfo ( shared_ptr<CIVecInfo<double>> Bra_info, shared_ptr<CIVecInfo<double>> Ket_info, 
                       shared_ptr<const vector<bool>> full_aops_vec, shared_ptr<const vector<string>> full_idx_ranges, 
                       shared_ptr<vector<int>> idxs_pos  ,
                       shared_ptr<map<string, shared_ptr<GammaInfo>>> Gamma_map ) :
                       order_(idxs_pos->size()), Bra_info_(Bra_info), Ket_info_(Ket_info) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_GammaInfo
cout << "GammaInfo::GammaInfo" <<  endl;
#endif 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

  id_ranges_ = make_shared<vector<string>>(idxs_pos->size());
  aops_      = make_shared<vector<bool>>(idxs_pos->size());
  for (int ii = 0 ; ii != idxs_pos->size(); ii++ ){ 
    id_ranges_->at(ii) = full_idx_ranges->at(idxs_pos->at(ii));     
    aops_->at(ii)      = full_aops_vec->at(idxs_pos->at(ii));     
  }

  map< string , vector<int> > shift_map ; 
  vector<string> diff_ranges = { "act" } ; // obtain from diff ranges function
  if ( diff_ranges.size() > 1 ){
    for ( string rng : diff_ranges){ 
      vector<int> shift_vec = vector<int>(idxs_pos->size());
      vector<string>::reverse_iterator rngs_it = id_ranges_->rbegin(); 
      vector<bool>::reverse_iterator   aops_it = aops_->rbegin(); 
      int shift = 0;
      for ( vector<int>::reverse_iterator shift_it = shift_vec.rbegin(); shift_it != shift_vec.rend(); shift_it++){
        if ( *rngs_it++ == rng ){
          if ( ( *aops_it++ ) ) {
            shift++;
          } else {
            shift--;
          }
        }
        *shift_it = shift;
        rngs_it++;
      }
      shift_map.emplace(rng, shift_vec); 
    }
  }

  sigma_id_ranges_ = make_shared<vector<string>>(id_ranges_->size()+1);
  sigma_id_ranges_->at(0) = Bra_info_->name();
  for ( int  ii = 1 ; ii != sigma_id_ranges_->size() ; ii++ ) 
    sigma_id_ranges_->at(ii) = id_ranges_->at(ii-1);

  name_ = get_gamma_name( full_idx_ranges, full_aops_vec, idxs_pos, Bra_info_->name(), Ket_info_->name() ); cout << "gamma name = " <<  name_ << endl;
  sigma_name_ = "S_"+name_; 

  if ( (idxs_pos->size() > 2 ) && ( idxs_pos->size() % 2 == 0 ) ) {

    prev_gammas_ = vector<string>(idxs_pos->size()/2); 
    prev_sigmas_ = vector<string>(idxs_pos->size()/2); 

    for (int ii = 2 ; ii != idxs_pos->size() ; ii+=2 ){ 

      shared_ptr<vector<int>> prev_gamma_idxs_pos = make_shared<vector<int>> ( idxs_pos->begin()+ii, idxs_pos->end());
      prev_gammas_[(ii/2)-1] = get_gamma_name( full_idx_ranges, full_aops_vec, prev_gamma_idxs_pos, Bra_info_->name(), Ket_info_->name());   
      prev_sigmas_[(ii/2)-1] = "S_"+prev_gammas_[(ii/2)-1];

      if ( Gamma_map->find( prev_gammas_[(ii/2)-1] )  == Gamma_map->end() ){ //TODO fix Bra_info for rel case
        shared_ptr<GammaInfo> prev_gamma_ = make_shared<GammaInfo>( Bra_info_, Ket_info_, full_aops_vec, full_idx_ranges, prev_gamma_idxs_pos, Gamma_map );
        Gamma_map->emplace( prev_gammas_[(ii/2)-1], prev_gamma_ );
      } 
        
      if(  ii == 2 )
        prev_Bra_info_ = Gamma_map->at( prev_gammas_[0] )->Bra_info() ; 
      
    }
    print_vector( prev_gammas_ , "prev gammas of " + name_ ); cout << endl;
  }

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
GammaGenerator::GammaGenerator( shared_ptr<StatesInfo<double>> target_states, int Bra_num, int Ket_num,
                                shared_ptr<const vector<string>> orig_ids, shared_ptr<const vector<bool>> orig_aops, 
                                shared_ptr<map<string, shared_ptr<GammaInfo>>> Gamma_map_in, 
                                shared_ptr<map<string, shared_ptr<map<string, AContribInfo >>>> G_to_A_map_in,
                                double bk_factor_in ):
                                target_states_(target_states), Bra_num_(Bra_num), Ket_num_(Ket_num), 
                                orig_ids_(orig_ids), orig_aops_(orig_aops), 
                                G_to_A_map(G_to_A_map_in), Gamma_map(Gamma_map_in), bk_factor(bk_factor_in) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_GammaGenerator
cout << "GammaGenerator::GammaGenerator" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "Bra_num_ = " << Bra_num_ << endl;
   cout << "Ket_num_ = " << Ket_num_ << endl;
  //needed to keep ordering of contractions consistent, should find better way of defining opname 
  auto opname = orig_ids_->at(0)[0];
  op_order = make_shared<map< char, int>>();
  int ii =0;
  op_order->emplace (opname, ii);
  for ( string elem : *orig_ids ){
    if ( opname != elem[0] ){
      opname = elem[0];
      op_order->emplace(opname, ++ii);
    }
  } 

  ii=0; //TODO This should not be needed here (I think); normal idx order when initialized
        //     Have shuffled idx order specified in range_block_info in add_gamma
  idx_order = make_shared<map< string, int>>();
  for ( string elem : *orig_ids_ )
    idx_order->emplace(elem, ii);

}
 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GammaGenerator::add_gamma( shared_ptr<range_block_info> block_info ) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::add_gamma" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //TODO have new gamma intermediate constructor with most of this as default.
  shared_ptr<vector<int>> ids_pos_init =  make_shared<vector<int>>(block_info->orig_idxs()->size());
  iota( ids_pos_init->begin() , ids_pos_init->end(), 0 );
  int my_sign_in = 1;
  shared_ptr<vector<pair<int,int>>> deltas_pos_in = make_shared<vector<pair<int,int>>>(0);
  shared_ptr<vector<string>> id_ranges_in = make_shared<vector<string>>(*block_info->orig_block());

  //print_vector(*id_ranges_in, " id_ranges_for gamma_init" ) ; print_vector( *ids_pos_init, "     id_pos_init" ) ; cout << endl; 
  gamma_vec = make_shared<vector<shared_ptr<GammaIntermediate>>>(1, make_shared<GammaIntermediate>(id_ranges_in, ids_pos_init, deltas_pos_in, my_sign_in)); 
  final_gamma_vec = make_shared<vector<shared_ptr<GammaIntermediate>>>(0);

  return; 

}                     
/////////////////////////////////////////////////////////////////////////////////////////////////////////                                                              
bool GammaGenerator::norm_order(){                                                                                   
//////////////////////////////////////////////////////////////////////////////////////////////////////////                                                              
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::norm_order" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////////////
  int kk = 0;                                                                                                      
 
  bool does_it_contribute = false;

  while ( kk != gamma_vec->size()){                                               
    shared_ptr<vector<pair<int,int>>> deltas_pos = gamma_vec->at(kk)->deltas_pos; 
    shared_ptr<const vector<string>>    full_id_ranges = gamma_vec->at(kk)->full_id_ranges;                  
    shared_ptr<vector<int>>              ids_pos = gamma_vec->at(kk)->ids_pos;        

    int num_pops = ( ids_pos->size()/2 )-1;     
 
    for (int ii = ids_pos->size()-1 ; ii != -1; ii--){            
      if ( ii > num_pops ) {                                      
        if (!orig_aops_->at(ids_pos->at(ii)))                      
          continue;                                               
                                                                  
        while(orig_aops_->at( ids_pos->at(ii) )){                  
          for ( int jj = (ii-1); jj != -1 ; jj--) {               
            if (!orig_aops_->at( ids_pos->at(jj) )){               
              swap( jj, jj+1, kk, gamma_vec);           
              break;                                              
            }                                                     
          }                                                       
        }                                                         
      } else if (ii<=num_pops){                                   
                                                                  
          if (orig_aops_->at(ids_pos->at(ii)))                     
            continue;                                             
                                                                  
        while(!orig_aops_->at(ids_pos->at(ii))){                   
          for ( int jj = (ii-1); jj != -1 ; jj--) {               
             if(orig_aops_->at(ids_pos->at(jj)) ){                 
               swap( jj, jj+1, kk, gamma_vec);          
               break;                                             
             }                                                    
           }                                                      
        }                                                         
      }                                                           
    }
    
//    if (!gamma_survives(ids_pos, full_id_ranges) && ids_pos->size() != 0){
//       cout << " These indexes won't survive : [ ";  for ( int pos : *ids_pos ) {cout <<  full_id_ranges->at(pos) << " " ;} cout << "]" << endl;
//       Contract_remaining_indexes(kk);
//    } else {
    //TODO Check this!! (removal contract_remaining_indexes). This makes sense, but seems wrong somehow.  
    if (gamma_survives(ids_pos, full_id_ranges) && ids_pos->size() != 0){
      cout << " These indexes will survive : [ ";  for ( int pos : *ids_pos ) {cout <<  full_id_ranges->at(pos) << " " ;} cout << "]" << endl;
      final_gamma_vec->push_back(gamma_vec->at(kk));
    } 
    kk++;                                                         
  }                                                               

  gamma_vec = final_gamma_vec;

  if ( gamma_vec->size() > 0 ){
    does_it_contribute = true;
    cout << "-----------------------------------------------------" << endl;
    cout << "     LIST OF GAMMAS FOLLOWING NORMAL ORDERING ";  cout << "N = " << final_gamma_vec->size() << endl;
    cout << "-----------------------------------------------------" << endl;
    for ( shared_ptr<GammaIntermediate> gint : *final_gamma_vec ) {
      cout <<  gint->full_id_ranges << endl;
  print_vector(*orig_aops_, "orig_aops"); cout  <<  endl;
  print_vector(*gint->ids_pos, "gint_ids_pos" ); cout << endl;
  cout << " target_states_->name("<<Bra_num_<<") = "; cout.flush(); cout << target_states_->name(Bra_num_) << endl;
  cout << " target_states_->name("<<Ket_num_<<") = "; cout.flush(); cout << target_states_->name(Ket_num_) << endl;
      cout <<  WickUtils::get_gamma_name( gint->full_id_ranges, orig_aops_,  gint->ids_pos, target_states_->name(Bra_num_), target_states_->name(Ket_num_) ) ;
      cout << "   ("<< gint->my_sign <<","<< gint->my_sign << ")       ";
      cout << get_Aname( *(orig_ids_), *(gint->full_id_ranges), *(gint->deltas_pos) ) << endl;
    }
    cout << "-----------------------------------------------------" << endl;
  }

  return does_it_contribute;
}
///////////////////////////////////////////////////////////////////////////////////////
// Replace this with something more sophisticated which uses constraint functions.
///////////////////////////////////////////////////////////////////////////////////////
bool GammaGenerator::Forbidden_Index( shared_ptr<const vector<string>> full_id_ranges,  int position ){
///////////////////////////////////////////////////////////////////////////////////////
//cout << "GammaGenerator::Forbidden_Index" << endl;

  if ( full_id_ranges->at(position)[0] != 'a'){
    return true;
  } else {
    return false;
  }
}
///////////////////////////////////////////////////////////////////////////////////////
void GammaGenerator::Contract_remaining_indexes( int kk ){
//////////////////////////////////////////////////////////////////////////////////////  
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::Contract_remaining_indexes" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////////////

  shared_ptr<vector<int>>           ids_pos = gamma_vec->at(kk)->ids_pos;        
  shared_ptr<const vector<string>>  full_id_ranges = gamma_vec->at(kk)->full_id_ranges;                  
  shared_ptr<vector<pair<int,int>>> deltas_pos = gamma_vec->at(kk)->deltas_pos; 
  int my_sign = gamma_vec->at(kk)->my_sign;
  
  //vector of different index ranges, and vectors containing list of index positions
  //associated with each of these ranges
  vector<string> diff_rngs(0);
  shared_ptr<vector<shared_ptr<vector<int>>>> make_ops_pos =  make_shared<vector<shared_ptr<vector<int>>>>(0);
  shared_ptr<vector<shared_ptr<vector<int>>>> kill_ops_pos =  make_shared<vector<shared_ptr<vector<int>>>>(0);

  int start_pos = 0;
  while( start_pos!= ids_pos->size()  ){
    if ( Forbidden_Index(full_id_ranges, ids_pos->at(start_pos)) )
      break;
    start_pos++;
  }

  // get the positions of the creation and annihilation operators associated with
  // each different range
  for ( int jj = start_pos;  jj != ids_pos->size() ; jj++){
    int ii = 0;
    string rng = full_id_ranges->at(ids_pos->at(jj));
    if ( Forbidden_Index(full_id_ranges, ids_pos->at(jj) ) ) {
      do  {
        if( jj != start_pos  &&  rng == diff_rngs[ii] ){
          if ( orig_aops_->at(ids_pos->at(jj)) ){
            make_ops_pos->at(ii)->push_back(ids_pos->at(jj));
          } else {
            kill_ops_pos->at(ii)->push_back(ids_pos->at(jj));
          }
          break;
        }
                                  
        if (jj == start_pos || ii == diff_rngs.size()-1 ){
          diff_rngs.push_back(rng);
          shared_ptr<vector<int>> init_vec1_ = make_shared<vector<int>>(1,ids_pos->at(jj));
          shared_ptr<vector<int>> init_vec2_ = make_shared<vector<int>>(0);
          if ( orig_aops_->at(ids_pos->at(jj)) ){
            make_ops_pos->push_back(init_vec1_);
            kill_ops_pos->push_back(init_vec2_);
          } else {
            make_ops_pos->push_back(init_vec2_);
            kill_ops_pos->push_back(init_vec1_);
          }
          break;
        }
        ii++; 
      } while (ii != diff_rngs.size()); 
    }
  } 

  // first index  = range.
  // second index = pair vector defining possible way of contracting indexes with that range.
  // third index  = pair defining contraction of indexes with the same range.
  vector<shared_ptr<vector<shared_ptr<vector<pair<int,int>>>>>> new_contractions(diff_rngs.size());

  for (int ii =0 ; ii != new_contractions.size(); ii++)
    new_contractions[ii] = get_unique_pairs( make_ops_pos->at(ii), kill_ops_pos->at(ii), make_ops_pos->at(ii)->size() );

  shared_ptr<vector<int>> forvec = make_shared<vector<int>>(diff_rngs.size(),0) ;
  shared_ptr<vector<int>> min = make_shared<vector<int>>(diff_rngs.size(),0) ;
  shared_ptr<vector<int>> max = make_shared<vector<int>>(diff_rngs.size()) ;

  for (int ii = 0; ii != max->size();  ii++)
    max->at(ii) = make_ops_pos->at(ii)->size()-1;

  do {

    shared_ptr<vector<pair<int,int>>> new_deltas_pos_tmp = make_shared<vector<pair<int,int>>>(*deltas_pos);
    for ( int qq = 0; qq != forvec->size(); qq++)
      new_deltas_pos_tmp->insert(new_deltas_pos_tmp->end(), new_contractions[qq]->at(forvec->at(qq))->begin(), new_contractions[qq]->at(forvec->at(qq))->end()); 

    shared_ptr<vector<pair<int,int>>> new_deltas_pos = Standardize_delta_ordering_generic( new_deltas_pos_tmp ) ;                                                                
    shared_ptr<vector<int>> new_ids_pos =  get_unc_ids_from_deltas_ids_comparison( ids_pos , new_deltas_pos );
    final_gamma_vec->push_back(make_shared<GammaIntermediate>(full_id_ranges, new_ids_pos, new_deltas_pos, my_sign)); 
   
  } while ( fvec_cycle_skipper(forvec, max, min) ) ;

  return;
}

///////////////////////////////////////////////////////////////////////////////////////
//This is absurdly inefficient; will probably need to be replaced.
//The ranged idxs are index+plus range, should switch to range+index later on.
//
//Must add special case for dealing with excitation operators.
///////////////////////////////////////////////////////////////////////////////////////
bool GammaGenerator::optimized_alt_order(){
//////////////////////////////////////////////////////////////////////////////////////  
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::optimized_alt_order" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////////////

  bool does_it_contribute = false;
  if ( gamma_vec->size() != 0 ) {

    string Bra_name = target_states_->name(Bra_num_);
    string Ket_name = target_states_->name(Ket_num_);

    int kk = 0;
    while ( kk != gamma_vec->size()){
      shared_ptr<vector<int>>              ids_pos = gamma_vec->at(kk)->ids_pos;
      shared_ptr<const vector<string>>    full_id_ranges = gamma_vec->at(kk)->full_id_ranges;
      shared_ptr<vector<pair<int,int>>> deltas_pos = gamma_vec->at(kk)->deltas_pos;

      cout << " reordering gamma : " <<  get_gamma_name( full_id_ranges, orig_aops_, ids_pos, Bra_name, Ket_name ) << " --> ";

      vector<string> unc_ranged_idxs(full_id_ranges->size() - 2*deltas_pos->size());
      vector<string>::iterator unc_ranged_idxs_it = unc_ranged_idxs.begin();
      vector<bool> unc_aops(orig_aops_->size() - 2*deltas_pos->size());

      vector<bool>::iterator unc_aops_it = unc_aops.begin();
      for (int pos : *ids_pos ) {
        *unc_ranged_idxs_it++ = orig_ids_->at(pos)+full_id_ranges->at(pos);
        *unc_aops_it++ = orig_aops_->at(pos);
      }

      vector<int> standard_alt_order = get_standardized_alt_order( unc_ranged_idxs, unc_aops );

      shared_ptr<vector<int>> new_ids_pos = reorder_vector( standard_alt_order, *ids_pos);    
      for (int ii = ids_pos->size()-1 ; ii != -1; ii-- ){
        if (ids_pos->at(ii) == new_ids_pos->at(ii))
          continue;
    
        while( ids_pos->at(ii) != new_ids_pos->at(ii) ){
          for ( int jj = (ii-1); jj != -1 ; jj--) {
            if (ids_pos->at(jj) == new_ids_pos->at(ii)){
              swap( jj, jj+1, kk, gamma_vec);
              break;
            }
          }
        }
      }

      // should be fully reordered by this point, so can apply factor. 
      // This section should be taking out and put into a seperate function; keep ordering and
      // map creation parts seperate
      double my_sign = bk_factor*gamma_vec->at(kk)->my_sign ;

      string Aname_alt = get_Aname( *orig_ids_, *full_id_ranges, *deltas_pos );
      string Gname_alt = get_gamma_name( full_id_ranges, orig_aops_, ids_pos, Bra_name, Ket_name );
      cout << Gname_alt << endl;

      if ( G_to_A_map->find( Gname_alt ) == G_to_A_map->end() ) 
        G_to_A_map->emplace( Gname_alt, make_shared<map<string, AContribInfo>>() );

      vector<int> Aid_order_new = get_Aid_order ( *ids_pos ) ; 
      auto AInfo_loc =  G_to_A_map->at( Gname_alt )->find(Aname_alt);
      if ( AInfo_loc == G_to_A_map->at( Gname_alt )->end() ) {
        AContribInfo AInfo( Aname_alt, Aid_order_new, make_pair(my_sign,my_sign), Bra_num_, Ket_num_ );
        G_to_A_map->at( Gname_alt )->emplace(Aname_alt, AInfo) ;
        cout << "adding " << Aname_alt << "contrib to " << Gname_alt << endl;

      } else {

        //put new contributions into map, change factors as appropriate, if all factors go to zero remove entry from map.
        AContribInfo AInfo = AInfo_loc->second;
        for ( int qq = 0 ; qq != AInfo.id_orders.size(); qq++ ) {
          if( Aid_order_new == AInfo.id_order(qq) ){
            AInfo.factors[qq].first  += my_sign;
            AInfo.factors[qq].second += my_sign; 
            AInfo.remaining_uses_ += 1;
            AInfo.total_uses_ += 1;
      //      if ( AInfo.factors[qq].first == 0 &&  AInfo.factors[qq].second == 0 ) {
      //        AInfo.factors.erase( AInfo.factors.begin()+qq ); 
      //        AInfo.id_orders.erase( AInfo.id_orders.begin()+qq ); 
      //        if (AInfo.id_orders.size() == 0 ){ 
      //          cout << "erasing " << Aname_alt << " contrib to " << Gname_alt << endl; 
      //          G_to_A_map->at( Gname_alt )->erase(AInfo_loc);
      //        }
      //    }
            break;

          } else if ( qq == AInfo.id_orders.size()-1) { 
            AInfo.id_orders.push_back(Aid_order_new);
            AInfo.factors.push_back(make_pair(my_sign,my_sign));
          }
        }
      }
      
      Gamma_map->emplace( Gname_alt, make_shared<GammaInfo>( target_states_->civec_info(Bra_num_), target_states_->civec_info(Ket_num_), 
                                                             orig_aops_, full_id_ranges, ids_pos, Gamma_map) );
      
      kk++;
    }
    if ( final_gamma_vec->size() > 0 ) {
    does_it_contribute = true; 
    cout << "-----------------------------------------------------" << endl;
      cout << "     LIST OF GAMMAS FOLLOWING ALT ORDERING " << endl; 
      cout << "-----------------------------------------------------" << endl;
      for ( shared_ptr<GammaIntermediate> gint : *final_gamma_vec ) {
        string Gname_tmp = WickUtils::get_gamma_name( gint->full_id_ranges, orig_aops_,  gint->ids_pos, target_states_->name(Bra_num_), target_states_->name(Ket_num_) ) ;
        cout <<Gname_tmp <<  "   ("<< gint->my_sign <<","<< gint->my_sign << ")       " ;
        cout << get_Aname( *(orig_ids_), *(gint->full_id_ranges), *(gint->deltas_pos) ) << endl;
      }
      cout << "-----------------------------------------------------" << endl;
      
    }
  } 
 
  return does_it_contribute;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////    
// Swaps indexes round, flips sign, and if ranges are the same puts new density matrix in the list. 
// CAREFUL : always keep creation operator as the left index in the contraction . 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////    
void GammaGenerator::swap( int ii, int jj, int kk, shared_ptr<vector<shared_ptr<GammaIntermediate>>> gamma_vec  ){                                                  
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////    
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::swap" << endl; 
#endif 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  shared_ptr<const vector<string>> full_id_ranges = gamma_vec->at(kk)->full_id_ranges;
  shared_ptr<vector<int>>  ids_pos             = gamma_vec->at(kk)->ids_pos;
  shared_ptr<vector<pair<int,int>>> deltas_pos = gamma_vec->at(kk)->deltas_pos;

  int idx_buff = ids_pos->at(ii);
  ids_pos->at(ii) = ids_pos->at(jj);
  ids_pos->at(jj) = idx_buff;
                                                                                                                                    
  if ( (full_id_ranges->at(ids_pos->at(jj)) == full_id_ranges->at(ids_pos->at(ii))) && 
       (orig_aops_->at(ids_pos->at(ii)) !=  orig_aops_->at(ids_pos->at(jj))) ){

    shared_ptr<pint_vec> new_deltas_tmp = make_shared<pint_vec>(*deltas_pos);                                                                       

    pair<int,int> new_delta = orig_aops_->at(ids_pos->at(jj)) ? make_pair(ids_pos->at(jj), ids_pos->at(ii)) : make_pair(ids_pos->at(ii), ids_pos->at(jj));
    new_deltas_tmp->push_back(new_delta);
    shared_ptr<pint_vec> new_deltas = Standardize_delta_ordering_generic( new_deltas_tmp );
    int new_sign = gamma_vec->at(kk)->my_sign;

    shared_ptr<vector<int>> new_ids_pos = make_shared<vector<int>>();
    for( int qq = 0 ; qq !=ids_pos->size() ; qq++)
      if ( (qq !=ii) && (qq!=jj))
        new_ids_pos->push_back(ids_pos->at(qq));

    shared_ptr<GammaIntermediate> new_gamma = make_shared<GammaIntermediate>(full_id_ranges, new_ids_pos, new_deltas, new_sign);
    gamma_vec->push_back(new_gamma);

  }
  gamma_vec->at(kk)->my_sign *= -1;

  return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
shared_ptr<pint_vec>  GammaGenerator::Standardize_delta_ordering_generic(shared_ptr<pint_vec> deltas_pos ) {
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::Standardize_delta_ordering_generic" << endl; 
#endif 
////////////////////////////////////////////////////////////////////////////////////////////////////

 shared_ptr<vector<pair<int,int>>> new_deltas_pos;

 if (deltas_pos->size() > 1 ) {
   new_deltas_pos =  make_shared <vector<pair<int,int>>>( deltas_pos->size());
   vector<int> posvec(deltas_pos->size(),0);
   for (int ii = 0 ; ii != deltas_pos->size() ; ii++) 
     for (int jj = 0 ; jj != deltas_pos->size() ; jj++) 
       if (deltas_pos->at(ii).first > deltas_pos->at(jj).first )
         posvec[ii]++ ;      
   
   for (int ii = 0 ; ii != deltas_pos->size() ; ii++) 
     new_deltas_pos->at(posvec[ii]) = deltas_pos->at(ii);

 } else { 
   new_deltas_pos = deltas_pos;
 }
  return new_deltas_pos;   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
shared_ptr<pint_vec> GammaGenerator::Standardize_delta_ordering(shared_ptr<pint_vec> deltas_pos ) {
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::Standardize_delta_ordering" << endl; 
#endif 
////////////////////////////////////////////////////////////////////////////////////////////////////

   bool(GammaGenerator::*orderfunc)(pair<string,string>, pair<string,string>) = &GammaGenerator::ordering;

   auto dtmp = make_shared<pint_vec>(*deltas_pos);

   if ( orig_aops_->at( dtmp->back().second ))
     dtmp->back() = make_pair( deltas_pos->back().second, deltas_pos->back().first );	
   

   //sort(dtmp->begin(), dtmp->end() );
   // note that by construction only the last element of dtmp could be in the wrong place.
   int ii = 0;
   while (ii < dtmp->size()-1) {
     if ( op_order->at(orig_ids_->at(dtmp->back().first)[0])  < op_order->at(orig_ids_->at(dtmp->at(ii).first)[0]) ){

       auto dtmp2 = make_shared<pint_vec>();
       
       for (int qq= 0 ; qq != ii ; qq++)
          dtmp2->push_back(dtmp->at(qq));
       
       dtmp2->push_back(dtmp->back());
       
       for (int qq= ii ; qq !=dtmp->size()-1 ; qq++)
          dtmp2->push_back(dtmp->at(qq));
         
       dtmp = dtmp2;
       break;
      }
      ii++;
  }
  deltas_pos = dtmp;
  return deltas_pos;   
}

///////////////////////////////////////////////////////////////////////////////                                           
//Most basic constraint of all active
//should instead take function
///////////////////////////////////////////////////////////////////////////////                                   
bool GammaGenerator::gamma_survives(shared_ptr<vector<int>> ids_pos, shared_ptr<const vector<string>> id_ranges) {
///////////////////////////////////////////////////////////////////////////////
#ifdef DBG_GammaGenerator 
cout << "GammaGenerator::gamma_survives" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////

 for (int ii = 0 ; ii != ids_pos->size(); ii++  )
   if ( Forbidden_Index(id_ranges, ids_pos->at(ii) ) ) 
     return false;
 
 return true;

}
//////////////////////////////////////////////////////////////////////////////
// This should not be necessary, but keep it for debugging
//////////////////////////////////////////////////////////////////////////////
bool GammaGenerator::RangeCheck(shared_ptr<const vector<string>> full_id_ranges) {
//////////////////////////////////////////////////////////////////////////////
#if defined DBG_GammaGenerator || defined DBG_all  
cout << "GammaGenerator::RangeCheck" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////

  vector<string> diff_rngs(1, full_id_ranges->at(0) );
  vector<int> updown(1, ( orig_aops_->at(0) ? 1 : -1)  );

  for ( int jj = 1;  jj !=full_id_ranges->size() ; jj++){
    int ii = 0;

    string rng = full_id_ranges->at(jj);

    do {
      if(rng == diff_rngs[ii]){
        if ( orig_aops_->at(jj)){
          updown[ii]+=1;
        } else {         
          updown[ii]-=1;
        }
        break;
      }
      if ( ii == diff_rngs.size()-1){
        diff_rngs.push_back(rng);
        if ( orig_aops_->at(jj)){
          updown.push_back(1);  
        } else {
          updown.push_back(-1); 

        }
        break;
      }

      ii++;
    } while (true);
  } 

  for (int ac : updown ) 
    if (ac != 0 )
      return false;
 
  return true;

} 

//////////////////////////////////////////////////////////////////////////////
vector<int> GammaGenerator::get_standard_range_order(const vector<string> &rngs) {
//////////////////////////////////////////////////////////////////////////////
  
  vector<int> pos(rngs.size());
  iota(pos.begin(), pos.end(), 0);
  sort(pos.begin(), pos.end(), [&rngs](int i1, int i2){
//                                 if ( rngs[i1][0] == 'X' ){
//                                   return false;
//                                 } else {
                                   return (bool)( rngs[i1] < rngs[i2] );
//                                 }
});
  
  return pos;
}

//////////////////////////////////////////////////////////////////////////////
vector<int> GammaGenerator::get_standard_idx_order(const vector<string>&idxs) {
//////////////////////////////////////////////////////////////////////////////
  
  vector<int> pos(idxs.size());
  iota(pos.begin(), pos.end(), 0);
 
  auto op_order_tmp = op_order; 
  sort(pos.begin(), pos.end(), [&idxs, &op_order_tmp](int i1, int i2){
                            //     if ( idxs[i1][0] == 'X' ){
                            //       return false;
                            //     } else {
                                   return (bool)( op_order_tmp->at(idxs[i1][0]) < op_order_tmp->at(idxs[i2][0]) );
                            //     }
                                 }
                            );
  
  return pos;
}


///////////////////////////////////////////////////////////////////////////////////////
//Returns ordering vector for ids_pos, e.g., if ids_pos = {6,4,5} , then pos = {1,2,0}
///////////////////////////////////////////////////////////////////////////////////////
vector<int> GammaGenerator::get_position_order(const vector<int> &ids_pos) {
///////////////////////////////////////////////////////////////////////////////////////
  
  vector<int> pos(ids_pos.size());
  iota(pos.begin(), pos.end(), 0);
  sort(pos.begin(), pos.end(), [&ids_pos](int i1, int i2){return (bool)( ids_pos[i1] < ids_pos[i2] ); });
  
  return pos;
}

///////////////////////////////////////////////////////////////////////////////////////
vector<int> GammaGenerator::get_standard_order ( const vector<string>& rngs ) { 
///////////////////////////////////////////////////////////////////////////////////////

  vector<string> new_rngs(rngs.size());
  vector<string>::iterator new_rngs_it = new_rngs.begin();
  vector<int> new_order = get_standard_idx_order(rngs) ;

  for ( int pos : new_order ) { *new_rngs_it++ = rngs[pos] ; }
  return new_order;
}
///////////////////////////////////////////////////////////////////////////////////////
// Getting reordering vec to go from Atens uncids to gamma uncids
// Running sort twice to get inverse; seems weird and there's probably a better way...
///////////////////////////////////////////////////////////////////////////////////////
vector<int> GammaGenerator::get_Aid_order ( const vector<int>& id_pos ) { 
///////////////////////////////////////////////////////////////////////////////////////
//   cout << "GammaGenerator::get_Aid_order " << endl;
  
  vector<int> new_id_pos(id_pos.size());
  vector<int> tmp_order = get_position_order(id_pos);
  vector<int> new_order = get_position_order(tmp_order) ;

  return new_order;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
vector<int> GammaGenerator::get_standardized_alt_order ( const vector<string>& rngs ,const vector<bool>& aops ) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   
  vector<int> standard_order = get_standard_order(rngs) ; 
  vector<int> standard_order_plus(standard_order.size()/2);
  vector<int> standard_order_kill(standard_order.size()/2);
  vector<int>::iterator standard_order_plus_it = standard_order_plus.begin();
  vector<int>::iterator standard_order_kill_it = standard_order_kill.begin();
  for ( int pos : standard_order) {
    if ( aops[pos] ) {
      *standard_order_plus_it++ = pos;
    } else { 
      *standard_order_kill_it++ = pos;
    }
  }

  vector<int> standard_alt_order(standard_order.size());
  vector<int>::iterator standard_alt_order_it = standard_alt_order.begin();
  standard_order_plus_it = standard_order_plus.begin();
  standard_order_kill_it = standard_order_kill.begin();
  while ( standard_alt_order_it != standard_alt_order.end()){
    *standard_alt_order_it++ = *standard_order_plus_it++;
    *standard_alt_order_it++ = *standard_order_kill_it++;
  }
  
  return standard_alt_order;
};


