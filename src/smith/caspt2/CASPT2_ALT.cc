//
// BAGEL - Brilliantly Advanced General Electronic Structure Library
// Filename: CASPT2.cc
// Copyright (C) 2014 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <bagel_config.h>
#ifdef COMPILE_SMITH
#include <src/smith/caspt2/CASPT2_ALT.h>

using namespace std;
using namespace bagel;
using namespace bagel::SMITH;
using namespace bagel::SMITH::TensOp_Computer; 
using namespace bagel::SMITH::Gamma_Computer; 
using namespace Tensor_Arithmetic_Utils;

////////////////////////////////////////////////////////////////////
CASPT2_ALT::CASPT2_ALT::CASPT2_ALT(const CASPT2::CASPT2& orig_cpt2_in ) { 
////////////////////////////////////////////////////////////////////
  cout << " CASPT2_ALT::CASPT2_ALT::CASPT2_ALT" << endl;

  civectors = orig_cpt2_in.info_->ciwfn()->civectors();

  T2_all     = orig_cpt2_in.t2all_;  
  lambda_all = orig_cpt2_in.lall_;   
  F_1el_all  = orig_cpt2_in.f1_;     cout << "F_1el_all->rms()" <<  F_1el_all->rms() << endl; 
  H_1el_all  = orig_cpt2_in.h1_;     cout << "H_1el_all->rms()" <<  H_1el_all->rms() << endl; 
  H_2el_all  = orig_cpt2_in.H_2el_;  cout << "H_2el_all->rms()" <<  H_2el_all->rms() << endl; 
  v2_        = orig_cpt2_in.v2_;     cout << "v2_->rms()" <<  v2_->rms() << endl; 


  ncore   = orig_cpt2_in.info_->ncore();
  nclosed = orig_cpt2_in.info_->nclosed();
  nact    = orig_cpt2_in.info_->nact();
  nvirt   = orig_cpt2_in.info_->nvirt();
  maxtile = orig_cpt2_in.info_->maxtile();
  cimaxtile = 100000;

  TensOp_data_map    = make_shared<map<string, shared_ptr<Tensor_<double>>>>();
  Gamma_data_map     = make_shared<map<string, shared_ptr<Tensor_<double>>>>();
  scalar_results_map = make_shared<map<string, double>>();
  range_conversion_map = make_shared<map<string, shared_ptr<IndexRange>>>();

  CIvec_data_map   = make_shared<std::map<std::string, std::shared_ptr<Tensor_<double>>>>();    
  Sigma_data_map   = make_shared<std::map<std::string, std::shared_ptr<Tensor_<double>>>>();    

  shared_ptr<vector<int>> states_of_interest = make_shared<vector<int>>( vector<int> { 0 } );
  set_target_info(states_of_interest) ;
  set_range_info(states_of_interest);
   
  Sys_Info = make_shared<System_Info<double>>(TargetsInfo, true);
  Expression_map = Sys_Info->expression_map;
  Expression_Machine = make_shared<Expression_Computer::Expression_Computer<double>>(  civectors, Expression_map, range_conversion_map, TensOp_data_map, 
                                                                                       Gamma_data_map, Sigma_data_map, CIvec_data_map );

  ////////////////// FOR TESTING ///////////////////////////////////////  
  shared_ptr<CASPT2::CASPT2> orig_cpt2 = make_shared<CASPT2::CASPT2>(orig_cpt2_in);
  orig_cpt2->set_rdm(0,0);
  Smith_rdm1 = orig_cpt2->rdm1_; TensOp_data_map->emplace( "Smith_rdm1", Smith_rdm1 );
  Smith_rdm2 = orig_cpt2->rdm2_; TensOp_data_map->emplace( "Smith_rdm2", Smith_rdm2 );
  Smith_rdm3 = orig_cpt2->rdm3_; TensOp_data_map->emplace( "Smith_rdm3", Smith_rdm3 );
  Smith_rdm4 = orig_cpt2->rdm4_; TensOp_data_map->emplace( "Smith_rdm4", Smith_rdm4 );

  shared_ptr<vector<int>> gorder = make_shared<vector<int>>( vector<int> {  1, 2, 0, 3} ); 
  shared_ptr<Tensor_<double>> rdm2_reord = 
  Tensor_Arithmetic::Tensor_Arithmetic<double>::reorder_block_Tensor( Smith_rdm2, gorder );
 
  vector<Index> id_block_rdm2 = { active_rng->range(0), active_rng->range(0), active_rng->range(0),active_rng->range(0)}; 
  vector<Index> id_block_rdm1 = { active_rng->range(0), active_rng->range(0) }; 

  unique_ptr<double[]> rdm2_data = rdm2_reord->get_block( id_block_rdm2 );
  unique_ptr<double[]> rdm1_data = Smith_rdm1->get_block( id_block_rdm1 );
 
}
//////////////////////////////////////////////////////////////////////////////////////////////
void CASPT2_ALT::CASPT2_ALT::set_range_info(shared_ptr<vector<int>> states_of_interest ) {
//////////////////////////////////////////////////////////////////////////////////////////////
  cout << "CASPT2_ALT::CASPT2_ALT::set_range_info " << endl;

  cout << maxtile << endl;
  int maxtile_buff = maxtile; 
  closed_rng  =  make_shared<IndexRange>(IndexRange(nclosed-ncore, maxtile, 0, ncore));
  active_rng  =  make_shared<IndexRange>(IndexRange(nact, min(10,maxtile_buff), closed_rng->nblock(), ncore + closed_rng->size()));
  virtual_rng =  make_shared<IndexRange>(IndexRange(nvirt, maxtile, closed_rng->nblock()+ active_rng->nblock(), ncore+closed_rng->size()+active_rng->size()));
  free_rng    = make_shared<IndexRange>(*closed_rng);
  free_rng->merge(*active_rng);
  free_rng->merge(*virtual_rng);

  not_closed_rng  =  make_shared<IndexRange>(*active_rng); not_closed_rng->merge(*virtual_rng);
  not_active_rng  =  make_shared<IndexRange>(*closed_rng); not_active_rng->merge(*virtual_rng);
  not_virtual_rng =  make_shared<IndexRange>(*closed_rng); not_virtual_rng->merge(*active_rng);

  PT2_ranges_ = { *not_closed_rng, *not_closed_rng, *not_virtual_rng, *not_virtual_rng};
  PT2_ranges_herm_conj_ = {  *not_virtual_rng, *not_virtual_rng, *not_closed_rng, *not_closed_rng};

  range_conversion_map->emplace("cor", closed_rng);//change the naming of the ranges from cor to clo... 
  range_conversion_map->emplace("act", active_rng);
  range_conversion_map->emplace("A", active_rng); //fudge, but leave for now
  range_conversion_map->emplace("vir", virtual_rng);
  range_conversion_map->emplace("free", free_rng);

  range_conversion_map->emplace("notcor", not_closed_rng);
  range_conversion_map->emplace("notact", not_active_rng);
  range_conversion_map->emplace("notvir", not_virtual_rng); 
 
  cout << "range sizes" << endl;
  cout << "cor    : " <<  closed_rng->size() << endl; 
  cout << "act    : " <<  active_rng->size() << endl; 
  cout << "A      : " <<  active_rng->size()<< endl;  
  cout << "vir    : " <<  virtual_rng->size()<< endl; 
  cout << "free   : " <<  free_rng->size()<< endl; 
  cout << "notcor : " <<  not_closed_rng->size()<< endl; 
  cout << "notact : " <<  not_active_rng->size() << endl; 
  cout << "notvir : " <<  not_virtual_rng->size() << endl;  
 
  for ( int ii : *states_of_interest ) {

    range_conversion_map->emplace( get_civec_name( ii , civectors->data(ii)->det()->norb(), civectors->data(ii)->det()->nelea(), civectors->data(ii)->det()->neleb()),
                                                   make_shared<IndexRange>(civectors->data(ii)->det()->size(), cimaxtile ));  
    
    shared_ptr<IndexRange>  ci_index_ranges =  make_shared<IndexRange>(civectors->data(ii)->det()->size(), cimaxtile );
    cout << "cirngs = [ ";  for (auto irng : ci_index_ranges->range()) { cout << irng.size()  << " "; }; cout << "] " << endl;

  }    

  return;

}
//////////////////////////////////////////////////////////////////////////////////////////////
// defining this so we don't need to put determinants class into gamma generator etc.
// At the moment this is quite silly; the determinant space is the same for every space,
// however, this is not necessarily the case. This function will need to do something 
// different in the relativistic case.
//////////////////////////////////////////////////////////////////////////////////////////////
void CASPT2_ALT::CASPT2_ALT::set_target_info( shared_ptr<vector<int>> states_of_interest) {
//////////////////////////////////////////////////////////////////////////////////////////////
  TargetsInfo = make_shared<StatesInfo<double>> ( *states_of_interest ) ;
  
  for ( int state_num : *states_of_interest ) 
     TargetsInfo->add_state( civectors->data(state_num)->det()->nelea(), civectors->data(state_num)->det()->neleb(),
                             civectors->data(state_num)->det()->norb(), state_num ) ;
  
  return;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CASPT2_ALT::CASPT2_ALT::calculate_term( vector<int>& Target_states_list, vector<pair<vector<string>,double>>& BK_info_list, string term_type ) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
  int num_states = Target_states_list.size();

  // Building all necessary expressions 
  vector<vector<Term_Info<double>>> Term_info_list( Target_states_list.size()*Target_states_list.size() );
  for ( int ii : Target_states_list ) {
    string Bra_name = TargetsInfo->name(ii); // TODO change to vector for relativistic case           
    for ( int  jj : Target_states_list ) {
      string Ket_name = TargetsInfo->name(jj); // TODO change to vector for relativistic case           

      shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
      for ( pair<vector<string>,double> BK_info : BK_info_list ){
        term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, term_type ));
        for ( string Op_name : BK_info.first )  
          Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
      }
  
      string expression_name = Sys_Info->Build_Expression( *term_info_ss );
      Expression_Machine->Evaluate_Expression( expression_name );
  
    }
  }

}
////////////////////////////////////////////////////////////////////
void CASPT2_ALT::CASPT2_ALT::solve() {
////////////////////////////////////////////////////////////////////
cout <<  " CASPT2_ALT::CASPT2_ALT::solve() " << endl;

 vector<string> lp = { "L", "P" };
 vector<string> lm = { "L", "M" };
 vector<string> ln = { "L", "N" };
 vector<string> lh = { "L", "H" };
 vector<string> lq = { "L", "Q" };

 {
   vector< pair<vector<string>,double> > BK_info_list = {  make_pair( lm, 1.0 ), make_pair(ln, 1.0), make_pair(lp, 1.0) }  ;
  
   map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
   // Building all necessary expressions 
   int  num_states = 1; 
   
   vector<vector<Term_Info<double>>> Term_info_list( num_states*num_states );
   for ( auto ii  :  TargetsInfo->civec_info_map) {
     for ( auto jj : TargetsInfo->civec_info_map) {
       string Bra_name = ii.first ;            
       string Ket_name = jj.first ;            

       shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
       Term_info_map.emplace(make_pair( Bra_name, Ket_name), term_info_ss); 

       for ( pair<vector<string>,double> BK_info : BK_info_list ){
         term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, "expectation" ));
         for ( string Op_name : BK_info.first )  
           Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
       }
   
       string expression_name = Sys_Info->Build_Expression( *term_info_ss );
       Expression_Machine->Evaluate_Expression( expression_name );
  
     }
   }
 }
 {
   vector< pair<vector<string>,double> > BK_info_list = {  make_pair(lm, 1.0) }  ;
  
   map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
   // Building all necessary expressions 
   int  num_states = 1; 
   
   vector<vector<Term_Info<double>>> Term_info_list( num_states*num_states );
   for ( auto ii  :  TargetsInfo->civec_info_map) {
     for ( auto jj : TargetsInfo->civec_info_map) {
       string Bra_name = ii.first ;            
       string Ket_name = jj.first ;            

       shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
       Term_info_map.emplace(make_pair( Bra_name, Ket_name), term_info_ss); 

       for ( pair<vector<string>,double> BK_info : BK_info_list ){
         term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, "expectation" ));
         for ( string Op_name : BK_info.first )  
           Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
       }
   
       string expression_name = Sys_Info->Build_Expression( *term_info_ss );
       Expression_Machine->Evaluate_Expression( expression_name );
  
     }
   }
 }


 {
   vector< pair<vector<string>,double> > BK_info_list = {  make_pair(ln, 1.0) }  ;
  
   map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
   // Building all necessary expressions 
   int  num_states = 1; 
   
   vector<vector<Term_Info<double>>> Term_info_list( num_states*num_states );
   for ( auto ii  :  TargetsInfo->civec_info_map) {
     for ( auto jj : TargetsInfo->civec_info_map) {
       string Bra_name = ii.first ;            
       string Ket_name = jj.first ;            

       shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
       Term_info_map.emplace(make_pair( Bra_name, Ket_name), term_info_ss); 

       for ( pair<vector<string>,double> BK_info : BK_info_list ){
         term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, "expectation" ));
         for ( string Op_name : BK_info.first )  
           Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
       }
   
       string expression_name = Sys_Info->Build_Expression( *term_info_ss );
       Expression_Machine->Evaluate_Expression( expression_name );
  
     }
   }
 }
 {
   vector< pair<vector<string>,double> > BK_info_list = { make_pair(lp, 1.0) }  ;
  
   map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
   // Building all necessary expressions 
   int  num_states = 1; 
   
   vector<vector<Term_Info<double>>> Term_info_list( num_states*num_states );
   for ( auto ii  :  TargetsInfo->civec_info_map) {
     for ( auto jj : TargetsInfo->civec_info_map) {
       string Bra_name = ii.first ;            
       string Ket_name = jj.first ;            

       shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
       Term_info_map.emplace(make_pair( Bra_name, Ket_name), term_info_ss); 

       for ( pair<vector<string>,double> BK_info : BK_info_list ){
         term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, "expectation" ));
         for ( string Op_name : BK_info.first )  
           Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
       }
   
       string expression_name = Sys_Info->Build_Expression( *term_info_ss );
       Expression_Machine->Evaluate_Expression( expression_name );
  
     }
   }
 }
 {
   vector< pair<vector<string>,double> > BK_info_list = { make_pair(lq, 1.0) }  ;
  
   map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
   // Building all necessary expressions 
   int  num_states = 1; 
   
   vector<vector<Term_Info<double>>> Term_info_list( num_states*num_states );
   for ( auto ii  :  TargetsInfo->civec_info_map) {
     for ( auto jj : TargetsInfo->civec_info_map) {
       string Bra_name = ii.first ;            
       string Ket_name = jj.first ;            

       shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
       Term_info_map.emplace(make_pair( Bra_name, Ket_name), term_info_ss); 

       for ( pair<vector<string>,double> BK_info : BK_info_list ){
         term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, "expectation" ));
         for ( string Op_name : BK_info.first )  
           Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
       }
   
       string expression_name = Sys_Info->Build_Expression( *term_info_ss );
       Expression_Machine->Evaluate_Expression( expression_name );
  
     }
   }
 }


