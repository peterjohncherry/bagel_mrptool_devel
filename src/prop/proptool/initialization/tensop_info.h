#ifndef __SRC_PROPTOOL_TENSOP_INFO_INIT
#define __SRC_PROPTOOL_TENSOP_INFO_INIT
#include <src/global.h>
#include <bagel_config.h>
#include <src/prop/proptool/algebraic_manipulator/tensop.h>
#include <src/prop/proptool/algebraic_manipulator/symmetry_operations.h>
#include <src/prop/proptool/algebraic_manipulator/constraints.h>

using namespace std;

namespace  TensOp_Info_Init {

/////////////////////////////////////////////////////////////////////////////////
//Build the operators here.
/////////////////////////////////////////////////////////////////////////////////
template<class DataType>
shared_ptr<TensOp::TensOp<DataType>> Initialize_Tensor_Op_Info( string op_name, shared_ptr<map<char, long unsigned int>> range_prime_map ) {
/////////////////////////////////////////////////////////////////////////////////
cout << "shared_ptr<TensOp::TensOp<DataType>>::Initialize_Tensor_Op_Info" << endl;

  vector<string> free     = {"c", "a", "v"};
  vector<string> not_core = {"a", "v"};
  vector<string> not_act  = {"c", "v"};
  vector<string> not_virt = {"c", "a"};
  vector<string> core     = {"c"};
  vector<string> act      = {"a"};
  vector<string> virt     = {"v"};

  pair<double,double>                factor = make_pair(1.0, 1.0);
  shared_ptr<vector<string>>         idxs;
  shared_ptr<vector<bool>>           aops;
  shared_ptr<vector<vector<string>>> idx_ranges;
  string                             time_symm;
  vector<shared_ptr<Transformation>> symmfuncs = vector<shared_ptr<Transformation>>(0);
  vector<shared_ptr<Constraint>> constraints = vector<shared_ptr<Constraint>>(0);
  int state_dep;

  static shared_ptr<Transformation_Hermitian> hconj = make_shared<Transformation_Hermitian>( "hconj" );  
  static shared_ptr<Transformation_Spinflip>  spinflip = make_shared<Transformation_Spinflip>( "spinflip" );
  static shared_ptr<Transformation_1032>  perm_1032 = make_shared<Transformation_1032>( "1032" ); 
  static shared_ptr<Transformation_2301>  perm_2301 = make_shared<Transformation_2301>( "2301" );
  static shared_ptr<Transformation_2103>  perm_2103 = make_shared<Transformation_2103>( "2103" );
  static shared_ptr<Transformation_3012>  perm_3012 = make_shared<Transformation_3012>( "3012" );
  static shared_ptr<Transformation_0321>  perm_0321 = make_shared<Transformation_0321>( "0321" );
  static shared_ptr<Transformation_1230>  perm_1230 = make_shared<Transformation_1230>( "1230" );
  static shared_ptr<Constraint_NotAllAct>  not_all_act = make_shared<Constraint_NotAllAct>( "NotAllAct" );

  if ( op_name == "H" ) {  /* ---- H Tensor (2 electron Hamiltonian ----  */

   idxs = make_shared<vector<string>>(vector<string> {"H0", "H1", "H2", "H3"});
   aops = make_shared<vector<bool>>(vector<bool>  { true, true, false, false});//TODO check this ordering is correct
   idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { free, free, free, free });
   symmfuncs = { hconj, perm_1032, perm_2301, perm_2103, perm_3012, perm_0321, perm_1230 }; 
   time_symm = "none";
   state_dep = 0;

  } else if ( op_name == "h" ) {  /* ---- h Tensor ( 1 electron Hamiltonian ) ----  */

   idxs = make_shared<vector<string>>(vector<string> {"h0", "h1"});
   aops = make_shared<vector<bool>>(vector<bool>  {true, false});
   idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { free,free });
   time_symm = "none";
   state_dep = 0;
  
  } else if ( op_name == "Q" ) {  /* ---- test six index ----  */

   idxs = make_shared<vector<string>>(vector<string> {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5" });
   aops = make_shared<vector<bool>>(vector<bool>  {true, true, true, false, false, false});
   idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { not_core,  not_core, not_core, not_virt, not_virt, not_virt });
   time_symm = "none";
   state_dep = 0;
 
  } else if ( op_name == "R" ) {  /* ---- test six index ----  */

   idxs = make_shared<vector<string>>(vector<string> {"R0", "R1", "R2", "R3", "R4", "R5" });
   aops = make_shared<vector<bool>>(vector<bool>  {true, true, true, false, false, false});
   idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { virt,  virt, act, act, core, core });
   time_symm = "none";
   state_dep = 0;

  } else if ( op_name == "f" ) {  /* ---- state averaged fock operator ----  */

   idxs = make_shared<vector<string>>(vector<string> {"f0", "f1"});
   aops = make_shared<vector<bool>>(vector<bool>  {true, false});
   idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { free,free });
   time_symm = "none";
   state_dep = 0;

  } else if ( op_name == "L" ) {  /* ---- L Tensor ----  */

    idxs = make_shared<vector<string>>(vector<string> {"L0", "L1", "L2", "L3"});
    aops = make_shared<vector<bool>>(vector<bool>  { false, false, true, true });
    idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { virt, virt, not_virt, not_virt });
    time_symm = "none";
    state_dep = 2;
    
      
  } else if ( op_name == "M" ) {  /* ---- M Tensor ----  */

    idxs = make_shared<vector<string>>(vector<string> {"M0", "M1", "M2", "M3"});
    aops = make_shared<vector<bool>>(vector<bool>  { true, true, false, false });
    idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { virt, virt, core, core });
    time_symm = "none";
    state_dep = 2;
 
  } else if ( op_name == "N" ) {  /* ---- N Tensor ----  */

    idxs = make_shared<vector<string>>(vector<string> {"N0", "N1", "N2", "N3"});
    aops = make_shared<vector<bool>>(vector<bool>  { true, true, false, false });
    idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { act, act, act, act });
    time_symm = "none";
    state_dep = 0;
    
  } else if ( op_name == "T" ) {  /* ---- T Tensor ----  */

    idxs = make_shared<vector<string>>(vector<string>{"T0", "T1", "T2", "T3"}  );
    aops = make_shared<vector<bool>>  (vector<bool>  {true, true, false, false} );
    idx_ranges =  make_shared<vector<vector<string>>>( vector<vector<string>> { not_core, not_core, not_virt, not_virt });
    time_symm = "none";
    state_dep = 2;
 
  } else if ( op_name == "S" ) {  /* ---- S Tensor ----  */

    idxs = make_shared<vector<string>>(vector<string>{"S0", "S1", "S2", "S3"}  );
    aops = make_shared<vector<bool>>  (vector<bool>  { true, true, false, false } );
    symmfuncs = { hconj }; 
    constraints = { not_all_act }; 
    idx_ranges =  make_shared<vector<vector<string>>>( vector<vector<string>> { not_core, not_core, not_virt, not_virt  });
    time_symm = "none";
    state_dep = 2;

  } else if ( op_name == "t" ) {  /* ---- T Tensor herm conj TODO  should find a better way fo dealing with this----  */
    cout << "getting t op " << endl;
    idxs = make_shared<vector<string>>(vector<string>{"t0", "t1", "t2", "t3"}  );
    aops = make_shared<vector<bool>>  (vector<bool>  {false, false, true, true } );
    idx_ranges =  make_shared<vector<vector<string>>>( vector<vector<string>> { not_core, not_core, not_virt, not_virt });
    time_symm = "none";
    state_dep = 0;

  } else if ( op_name == "X" ) {

    idxs = make_shared<vector<string>>( vector<string> {"X3", "X2", "X1", "X0"} );
    aops = make_shared<vector<bool>>( vector<bool> { false, false, true, true } );
    idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { not_core, not_core, not_virt, not_virt } );
    time_symm = "none";
    state_dep = 0;

  } else if ( op_name == "x" ) {

    idxs = make_shared<vector<string>>( vector<string> {"X0", "X1"} );
    aops = make_shared<vector<bool>>( vector<bool> { false, true } );
    idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { act, act } );
    time_symm = "none";
    state_dep = 2;

   } else if ( op_name == "Z" ) { /* 2el test op */

    idxs = make_shared<vector<string>>( vector<string> { "Z0", "Z1", "Z2", "Z3" } );
    aops = make_shared<vector<bool>>( vector<bool>  { false, false, true, true } );
    idx_ranges = make_shared<vector<vector<string>>>( vector<vector<string>> { act, act, act, act } );
    time_symm = "none";
    state_dep = 0;

  } else {
    
    throw runtime_error("Do not have in-built definition for operator \"" + op_name + "\", aborting !!!" ) ;

  }

  cout << "initializing tensop" << endl;
  shared_ptr<TensOp::TensOp<DataType>> new_tens =  make_shared<TensOp::TensOp<DataType>>( op_name, *idxs, *idx_ranges, *aops,
                                                                                          factor, symmfuncs, constraints, time_symm, state_dep, range_prime_map);
  return new_tens;
}
}
#endif
