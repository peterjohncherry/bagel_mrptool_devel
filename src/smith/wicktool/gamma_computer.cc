#include <bagel_config.h>
#ifdef COMPILE_SMITH
#include <src/smith/wicktool/gamma_computer.h>
#include <src/smith/wicktool/WickUtils.h>

using namespace std;
using namespace bagel;
using namespace bagel::SMITH;
using namespace Tensor_Arithmetic;
using namespace Tensor_Arithmetic_Utils;
using namespace WickUtils;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Gamma_Computer::Gamma_Computer::Gamma_Computer( shared_ptr< map< string, shared_ptr<GammaInfo>>>          Gamma_info_map_in,
                                                shared_ptr< map< string, shared_ptr<Tensor_<double>>>>    CIvec_data_map_in,
                                                shared_ptr< map< string, shared_ptr<Tensor_<double>>>>    Sigma_data_map_in,
                                                shared_ptr< map< string, shared_ptr<Tensor_<double>>>>    Gamma_data_map_in,
                                                shared_ptr< map< string, shared_ptr<const Determinants>>> Determinants_map_in,
						shared_ptr< map< string, shared_ptr<IndexRange>>>         range_conversion_map_in ){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Gamma_Computer::Gamma_Computer::Gamma_Computer" << endl;
  maxtile  = 10000;

  Gamma_info_map       = Gamma_info_map_in;             cout << "set Gamma_info_map            "<< endl; 
  CIvec_data_map       = CIvec_data_map_in;       cout << "set CIvec_data_map      "<< endl;  
  Sigma_data_map       = Sigma_data_map_in;       cout << "set Sigma_data_map      "<< endl;  
  Gamma_data_map       = Gamma_data_map_in;       cout << "set Gamma_data_map      "<< endl;  
  Determinants_map     = Determinants_map_in;     cout << "set Determinants_map    "<< endl; 
  range_conversion_map = range_conversion_map_in; cout << "set range_conversion_map"<< endl; 

  Determinants_map_new = make_shared< map < string, shared_ptr<Determinants> >>();     cout << "building Determinants_map_new    "<< endl; 

  Tensor_Calc = make_shared<Tensor_Arithmetic::Tensor_Arithmetic<double>>();

  //tester();
    
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Gets the gammas in tensor format. 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::get_gamma_tensor( string gamma_name ) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Gamma_Computer::Gamma_Computer::get_gamma_tensor_test"  << endl;
  cout << "Gamma name = " << gamma_name << endl;

  if ( gamma_name == "ID" ) { 
 
    cout << "nothing todo for ID" << endl;
 
  } else if( Gamma_data_map->find(gamma_name) != Gamma_data_map->end()){ 

    cout << "already have data for " << gamma_name << endl;  

  } else { 
    
    shared_ptr<GammaInfo> gamma_info = Gamma_info_map->at(gamma_name) ;
    int order = gamma_info->order;
    //for now just use specialized routines, this must be made generic at some point

    if ( order == 2 ) { 

      build_gamma2_tensor( gamma_info );
      
    } else {
    
      build_gammaN_tensor( gamma_info ) ;

    }
  }
  
  return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::build_gamma2_tensor(shared_ptr<GammaInfo> gamma2_info )  {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "Gamma_Computer::build_gamma2_tensor" << endl;
  
   build_detspace( gamma2_info->Bra_info );
   build_detspace( gamma2_info->Ket_info );

   build_sigma2_tensor(gamma2_info);
   shared_ptr<Tensor_<double>> sigma2 = Sigma_data_map->at(gamma2_info->sigma_name);

   shared_ptr<Tensor_<double>> gamma2_test =
   Tensor_Arithmetic::Tensor_Arithmetic<double>::contract_different_tensors( sigma2, sigma2, make_pair(2,2) ); 

   Print_Tensor(gamma2_test, "gamma4 from sigma2 contraction" ) ; cout << endl;

   shared_ptr<Tensor_<double>> gamma2 =
   Tensor_Arithmetic::Tensor_Arithmetic<double>::contract_tensor_with_vector( Sigma_data_map->at(gamma2_info->sigma_name),
                                                                              CIvec_data_map->at(gamma2_info->Bra_name()),
                                                                              make_pair(2, 0) );

   Print_Tensor(gamma2, "gamma2 from recursive" ) ; cout << endl;
  
   Gamma_data_map->emplace(gamma2_info->name, gamma2); 
  
   return;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::build_sigma2_tensor(shared_ptr<GammaInfo> gamma2_info )  {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Gamma_Computer::build_sigma2_tensor" << endl;
 
  string sigma2_name     = gamma2_info->sigma_name; cout << "sigma2_name = " << sigma2_name << endl;

  shared_ptr<Tensor_<double>> Ket = CIvec_data_map->at( gamma2_info->Ket_name() );  
  shared_ptr<Determinants>    Ket_det = Determinants_map_new->at( gamma2_info->Ket_name() ); 
  IndexRange                  Ket_range = Ket->indexrange()[0];

  int order = gamma2_info->order;  

  shared_ptr<vector<IndexRange>> ranges = Get_Bagel_IndexRanges( gamma2_info->sigma_id_ranges ) ;

  shared_ptr<Tensor_<double>> sigma2 = make_shared<Tensor_<double>>( *ranges );
  sigma2->allocate();
  sigma2->zero();
  Sigma_data_map->emplace( sigma2_name , sigma2 );

  shared_ptr<vector<int>> mins          = make_shared<vector<int>>( 3, 0 );  
  shared_ptr<vector<int>> block_pos     = make_shared<vector<int>>( 3, 0 );  
  shared_ptr<vector<int>> range_lengths = get_range_lengths( *ranges ); 

  shared_ptr<vector<vector<int>>> block_offsets = get_block_offsets( *ranges ) ;

  vector<int> sigma2_id_offsets(3); 
  do { 
     vector<Index> sigma2_id_blocks = *(get_rng_blocks( block_pos, *ranges ));
     for ( int ii = 0 ; ii != 3 ; ii++ ) 
       sigma2_id_offsets[ii] = block_offsets->at(ii)[block_pos->at(ii)];    

     vector<int> sigma2_block_sizes = { (int)sigma2_id_blocks[0].size(), (int)sigma2_id_blocks[1].size(), (int)sigma2_id_blocks[2].size() };
     size_t sigma2_block_size = sigma2_block_sizes[0]*sigma2_block_sizes[1]*sigma2_block_sizes[2];

     unique_ptr<double[]> sigma2_block( new double[sigma2_block_size] ) ;
     std::fill_n(sigma2_block.get(), sigma2_block_size, 0.0);
    
     int Ket_offset = 0 ;
     for ( Index Ket_id_block : Ket_range ) { 

       unique_ptr<double[]> Ket_block = Ket->get_block( (vector<Index>({Ket_id_block}) ) );
       
       sigma_2a1( Ket_block.get(), sigma2_block.get(), Ket_det,
                  Ket_offset, sigma2_id_offsets, Ket_id_block.size(), sigma2_block_sizes );

       sigma_2a2( Ket_block.get(), sigma2_block.get(), Ket_det,
                  Ket_offset, sigma2_id_offsets, Ket_id_block.size(), sigma2_block_sizes );

       Ket_offset += Ket_id_block.size();

     }

  } while (fvec_cycle( block_pos, range_lengths, mins ));
  

  return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::build_gammaN_tensor(shared_ptr<GammaInfo> gammaN_info )  {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "Gamma_Computer::build_gammaN_tensor" << endl;
  
   build_detspace( gammaN_info->Bra_info );
   build_detspace( gammaN_info->Ket_info );

   build_sigmaN_tensor(gammaN_info);
   
   shared_ptr<Tensor_<double>> gammaN =
   Tensor_Arithmetic::Tensor_Arithmetic<double>::contract_tensor_with_vector( Sigma_data_map->at(gammaN_info->sigma_name),
                                                                              CIvec_data_map->at(gammaN_info->Bra_name()),
                                                                              make_pair(gammaN_info->order, 0) );
 
   Print_Tensor( gammaN, "gamma "+to_string(gammaN_info->order)+ " recursive tensor version "  ); cout << endl;

   Gamma_data_map->emplace(gammaN_info->name, gammaN); 
   
   return;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::build_sigmaN_tensor( shared_ptr<GammaInfo> gammaN_info )  {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sn  :  sigmaN   ,  ps : prev_sigma    
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Gamma_Computer::build_sigmaN_tensor" << endl;
 
  string sigmaN_name     = gammaN_info->sigma_name;        cout << "sigmaN_name = " << sigmaN_name << endl;
  string prev_sigma_name = gammaN_info->prev_sigma_name(); cout << "prev_sigma_name = " << prev_sigma_name << endl; 

  int order = gammaN_info->order;  

  shared_ptr<vector<IndexRange>> ranges_sn = Get_Bagel_IndexRanges( gammaN_info->sigma_id_ranges ) ;

  shared_ptr<Tensor_<double>> sigmaN = make_shared<Tensor_<double>>( *ranges_sn );
  sigmaN->allocate();
  sigmaN->zero();
  Sigma_data_map->emplace( sigmaN_name , sigmaN );

  shared_ptr<Tensor_<double>> prev_sigma = Sigma_data_map->at( prev_sigma_name );
  shared_ptr<GammaInfo>       prev_gamma_info = Gamma_info_map->at( gammaN_info->prev_gamma_name() );

  shared_ptr<vector<IndexRange>>  ranges_ps = Get_Bagel_IndexRanges( prev_gamma_info->sigma_id_ranges ) ;
  shared_ptr<vector<vector<int>>> block_offsets_ps = get_block_offsets( *ranges_ps ) ;

  shared_ptr<vector<int>> mins_ps          = make_shared<vector<int>>( ranges_ps->size(), 0 );  
  shared_ptr<vector<int>> block_pos_ps     = make_shared<vector<int>>( ranges_ps->size(), 0 );  
  shared_ptr<vector<int>> range_lengths_ps = get_range_lengths( *ranges_ps ); 

  vector<IndexRange> ranges_Iij = { ranges_sn->at(0), ranges_sn->at(1), ranges_sn->back() }; 
  shared_ptr<vector<vector<int>>> block_offsets_Iij = get_block_offsets( ranges_Iij ) ;

  vector<int> offsets_ps(order-1); 
  do {

    for ( int ii = 0 ; ii != offsets_ps.size(); ii++ ) offsets_ps[ii] = block_offsets_ps->at(ii)[block_pos_ps->at(ii)]; 

    print_vector( *block_pos_ps , "block_pos_ps"); cout <<endl;
    print_vector( offsets_ps , "offsets_ps"); cout <<endl;

    vector<Index> id_blocks_ps = *(get_rng_blocks( block_pos_ps, *ranges_ps));

    shared_ptr<vector<int>> mins_Iij          = make_shared<vector<int>>( 3, 0 );  
    shared_ptr<vector<int>> block_pos_Iij     = make_shared<vector<int>>( 3, 0 );  
    shared_ptr<vector<int>> range_lengths_Iij = get_range_lengths(ranges_Iij); 

    vector<int> offsets_Iij(3); 
    do { 
     
      for ( int ii = 0 ; ii != 3 ; ii++ )  offsets_Iij[ii] = block_offsets_Iij->at(ii)[block_pos_Iij->at(ii)];    

      print_vector( *block_pos_Iij , "block_pos_Iij"); cout <<endl;
      print_vector( offsets_Iij   , "offsets_Iij"); cout <<endl;

      vector<Index> id_blocks_Iij = *( get_rng_blocks( block_pos_Iij, ranges_Iij) );
      
      build_sigmaN_block( sigmaN_name, id_blocks_ps, offsets_ps, prev_sigma_name, id_blocks_Iij, offsets_Iij ) ;

    } while (fvec_cycle( block_pos_Iij, range_lengths_Iij, mins_Iij ));
  
  } while (fvec_cycle( block_pos_ps, range_lengths_ps, mins_ps ));

  return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
Gamma_Computer::Gamma_Computer::build_sigmaN_block( string sigmaN_name,     vector<Index>& id_blocks_ps, vector<int>& offsets_ps,
                                                    string prev_sigma_name, vector<Index>& id_blocks_Iij, vector<int>& offsets_Iij ) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Gamma_Computer::build_sigmaN_block" << endl;
  cout << "sigmaN_name = " << sigmaN_name << endl;
  cout << "prev_sigma_name = " << prev_sigma_name << endl;

  if ( Gamma_info_map->at(sigmaN_name.substr(2))->Bra_info->name()  == Gamma_info_map->at(prev_sigma_name.substr(2))->Ket_info->name() ){//fix for rel case
   
    shared_ptr<Tensor_<double>> sigmaN      = Sigma_data_map->at(sigmaN_name);
    shared_ptr<Tensor_<double>> prev_sigma  = Sigma_data_map->at(prev_sigma_name);

    unique_ptr<double[]> prev_sigma_block = prev_sigma->get_block( id_blocks_ps );

    vector<int> Iij_block_sizes = { (int)id_blocks_Iij[0].size(), (int)id_blocks_Iij[1].size(), (int)id_blocks_Iij[2].size() };
    int Iij_block_size  = Iij_block_sizes[0]*Iij_block_sizes[1]*Iij_block_sizes[2]; 

    int Ket_block_size  = id_blocks_ps.back().size();
    int ps_orb_block_size  = get_block_size( id_blocks_ps.begin(), id_blocks_ps.end()-1 ); 

    unique_ptr<double[]> sigmaN_block( new double[Iij_block_size*ps_orb_block_size] );
    fill_n( sigmaN_block.get(), Iij_block_size*ps_orb_block_size, 0.0 );

    shared_ptr<Determinants> Ket_det = Determinants_map_new->at(Gamma_info_map->at(sigmaN_name.substr(2))->Bra_info->name()); 

    cout << " Ket_block_size    = " << Ket_block_size << endl;
    cout << " Iij_block_size    = " << Iij_block_size << endl;
    cout << " ps_orb_block_size = " << ps_orb_block_size << endl;
    for ( int  ii = 0; ii != ps_orb_block_size; ii++) {
 
      cout << " ii = " << ii ; cout.flush() ; 
      cout << " ii*Ket_block_size = " << ii*Ket_block_size << endl;
      cout << " ii*Iij_block_size = " << ii*Iij_block_size << endl;

      sigma_2a1( prev_sigma_block.get()+(ii*Ket_block_size), sigmaN_block.get()+(ii*Iij_block_size), Ket_det,
                 offsets_ps.back(), offsets_Iij, Ket_block_size, Iij_block_sizes );

      sigma_2a2( prev_sigma_block.get()+(ii*Ket_block_size), sigmaN_block.get()+(ii*Iij_block_size), Ket_det,
                 offsets_ps.back(), offsets_Iij, Ket_block_size, Iij_block_sizes );

    }
    cout << "A5" << endl; 
    
    vector<Index> id_blocks_sn = {id_blocks_Iij[0], id_blocks_Iij[1]} ;
    id_blocks_sn.insert( id_blocks_sn.end(), id_blocks_ps.begin(), id_blocks_ps.end()-1) ;
    id_blocks_sn.push_back(id_blocks_Iij[2]);
    sigmaN->put_block( sigmaN_block, id_blocks_sn); 

  } else {

    cout << "spin flips not implemented yet " <<endl;

  }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::sigma_2a1(double* cvec_ptr, double* sigma_ptr, shared_ptr<const Determinants> dets,
                                               int cvec_offset, vector<int>& sigma_offsets, 
                                               int cvec_block_size, vector<int>& sigma_block_sizes ){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////                               
//  cout << "Gamma_Computer::sigma_2a1" << endl;                                                                                          
  const int lb = dets->lenb();                                                                                            
  const int ij = dets->norb()*dets->norb();                                                                               
                                                                                                                          
  for (int ip = 0; ip != ij; ++ip) {                                                                                      
    double* target_base = sigma_ptr+dets->size()*ip;                                                                              
    for (auto& iter : dets->phia(ip)) {                                                                                   
      const double sign = static_cast<double>(iter.sign);                                                                 
      double* const target_array = target_base + iter.source*lb;                                                          
      blas::ax_plus_y_n(sign, cvec_ptr + iter.target*lb, lb, target_array);                                               
    }                                                                                                                     
  }                                                                                                                       
}                                                                                                                         
                                                                                                                          
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////                             
void Gamma_Computer::Gamma_Computer::sigma_2a2( double* cvec_ptr, double* sigma_ptr, shared_ptr<const Determinants> dets, 
                                               int cvec_offset, vector<int>& sigma_offsets, 
                                               int cvec_block_size, vector<int>& sigma_block_sizes ){
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////                             
//  cout << "Gamma_Computer::sigma_2a2" << endl;
  const int la = dets->lena();
  const int lb = dets->lenb();
  const int ij = dets->norb()*dets->norb();

  for (int i = 0; i < la; ++i) {
    double* source_array0 = cvec_ptr+i*lb;

    for (int ip = 0; ip != ij; ++ip) {
      double* target_array0 = sigma_ptr + (ip * dets->size()) + i*lb;

      for (auto& iter : dets->phib(ip)) {
        const double sign = static_cast<double>(iter.sign);
        target_array0[iter.source] += sign * source_array0[iter.target];
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
shared_ptr<vector<IndexRange>> Gamma_Computer::Gamma_Computer::Get_Bagel_IndexRanges(shared_ptr<vector<string>> ranges_str){ 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "Gamma_Computer::Get_Bagel_IndexRanges 1arg" << endl;
  print_vector(*ranges_str, "ranges_str" );
  cout << "ranges_str = [ "; cout.flush();
  shared_ptr<vector<IndexRange>> ranges_Bagel = make_shared<vector<IndexRange>>(ranges_str->size());
  for ( int ii =0 ; ii != ranges_str->size(); ii++) {
    cout << ranges_str->at(ii) << " " ; cout.flush(); 
    ranges_Bagel->at(ii) = *range_conversion_map->at(ranges_str->at(ii));
    
  }
  cout << endl;
  return ranges_Bagel;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
shared_ptr<Tensor_<double>> 
Gamma_Computer::Gamma_Computer::convert_civec_to_tensor( shared_ptr<const Civec> civector, int state_num ) const {
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "Gamma_Computer::convert_civec_to_tensor" << endl;

  //NOTE: must be adapted to handle arbitrary spin sectors
  string civec_name = get_civec_name(state_num, civector->det()->norb(), civector->det()->nelea(), civector->det()->neleb());  
  vector<IndexRange> civec_idxrng(1, *(range_conversion_map->at(civec_name)) );  

  cout <<" civec_name = " << civec_name << endl;
  cout <<" civec_idxrng[0].nblock()       = " << civec_idxrng[0].nblock()     <<  endl;
  cout <<" civec_idxrng[0].size()         = " << civec_idxrng[0].size()       <<  endl;
  cout <<" civec_idxrng[0].range().size() = " << civec_idxrng[0].range().size() <<  endl;
  cout <<" civec_idxrng[0].range().size() = " << civec_idxrng[0].range().size() <<  endl;
  
  shared_ptr<Tensor_<double>> civec_tensor = make_shared<Tensor_<double>>( civec_idxrng );
  civec_tensor->allocate();
  civec_tensor->zero();

  size_t idx_position = 0;

  cout << "civectordata = " ; cout.flush(); 
  for ( Index idx_block : civec_idxrng[0].range() ){
     unique_ptr<double[]> civec_block(new double[idx_block.size()]);
     std::fill_n(civec_block.get(), idx_block.size(), 0.0);
     copy_n( civector->data() + idx_position, idx_block.size(), civec_block.get());

     for ( int ii = 0 ; ii != idx_block.size() ; ii++ ) 
       cout << *(civector->data() + idx_position + ii) << " "; 
     cout.flush();
  
     civec_tensor->add_block(civec_block, vector<Index>({ idx_block })) ;  
     idx_position += idx_block.size();  
  }

  cout <<endl;

  //will have to modify for relativistic case
  CIvec_data_map->emplace( civec_name, civec_tensor); 
  Determinants_map->emplace( civec_name, civector->det() ); 

  return civec_tensor;
}

#ifndef NDEBUG
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Tests 2idx gamma matrix by taking trace  and checking symmetry. Non-rel only.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Gamma_Computer::Gamma_Computer::gamma_2idx_contract_test( string gamma_name ) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
   bool passed = true;

   cout << endl << "------------------------------------------------------------------------------------------------------" << endl;
   Print_Tensor(Gamma_data_map->at(gamma_name), gamma_name);
   cout << endl << "------------------------------------------------------------------------------------------------------" << endl;
   cout << "Gamma_data_map->at("<<gamma_name<<")->norm() = "<< Gamma_data_map->at(gamma_name)->norm() <<  endl;  
   cout << "Gamma_data_map->at("<<gamma_name<<")->rms()  = "<< Gamma_data_map->at(gamma_name)->rms() <<  endl; 

   shared_ptr<Tensor_<double>> gamma_2idx_trace  =  Tensor_Arithmetic::Tensor_Arithmetic<double>::contract_on_same_tensor( Gamma_data_map->at(gamma_name),  make_pair(0,1) );
   cout << "------------------------------------------------------------------------------------------------------" << endl; 
   cout << "gamma_2idx_trace->rms() = "<< gamma_2idx_trace->rms() << endl;
   cout << "------------------------------------------------------------------------------------------------------" << endl; 

   shared_ptr<vector<int>>     new_order = make_shared<vector<int>>(vector<int> { 1, 0} ); 

   shared_ptr<Tensor_<double>> gamma_2idx_orig_order = make_shared<Tensor_<double>>( *(Gamma_data_map->at(gamma_name)) );
   shared_ptr<Tensor_<double>> gamma_2idx_transposed = Tensor_Arithmetic::Tensor_Arithmetic<double>::reorder_block_Tensor( gamma_2idx_orig_order, new_order);

   gamma_2idx_transposed->ax_plus_y(-1, gamma_2idx_orig_order) ;


   int nel = Gamma_info_map->at(gamma_name)->Bra_info->nele();
   if ( (abs(gamma_2idx_trace->rms() -nel) > 0.00000001 ) || (gamma_2idx_transposed->rms() > 0.00000001 )  )  passed = false;

   return passed;

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Tests 4idx gamma matrix by contracting in different ways and compairing with 2idx gamma matrices.  Only set up for non-rel at present.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Gamma_Computer::Gamma_Computer::gamma_4idx_contract_test( string gamma_name ) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
   cout << endl << "------------------------------------------------------------------------------------------------------" << endl; 
   Print_Tensor(Gamma_data_map->at(gamma_name));
   cout << endl << "------------------------------------------------------------------------------------------------------" << endl; 
   cout << "Gamma_data_map->at("<<gamma_name<<")->norm() = "<< Gamma_data_map->at(gamma_name)->norm() <<  endl;  
   cout << "Gamma_data_map->at("<<gamma_name<<")->rms()  = "<< Gamma_data_map->at(gamma_name)->rms() <<  endl; 

   shared_ptr<Tensor_<double>> gamma_2idx_from_4idx_A  =  Tensor_Arithmetic::Tensor_Arithmetic<double>::contract_on_same_tensor( Gamma_data_map->at(gamma_name),  make_pair(2,3) );
   cout << endl << "------------------------------------------------------------------------------------------------------" << endl; 
   Print_Tensor(gamma_2idx_from_4idx_A, "gamma_2idx_from_4idx" ); 
   cout << endl << "------------------------------------------------------------------------------------------------------" << endl; 
   cout << "gamma_2idx_from_4idx_A->norm()  = "<< gamma_2idx_from_4idx_A->norm() << endl;
   cout << "gamma_2idx_from_4idx_A->rms()   = "<< gamma_2idx_from_4idx_A->rms() << endl;
   cout << "------------------------------------------------------------------------------------------------------" << endl; 

   vector<int> ctrs_pos = {0,1,2,3};
   shared_ptr<Tensor_<double>> gamma_2idx_from_4idx_B = Tensor_Arithmetic::Tensor_Arithmetic<double>::contract_on_same_tensor( Gamma_data_map->at(gamma_name),  make_pair(0,1) );
   cout << " gamma_2idx_from_4idx_B->norm() = " << gamma_2idx_from_4idx_B->norm()  << endl; 
   cout << " gamma_2idx_from_4idx_B->rms()  = " << gamma_2idx_from_4idx_B->rms()  << endl; 

   gamma_2idx_from_4idx_B->ax_plus_y(-1.0, gamma_2idx_from_4idx_A );
   cout << " gamma_2idx_from_4idx_B->norm() - gamma_2idx_from_4idx_A->norm()  = " << gamma_2idx_from_4idx_B->norm()  << endl; 
   cout << " gamma_2idx_from_4idx_B->rms()  - gamma_2idx_from_4idx_A->rms()   = " << gamma_2idx_from_4idx_B->rms()  << endl; 

   return ( gamma_2idx_from_4idx_B->norm() < 0.00000001 );

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Consistency tests for building up gamma 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Gamma_Computer::Gamma_Computer::build_sigma_4idx_tensor_tests(shared_ptr<GammaInfo> gamma_4idx_info ) { 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  bool passed = true;

  shared_ptr<GammaInfo> sigma_2idx_KJ_info = Gamma_info_map->at( gamma_4idx_info->prev_gammas(1) );
  shared_ptr<GammaInfo> sigma_2idx_IK_info = Gamma_info_map->at( gamma_4idx_info->prev_gammas(0) );

  string IJ_Bra_name = gamma_4idx_info->Bra_info->name();
  string IJ_Ket_name = gamma_4idx_info->Ket_info->name();
 
  string IK_Bra_name = sigma_2idx_IK_info->Bra_info->name();
  string IK_Ket_name = sigma_2idx_IK_info->Ket_info->name();

  string KJ_Bra_name = sigma_2idx_KJ_info->Bra_info->name();
  string KJ_Ket_name = sigma_2idx_KJ_info->Ket_info->name();

  if ( IJ_Bra_name != IK_Bra_name ) {
    passed =false;  cout << "IJ_Bra_name != IK_Bra_name   : " <<  IJ_Bra_name << " != " << IK_Bra_name  << endl; 

  } else if ( IJ_Ket_name != KJ_Ket_name ) {
    passed =false;  cout << "IJ_Ket_name != KJ_Ket_name   : " <<  IJ_Ket_name << " != " << KJ_Ket_name  << endl; 

  } else if ( IK_Ket_name != KJ_Bra_name ) {
    passed =false;  cout << "IK_Ket_name != KJ_Bra_name   : " <<  IK_Ket_name << " != " << KJ_Bra_name  << endl; 

  }
 
  return passed;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Gamma_Computer::Gamma_Computer::build_detspace(shared_ptr<CIVecInfo<double>>  Psi_info ) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
   if (Determinants_map_new->find( Psi_info->name() ) ==  Determinants_map_new->end() ) 
     Determinants_map_new->emplace( Psi_info->name(), make_shared<Determinants> (Psi_info->nact(), Psi_info->nalpha(), Psi_info->nbeta(), false, /*mute=*/true) );

   return;

}
#endif
#endif
// auto in_Bra_range = [&sigma_offsets, &Bra_block_size ]( size_t& pos) {
//     if (( pos > (sigma_offsets[0]+Bra_block_size)) || ( pos < sigma_offsets[0] ) ){
//        cout << "out of sigma range " << endl;
//        cout << "pos            = " << pos << endl;
//        cout << "Bra_offset     = " << sigma_offsets[2] << endl;
//        cout << "Bra_Block_size = " << Bra_block_size << endl;
//        return false;
//     }
//     return true;
//     };
//
//   auto in_Ket_range = [&Ket_offset ]( size_t& pos , size_t Ket_block_size ) {
//      if ( ( pos  > (Ket_offset+ Ket_block_size )) || ( pos < Ket_offset ) ){
//         cout << "out of Ket range " << endl;
//         cout << "pos             = " << pos << endl;
//         cout << "Ket_offset      = " << Ket_offset << endl;
//         cout << "Ket_Block_size  = " << Ket_block_size << endl;
//         return false;
//      }
//      return true;
//      };