{
   vector< pair<vector<string>,double> > BK_info_list = { make_pair(lh, 1.0) }  ;
   map< pair<string,string>, shared_ptr<vector<Term_Info<double>>> > Term_info_map;
  
   // Building all necessary expressions 
   int  num_states = 1; 
   
   vector<vector<Term_Info<double>>> Term_info_list( num_states*num_states );
   for ( auto ii  :  TargetsInfo->civec_info_map) {
     for ( auto jj : TargetsInfo->civec_info_map) {
       string Bra_name = ii.first ;            
       string Ket_name = jj.first ;            

       shared_ptr<vector<Term_Info<double>>> term_info_ss = make_shared<vector<Term_Info<double>>>(); 
       Term_info_map.emplace(make_pair( Bra_name, Ket_name), term_info_ss); 

       for ( pair<vector<string>,double> BK_info : BK_info_list ){
         term_info_ss->push_back(Term_Info<double>( BK_info, Bra_name, Ket_name, "expectation" ));
         for ( string Op_name : BK_info.first )  
           Set_Tensor_Ops_Data( Op_name, Bra_name, Ket_name ); 
       }
   
       string expression_name = Sys_Info->Build_Expression( *term_info_ss );
       Expression_Machine->Evaluate_Expression( expression_name );
  
     }
   }
 }

 return;
} 
/////////////////////////////////////////////////////////////////////////////////
//Build the operators here. 
/////////////////////////////////////////////////////////////////////////////////
void CASPT2_ALT::CASPT2_ALT::Set_Tensor_Ops_Data( string op_name, string bra_name , string ket_name )  { 
/////////////////////////////////////////////////////////////////////////////////
cout << "CASPT2_ALT::CASPT2_ALT::Set_Tensor_Ops_Data() " << endl;

  vector<IndexRange> ffff = { *free_rng, *free_rng, *free_rng, *free_rng};

  vector<IndexRange> aaaa = { *active_rng,  *active_rng,  *active_rng, *active_rng};
  vector<IndexRange> vvaa = { *virtual_rng, *virtual_rng, *active_rng, *active_rng} ;

  vector<IndexRange> vvcc = { *virtual_rng, *virtual_rng, *closed_rng, *closed_rng} ;
  vector<IndexRange> vvac = { *virtual_rng, *virtual_rng, *active_rng, *closed_rng} ;
  vector<IndexRange> vvca = { *virtual_rng, *virtual_rng, *closed_rng, *active_rng} ;

  vector<IndexRange> vcvc = { *virtual_rng, *closed_rng, *virtual_rng, *closed_rng} ;
  vector<IndexRange> vavc = { *virtual_rng, *active_rng, *virtual_rng, *closed_rng} ;
  vector<IndexRange> vcva = { *virtual_rng, *closed_rng, *virtual_rng, *active_rng} ;
  vector<IndexRange> vava = { *virtual_rng, *active_rng, *virtual_rng, *active_rng} ;

  vector<IndexRange> cvcv = { *closed_rng, *virtual_rng,  *closed_rng,      *virtual_rng } ;
  vector<IndexRange> avcv = { *active_rng, *virtual_rng,  *closed_rng,      *virtual_rng } ;
  vector<IndexRange> cvav = { *closed_rng, *virtual_rng,  *active_rng,      *virtual_rng } ;
  vector<IndexRange> avav = { *active_rng, *virtual_rng,  *active_rng,      *virtual_rng } ;
  vector<IndexRange> avov = { *active_rng, *virtual_rng,  *not_virtual_rng, *virtual_rng } ;

  vector<IndexRange> vvoo = { *virtual_rng,     *virtual_rng,     *not_virtual_rng, *not_virtual_rng } ;
  vector<IndexRange> vovo = { *virtual_rng,     *not_virtual_rng, *virtual_rng,     *not_virtual_rng } ;
  vector<IndexRange> ovov = { *not_virtual_rng, *virtual_rng,     *not_virtual_rng, *virtual_rng     } ;

  vector<IndexRange> vvoc = { *virtual_rng, *virtual_rng, *not_virtual_rng, *closed_rng} ;
  vector<IndexRange> vvco = { *virtual_rng, *virtual_rng, *closed_rng,      *not_virtual_rng} ;
  vector<IndexRange> vvoa = { *virtual_rng, *virtual_rng, *not_virtual_rng, *active_rng} ;
  vector<IndexRange> vvao = { *virtual_rng, *virtual_rng, *active_rng,      *not_virtual_rng} ;

  shared_ptr<Tensor_<double>> RTens = Tensor_Arithmetic::Tensor_Arithmetic<double>::get_test_Tensor_column_major(make_shared<vector<IndexRange>>(vvaa));
  shared_ptr<Tensor_<double>> YTens = Tensor_Arithmetic::Tensor_Arithmetic<double>::get_test_Tensor_column_major(make_shared<vector<IndexRange>>(vvcc));
  shared_ptr<Tensor_<double>> ZTens = Tensor_Arithmetic::Tensor_Arithmetic<double>::get_test_Tensor_column_major(make_shared<vector<IndexRange>>(vvac));

  shared_ptr<vector<int>> normal_to_alt_order1 = make_shared<vector<int>>( vector<int> { 0, 2, 1, 3} );   // ++-- -> +-+- 
  shared_ptr<vector<int>> normal_to_alt_order2 = make_shared<vector<int>>( vector<int> { 0, 3, 1, 2} );   // ++-- -> +-+- 
  shared_ptr<vector<int>> switch_last_two_ids = make_shared<vector<int>>( vector<int> { 0, 1, 3, 2} );   // ++-- -> +-+- 
  shared_ptr<vector<int>> smith_to_normal_order1 = make_shared<vector<int>>( vector<int> { 1, 3, 0, 2} );   // -+-+ -> ++-- 
 
  Tensor_Arithmetic::Tensor_Arithmetic<double>::set_tensor_elems( v2_, 1.0);
  shared_ptr<Tensor_<double>> HTens = Tensor_Arithmetic::Tensor_Arithmetic<double>::reorder_block_Tensor( v2_, smith_to_normal_order1 );
  TensOp_data_map->emplace( "H" , HTens );
  //Setting_data for TensOps
  if ( op_name  == "Smith_rdms" ) { 
   
    TensOp_data_map->emplace("Smith_rdm1", Smith_rdm1 );
    TensOp_data_map->emplace("Smith_rdm2", Smith_rdm2 );

  } else if ( op_name  == "h" ) { 

    TensOp_data_map->emplace("h" , H_1el_all);

  } else if ( op_name  == "L" ) { 

    shared_ptr<Tensor_<double>> T2 = T2_all[0]->at(0);
    vector<IndexRange> T2_id_ranges = T2->indexrange(); 
    cout << " T id_ranges :" ; cout.flush(); 
    for ( IndexRange idxrange : T2_id_ranges ) 
       cout <<  idxrange.size() << "  " ;
    cout << endl; 
 
    shared_ptr<Tensor_<double>> LTens = make_shared<Tensor_<double>>(vvoo);
    LTens->allocate();
    LTens->zero();
   
    cout << "T2->norm() = " << T2->norm() << endl; 
    cout << "LTens->norm() = " << LTens->norm() << endl; 
    Tensor_Arithmetic::Tensor_Arithmetic<double>::set_tensor_elems( LTens, 1.0);
//  Tensor_Arithmetic::Tensor_Arithmetic<double>::put_reordered_range_block( T2, avov, LTens, vvao, smith_to_normal_order1 );
    cout << "reordered LTens " <<  endl;

    vector<IndexRange> L_id_ranges = LTens->indexrange(); 
    cout << " L id_ranges :" ; cout.flush(); 
    for ( IndexRange idxrange : L_id_ranges ) 
       cout <<  idxrange.size() << "  " ;
    cout << endl; 
  
    cout << "LTens->norm() = " << LTens->norm() << endl; 
   
    TensOp_data_map->emplace("L" , LTens);
     cout << "put L into map" << endl;
    
  } else if ( op_name  == "P" ) { 

    shared_ptr<Tensor_<double>> PTens = make_shared<Tensor_<double>>( vvaa ); 
    PTens->allocate();
    PTens->zero();
    Tensor_Arithmetic::Tensor_Arithmetic<double>::put_reordered_range_block( v2_, avav, PTens, vvaa, smith_to_normal_order1 );
    TensOp_data_map->emplace("P" , PTens);

  } else if ( op_name  == "N" ) { 

    shared_ptr<Tensor_<double>> NTens = make_shared<Tensor_<double>>( vvac ); 
    NTens->allocate();
    Tensor_Arithmetic::Tensor_Arithmetic<double>::set_tensor_elems( NTens, 1.0);
    TensOp_data_map->emplace("N" , NTens);

  } else if ( op_name  == "M" ) { 

    shared_ptr<Tensor_<double>> MTens = make_shared<Tensor_<double>>( vvcc ); 
    MTens->allocate();
    Tensor_Arithmetic::Tensor_Arithmetic<double>::set_tensor_elems( MTens, 1.0);
    TensOp_data_map->emplace("M" , MTens);

  } else if ( op_name  == "Q" ) { 

    shared_ptr<Tensor_<double>> QTens = make_shared<Tensor_<double>>(vvca);
    QTens->allocate();
    Tensor_Arithmetic::Tensor_Arithmetic<double>::set_tensor_elems( QTens, 1.0);
    TensOp_data_map->emplace("Q" , QTens);

  } else if ( op_name  == "R" ) { 

    TensOp_data_map->emplace( "R" , RTens);

  } else if ( op_name  == "S" ) { 
   
    shared_ptr<Tensor_<double>> STens = make_shared<Tensor_<double>>(vvoa);
    STens->allocate();
    Tensor_Arithmetic::Tensor_Arithmetic<double>::put_tensor_range_block( RTens, STens, vvaa );
    Tensor_Arithmetic::Tensor_Arithmetic<double>::put_reordered_range_block( ZTens, vvac, STens, vvca, switch_last_two_ids );
    TensOp_data_map->emplace( "S", STens );

  } else if ( op_name  == "T" ) { 

    shared_ptr<Tensor_<double>> TTens = make_shared<Tensor_<double>>(vvao);
    TTens->allocate();
    Tensor_Arithmetic::Tensor_Arithmetic<double>::put_tensor_range_block( RTens, TTens, vvaa);
    Tensor_Arithmetic::Tensor_Arithmetic<double>::put_tensor_range_block( ZTens, TTens, vvac);
    TensOp_data_map->emplace("T", TTens);

  } else if ( op_name  == "W" ) { 

    shared_ptr<Tensor_<double>> WTens = make_shared<Tensor_<double>>(vvca);
    WTens->allocate();
    
    Tensor_Arithmetic::Tensor_Arithmetic<double>::put_reordered_range_block( ZTens, vvac, WTens, vvca, switch_last_two_ids);
    TensOp_data_map->emplace("W", WTens);

  } else if ( op_name  == "X" ) { 

    TensOp_data_map->emplace( "X" , RTens);
 
  } else if ( op_name  == "Y" ) {
 
    TensOp_data_map->emplace( "Y", YTens );
   
  } else if ( op_name  == "Z" ) {
 
    TensOp_data_map->emplace( "Z", ZTens );
  }

  return;
}
#endif
