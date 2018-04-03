#ifndef __SRC_PROP_PROPTOOL_PROPUtils_H
#define __SRC_PROP_PROPTOOL_PROPUtils_H

#include<stdio.h>
#include<iostream>
#include<sstream>
#include<stdlib.h>
#include<math.h>
#include<vector>
#include<algorithm>
#include<utility>
#include<tuple>
#include<string>
#include<list>
#include<memory>
#include<map>
#include <iostream>
#include <numeric>
#include <complex> 
#include <cassert>
namespace WickUtils {  

  using delta_ints = std::vector<std::vector<std::pair<int,int>>>;
  using delta_strs = std::vector<std::vector<std::pair<std::string,std::string>>>;
  using delta_bools = std::vector<std::vector<std::pair<bool,bool>>>;

  using vv_ints  = std::vector< std::vector<int> >;
  using vv_strs  = std::vector< std::vector<std::string> >;
  using vv_bools = std::vector< std::vector<bool> >;

  using pint_vec = std::vector<std::pair<int,int>>;
  using pstr_vec = std::vector<std::pair<std::string,std::string>>;
  using pbool_vec = std::vector<std::pair<bool,bool>>;
 
  //routines for mimicking for loop of arbitrary depth
  void fvec_cycle(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max ) ;
  bool fvec_cycle_test(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max ) ;
  bool fvec_cycle(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max , std::shared_ptr<std::vector<int>> min) ;
  bool constrained_fvec_cycle(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max) ;
 
  bool fvec_cycle_skipper(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max,
                          std::shared_ptr<std::vector<int>> min,  int fixed_index_position );
 
  bool fvec_cycle_skipper(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max, int fixed_index_position );

  bool fvec_cycle_skipper(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max ,
                          std::shared_ptr<std::vector<int>> min ) ;

  bool fvec_cycle_skipper_f2b(std::shared_ptr<std::vector<int>> forvec, std::shared_ptr<std::vector<int>> max,
                              std::shared_ptr<std::vector<int>> min ) ;

  template<class T1>
  std::shared_ptr<std::vector<std::shared_ptr<std::vector<T1>>>> combgen( std::shared_ptr<std::vector<T1>> invec);

  std::shared_ptr<std::vector<std::shared_ptr<std::vector<int>>>> get_N_in_M_combsX( std::shared_ptr<std::vector<int>> vec1, int NN );
  std::shared_ptr<std::vector<std::shared_ptr<std::vector<int>>>> get_N_in_M_combsX( std::shared_ptr<const std::vector<int>> vec1, int NN );
 
  std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::pair<int,int>>>>>
  get_unique_pairs(std::shared_ptr<std::vector<int>> ids1 , std::shared_ptr<std::vector<int>> ids2 , int num_pairs);
 
  std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::pair<int,int>>>>>
  get_unique_pairs(std::shared_ptr<std::vector<int>> ids1 , std::shared_ptr<std::vector<int>> ids2 ); 
 
  std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::pair<int,int>>>>>
  get_unique_pairs(std::shared_ptr< const std::vector<int>> ids1 , std::shared_ptr< const std::vector<int>> ids2 , int num_pairs);
  std::shared_ptr<std::vector<int>> reorder_vector(std::vector<int>& neworder , const std::vector<int>& origvec ) ;

  void print_pvec (pint_vec pvec) ;

  std::shared_ptr<std::vector<int>> get_unc_ids_from_deltas_ids_comparison(std::shared_ptr<std::vector<int>> ids , std::shared_ptr<std::vector<std::pair<int,int>>> deltas );

  std::shared_ptr<std::vector<int>> get_unc_ids_from_deltas_ids(std::shared_ptr<std::vector<int>> ids , std::shared_ptr<std::vector<std::pair<int,int>>> deltas );

  std::string get_Aname( const std::vector<std::string>& full_idxs, const std::vector<std::string>& full_idx_ranges );

  std::string get_Aname( const std::vector<std::string>& full_idxs, const std::vector<std::string>& full_idx_ranges,
                         const std::vector<char>& proj_names );

  std::string get_Aname( const std::vector<std::string>& full_idxs, const std::vector<std::string>& full_idx_ranges, 
                         const std::vector<std::pair<int,int>>& all_ctrs_pos  );
 
  std::string get_Aname( const std::vector<std::string>& full_idxs, const std::vector<std::string>& full_idx_ranges,
                         const std::vector<std::pair<int,int>>& all_ctrs_pos, const std::vector<char>& proj_names );


  std::string get_civec_name( const int state_num,  const int norb,  const int nalpha, const int nbeta);

  std::string get_gamma_name( std::shared_ptr<std::vector<std::string>> full_idx_ranges,  std::shared_ptr<std::vector<bool>> aops_vec,
                              std::shared_ptr<std::vector<int>> idxs_pos, std::string Bra_name, std::string Ket_name );

