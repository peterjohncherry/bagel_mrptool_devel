#include <bagel_config.h>
#include <src/prop/proptool/moint.h>
#include <src/prop/proptool/moint_computer.h>

using namespace std;
using namespace bagel; 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//note, this does not have the diagonal component
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
shared_ptr<SMITH::Tensor_<DataType>> MOInt_Computer<DataType>::get_v2( const vector<SMITH::IndexRange>& blocks ) { 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  K2ext<DataType> v2 =  K2ext<DataType>( info_, coeffs_, blocks );
  shared_ptr<Tensor> t = v2.tensor();
  return t;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Dupe routine with string input
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
shared_ptr<SMITH::Tensor_<DataType>> MOInt_Computer<DataType>::get_v2( const vector<string>& blocks ) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  shared_ptr<vector<SMITH::IndexRange>> id_ranges;// = Get_Bagel_Index_Ranges( blocks ); 
  return  K2ext<DataType>( info_, coeffs_, *id_ranges ).tensor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//is the core fock minus diagonal component from above
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
shared_ptr<SMITH::Tensor_<DataType>> MOInt_Computer<DataType>::get_h1( const vector<SMITH::IndexRange>& blocks ) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  return  MOFock<DataType>( info_, blocks ).tensor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Dupe routine with string input
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
shared_ptr<SMITH::Tensor_<DataType>> MOInt_Computer<DataType>::get_h1( const vector<string>& blocks ) {
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  shared_ptr<vector<SMITH::IndexRange>> id_ranges;// = Get_Bagel_Index_Ranges( blocks ); 
  return  MOFock<DataType>( info_, *id_ranges ).tensor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template class MOInt_Computer<double>;
template class MOInt_Computer<std::complex<double>>;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
