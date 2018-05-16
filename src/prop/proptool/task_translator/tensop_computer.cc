#include <bagel_config.h>
#include <src/prop/proptool/task_translator/tensop_computer.h>
using namespace std;
using namespace bagel;
using namespace bagel::SMITH;
using namespace bagel::Tensor_Sorter;
using namespace bagel::Tensor_Arithmetic; 
using namespace bagel::Tensor_Arithmetic_Utils; 
using namespace WickUtils;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
void
TensOp_Computer::TensOp_Computer<DataType>::Calculate_CTP( AContribInfo_Base& AInfo ){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << " TensOp_Computer::TensOp_Computer::Calculate_CTP : "; cout.flush(); cout <<  AInfo.name_ << endl;

  string A_contrib = AInfo.name_;

  if (ACompute_map_->at(A_contrib)->size() == 0 )
    throw logic_error( "ACompute list is empty!! Aborting!!" );

  for (shared_ptr<CtrOp_base> ctr_op : *(ACompute_map_->at(A_contrib))){

     cout << "getting " <<  ctr_op->Tout_name() << endl;
   
    //TODO don't check ctr_type(), check if dynamic_pointer_cast<X>( ctr_op ) is null (where X is the type of contraction you're casting to).
    // check if this is an uncontracted multitensor (0,0) && check if the data is in the map
    if( tensop_data_map_->find(ctr_op->Tout_name()) == tensop_data_map_->end() ) {
       cout << A_contrib << " not in data_map, must calculate it " << endl;
       shared_ptr<Tensor_<DataType>>  New_Tdata; 
 
      if ( ctr_op->ctr_type()[0] == 'g'){  cout << " : no contraction, fetch this tensor part" << endl; 
        New_Tdata =  get_block_Tensor(ctr_op->Tout_name());
        tensop_data_map_->emplace(ctr_op->Tout_name(), New_Tdata); 
        cout << ctr_op->Tout_name() << "->norm() = " << New_Tdata->norm() << endl;
  
      } else if ( ctr_op->ctr_type()[0] == 'd' ){ cout << " : contract different tensors" << endl; 
        New_Tdata = contract_different_tensors( ctr_op->T1name(), ctr_op->T2name(), ctr_op->Tout_name(), make_pair(ctr_op->T1_ctr_rel_pos(), ctr_op->T2_ctr_rel_pos()) );
        tensop_data_map_->emplace(ctr_op->Tout_name(), New_Tdata); 
        cout << ctr_op->Tout_name() << "->norm() = " << New_Tdata->norm() << endl;
      
      } else if ( ctr_op->ctr_type()[0] == 's' ) { cout << " : contract on same tensor" <<  endl; 
        New_Tdata = contract_on_same_tensor( ctr_op->T1name(), ctr_op->Tout_name(), ctr_op->ctr_rel_pos() ); 
        tensop_data_map_->emplace(ctr_op->Tout_name(), New_Tdata); 
        cout << ctr_op->Tout_name() << "->norm() = " << New_Tdata->norm() << endl;

      } else if ( ctr_op->ctr_type()[0] == 'r' ) { cout << " : reorder tensor" <<  endl; 
        cout << "ctr_op->T1name() = " << ctr_op->T1name() << "    ctr_op->Toutname() = " << ctr_op->Tout_name() << endl;
        New_Tdata = reorder_block_Tensor( ctr_op->T1name(), ctr_op->new_order() ); 
        tensop_data_map_->emplace(ctr_op->Tout_name(), New_Tdata); 
        cout << ctr_op->Tout_name() << "->norm() = " << New_Tdata->norm() << endl;

      } else if ( ctr_op->ctr_type()[0] == 'c' ) { cout << " : cartesian (direct) product tensors" <<  endl; 
        New_Tdata = direct_product_tensors( *(ctr_op->tensor_list()) );
        tensop_data_map_->emplace(ctr_op->Tout_name(), New_Tdata);
        cout << ctr_op->Tout_name() << "->norm() = " << New_Tdata->norm() << endl;
      } else { 
        throw std::runtime_error(" unknown contraction type : " + ctr_op->ctr_type() ) ;
      }
    }
    cout << "A_contrib.first = " << A_contrib << endl;
    cout << "CTPout_name =  " << ctr_op->Tout_name() << endl;
  }
  
  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Returns a tensor T3 with elements T3_{ijkl..} = T1_{ijkl..}/T2_{ijkl..} 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>> TensOp_Computer::TensOp_Computer<DataType>::divide_tensors(string T1_name, string T2_name ){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << " TensOp_Computer::TensOp_Computer::divide_tensors " << endl;
 
   return Tensor_Calc_->divide_tensors( find_or_get_CTP_data(T1_name),  find_or_get_CTP_data(T2_name)); 

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//As above, but does division in place, modifying the input;  T1_{ijkl..} = T1_{ijkl..}/T2_{ijkl..} 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
void TensOp_Computer::TensOp_Computer<DataType>::divide_tensors_in_place(string T1_name, string T2_name ){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << " TensOp_Computer::TensOp_Computer::divide_tensors_in_place" << endl;
 
  Tensor_Calc_->divide_tensors_in_place( find_or_get_CTP_data(T1_name),  find_or_get_CTP_data(T2_name)); 

  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Returns a block of a tensor, defined as a new tensor, is copying needlessly, so find another way. 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>> TensOp_Computer::TensOp_Computer<DataType>::get_block_Tensor(string tens_block_name){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "TensOp_Computer::TensOp_Computer::get_block_Tensor : " << tens_block_name << endl;
 
   shared_ptr<Tensor_<DataType>> tens; 
   
   if(  tensop_data_map_->find(tens_block_name) != tensop_data_map_->end()){
     tens = tensop_data_map_->at(tens_block_name);

   } else {
     cout <<"not in map ... " <<  tens_block_name << " must be formed from direct product tensor " << endl; 

     vector<string> sub_tens_names(0);
     for ( shared_ptr<CtrTensorPart_Base>& ctp : *(CTP_map_->at(tens_block_name)->CTP_vec()))
       sub_tens_names.push_back(ctp->name());
     
     print_vector(sub_tens_names, "sub_tens_names" ); cout << endl; 
     tens = direct_product_tensors( sub_tens_names ); 

   }

   cout << "leaving TensOp_Computer::TensOp_Computer::get_block_Tensor : " << tens_block_name << endl;
   return tens;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//TODO this can end up copying a lot; ideally should call the integrals just for these blocks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
void TensOp_Computer::TensOp_Computer<DataType>::get_tensor_data_blocks(shared_ptr<set<shared_ptr<Range_Block_Info>>> required_blocks ){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "TensOp_Computer::TensOp_Computer::get_tensor_data_blocks " << endl;

   for ( auto& block : *required_blocks ) { 
 
     string block_name = block->name();
     cout << "fetching block " << block_name ; cout.flush(); 
    
     shared_ptr<Tensor_<DataType>> tens; 
     
     if(  tensop_data_map_->find(block_name) != tensop_data_map_->end()){
        cout << " .. already in map" <<  endl;

     } else {

       string full_tens_name = block->op_info_->op_state_name_;

       cout << " from full tensor " << full_tens_name << endl;

       shared_ptr<vector<IndexRange>> id_block = Get_Bagel_IndexRanges( CTP_map_->at(block_name)->unc_id_ranges() ) ;
       print_vector( *( block->orig_rngs_ ), "id_block_rb"  ) ; cout << endl;
       print_vector( *( CTP_map_->at(block_name)->unc_id_ranges() ), "id_block_ctp"  ) ; cout << endl;
     
       if( tensop_data_map_->find(full_tens_name) == tensop_data_map_->end()){

         // TODO This will get the whole tensor, really, we should just get the blocks we want
         if ( full_tens_name[0] == 'H' || full_tens_name[0] == 'h' || full_tens_name[0] == 'f' ) {  
           cout << "getting the full mo tensor for " << full_tens_name << endl;
           build_mo_tensor( full_tens_name ); 
           cout << "initializing block " << block_name << " using full tensor \"" << full_tens_name << "\"" << endl;
           tens = get_sub_tensor( tensop_data_map_->at(full_tens_name), *id_block );
         }
    
         else if ( full_tens_name[0] == 'X' || full_tens_name[0] == 'T' || full_tens_name[0] == 't' || full_tens_name[0] == 'S'  ) {  
           cout << "new tensor block : " << full_tens_name << " is being initialized to 1.0" << endl; 
           tens = make_shared<Tensor_<DataType>>(*id_block);
           tens->allocate();
           Tensor_Arithmetic::Tensor_Arithmetic<DataType>::set_tensor_elems( tens, 1.0);
         
         } else {  
           cout << "new tensor block : " << full_tens_name << " is being initialized to zero" << endl; 
           tens = make_shared<Tensor_<DataType>>(*id_block);
           tens->allocate();
           tens->zero();
         }
 
       } else {
       
         tens = get_sub_tensor( tensop_data_map_->at(full_tens_name), *id_block );
  
       }
     }
     tensop_data_map_->emplace(block_name, tens) ;
   } 
   cout << "leaving TensOp_Computer::TensOp_Computer::get_tensor_data_blocks" << endl;
   return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Gets ranges and factors from the input which will be used in definition of terms
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
void TensOp_Computer::TensOp_Computer<DataType>::build_mo_tensor( string mo_tensor_name ) {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "TensOp_Computer::TensOp_Computer::calculate_mo_integrals()" << endl;

  if ( mo_tensor_name[0] == 'H' ) {   
    auto H_loc = tensop_data_map_->find( "H_{00}" );

    if ( H_loc == tensop_data_map_->end() ) {
      vector<string> free4 = { "free", "free", "free", "free" };
      auto v2  =  moint_computer_->get_v2( free4 ) ;
      tensop_data_map_->emplace( "H_{00}" , v2 );
 
      if ( mo_tensor_name != "H_{00}" ) 
        tensop_data_map_->emplace( mo_tensor_name , v2 );

    } else { 
      tensop_data_map_->emplace( mo_tensor_name , H_loc->second );

    }

  } else if ( mo_tensor_name[0] == 'h' ) {   
    auto f_loc = tensop_data_map_->find( "h_{00}" );

    if ( f_loc == tensop_data_map_->end() ) {
      vector<string> free2 = { "free", "free" };
      auto h1  =  moint_computer_->get_h1( free2 );
      tensop_data_map_->emplace( "h_{00}" , h1 );
 
      if ( mo_tensor_name != "h_{00}" )
        tensop_data_map_->emplace( mo_tensor_name , h1 );
    }

  } else if ( mo_tensor_name[0] == 'f' ) {   
    auto f_loc = tensop_data_map_->find( "f_{00}" );

    if ( f_loc == tensop_data_map_->end() ) {
      vector<string> free2 = { "free", "free" };
      auto f1  =  moint_computer_->get_fock( free2 );
      tensop_data_map_->emplace( "f_{00}" , f1 );
 
      if ( mo_tensor_name != "f_{00}" )
        tensop_data_map_->emplace( mo_tensor_name , f1 );
    }
  }
  
  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Returns a tensor with ranges specified by unc_ranges, where all values are equal to XX  
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::get_uniform_Tensor(shared_ptr<vector<string>> unc_ranges, DataType XX ){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "TensOp_Computer::TensOp_Computer::get_uniform_Tensor" << endl;

   shared_ptr<vector<IndexRange>> T_id_ranges = Get_Bagel_IndexRanges(unc_ranges);

   return Tensor_Calc_->get_uniform_Tensor(T_id_ranges, XX);

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::contract_on_same_tensor( std::string T_in_name, std::string T_out_name, pair<int,int> ctr_todo) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "TensOp_Computer::TensOp_Computer::contract_on_same_tensor "; cout.flush();
   cout << ": "  << T_in_name << " over (" << ctr_todo.first << ", " << ctr_todo.second << ") to get " << T_out_name <<  endl;
   return Tensor_Calc_->contract_on_same_tensor( find_or_get_CTP_data(T_in_name), ctr_todo  ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::contract_on_same_tensor( string T_in_name, shared_ptr<vector<int>> ctrs_pos) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "TensOp_Computer::TensOp_Computer::contract_on_same_tensor" << ": "  << T_in_name << " over "; print_vector( *ctrs_pos) ; cout << endl;
   return Tensor_Calc_->contract_on_same_tensor( find_or_get_CTP_data(T_in_name), *ctrs_pos  ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::contract_different_tensors( std::string T1_in_name, std::string T2_in_name, std::string T_out_name,
                                                                  pair<int,int> ctr_todo ){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "TensOp_Computer::contract_on_different_tensor" << ": "  << T1_in_name << " and " << T2_in_name << " over T1[" << ctr_todo.first << "] and T2[" << ctr_todo.second << "] to get " << T_out_name <<  endl;
 
  shared_ptr<Tensor_<DataType>> Tens1_in = find_or_get_CTP_data(T1_in_name);
  shared_ptr<Tensor_<DataType>> Tens2_in = find_or_get_CTP_data(T2_in_name);
  return Tensor_Calc_->contract_different_tensors(Tens1_in, Tens2_in, ctr_todo );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::contract_different_tensors( std::string T1_in_name, std::string T2_in_name, std::string T_out_name,
                                                                        std::pair<std::vector<int>,std::vector<int>> ctrs_todo                  ){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "TensOp_Computer::contract_on_different_tensor" << endl; 

  shared_ptr<Tensor_<DataType>> Tens1_in = find_or_get_CTP_data(T1_in_name);
  shared_ptr<Tensor_<DataType>> Tens2_in = find_or_get_CTP_data(T2_in_name);

  return Tensor_Calc_->contract_different_tensors(Tens1_in, Tens2_in, ctrs_todo );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::direct_product_tensors( std::vector<std::string>& tensor_names ){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "TensOp_Computer::direct_product_tensors" << endl; 

  assert(tensor_names.size() > 1 );

  print_vector(tensor_names, "tensor_names"); cout <<endl; 

  string Tname_comp = tensor_names.front();
  shared_ptr<Tensor_<DataType>> Tens_prod = find_or_get_CTP_data( Tname_comp );
  shared_ptr<Tensor_<DataType>> Tens_intermediate;
  for ( vector<string>::iterator tn_it = tensor_names.begin()+1 ; tn_it != tensor_names.end(); tn_it++ ) {
    cout << "*tn_it = " << *tn_it << endl;
    shared_ptr<Tensor_<DataType>> Tens_next = find_or_get_CTP_data(*tn_it);
    Tens_intermediate = Tensor_Arithmetic::Tensor_Arithmetic<DataType>::direct_tensor_product( Tens_prod, Tens_next ); 
    Tens_prod = std::move(Tens_intermediate);
    Tname_comp += *tn_it;
    cout << "  Tname_comp = " << Tname_comp << endl;
  }           
 
  return Tens_prod;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Returns a block of a tensor, defined as a new tensor, is copying needlessly, so find another way. 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::reorder_block_Tensor(string tens_block_name, shared_ptr<vector<int>> new_order){
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "TensOp_Computer::TensOp_Computer::reorder_block_Tensor "; cout.flush();
  cout << " : " << tens_block_name ; cout.flush();  WickUtils::print_vector( *new_order, "new_order");
 
  auto tensop_data_map_loc = tensop_data_map_->find( tens_block_name ); 
  shared_ptr<Tensor_<DataType>> T_part; 
  if( tensop_data_map_loc == tensop_data_map_->end() ){
    T_part =  get_block_Tensor( tens_block_name ); 
    tensop_data_map_->emplace( tens_block_name , T_part );
  } else { 
    T_part = tensop_data_map_loc->second; 
  }

  return Tensor_Calc_->reorder_block_Tensor( T_part , new_order );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
pair<int,int>
TensOp_Computer::TensOp_Computer<DataType>::relativize_ctr_positions( pair <int,int> ctr_todo, 
                                                            shared_ptr<CtrTensorPart_Base> CTP1,
                                                            shared_ptr<CtrTensorPart_Base> CTP2 ){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 cout << "TensOp_Computer::TensOp_Computer::relativize_ctr_positions" << endl;
   pair<int,int> rel_ctr;

   int T1_orig_size = CTP1->full_id_ranges()->size(); 
   int T2_orig_size = CTP2->full_id_ranges()->size(); 

   if (ctr_todo.first >= T1_orig_size ){ 
     rel_ctr = make_pair(ctr_todo.second, ctr_todo.first-T2_orig_size);
   } else {
     rel_ctr = make_pair(ctr_todo.first, ctr_todo.second-T1_orig_size);
   }

  for ( int ii = 0 ; ii != CTP1->unc_pos()->size() ; ii++ )
     if ( CTP1->unc_pos(ii) == rel_ctr.first){
       rel_ctr.first = ii;
       break;
     }

  for ( int ii = 0 ; ii != CTP2->unc_pos()->size() ; ii++ )
     if (CTP2->unc_pos(ii) == rel_ctr.second){ 
       rel_ctr.second = ii;
       break;
     }

 return rel_ctr;

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//TODO This should be replaced; ultimately, each of the blocks should be it's own tensor; this duplicates everything.
//     Which blocks exist should be determined after the equation editor has run, so this should ultimately link to
//     allocation and initialization routines, etc..
//     As an intermediate step could perhaps erase the newly created CTP block as soon as it's extracted from the tensor.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<Tensor_<DataType>>
TensOp_Computer::TensOp_Computer<DataType>::find_or_get_CTP_data(string CTP_name){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 cout << "TensOp_Computer::TensOp_Computer::find_or_get_CTP_data  : " <<  CTP_name <<  endl;

  shared_ptr<Tensor_<DataType>> CTP_data;
  auto Data_loc =  tensop_data_map_->find(CTP_name); 
  if ( Data_loc == tensop_data_map_->end() ){
    CTP_data = get_block_Tensor(CTP_name);   
    tensop_data_map_->emplace(CTP_name, CTP_data);   
  } else {
    CTP_data = Data_loc->second;
  }
  
  return CTP_data;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<vector<shared_ptr<const IndexRange>>>
TensOp_Computer::TensOp_Computer<DataType>::Get_Bagel_const_IndexRanges(shared_ptr<vector<string>> ranges_str){ 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "Get_Bagel_const_IndexRanges 1arg" << endl;

  shared_ptr<vector<shared_ptr<const IndexRange>>> ranges_Bagel = make_shared<vector<shared_ptr<const IndexRange>>>(0);
  for ( auto rng : *ranges_str) 
    ranges_Bagel->push_back(make_shared<const IndexRange>(*range_conversion_map_->at(rng)));

  return ranges_Bagel;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<vector<shared_ptr<const IndexRange>>>
TensOp_Computer::TensOp_Computer<DataType>::Get_Bagel_const_IndexRanges(shared_ptr<vector<string>> ranges_str, shared_ptr<vector<int>> unc_pos){ 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "TensOp_Computer::Get_Bagel_const_IndexRanges 2arg" ; print_vector(*ranges_str, "ranges_str" ) ;
cout <<  "  "; cout.flush();  print_vector(*unc_pos, "unc_pos" ) ; cout << endl;

  vector<shared_ptr<const IndexRange>>  ranges_Bagel(unc_pos->size());
  for ( int ii = 0 ; ii != unc_pos->size() ; ii++) 
    ranges_Bagel[ii]=(make_shared<const IndexRange>(*range_conversion_map_->at(ranges_str->at(unc_pos->at(ii)))));

  return make_shared<vector<shared_ptr<const IndexRange>>>(ranges_Bagel);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<vector<IndexRange>>
TensOp_Computer::TensOp_Computer<DataType>::Get_Bagel_IndexRanges( shared_ptr<vector<string>> ranges_str ){ 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << "TensOp_Computer::Get_Bagel_IndexRanges 1arg "; print_vector(*ranges_str, "ranges_str" ) ; cout << endl;

  shared_ptr<vector<IndexRange>> ranges_Bagel = make_shared<vector<IndexRange>>(ranges_str->size());
  for ( int ii = 0 ; ii != ranges_str->size(); ii++)
    ranges_Bagel->at(ii) = *range_conversion_map_->at(ranges_str->at(ii));

  return ranges_Bagel;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template class TensOp_Computer::TensOp_Computer<double>;
template class TensOp_Computer::TensOp_Computer<complex<double>>;
///////////////////////////////////////////////////////////////////////////////////////////////////////
