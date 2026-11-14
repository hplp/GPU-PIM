/***************************************************************************************************
 * Copyright (C) 2021 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed,
 * transmitted, transcribed, stored in a retrieval system, or translated into any human
 * or computer language in any form by any means,electronic, mechanical, manual or otherwise,
 * or disclosed to third parties without the express written permission of Samsung Electronics.
 * (Use of the Software is restricted to non-commercial, personal or academic, research purpose
 * only)
 **************************************************************************************************/

#ifndef __PIM_KERNEL_HPP__
#define __PIM_KERNEL_HPP__

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "MultiChannelMemorySystem.h"
#include "PIMCmd.h"
#include "SystemConfiguration.h"
#include "tests/KernelAddrGen.h"

using namespace std;
using namespace DRAMSim;

class PIMKernel
{
  public:
    PIMKernel(shared_ptr<MultiChannelMemorySystem> mem, int num_pim_chan, int num_pim_rank)
        : mem_(mem),
          num_pim_chans_(num_pim_chan),
          num_pim_ranks_(num_pim_rank),
          mode_(PIMConfiguration::getPIMMode()),
          num_banks_(getConfigParam(UINT, "NUM_BANKS")),
          num_pim_blocks_(getConfigParam(UINT, "NUM_PIM_BLOCKS")),
          num_bank_groups_(getConfigParam(UINT, "NUM_BANK_GROUPS")),
          srf_bst_(NULL),
          cycle_(0)
    {
        transaction_size_ = getConfigParam(UINT, "BL") *
                            (getConfigParam(UINT, "JEDEC_DATA_BUS_BITS") / 8);  // in byte

        num_banks_per_bg_ = num_banks_ / num_bank_groups_;

        // FIXME: HARDCODED
        num_grf_ = num_grfA_ = num_grfB_ = num_grfC_ = 16; // TODO: Give proper solution
        num_total_pim_blocks_ = num_pim_blocks_ * num_pim_chans_ * num_pim_ranks_;

        pim_chans_.clear();
        for (int i = 0; i < num_pim_chans_; i++) pim_chans_.push_back(i);

        pim_ranks_.clear();
        for (int i = 0; i < num_pim_ranks_; i++) pim_ranks_.push_back(i);

        pim_addr_mgr_ = make_shared<PIMAddrManager>(num_pim_chan, num_pim_rank);
    }

    int transaction_size_;
    int num_pim_chans_, num_pim_ranks_;
    int num_grfA_, num_grfB_, num_grfC_, num_grf_;
    shared_ptr<PIMAddrManager> pim_addr_mgr_;

    void addBarrier(int active_channels = 0);
    void runPIM();
    uint64_t getCycle();
    void parkIn();
    void parkOut();
    void changePIMMode(dramMode mode1, dramMode mode2, int active_channels = 0);
    void addTransactionAll(bool isWrite, int bg, int bank, int row, int col, const std::string tag,
                           BurstType* bst, bool use_barrier = false, int num_loop = 1, int active_channels = 0);
    void addTransactionAll(bool isWrite, int bg, int bank, int row, int col, BurstType* bst,
                           bool use_barrier = false, int num_loop = 1, int active_channels = 0);
    /*
    void preprocessBn(NumpyBurstType* mean_npbst, NumpyBurstType* var_npbst,
                      NumpyBurstType* gamma_npbst, NumpyBurstType* beta_npbst,
                      NumpyBurstType* input_npbst, fp16** params, float eps);
    void preprocessSrf(NumpyBurstType* input_npbst, fp16** params, int burst_offset,
                       int num_srf_usage);
    */
    /*
    void programSrf();
    */
    void programCrf(vector<PIMCmd>& cmds, int crf_idx = 0, int active_channels = 0);
    void setControl(BurstType* bst, bool op, int crf_toggle_cond, bool grfA_zero, bool grfB_zero, bool grfC_zero = true, bool change_mode = true, bool is_nom = false);
    unsigned getResultColGemv(int input_dim, int output_dim);
    void changeBank(pimBankType bank_types, int& cidx, int& rank, int& bg, int& bank,
                    unsigned& startingRow, unsigned& startingCol, unsigned& row, unsigned& col);
    void preloadConv(NumpyBurstType* operand, unsigned& ending_row, unsigned& unused_channels, unsigned starting_row = 0, bool skip_allocation = false);
    void preloadGemv(NumpyBurstType* operand, unsigned starting_row = 0, unsigned starting_col = 0);
    void preloadNoReplacement(NumpyBurstType* operand, unsigned startingRow, unsigned startingCol);
    /*
    void preloadEltwise(NumpyBurstType* operand, pimBankType bank_types, unsigned startingRow,
                        unsigned startingCol);
    */
    void executeConv(NumpyBurstType* w_data, NumpyBurstType* i_data, unsigned& out_ending_row,
                     unsigned in_ending_row, unsigned unused_channels = 0,
                     unsigned in_starting_row = 0, bool use_nom = false);
    void executeGemv(NumpyBurstType* w_data, NumpyBurstType* i_data, bool is_tree);
    void executeEltwise(int dim, pimBankType bank_types, KernelType ktype, int input0_row,
                        int result_row, int input1_row = 0);
    void computeConv(NumpyBurstType* data, unsigned starting_row, int activation_row, int kernel_row, 
                     int k1, int m1, int z_per_pim, unsigned num_working_channels);
    void computeGemv(NumpyBurstType* data, int num_input_tiles, int num_output_tile, int input_tile,
                     int output_tile, int batch_idx, pimBankType bank_types);
    void computeAddOrMul(int numTile, int input0Row, int resultRow, int input1Row);
    void computeRelu(int numTile, int input0Row, int resultRow);
    // void computeBn(int numTile, int input0Row, int resultRow);

