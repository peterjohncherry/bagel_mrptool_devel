#include <bagel_config.h>
#ifdef COMPILE_SMITH

 #include <src/smith/wicktool/CtrTensOp.h>
 #include <src/smith/wicktool/WickUtils.h>
 #include <src/smith/wicktool/gamma_generator.h>

 //#include "WickUtils.h"
 //#include "CtrTensOp.h"
 //#include "gamma_generator.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////
template<typename DType>
void TensorPart<DType>::get_name(){
////////////////////////////////////////////////////////////////////////////
 name = "";

 for(std::string id : *idxs)
   name += id;
 name+="_"; 

 for(std::string id : *id_ranges)
   name += id[0];

 return;
};

/////////////////////////////////////////////////////////////////////////////
template<class DType>
void CtrTensorPart<DType>::get_name(){
/////////////////////////////////////////////////////////////////////////////
  name = "";
  for(string id : *full_idxs)
    name += id;
  name+="_"; 

  for(string id : *full_id_ranges)
    name += id[0];

  auto ctrs_buff = make_shared<vector<pair<int,int>>>(*ctrs_pos);
  auto ctrs_buff_standard = GammaGenerator::Standardize_delta_ordering_generic(ctrs_buff ) ;

  if (ctrs_buff_standard->size() !=0){
    name+="_"; 
    for(pair<int,int> ctr : *ctrs_buff_standard)
      name += to_string(ctr.first)+to_string(ctr.second);
  }
  return;
};

/////////////////////////////////////////////////////////////////////////////
template<class DType>
void CtrMultiTensorPart<DType>::get_name(){
/////////////////////////////////////////////////////////////////////////////
  name = "";
  for(string id : *full_idxs)
    name += id;
  name+="_"; 

  for(string id : *full_id_ranges)
    name += id[0];

  auto ctrs_buff = make_shared<vector<pair<int,int>>>(*all_ctrs_pos);
  auto ctrs_buff_standard = GammaGenerator::Standardize_delta_ordering_generic(ctrs_buff ) ;

  if (ctrs_buff_standard->size() !=0){
    name+="_"; 
    for(pair<int,int> ctr : *ctrs_buff_standard)
      name += to_string(ctr.first)+to_string(ctr.second);
  }
  return;
};

