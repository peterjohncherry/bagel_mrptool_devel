#ifndef __SRC_PROP_PROPTOOL_Range_Block_Info_H
#define __SRC_PROP_PROPTOOL_Range_Block_Info_H

// On reflection, all this information is not needed, only the original ranges (maybe also idxs and aops), and the transformation vector.
// It could be done this way, but a lot will need to be fed into the gamma generator.
// Factor is still necessary though.
#include <set> 
#include <vector>
#include <memory>
#include <src/prop/proptool/proputils.h>
 
class SRBI_Helper; 
class Split_Range_Block_Info; 
class Range_Block_Info : public std::enable_shared_from_this<Range_Block_Info> {
 
  friend SRBI_Helper;
  friend Split_Range_Block_Info;

  protected :
    std::pair<double,double> factors_; 
    std::shared_ptr<std::vector<int>> idxs_trans_;
    std::shared_ptr<std::vector<int>> aops_trans_;
    std::shared_ptr<std::vector<int>> rngs_trans_;

    std::shared_ptr<const std::vector<std::string>> orig_rngs_;
    std::shared_ptr<std::vector<char>> orig_rngs_ch_;
    std::shared_ptr<const std::vector<bool>> orig_aops_;

    std::set<std::vector<int>> sparsity_ ;
    std::string name_;

  public :

    long unsigned int plus_pnum_;
    long unsigned int kill_pnum_;
    bool no_transition_;
    int num_idxs_;
    
    std::shared_ptr<const std::vector<std::string>> unique_block_;

    Range_Block_Info( std::shared_ptr<const std::vector<std::string>> orig_block, std::shared_ptr<const std::vector<std::string>> unique_block, 
                      std::shared_ptr<std::vector<int>> idxs_trans,  std::pair<double,double> factors, const std::vector<bool>& aops );

    ~Range_Block_Info(){};
    
    std::pair<double,double> factors() const { return factors_; } 
    double Re_factor() const { return factors_.first; } 
    double Im_factor() const { return factors_.second; } 
  
    std::string name() const { return name_; } 
     
    std::shared_ptr<const std::vector<std::string>> orig_rngs() { return orig_rngs_; } 
    std::shared_ptr< std::vector<char>> orig_rngs_ch() { return orig_rngs_ch_; } 
 
    std::shared_ptr<std::vector<int>> idxs_trans() const { return idxs_trans_; }
    std::shared_ptr<std::vector<int>> aops_trans() const { return aops_trans_; }
    std::shared_ptr<std::vector<int>> rngs_trans() const { return rngs_trans_; }

    void transform_aops_rngs ( std::vector<bool>& aops, std::vector<char>& aops_rngs,  std::pair<double,double>& factors , char transformation );
    
    std::shared_ptr<Range_Block_Info> 
    get_transformed_block( std::shared_ptr<const std::vector<std::string> > trans_block, std::shared_ptr<const std::vector<std::string>> unique_block,
                           char op_trans, std::vector<bool>& aops );
};

class SRBI_Helper { 

  public :
    bool unique_;
    bool survives_;
    std::pair<double,double> factors_; 
    int num_idxs_; 
    std::shared_ptr<std::vector<std::shared_ptr<Range_Block_Info>>> rxnge_blocks_;
 
    std::shared_ptr<const std::vector<std::string>> orig_rngs_;
    std::shared_ptr<const std::vector<std::string>> orig_idxs_;
    std::shared_ptr<const std::vector<bool>> orig_aops_;

    std::shared_ptr<std::vector<int>> idxs_trans_;
    std::shared_ptr<std::vector<int>> aops_trans_;
    std::shared_ptr<std::vector<int>> rngs_trans_;
                              
    std::shared_ptr<const std::vector<std::string>> unique_block_;

    SRBI_Helper( std::shared_ptr<std::vector<std::shared_ptr<Range_Block_Info>>> range_blocks );

    SRBI_Helper( std::vector<std::shared_ptr<Range_Block_Info>>& range_blocks );

    void add_trans( std::shared_ptr<Split_Range_Block_Info> srbi, std::vector<int>&  op_order, std::vector<char> op_trans );
   ~SRBI_Helper(){};
 
};
 

class Split_Range_Block_Info : public  Range_Block_Info { 

  private : 
    std::shared_ptr<std::vector<std::shared_ptr<Range_Block_Info>>> range_blocks_;

  public :

    Split_Range_Block_Info(  std::shared_ptr<const std::vector<bool>> orig_aops, SRBI_Helper& helper ) : 
                             Range_Block_Info( helper.orig_rngs_, helper.unique_block_, helper.idxs_trans_, helper.factors_, *orig_aops ),
                             range_blocks_(helper.rxnge_blocks_) {} 
                     
   ~Split_Range_Block_Info(){};

    std::shared_ptr<std::vector<std::shared_ptr<Range_Block_Info>>> range_blocks(){ return range_blocks_ ;} 
    std::shared_ptr<Range_Block_Info> range_blocks(int ii){ return range_blocks_->at(ii) ;} 

    bool is_sparse( const std::shared_ptr<std::vector<std::vector<int>>> state_idxs );
};

#endif