    void readResult(BurstType* resultBst, pimBankType bank_types, int output_dim,
                    uint64_t baseAddr = 0, unsigned startingRow = 0, unsigned startingCol = 0);
    void readResultConv(BurstType* resultBst, NumpyBurstType* result, unsigned starting_row, unsigned ending_row, unsigned unused_channels = 0);
    void readData(BurstType* bst_data, size_t bst_cnt, unsigned s_row = 0, unsigned s_col = 0);
    void adderTree(BurstType* result, int output_dim, int numTile, int step, fp16* temp);

  private:
    struct Transfers
    {
        uint64_t read_addr;
        uint64_t write_addr;
    };

    vector<Transfers> halo_transfers_;
    vector<BurstType> halo_transfer_data_;

    vector<Transfers> align_transfers_;
    vector<BurstType> align_transfer_data_;
    vector<uint64_t> zero_write_addrs_;

    void issueConvHaloFills(unsigned in_starting_row, unsigned in_ending_row,
                            int channel_shift, int num_channels_per_row,
                            unsigned num_working_channels, bool use_nom);
    void issueConvAlignTransfers(unsigned in_ending_row, int output_rows_per_pim,
                                 int y_per_pim, int iter_count,
                                 int channel_shift, int num_channels_per_row,
                                 unsigned num_working_channels, bool use_nom);
    void issueNomHaloTransfers(unsigned in_starting_row, unsigned in_ending_row, unsigned num_working_channels);
    void issueNomAlignTransfers(unsigned base_partial_row, int y_per_pim, int iter_count,
                                unsigned num_working_channels);

    unsigned cycle_;
    unsigned num_banks_, num_pim_blocks_, num_bank_groups_, num_total_pim_blocks_, num_banks_per_bg_;
    BurstType null_bst_, bst_hab_pim_, bst_hab_nom_, bst_hab_, bst_rst_grf_;
    BurstType crf_bst_[8][4];
    BurstType* srf_bst_;
    vector<int> pim_chans_;
    vector<int> pim_ranks_;
    PIMMode mode_;
    shared_ptr<MultiChannelMemorySystem> mem_;
    const uint32_t pim_reg1_ra_ = 0x37ff;
    const uint32_t pim_reg2_ra_ = 0x3fff;
    const uint32_t pim_abmr_ra_ = 0x27ff;
    const uint32_t pim_sbmr_ra_ = 0x2fff;

    int inline getToggleCond(pimBankType pb_type = pimBankType::ALL_BANK)
    {
        // set Toggle Condition
        switch (pb_type)
        {
            case pimBankType::EVEN_BANK:
                return 2;
            case pimBankType::ODD_BANK:
                return 1;
            case pimBankType::ALL_BANK:
                return 0;
            default:
                return -1;
        }
    }
};

#endif