/////////////////////////////////////////////////////////////////////////////
template<class DType>
string CtrTensorPart<DType>::get_next_name(shared_ptr<vector<pair<int,int>>> new_ctrs_pos){
/////////////////////////////////////////////////////////////////////////////
  string new_name = "";
  for(string id : *full_idxs)
    new_name += id;
  new_name+="_"; 

  for(string id : *full_id_ranges)
    new_name += id[0];

  if (new_ctrs_pos->size() >=1){
    new_name+="_"; 
    for(int ii=0;  ii!= new_ctrs_pos->size(); ii++){
      new_name += to_string(new_ctrs_pos->at(ii).first)+to_string(new_ctrs_pos->at(ii).second);
    }
  }

  return new_name;
}
/////////////////////////////////////////////////////////////////////////////
template<class DType>
string CtrMultiTensorPart<DType>::get_next_name(shared_ptr<vector<pair<int,int>>> new_ctrs_pos){
/////////////////////////////////////////////////////////////////////////////
  string new_name = "";
  for(string id : *full_idxs)
    new_name += id;
  new_name+="_"; 

  for(string id : *full_id_ranges)
    new_name += id[0];

  if (new_ctrs_pos->size() >=1){
    new_name+="_"; 
    for(int ii=0;  ii!= new_ctrs_pos->size(); ii++){
      new_name += to_string(new_ctrs_pos->at(ii).first)+to_string(new_ctrs_pos->at(ii).second);
    }
  }

  return new_name;
};
//////////////////////////////////////////////////////////////////////////////
template<typename DType>
void CtrTensorPart<DType>::get_ctp_idxs_ranges(){
//////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrTensorPart
cout << "CtrTensorPart<DType>::get_ctp_idxs_ranges" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////

  vector<bool> get_unc(full_idxs->size(), true);
  for (int ii =0; ii<ctrs_pos->size() ; ii++){
    if ( ctrs_pos->at(ii).first == ctrs_pos->at(ii).second)
      break;
    get_unc[ctrs_pos->at(ii).first] = false;
    get_unc[ctrs_pos->at(ii).second] = false;
   }

  bool survive_indep = true;
  unc_pos = make_shared<vector<int>>(0);
  id_ranges = make_shared<vector<string>>(0);
  idxs = make_shared<vector<string>>(0);
  for ( int ii = 0 ; ii !=get_unc.size() ; ii++ ) {
    if (get_unc[ii]){
      id_ranges->push_back(full_id_ranges->at(ii));
      idxs->push_back(full_idxs->at(ii));
      unc_pos->push_back(ii);
    }
  } 
  
  unc_rel_pos = make_shared<map<int,int>>();
  for( int ii =0 ; ii != unc_pos->size(); ii++) 
    unc_rel_pos->emplace(unc_pos->at(ii), ii);

//  vprint(*unc_pos, "unc_pos"); 
//  vprint(*idxs, "idxs"); 
//  vprint(*id_ranges, "id_ranges"); 
 
  return; 
}
//////////////////////////////////////////////////////////////////////////////
template<typename DType>
void CtrMultiTensorPart<DType>::get_cmtp_idxs_ranges(){
//////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrMultiTensorPart
cout << "CtrMultiTensorPart<DType>::get_ctp_idxs_ranges" << endl; 
#endif 
//////////////////////////////////////////////////////////////////////////////

  vector<bool> get_unc(full_idxs->size(), true);
  for (int ii =0; ii<all_ctrs_pos->size() ; ii++){
    if ( all_ctrs_pos->at(ii).first == all_ctrs_pos->at(ii).second)
      break;
    get_unc[all_ctrs_pos->at(ii).first] = false;
    get_unc[all_ctrs_pos->at(ii).second] = false;
   }

  bool survive_indep = true;
  all_unc_pos = make_shared<vector<int>>(0);
  for ( int ii = 0 ; ii !=get_unc.size() ; ii++ ) {
    if (get_unc[ii]){
      all_unc_pos->push_back(ii);
    }
  } 
  
  all_unc_rel_pos = make_shared<map<int,int>>();
  for( int ii =0 ; ii != all_unc_pos->size(); ii++) 
    all_unc_rel_pos->emplace(all_unc_pos->at(ii), ii);

//  vprint(*unc_pos, "unc_pos"); 
//  vprint(*idxs, "idxs"); 
//  vprint(*id_ranges, "id_ranges"); 
 
  return; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void CtrTensorPart<DType>::FullContract( shared_ptr<map<string,shared_ptr<CtrTensorPart<DType>> >> Tmap,
                                         shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list,
                                         shared_ptr<map<string, shared_ptr<vector<shared_ptr<CtrOp_base>> > >> ACompute_map ){
/////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrTensorPart
cout << "CtrTensorPart<DType>::FullContract new" << endl; 
#endif 
/////////////////////////////////////////////////////////////////////////////////////////////////////
cout << endl <<  "CtrTensorPart<DType>::FullContract NEWVER : CTP name =  " << name << endl;

  cout << "ctrs_todo = ( " ; cout.flush();
  for ( pair<int,int> ctr : *ctrs_todo)
    cout << "(" <<  ctr.first << "," <<  ctr.second << ") " ;   
  cout <<")" << "     ctrs_todo->size() = " << ctrs_todo->size() <<  endl;


  while ( ctrs_todo->size() != 0 ){ 
    string CTP_in_name = get_next_name(ctrs_done);
    ctrs_done->push_back(ctrs_todo->back());
    string CTP_out_name = get_next_name(ctrs_done);
    ctrs_todo->pop_back();

    shared_ptr<CtrTensorPart<DType>> CTP_in;
    if ( Tmap->find(CTP_in_name) == Tmap->end()) {
      shared_ptr<vector<pair<int,int>>> ctrs_pos_in = make_shared<vector<pair<int,int>>>(*ctrs_todo);
      shared_ptr<vector<pair<int,int>>> new_ReIm_factors = make_shared<vector<pair<int,int>>>(1, make_pair(1,1));
      CTP_in = make_shared< CtrTensorPart<DType> >( full_idxs, full_id_ranges, ctrs_pos_in, new_ReIm_factors );
    } else {
      CTP_in = Tmap->at(CTP_in_name);
    }
    pair<int,int> ctrs_rel_pos_in = make_pair(CTP_in->unc_rel_pos->at(ctrs_done->back().first), CTP_in->unc_rel_pos->at(ctrs_done->back().second));   

    if ( ACompute_map->find(CTP_in_name) == ACompute_map->end()) {                                                       
      shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list_new = make_shared<vector<shared_ptr<CtrOp_base> >>(0);   
      CTP_in->FullContract(Tmap, ACompute_list_new, ACompute_map);                                                       
    }
    cout << "Contract " << CTP_in_name << " over  (" << ctrs_done->back().first << ","<< ctrs_done->back().second << ") to get " << CTP_out_name <<  endl;
    ACompute_list->push_back( make_shared<CtrOp_same_T> (CTP_in_name, CTP_out_name, ctrs_done->back(), ctrs_rel_pos_in, "same_T new" ));
  } 
  ACompute_map->emplace(myname(), ACompute_list);
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void CtrMultiTensorPart<DType>::FullContract(shared_ptr<map<string,shared_ptr<CtrTensorPart<DType>> >> Tmap,
                                             shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list ,
                                             shared_ptr<map<string, shared_ptr<vector<shared_ptr<CtrOp_base>> > >> ACompute_map ){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrMultiTensorPart
cout << "CtrMultiTensorPart<DType>::FullContract" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << endl << "CtrMultiTensorPart<DType>::FullContract NEWVER :   CMTP name = " << myname() << endl;

   if (all_ctrs_pos->size() > 0 ) {
     int T1loc, T2loc;
     T1loc = cross_ctrs_pos->back().first.first;
     T2loc = cross_ctrs_pos->back().second.first;
    
     if ( (CTP_vec->size() == 2) && ( cross_ctrs_pos->size() > 0 ) ) {
     
       shared_ptr<CtrTensorPart<DType>> new_CTP = Binary_Contract_diff_tensors(cross_ctrs_pos->back(), all_ctrs_pos->back(), Tmap,  ACompute_list, ACompute_map);
         
       if ( cross_ctrs_pos->size() >1 )   {
         cout << "Obtaining contraction list for " << new_CTP->myname() << " using single tensor approach" <<endl;  
         new_CTP->FullContract(Tmap, ACompute_list, ACompute_map);
       }
     } else {
       cout << "USING MT BINARY CONTRACT DIFF TENSORS" << endl;
       auto new_CMTP = Binary_Contract_diff_tensors_MT(CTP_vec->at(cross_ctrs_pos->back().first.first)->myname(),CTP_vec->at(cross_ctrs_pos->back().second.first)->myname(),
                                                       all_ctrs_pos->back(), Tmap, ACompute_list);
       new_CMTP->FullContract(Tmap, ACompute_list);
     }
  }
  return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
shared_ptr<CtrTensorPart<DType>>
 CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors( pair<pair<int,int>, pair<int,int>> cross_ctr,// These two arguments should be 
                                                          pair<int,int> abs_ctr,                      // equivalent ! 
                                                          shared_ptr<map<string,shared_ptr<CtrTensorPart<DType>> >> Tmap,
                                                          shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list,
                                                          shared_ptr<map<string, shared_ptr<vector<shared_ptr<CtrOp_base>> > >> ACompute_map ){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrMultiTensorPart
cout << "CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors  NEWVER" << endl; 
   cout << "name = " << name << endl; 

   shared_ptr<CtrTensorPart<DType>> T1;
   shared_ptr<CtrTensorPart<DType>> T2;

   int T1pos = cross_ctr.first.first;
   int T2pos = cross_ctr.second.first;
   int T1ctr = cross_ctr.first.second;
   int T2ctr = cross_ctr.second.second;

   CTP_vec->at(T1pos)->FullContract(Tmap, ACompute_list, ACompute_map);
   CTP_vec->at(T2pos)->FullContract(Tmap, ACompute_list, ACompute_map);
   int num_internal_ctrs = CTP_vec->at(T1pos)->ctrs_pos->size() + CTP_vec->at(T2pos)->ctrs_pos->size();

   //Swapping tensors round to maintain consistent ordering
   if (cross_ctr.first.first < cross_ctr.second.first) {
     T1 = CTP_vec->at(cross_ctr.first.first);
     T2 = CTP_vec->at(cross_ctr.second.first);
   } else { 
     T2 = CTP_vec->at(cross_ctr.first.first);
     T1 = CTP_vec->at(cross_ctr.second.first);
     int buff = T1pos;
     T1pos = T2pos;
     T2pos = buff;
     buff = T1ctr ;
     T1ctr = T2ctr;
     T2ctr = buff;
   }
   string T1name = T1->name; 
   string T2name = T2->name; 

   cout << "T1name = "<< T1name << "    "  << "T1pos = "<< T1pos << "    " << "T1ctr = "<< T1ctr << endl;
   cout << "T2name = "<< T2name << "    "  << "T2pos = "<< T2pos << "    " << "T2ctr = "<< T2ctr << endl;

   auto full_id_ranges = make_shared<vector<string>>(T1->full_id_ranges->begin(), T1->full_id_ranges->end()) ;
   full_id_ranges->insert(full_id_ranges->end(), T2->full_id_ranges->begin(), T2->full_id_ranges->end()); 

   auto full_idxs = make_shared<vector<string>>(T1->full_idxs->begin(), T1->full_idxs->end()) ;
   full_idxs->insert(full_idxs->end(), T2->full_idxs->begin(), T2->full_idxs->end()) ;

   shared_ptr<vector<pair<int,int>>> ctrs_done = make_shared<vector<pair<int,int>>>(0);
   
   int T1shift =  Tsizes_cml->at(T1pos);
   int T2shift =  Tsizes_cml->at(T2pos);
   for (auto ctr : *T1->ctrs_pos)
     ctrs_done->push_back( make_pair(ctr.first+T1shift, ctr.second+T1shift));
   for (auto ctr : *T2->ctrs_pos)
     ctrs_done->push_back( make_pair(ctr.first+T2shift, ctr.second+T2shift));

   shared_ptr<vector<pair<int,int>>> ctrs_todo = make_shared<vector<pair<int,int>>>(0);
   for (int ii = 0 ;  ii != cross_ctrs_pos->size() ; ii++) 
     if ( (cross_ctrs_pos->at(ii).first.first + cross_ctrs_pos->at(ii).second.first) == (T1pos + T2pos) )
       ctrs_todo->push_back(make_pair(Tsizes_cml->at(cross_ctrs_pos->at(ii).first.first)  +  cross_ctrs_pos->at(ii).first.second,
                                     Tsizes_cml->at(cross_ctrs_pos->at(ii).second.first) +  cross_ctrs_pos->at(ii).second.second));

    
   auto full_ctrs = make_shared<vector<pair<int,int>>>(0);
   full_ctrs->insert(full_ctrs->end(), ctrs_done->begin(), ctrs_done->end());
   full_ctrs->insert(full_ctrs->end(), ctrs_todo->begin(), ctrs_todo->end());
 
   ctrs_done->push_back(ctrs_todo->back());
   ctrs_todo->pop_back();
  
   int T1_ctr_rel_pos = T1->unc_rel_pos->at(T1ctr);
   int T2_ctr_rel_pos = T2->unc_rel_pos->at(T2ctr);

   auto new_CTP = make_shared< CtrTensorPart<DType> >(full_idxs, full_id_ranges, full_ctrs, make_shared<vector<pair<int,int>>>(1, abs_ctr )); 
   new_CTP->ctrs_todo = ctrs_todo;
   new_CTP->ctrs_done = ctrs_done;
   
   ACompute_list->push_back(make_shared<CtrOp_diff_T>( T1name, T2name, new_CTP->get_next_name(new_CTP->ctrs_done),  abs_ctr.first, abs_ctr.second, T1_ctr_rel_pos, T2_ctr_rel_pos, "diff_T_prod"));
   auto new_CTData = make_shared<DType>();
    
   if (Tmap->find(new_CTP->name) == Tmap->end()){
     Tmap->emplace(new_CTP->name, new_CTP);
   } else {
     cout << "did not put " << myname() << " into Tmap" << endl;
   }

   cout << "BCDT contracting " << T1name << " and " << T2name << " over (" << abs_ctr.first << "," << abs_ctr.second << ") to get " << get_next_name(new_CTP->ctrs_done) << endl;
   return new_CTP;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void CtrTensorPart<DType>::FullContract(shared_ptr<map<string,shared_ptr<CtrTensorPart<DType>> >> Tmap,
                                        shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list ){
/////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrTensorPart
cout << "CtrTensorPart<DType>::FullContract" << endl; 
#endif 
/////////////////////////////////////////////////////////////////////////////////////////////////////
cout << endl <<  "CtrTensorPart<DType>::FullContract : CTP name =  " << name << endl;
 
  auto ctp_loc = Tmap->find(name);
  if(ctp_loc == Tmap->end())
    Tmap->emplace(name , make_shared<CtrTensorPart>(*this));
  
  if( ctrs_todo->size() == 0 ) { 
    auto new_ctrs_pos = make_shared<vector<pair<int,int>>>(0);
    auto new_ReIm_factors = make_shared<vector<pair<int,int>>>(0); cout << "hello1" <<  endl;
    auto new_CTP = make_shared< CtrTensorPart<DType> >(full_idxs, full_id_ranges, new_ctrs_pos, new_ReIm_factors );     cout << "hello2" <<  endl;
    string new_name = get_next_name(new_ctrs_pos); cout << "new_name for unc_block = " <<   new_name << endl;
    ACompute_list->push_back(make_shared<CtrOp_base>(new_name, "get_block"));
    Tmap->emplace(new_name, new_CTP);

  } else if( ctrs_todo->size() > 1) { 
    cout << "A1" ; cout.flush();    cout <<"ctrs_pos->size() = " << ctrs_pos->size() << endl;    cout <<"ctrs_todo->size() = " << ctrs_todo->size() << endl;
    auto new_ctrs_pos = make_shared<vector<pair<int,int>>>(ctrs_pos->begin(), ctrs_pos->end()-1);   cout << "A2" ; cout.flush();
    auto new_ctrs_todo = make_shared<vector<pair<int,int>>>(ctrs_todo->begin(), ctrs_todo->end()-1);   cout << "A3" ; cout.flush();
    cout << "A3" ; cout.flush();    cout <<"new_ctrs_pos->size() = " << new_ctrs_pos->size() << endl;    cout <<"new_ctrs_todo->size() = " << new_ctrs_todo->size() << endl;
    auto new_ReIm_factors = make_shared<vector<pair<int,int>>>(ReIm_factors->begin(), ReIm_factors->end());   cout << "A4" ; cout.flush();
    string new_name = get_next_name(new_ctrs_pos);  cout << "A5" ; cout.flush();
    shared_ptr<CtrTensorPart<DType>> new_CTP; 
   
    if ( Tmap->find(new_name) == Tmap->end() ){ 
      cout << "A5a" ; cout.flush(); new_CTP = make_shared< CtrTensorPart<DType> >(full_idxs, full_id_ranges, new_ctrs_pos, new_ReIm_factors ); cout << "A6" ; cout.flush();
      Tmap->emplace(new_CTP->name, new_CTP);
    } else {
      new_CTP = Tmap->at(new_name);    cout << "A9" ; cout.flush(); cout << "new_name = " << new_name << " ?= " << new_CTP->myname() <<  " = new_CTP->myname() " << endl;
    }
    new_CTP->ctrs_todo = new_ctrs_todo;

    cout << "A10" ; cout.flush();
    pair<int,int> ctrs_rel_pos = make_pair(new_CTP->unc_rel_pos->at(ctrs_todo->back().first), new_CTP->unc_rel_pos->at(ctrs_todo->back().second));    cout << "A11" ; cout.flush();
    ACompute_list->push_back(make_shared<CtrOp_same_T> (new_name, name, ctrs_todo->back(), ctrs_rel_pos, "same_T C" ));    cout << "A12" ; cout.flush();
    new_CTP->FullContract(Tmap,  ACompute_list); cout << "A13" ; cout.flush();

    
  } else if( ctrs_todo->size() == 1 ){ 
    cout << "hello4" << endl;
    auto unc_ctrs_pos = make_shared<vector<pair<int,int>>>(1, make_pair(0,0) );
    auto unc_ReIm_factors = make_shared<vector<pair<int,int>>>(0); 
    auto unc_CTP = make_shared< CtrTensorPart<DType> >(full_idxs, full_id_ranges, unc_ctrs_pos, unc_ReIm_factors ); 

    cout << "A1" ; cout.flush();
    // Need to create a list of uncontracted tensors
    unc_CTP->CTdata = make_shared<DType>(); 
    Tmap->emplace(unc_CTP->name, unc_CTP);
    required_Tblocks->push_back(unc_CTP->name);
    pair<int,int> ctrs_rel_pos = make_pair(unc_CTP->unc_rel_pos->at(ctrs_todo->back().first), unc_CTP->unc_rel_pos->at(ctrs_todo->back().second));

    ACompute_list->push_back(make_shared<CtrOp_same_T> (unc_CTP->name, name, ctrs_todo->back(), ctrs_rel_pos, "same_T B" ));

  }
  cout << endl << endl;
  return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
void CtrMultiTensorPart<DType>::FullContract(shared_ptr<map<string,shared_ptr<CtrTensorPart<DType>> >> Tmap,
                                             shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list ){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrMultiTensorPart
cout << "CtrMultiTensorPart<DType>::FullContract" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cout << endl << "CtrMultiTensorPart<DType>::FullContract :   CMTP name = " << myname() << endl;

   if (all_ctrs_pos->size() > 0 ) {
     int T1loc, T2loc;
     T1loc = cross_ctrs_pos->back().first.first;
     T2loc = cross_ctrs_pos->back().second.first;
    
     if ( (CTP_vec->size() == 2) && ( cross_ctrs_pos->size() > 0 ) ) {
     
       shared_ptr<CtrTensorPart<DType>> new_CTP = Binary_Contract_diff_tensors(cross_ctrs_pos->back(), all_ctrs_pos->back(), Tmap,  ACompute_list);
       cout << " new_CTP from binary contract diff tensors=  = " << new_CTP->myname() << endl;
         
       if ( cross_ctrs_pos->size() >1 )   {
         cout << "Contracting " << new_CTP->myname() << " using single tensor approach" <<endl;  
         new_CTP->FullContract(Tmap, ACompute_list);
       }
     } else {
       cout << "USING MT BINARY CONTRACT DIFF TENSORS" << endl;
       auto new_CMTP = Binary_Contract_diff_tensors_MT(CTP_vec->at(cross_ctrs_pos->back().first.first)->myname(),CTP_vec->at(cross_ctrs_pos->back().second.first)->myname(),
                                                       all_ctrs_pos->back(), Tmap, ACompute_list);
       new_CMTP->FullContract(Tmap, ACompute_list);
     }
  }
  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
shared_ptr<CtrMultiTensorPart<DType>>
CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors_MT(string T1name, string T2name, pair<int,int> ctr,
                                                           shared_ptr< map<string, shared_ptr<CtrTensorPart<DType>>> > Tmap,
                                                           shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list){
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrMultiTensorPart
cout << "CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors_MT" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors_MT" << endl; 

   auto T1T2_ctrd =  Binary_Contract_diff_tensors(cross_ctrs_pos->back(), ctr, Tmap, ACompute_list );
   Tmap->emplace(T1T2_ctrd->name, T1T2_ctrd);

   auto new_CTP_vec = make_shared<vector<shared_ptr<CtrTensorPart<DType>>>>(0);
   for ( auto CTP : *CTP_vec ) {  
     if (CTP->name == T1name || CTP->name == T2name)
       continue;
     new_CTP_vec->push_back(CTP);
   }
   
   CTP_vec->push_back(T1T2_ctrd);
   auto new_cross_ctrs_pos = make_shared<vector<pair<pair<int,int>, pair<int,int>>>>(cross_ctrs_pos->begin(), cross_ctrs_pos->end()-1);

   auto new_CMTP = make_shared< CtrMultiTensorPart<DType> >(new_CTP_vec, new_cross_ctrs_pos); 

   return new_CMTP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class DType>
shared_ptr<CtrTensorPart<DType>>
 CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors( pair<pair<int,int>, pair<int,int>> cross_ctr,// These two arguments should be 
                                                          pair<int,int> abs_ctr,                      // equivalent ! 
                                                          shared_ptr<map<string,shared_ptr<CtrTensorPart<DType>> >> Tmap,
                                                          shared_ptr<vector<shared_ptr<CtrOp_base> >> ACompute_list){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DBG_CtrMultiTensorPart
cout << "CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors" << endl; 
#endif 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   cout << "CtrMultiTensorPart<DType>::Binary_Contract_diff_tensors" << endl; 
   cout << "name = " << name << endl; 

   shared_ptr<CtrTensorPart<DType>> T1;
   shared_ptr<CtrTensorPart<DType>> T2;

   int T1pos = cross_ctr.first.first;
   int T2pos = cross_ctr.second.first;
   int T1ctr = cross_ctr.first.second;
   int T2ctr = cross_ctr.second.second;

   CTP_vec->at(T1pos)->FullContract(Tmap, ACompute_list);
   CTP_vec->at(T2pos)->FullContract(Tmap, ACompute_list);
   int num_internal_ctrs = CTP_vec->at(T1pos)->ctrs_pos->size() + CTP_vec->at(T2pos)->ctrs_pos->size();

   //Swapping tensors round to maintain consistent ordering
   if (cross_ctr.first.first < cross_ctr.second.first) {
     T1 = CTP_vec->at(cross_ctr.first.first);
     T2 = CTP_vec->at(cross_ctr.second.first);
   } else { 
     T2 = CTP_vec->at(cross_ctr.first.first);
     T1 = CTP_vec->at(cross_ctr.second.first);
     int buff = T1pos;
     T1pos = T2pos;
     T2pos = buff;
     buff = T1ctr ;
     T1ctr = T2ctr;
     T2ctr = buff;
   }
   string T1name = T1->name; 
   string T2name = T2->name; 

   cout << "T1name = "<< T1name << "    "  << "T1pos = "<< T1pos << "    " << "T1ctr = "<< T1ctr << endl;
   cout << "T2name = "<< T2name << "    "  << "T2pos = "<< T2pos << "    " << "T2ctr = "<< T2ctr << endl;

   auto full_id_ranges = make_shared<vector<string>>(T1->full_id_ranges->begin(), T1->full_id_ranges->end()) ;
   full_id_ranges->insert(full_id_ranges->end(), T2->full_id_ranges->begin(), T2->full_id_ranges->end()); 

   auto full_idxs = make_shared<vector<string>>(T1->full_idxs->begin(), T1->full_idxs->end()) ;
   full_idxs->insert(full_idxs->end(), T2->full_idxs->begin(), T2->full_idxs->end()) ;

   shared_ptr<vector<pair<int,int>>> ctrs_done = make_shared<vector<pair<int,int>>>(0);
   
   int T1shift =  Tsizes_cml->at(T1pos);
   int T2shift =  Tsizes_cml->at(T2pos);
   for (auto ctr : *T1->ctrs_pos)
     ctrs_done->push_back( make_pair(ctr.first+T1shift, ctr.second+T1shift));
   for (auto ctr : *T2->ctrs_pos)
     ctrs_done->push_back( make_pair(ctr.first+T2shift, ctr.second+T2shift));

   shared_ptr<vector<pair<int,int>>> ctrs_todo = make_shared<vector<pair<int,int>>>(cross_ctrs_pos->size()-1);
   for (int ii = 0 ;  ii != ctrs_todo->size() ; ii++) 
     if ( (cross_ctrs_pos->at(ii).first.first + cross_ctrs_pos->at(ii).second.first) == (T1pos + T2pos) )
       ctrs_todo->at(ii) = make_pair(Tsizes_cml->at(cross_ctrs_pos->at(ii).first.first)  +  cross_ctrs_pos->at(ii).first.second,
                                     Tsizes_cml->at(cross_ctrs_pos->at(ii).second.first) +  cross_ctrs_pos->at(ii).second.second);

   auto full_ctrs = make_shared<vector<pair<int,int>>>(0);
   full_ctrs->insert(full_ctrs->end(), ctrs_done->begin(), ctrs_done->end());
   full_ctrs->insert(full_ctrs->end(), ctrs_todo->begin(), ctrs_todo->end());

   int T1_ctr_rel_pos = T1->unc_rel_pos->at(T1ctr);
   int T2_ctr_rel_pos = T2->unc_rel_pos->at(T2ctr);

   auto new_CTP = make_shared< CtrTensorPart<DType> >(full_idxs, full_id_ranges, full_ctrs, make_shared<vector<pair<int,int>>>(1, abs_ctr )); 
   new_CTP->ctrs_todo = ctrs_todo;
   ACompute_list->push_back(make_shared<CtrOp_diff_T>( T1name, T2name, new_CTP->name,  abs_ctr.first, abs_ctr.second, T1_ctr_rel_pos, T2_ctr_rel_pos, "diff_T_prod"));
   auto new_CTData = make_shared<DType>();
    
   if (Tmap->find(new_CTP->name) == Tmap->end()){
     Tmap->emplace(new_CTP->name, new_CTP);
     cout << "put " << name << " into Tmap" << endl;
   } else {
     cout << "did not put " << myname() << " into Tmap" << endl;
   }

   cout << "BCDT contracting " << T1name << " and " << T2name << " over (" << abs_ctr.first << "," << abs_ctr.second << ") to get " << new_CTP->name << endl;
   return new_CTP;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template class TensorPart<double>;
template class CtrTensorPart<double>;
template class CtrMultiTensorPart<double>;
///////////////////////////////////////////////////////////////////////////////////////////////////////
    
#endif