  std::string get_gamma_name( std::shared_ptr<const std::vector<std::string>> full_idx_ranges,  std::shared_ptr<const std::vector<bool>> aops_vec,
                              std::shared_ptr<std::vector<int>> idxs_pos, std::string Bra_name, std::string Ket_name );

  std::string get_gamma_name( const std::vector<std::string>& full_idx_ranges, const std::vector<bool>& aops_vec,
                              const std::vector<int>& idxs_pos, std::string Bra_name, std::string Ket_name );


  std::shared_ptr<std::vector<pint_vec>>  
  get_cross_pairs( std::shared_ptr<std::vector<int>> vec1 , std::shared_ptr<std::vector<int>> vec2, std::shared_ptr<std::vector<std::string>> id_names );

  bool RangeCheck(const std::vector<std::string>& id_ranges, const std::vector<bool>& aops ) ;
  
  std::shared_ptr<std::vector<std::pair<int,int>>>
  standardize_delta_ordering_generic( const std::vector<std::pair<int,int>>& deltas_pos, const std::vector<std::string>& idxs  );

  std::string get_ctp_name( const std::vector<std::string>& idxs, const std::vector<std::string>& id_ranges, const std::vector<std::pair<int,int>>& ctrs_pos ); 

  std::string get_ctp_name( const std::vector<std::string>& idxs, const std::vector<std::string>& id_ranges );

  unsigned int range_to_prime(char range );
  unsigned int range_to_prime_spinfree(char range );

  template<class DataType>
  void print_vector(std::vector<DataType> invec, std::string name =""){
    if (name != "" ) 
      std::cout << name << " ="; 
    std::cout << " [ ";
    for (auto  elem : invec)
      std::cout << elem << " " ;
    std::cout << "]  " ;
    return;
  }

  template<class T1, class T2  >
  void print_pair_vector(std::vector<std::pair<T1,T2>> invec, std::string name =""){
    if (name != "" ) 
      std::cout << name << " ="; 
    std::cout << " [ ";
    for (auto  elem : invec)
      std::cout << "(" << elem.first << "," << elem.second << ") ";
    std::cout << "]  " ;
    return;
  }

  template<class T1, class T2, class T3, class T4 >
  void print_pair_pair_vector( std::vector<std::pair<std::pair<T1,T2>, std::pair<T3,T4>>> ccp_vec,
                               std::string name = "" ) {
    std::cout << name << " = [ "; std::cout.flush();
    for ( auto& elem : ccp_vec ){
      std::cout << "{(" << elem.first.first << "," << elem.first.second << "),("  ;std::cout.flush();
      std::cout << elem.second.first << "," << elem.second.second << ")} "  ; std::cout.flush();
    }
    std::cout << "]" ; std::cout.flush();
    
    return;
  }
 
  template<typename DataType>
  void print_vec_elem_names( std::vector<std::shared_ptr<DataType>>& invec, std::string name = "" ) {
    std::cout <<  name << " = [ " ; std::cout.flush();
    for ( auto& elem : invec )
      std::cout << elem->name() << " " ; std::cout.flush();
    std::cout << " ] "; std::cout.flush();
  }
 
  template<typename DataType>
  void print_vec_elem_names( std::vector<DataType>& invec, std::string name = "" ) {
    std::cout <<  name << " = [ " ; std::cout.flush();
    for ( auto& elem : invec )
      std::cout << elem.name() << " " ; std::cout.flush();
    std::cout << " ] "; std::cout.flush();
  }

  template<typename DataType> // returns the relative order of the elements in destination and origin
  std::shared_ptr<std::vector<int>> get_pattern_match_order( std::shared_ptr<std::vector<DataType>> destination , std::shared_ptr<std::vector<DataType>> origin ) { 
    assert( destination->size() == origin->size() ); 
    std::shared_ptr<std::vector<int>> new_order = std::make_shared<std::vector<int>>(destination->size()); 
    std::vector<int>::iterator new_order_it = new_order->begin();  
    for ( int ii = 0 ; ii != destination->size() ; ii++  ) 
      for ( int jj = 0 ; jj != origin->size() ; jj++  ) 
        if ( destination->at(ii) == origin->at(jj) ) {
          *new_order_it++ = jj;
          break;
        }
    assert( new_order_it == new_order->end()); // if this trips the vectors don't have the same elements 
    return new_order;
  }


  //TODO you need type name here, but why ? Find out, it could be a problem.
  template<typename DataType>
  std::shared_ptr<std::vector<DataType>> dereference_vector(std::vector<DataType*>& invec){
    std::shared_ptr<std::vector<DataType>> dereffed_vec = std::make_shared<std::vector<DataType>>(invec.size());
    typename std::vector<DataType>::iterator dv_iter; 
    for ( typename std::vector<DataType*>::iterator iv_iter = invec.begin() ; iv_iter!= invec.end(); iv_iter++ ) 
      *dv_iter++ = **iv_iter;  
    return dereffed_vec;
  }


}

#endif
