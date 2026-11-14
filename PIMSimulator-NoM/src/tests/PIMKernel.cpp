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

#include "tests/PIMKernel.h"

#include <iomanip>
#include <string>

#include "AddressMapping.h"
#include "tests/PIMCmdGen.h"

void PIMKernel::runPIM()
{
    while (mem_->hasPendingTransactions())
    {
        cycle_++;
        mem_->update();
    }
}

uint64_t PIMKernel::getCycle()
{
    return cycle_;
}

void PIMKernel::parkIn()
{
    addBarrier();
    for (int& ch_idx : pim_chans_)
    {
        for (int& ra_idx : pim_ranks_)
        {
            for (int bank_idx = 0; bank_idx < num_banks_per_bg_; bank_idx++)
            {
                for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
                {
                    string str = "PARK_IN_";
                    if (bg_idx == 0 && bank_idx == 0)
                        str = "START_" + str;
                    else if (bg_idx == 3 && bank_idx == 3)
                        str = "END_" + str;
                    mem_->addTransaction(
                        false,
                        pim_addr_mgr_->addrGen(ch_idx, ra_idx, bg_idx, bank_idx, (1 << 13), 0), str,
                        &null_bst_);
                }
            }
        }
    }
    addBarrier();
}

void PIMKernel::parkOut()
{
    for (int& ch_idx : pim_chans_)
    {
        for (int& ra_idx : pim_ranks_)
        {
            for (int bank_idx = 0; bank_idx < num_banks_per_bg_; bank_idx++)
            {
                for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
                {
                    string str = "PARK_OUT_";
                    if (bg_idx == 0 && bank_idx == 0)
                        str = "START_" + str;
                    else if (bg_idx == 3 && bank_idx == 3)
                        str = "END_" + str;
                    mem_->addTransaction(
                        false,
                        pim_addr_mgr_->addrGen(ch_idx, ra_idx, bg_idx, bank_idx, (1 << 13), 0), str,
                        &null_bst_);
                }
            }
        }
    }
    addBarrier();
}

void PIMKernel::addTransactionAll(bool is_write, int bg_idx, int bank_idx, int row, int col,
                                  const string tag, BurstType* bst, bool use_barrier, int num_loop, int active_channels)
{
    int channel_num = (active_channels == 0) ? num_pim_chans_ : active_channels;

    for (int ch_idx = 0; ch_idx < channel_num; ch_idx++)
        for (int& ra_idx : pim_ranks_)
        {
            unsigned local_row = row;
            unsigned local_col = col;
            for (int i = 0; i < num_loop; i++)
            {
                uint64_t addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx, bank_idx,
                                                           local_row, local_col);
                (tag != "") ? mem_->addTransaction(is_write, addr, tag, bst)
                            : mem_->addTransaction(is_write, addr, bst);
                local_col++;
            }
        }

    if (use_barrier)
        addBarrier(active_channels);
}

void PIMKernel::addTransactionAll(bool is_write, int bg_idx, int bank_idx, int row, int col,
                                  BurstType* bst, bool use_barrier, int num_loop, int active_channels)
{
    addTransactionAll(is_write, bg_idx, bank_idx, row, col, "", bst, use_barrier, num_loop, active_channels);
}

void PIMKernel::addBarrier(int active_channels)
{
    int channel_num = (active_channels == 0) ? num_pim_chans_ : active_channels;
    for (int ch_idx = 0; ch_idx < channel_num; ch_idx++) mem_->addBarrier(ch_idx);
}

void PIMKernel::changePIMMode(dramMode curMode, dramMode nextMode, int active_channels)
{
    if (curMode == dramMode::SB && nextMode == dramMode::HAB)
    {
        addTransactionAll(true, 0, 0, pim_abmr_ra_, 0x1f, "START_SB_TO_HAB_", &null_bst_, false, 1, active_channels);
        addTransactionAll(true, 0, 1, pim_abmr_ra_, 0x1f, &null_bst_, false, 1, active_channels);
        if (num_banks_ >= 2)
        {
            addTransactionAll(true, 2, 0, pim_abmr_ra_, 0x1f, &null_bst_, false, 1, active_channels);
            addTransactionAll(true, 2, 1, pim_abmr_ra_, 0x1f, "END_SB_TO_HAB_", &null_bst_, false, 1, active_channels);
        }
    }
    else if (curMode == dramMode::HAB)
    {
        if (nextMode == dramMode::SB)
        {
            addTransactionAll(true, 0, 0, pim_sbmr_ra_, 0x1f, "START_HAB_TO_SB", &null_bst_, false, 1, active_channels);
            addTransactionAll(true, 0, 1, pim_sbmr_ra_, 0x1f, "END_HAB_TO_SB", &null_bst_, false, 1, active_channels);
        }
        else if (nextMode == dramMode::HAB_PIM)
        {
            addTransactionAll(true, 0, 0, pim_reg1_ra_, 0x0, "PIM", &bst_hab_pim_, false, 1, active_channels);
        }
        else if (nextMode == dramMode::HAB_NOM)
        {
            addTransactionAll(true, 0, 0, pim_reg1_ra_, 0x0, "NOM", &bst_hab_nom_, false, 1, active_channels);
        }
    }
    else if (curMode == dramMode::HAB_PIM && nextMode == dramMode::HAB)
        addTransactionAll(true, 0, 0, pim_reg1_ra_, 0x0, "PIM", &bst_hab_, false, 1, active_channels);
    else if (curMode == dramMode::HAB_NOM && nextMode == dramMode::HAB)
        addTransactionAll(true, 0, 0, pim_reg1_ra_, 0x0, "NOM", &bst_hab_, false, 1, active_channels);

    addBarrier(active_channels);
}

/*
void PIMKernel::preprocessBn(NumpyBurstType* mean_npbst, NumpyBurstType* var_npbst,
                             NumpyBurstType* gamma_npbst, NumpyBurstType* beta_npbst,
                             NumpyBurstType* input_npbst, fp16** params, float eps)
{
    for (int i = 0; i < input_npbst->bShape[0]; i++)
    {
        params[i][0] = 1 / sqrt((float)var_npbst->getBurst(i / 16).fp16Data_[i % 16] + eps);
        params[i][1] = gamma_npbst->getBurst(i / 16).fp16Data_[i % 16];
        params[i][2] = -mean_npbst->getBurst(i / 16).fp16Data_[i % 16] /
                       sqrt((float)var_npbst->getBurst(i / 16).fp16Data_[i % 16] + eps);
        params[i][3] = beta_npbst->getBurst(i / 16).fp16Data_[i % 16];
    }
}

// FIXME : FIX size of srf_bst_. if ch_model is bigger than memory channel, it is not defined.
void PIMKernel::preprocessSrf(NumpyBurstType* input_npbst, fp16** params, int burst_offset,
                              int num_srf_usage)
{
    int ch_idx = 0;
    int ra_idx = 0;
    int burst_idx = 0;
    int num_stride_reg = 2;
    srf_bst_ = new BurstType[num_pim_chans_ * num_pim_ranks_];

    for (int ch_model = 0; ch_model < input_npbst->bShape[0]; ch_model++)
    {
        srf_bst_[ch_idx * num_pim_ranks_ + ra_idx].fp16Data_[burst_idx] =
            params[ch_model][0]; // scale
        srf_bst_[ch_idx * num_pim_ranks_ + ra_idx].fp16Data_[burst_idx + 1] =
            params[ch_model][1]; // gamma
        srf_bst_[ch_idx * num_pim_ranks_ + ra_idx].fp16Data_[burst_idx + 8] =
            params[ch_model][2]; // shift
        srf_bst_[ch_idx * num_pim_ranks_ + ra_idx].fp16Data_[burst_idx + 9] =
            params[ch_model][3]; // beta

        ra_idx++;
        if (ra_idx >= num_pim_ranks_)
        {
            ra_idx = 0;
            ch_idx++;
        }
        if (ch_idx >= num_pim_chans_)
        {
            ch_idx = 0;
            burst_idx += num_stride_reg;
        }
        if (burst_idx >= 8)
        {
            cout << "error: this is not defined" <<endl;
        }
    }
}

void PIMKernel::programSrf()
{
    for (int ch_idx = 0; ch_idx < num_pim_chans_; ch_idx++)
    {
        for (int ra_idx = 0; ra_idx < num_pim_ranks_; ra_idx++)
        {
            mem_->addTransaction(true,
                                 pim_addr_mgr_->addrGen(ch_idx, ra_idx, 0, 0, pim_reg1_ra_, 0x1),
                                 &srf_bst_[ch_idx * num_pim_ranks_ + ra_idx]);
        }
    }
    addBarrier();
}
*/

void PIMKernel::programCrf(vector<PIMCmd>& cmds, int crf_idx, int active_channels)
{
    PIMCmd nop_cmd(PIMCmdType::NOP, 0);
    for (int i = 0; i < 4; i++)
    {
        if (i * 8 >= cmds.size())
            break;
        crf_bst_[crf_idx][i].set(nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt(),
                        nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt());
        for (int j = 0; j < 8; j++)
        {
            if (i * 8 + j >= cmds.size())
                break;
            crf_bst_[crf_idx][i].u32Data_[j] = cmds[i * 8 + j].toInt();
        }
        addTransactionAll(true, 0, 1, pim_reg1_ra_, 0x4 + i, "PROGRAM_CRF", &(crf_bst_[crf_idx][i]), false, 1, active_channels);
    }
    addBarrier(active_channels);
}

void PIMKernel::setControl(BurstType* bst, bool pim_op, int crf_toggle_cond, bool grfA_zero,
                           bool grfB_zero, bool grfC_zero, bool change_mode, bool is_nom)
{
    bst->u8Data_[0] = change_mode | (pim_op << 1) | (is_nom << 2);
    bst->u8Data_[16] = crf_toggle_cond;
    bst->u8Data_[20] = grfA_zero;
    bst->u8Data_[21] = grfB_zero;
    bst->u8Data_[22] = grfC_zero;
}

unsigned PIMKernel::getResultColGemv(int input_dim, int output_dim)
{
    int num_output_tiles = ceil(((double)output_dim / (num_total_pim_blocks_)) / num_grfB_);
    int num_input_tiles = ceil((double)input_dim / (double)num_grfA_);

    return num_output_tiles * num_input_tiles / 2 * num_grfA_ * num_grfB_;
}

void PIMKernel::changeBank(pimBankType pb_type, int& ch_idx, int& ra_idx, int& bg_idx,
                           int& bank_idx, unsigned& starting_row, unsigned& starting_col,
                           unsigned& row, unsigned& col)
{
    bank_idx += 1;

    if (bank_idx >= (num_banks_per_bg_))
    {
        bank_idx = 0;
        bg_idx += (pb_type == pimBankType::ALL_BANK) ? 1 : (num_banks_ / num_pim_blocks_);
        if (bg_idx >= num_bank_groups_)
        {
            bg_idx = 0;
            if (++ra_idx >= num_pim_ranks_)
            {
                ra_idx = 0;
                if (++ch_idx >= num_pim_chans_)
                {
                    ch_idx = 0;
                    starting_row = row;
                    starting_col = col;
                }
            }
        }
    }
}

void PIMKernel::preloadConv(NumpyBurstType* operand, unsigned& ending_row, unsigned& unused_channels, unsigned starting_row, bool skip_allocation)
{
    int ch_idx = 0, ra_idx = 0, bg_idx = 0, bank_idx = 0;
    unsigned row = starting_row, col = 0, starting_col = 0;
    uint64_t addr;

    int x_count = operand->bShape[2] / 14;
    int pim_per_x =  num_total_pim_blocks_ / x_count;
    int z_count = operand->bShape[0] / 4; //TODO: Check whether ceil is needed
    int allo_per_pim = ceil((double)(z_count * operand->bShape[1]) / pim_per_x);
    int z_per_pim = (z_count <= allo_per_pim) ? z_count : allo_per_pim;
    int y_per_pim = allo_per_pim / z_per_pim;
    int pim_per_channel = num_total_pim_blocks_ / num_pim_chans_; // TODO: Check whether we can use num_pim_blocks_ since number of ranks is always 1
    
    unused_channels = x_count * (pim_per_x - (z_count * operand->bShape[1] / allo_per_pim)) / pim_per_channel;
    ending_row = starting_row + allo_per_pim;

    if (skip_allocation) return;

    for (int x = 0; x < operand->bShape[2]; x += 28)
    {
        int y1 = 0, y2 = 0; // TODO: Need extra code to support more than 4 bank groups
        int y = 0;
        while (y2 < operand->bShape[1])
        {   
            for (int z = 0; z < z_count; z += 1)
            {
                y = (bg_idx < 2) ? y1 : y2;

                for (int i = 0; i < 4; i++)
                {
                    if (((4*z)+i) >= operand->bShape[0]) break;
                    bool is_odd = (i < 2) ? false : true;
                    int col_shift = (bg_idx < 2) ? 0 : 14;

                    for (int j = 0; j < 14; j++)
                    {
                        col = (i % 2) * 16 + j + 1;
                        addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx + is_odd, bank_idx,
                                                        row, col);
                        mem_->addTransaction(true, addr, &operand->getBurst((4*z) + i, y, x + col_shift + j));

                    }
                }
                row++;
                if(((z % z_per_pim) == (z_per_pim - 1)) && (y_per_pim == 1))
                {
                    changeBank(pimBankType::EVEN_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                            starting_row, starting_col, row, col);
                    row = starting_row;
                }
                if (((((z + 1) / z_per_pim) % 4) == 0) && (bg_idx == 2))
                {
                    z -= (z_per_pim * 4);
                } 
                else if ((((z + 1) / z_per_pim) == 8) && (bg_idx == 0))
                {
                    y2++;
                } 
            }
             
            if (bg_idx < 2) y1++;
            else y2++;

            if(((y % y_per_pim) == (y_per_pim - 1)) && (y_per_pim != 1))
            {
                changeBank(pimBankType::EVEN_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                        starting_row, starting_col, row, col);
                row = starting_row;
            }
        }
    }
    return;
}

void PIMKernel::preloadGemv(NumpyBurstType* operand, unsigned starting_row, unsigned starting_col)
{
    int input_tile_size = num_grfA_;
    int output_tile_size = num_grfB_ * num_total_pim_blocks_;

    int ch_idx = 0, ra_idx = 0, bg_idx = 0, bank_idx = 0;
    unsigned row = 0, col = 0;
    uint64_t addr;

    unsigned even_starting_row = starting_row, odd_starting_row = starting_row;
    unsigned even_starting_col = starting_col, odd_starting_col = starting_col;

    for (int y = 0; y < operand->bShape[0]; y += output_tile_size)
    {
        for (int x = 0; x < operand->bShape[1]; x += input_tile_size)
        {
            bool is_odd = ((x / input_tile_size) % 2 == 1) ? true : false;

            for (int tiled_y = 0; tiled_y < output_tile_size; tiled_y += num_grfB_)
            {
                row = (is_odd) ? odd_starting_row : even_starting_row;
                col = (is_odd) ? odd_starting_col : even_starting_col;

                for (int grfb_idx = 0; grfb_idx < num_grfB_; grfb_idx++)
                {
                    for (int grfa_idx = 0; grfa_idx < num_grfA_; grfa_idx++, col++)
                    {
                        addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx + is_odd, bank_idx,
                                                          row, col);
                        int d_idx = (y + tiled_y + grfb_idx) * operand->bShape[1] + x + grfa_idx;
                        mem_->addTransaction(true, addr, &operand->bData[d_idx]);
                    }
                }
                is_odd ? changeBank(pimBankType::ODD_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                    odd_starting_row, odd_starting_col, row, col)
                       : changeBank(pimBankType::EVEN_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                    even_starting_row, even_starting_col, row, col);
            }
        }
    }
}

void PIMKernel::preloadNoReplacement(NumpyBurstType* operand, unsigned starting_row,
                                     unsigned starting_col)
{
    uint64_t init_addr = pim_addr_mgr_->addrGenSafe(0, 0, 0, 0, starting_row, starting_col);

    for (int x = 0; x < operand->getTotalDim(); x++)
    {
        uint64_t addr = init_addr + x * transaction_size_;
        mem_->addTransaction(true, addr, &operand->bData[x]);
    }
}
/*
void PIMKernel::preloadEltwise(NumpyBurstType* operand, pimBankType pb_type,
                              unsigned starting_row, unsigned starting_col)
{
   int ch_idx = 0;
   int ra_idx = 0;
   int bg_idx = 0;
   int bank_idx = 0;
   int bank_offset =  (int)pb_type % 2;
   uint64_t addr_op;
   int dim_operand = operand->getTotalDim();

   for (int x=0; x < dim_operand; x+=num_grf_)
   {
       unsigned col = starting_col;
       unsigned row = starting_row;

       for (int grf_idx = 0; grf_idx < num_grf_; grf_idx++)
       {
           addr_op = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx + bank_offset, bank_idx, row,
                                                col);
           mem_->addTransaction(true, addr_op, &operand->bData[x + grf_idx]);
           col++;
       }
       changeBank(pb_type, ch_idx, ra_idx, bg_idx, bank_idx, starting_row, starting_col, row, col);
   }
}
*/

void PIMKernel::issueNomHaloTransfers(unsigned in_starting_row, unsigned in_ending_row, unsigned num_working_channels)
{
    vector<PIMCmd> pim_cmds = PIMCmdGen::getPIMCmds(KernelType::CONV, 2, in_ending_row - in_starting_row);
    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB, num_working_channels);
    programCrf(pim_cmds, 3, num_working_channels);
    changePIMMode(dramMode::HAB, dramMode::HAB_PIM, num_working_channels);

    for (unsigned row = in_starting_row; row < in_ending_row; row++)
    {
        for (int c_idx = 1; c_idx < 32; c_idx = c_idx + 16)
        {
            addTransactionAll(false, 0, 0, row, c_idx, "NOM_HALO_RIGHT", &null_bst_, false, 1, num_working_channels);
        }
        addBarrier(num_working_channels);
        for (int c_idx = 1; c_idx < 32; c_idx = c_idx + 16)
        {
            addTransactionAll(false, 0, 0, row, c_idx, "NOM_HALO_LEFT", &null_bst_, false, 1, num_working_channels);
        }
        addBarrier(num_working_channels);
    }
        
    changePIMMode(dramMode::HAB_PIM, dramMode::HAB, num_working_channels);
    changePIMMode(dramMode::HAB, dramMode::SB, num_working_channels);
    parkOut();
}

void PIMKernel::issueConvHaloFills(unsigned in_starting_row, unsigned in_ending_row,
                                   int channel_shift, int num_channels_per_row,
                                   unsigned num_working_channels, bool use_nom)
{
    halo_transfers_.clear();
    halo_transfer_data_.clear();
    auto addHaloTransfer = [&](uint64_t read_addr, uint64_t write_addr)
    {
        halo_transfers_.push_back({read_addr, write_addr});
    };

    for (unsigned i = in_starting_row; i < in_ending_row; i++)
    {
        for (int j = 0; j < channel_shift; j++)
        {
            for (int k = 0; k < num_channels_per_row; k++)
            {
                for (int& ra_idx : pim_ranks_)
                {
                    for (int bank_idx = 0; bank_idx < num_banks_per_bg_; bank_idx++)
                    {
                        for (int bg_idx = 0; bg_idx < (num_bank_groups_ / 2); bg_idx++)
                        {
                            for (unsigned m = 0; m < 32; m += 16)
                            {
                                int ch = j + k * channel_shift;
                                unsigned cols[4] = {m + 14, m + 0, m + 1, m + 15};

                                if (!use_nom)
                                {
                                    uint64_t read_addr = pim_addr_mgr_->addrGenSafe(ch, ra_idx, bg_idx, bank_idx, i, cols[0]);
                                    uint64_t write_addr = pim_addr_mgr_->addrGenSafe(ch, ra_idx, bg_idx + 2, bank_idx, i, cols[1]);
                                    addHaloTransfer(read_addr, write_addr);

                                    read_addr = pim_addr_mgr_->addrGenSafe(ch, ra_idx, bg_idx + 2, bank_idx, i, cols[2]);
                                    write_addr = pim_addr_mgr_->addrGenSafe(ch, ra_idx, bg_idx, bank_idx, i, cols[3]);
                                    addHaloTransfer(read_addr, write_addr);
                                }

                                if (k != (num_channels_per_row - 1))
                                {
                                    int next_ch = ch + channel_shift;

                                    uint64_t read_addr = pim_addr_mgr_->addrGenSafe(ch, ra_idx, bg_idx + 2, bank_idx, i, cols[0]);
                                    uint64_t write_addr = pim_addr_mgr_->addrGenSafe(next_ch, ra_idx, bg_idx, bank_idx, i, cols[1]);
                                    addHaloTransfer(read_addr, write_addr);

                                    read_addr = pim_addr_mgr_->addrGenSafe(next_ch, ra_idx, bg_idx, bank_idx, i, cols[2]);
                                    write_addr = pim_addr_mgr_->addrGenSafe(ch, ra_idx, bg_idx + 2, bank_idx, i, cols[3]);
                                    addHaloTransfer(read_addr, write_addr);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (use_nom)
        issueNomHaloTransfers(in_starting_row, in_ending_row, num_working_channels);

    halo_transfer_data_.resize(halo_transfers_.size());

    for (size_t t = 0; t < halo_transfers_.size(); t++)
    {
        mem_->addTransaction(false, halo_transfers_[t].read_addr,
                             use_nom ? "HALO_FILL_READ_CHANNEL_BOUNDARY" : "HALO_FILL_READ",
                             &halo_transfer_data_[t]);
    }

    addBarrier(num_working_channels);
    runPIM();

    for (size_t t = 0; t < halo_transfers_.size(); t++)
    {
        mem_->addTransaction(true, halo_transfers_[t].write_addr,
                             use_nom ? "HALO_FILL_WRITE_CHANNEL_BOUNDARY" : "HALO_FILL_WRITE",
                             &halo_transfer_data_[t]);
    }

    addBarrier(num_working_channels);
}

void PIMKernel::issueNomAlignTransfers(unsigned base_partial_row, int y_per_pim, int iter_count,
                                       unsigned num_working_channels)
{
    vector<PIMCmd> pim_cmds = PIMCmdGen::getPIMCmds(KernelType::CONV, 3);
    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB, num_working_channels);
    programCrf(pim_cmds, 4, num_working_channels);
    changePIMMode(dramMode::HAB, dramMode::HAB_PIM, num_working_channels);

    for (int k = 0; k < iter_count; k++)
    {
        unsigned prev_row = base_partial_row + ((y_per_pim - 1) * iter_count + k) * 3;
        unsigned next_row = base_partial_row + k * 3 + 2;

        for (int c_idx = 1; c_idx < 32; c_idx = c_idx + 16)
        {
            addTransactionAll(false, 0, 0, next_row, c_idx, "NOM_ALIGN_PREV_RIGHT", &null_bst_, true, num_grfB_-2, num_working_channels);
        }
        for (int c_idx = 1; c_idx < 32; c_idx = c_idx + 16)
        {
            addTransactionAll(false, 0, 0, prev_row, c_idx, "NOM_ALIGN_NEXT_LEFT", &null_bst_, true, num_grfB_-2, num_working_channels);
        }
    }

    changePIMMode(dramMode::HAB_PIM, dramMode::HAB, num_working_channels);
    changePIMMode(dramMode::HAB, dramMode::SB, num_working_channels);
    parkOut();
}

void PIMKernel::issueConvAlignTransfers(unsigned in_ending_row, int output_rows_per_pim,
                                        int y_per_pim, int iter_count,
                                        int channel_shift, int num_channels_per_row,
                                        unsigned num_working_channels, bool use_nom)
{
    align_transfers_.clear();
    align_transfer_data_.clear();
    zero_write_addrs_.clear();

    auto addZeroWrite = [&](uint64_t write_addr)
    {
        zero_write_addrs_.push_back(write_addr);
    };

    auto addAlignTransfer = [&](uint64_t read_addr, uint64_t write_addr)
    {
        align_transfers_.push_back({read_addr, write_addr});
    };

    unsigned base_partial_row = in_ending_row + output_rows_per_pim;

    for (int seg_idx = 0; seg_idx < num_channels_per_row; seg_idx++)
    {
        for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
        {
            for (int j = 0; j < channel_shift; j++)
            {
                int ch_idx = seg_idx * channel_shift + j;

                for (int ra_idx = 0; ra_idx < num_pim_ranks_; ra_idx++)
                {
                    for (int bank_idx = 0; bank_idx < num_banks_per_bg_; bank_idx++)
                    {
                        int local_pos = j * num_banks_per_bg_ + bank_idx;
                        int last_pos = channel_shift * num_banks_per_bg_ - 1;

                        bool has_prev_bank = (local_pos > 0);
                        bool has_next_bank = (local_pos < last_pos);

                        int prev_ch = ch_idx;
                        int prev_bank = bank_idx;

                        if (has_prev_bank)
                        {
                            int prev_pos = local_pos - 1;
                            prev_ch = seg_idx * channel_shift + (prev_pos / num_banks_per_bg_);
                            prev_bank = use_nom ? 0 : (prev_pos % num_banks_per_bg_);
                        }

                        int next_ch = ch_idx;
                        int next_bank = bank_idx;

                        if (has_next_bank)
                        {
                            int next_pos = local_pos + 1;
                            next_ch = seg_idx * channel_shift + (next_pos / num_banks_per_bg_);
                            next_bank = use_nom ? (num_banks_per_bg_ - 1) : (next_pos % num_banks_per_bg_);
                        }

                        for (int k = 0; k < iter_count; k++)
                        {
                            for (unsigned n = 1; n < 32; n = n + 16)
                            {
                                for (unsigned m = 0; m < 14; m++)
                                {
                                    unsigned col_val = n + m;

                                    unsigned dst_row_prev = base_partial_row + ((y_per_pim - 1) * iter_count + k) * 3;
                                    uint64_t dst_addr_prev = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx, bank_idx, dst_row_prev, col_val);

                                    if (has_prev_bank)
                                    {
                                        if (!use_nom || prev_ch != ch_idx)
                                        {
                                            uint64_t src_addr_prev = pim_addr_mgr_->addrGenSafe(prev_ch, ra_idx, bg_idx, prev_bank, dst_row_prev, col_val);
                                            addAlignTransfer(src_addr_prev, dst_addr_prev);
                                        }
                                    }
                                    else
                                    {
                                        addZeroWrite(dst_addr_prev);
                                    }

                                    unsigned dst_row_next = base_partial_row + k * 3 + 2;
                                    uint64_t dst_addr_next = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx, bank_idx, dst_row_next, col_val);

                                    if (has_next_bank)
                                    {
                                        if (!use_nom || next_ch != ch_idx)
                                        {
                                            uint64_t src_addr_next = pim_addr_mgr_->addrGenSafe(next_ch, ra_idx, bg_idx, next_bank, dst_row_next, col_val);
                                            addAlignTransfer(src_addr_next, dst_addr_next);
                                        }
                                    }
                                    else
                                    {
                                        addZeroWrite(dst_addr_next);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (use_nom)
        issueNomAlignTransfers(base_partial_row, y_per_pim, iter_count, num_working_channels);

    align_transfer_data_.resize(align_transfers_.size());

    for (size_t t = 0; t < align_transfers_.size(); t++)
    {
        mem_->addTransaction(false, align_transfers_[t].read_addr,
                             use_nom ? "ALIGN_PARTIAL_RESULTS_READ_CHANNEL_BOUNDARY"
                                     : "ALIGN_PARTIAL_RESULTS_READ",
                             &align_transfer_data_[t]);
    }

    addBarrier(num_working_channels);
    runPIM();

    for (size_t t = 0; t < align_transfers_.size(); t++)
    {
        mem_->addTransaction(true, align_transfers_[t].write_addr,
                             use_nom ? "ALIGN_PARTIAL_RESULTS_WRITE_CHANNEL_BOUNDARY"
                                     : "ALIGN_PARTIAL_RESULTS_WRITE",
                             &align_transfer_data_[t]);
    }

    for (uint64_t write_addr : zero_write_addrs_)
    {
        mem_->addTransaction(true, write_addr, "ALIGN_PARTIAL_RESULTS_ZERO", &null_bst_);
    }

    addBarrier(num_working_channels);
}

void PIMKernel::executeConv(NumpyBurstType* w_data, NumpyBurstType* i_data, unsigned& out_ending_row,
                            unsigned in_ending_row, unsigned unused_channels,
                            unsigned in_starting_row, bool use_nom)
{
    bool is_odd = false;
    unsigned current_row = in_starting_row;
    unsigned row, col;
    unsigned num_working_channels = num_pim_chans_ - unused_channels;
    unsigned num_working_pim_blocks = num_working_channels * num_total_pim_blocks_ / num_pim_chans_;

    int z_count = i_data->bShape[0] / 4; //TODO: Check whether ceil is needed
    int iter_count_1 = w_data->bShape[0] / 64; //TODO: Check whether ceil is needed
    int allo_per_pim = (z_count * i_data->bShape[1] * i_data->bShape[2]) / (14 * num_working_pim_blocks); // Doesn't need ceil_div since it is handled earlier
    int z_per_pim = (z_count <= allo_per_pim) ? z_count : allo_per_pim;
    int y_per_pim = allo_per_pim / z_per_pim;
    int output_rows_per_pim = y_per_pim * iter_count_1;

    out_ending_row = in_ending_row + output_rows_per_pim;

    if (allo_per_pim != (in_ending_row - in_starting_row)) {
        std::cerr << "ERROR: allo_per_pim != (in_ending_row - in_starting_row) not handled" << std::endl;
        exit(-1);
    }

    int channel_shift = i_data->bShape[2] / 28;
    if (z_count <= 0 || iter_count_1 <= 0 || allo_per_pim <= 0 || z_per_pim <= 0 ||
        y_per_pim <= 0)
    {
        std::cerr << "ERROR: Invalid CONV tiling: z_count=" << z_count
                  << " iter_count_1=" << iter_count_1
                  << " allo_per_pim=" << allo_per_pim
                  << " z_per_pim=" << z_per_pim
                  << " y_per_pim=" << y_per_pim
                  << " output_rows_per_pim=" << output_rows_per_pim << std::endl;
        exit(-1);
    }
    if (i_data->bShape[2] % 28 != 0 || channel_shift <= 0)
    {
        std::cerr << "ERROR: CONV width must be a positive multiple of 28. width="
                  << i_data->bShape[2] << std::endl;
        exit(-1);
    }
    if ((num_working_channels % channel_shift) != 0)
    {
        std::cerr << "ERROR: CONV channel count must be divisible by channel_shift. channels="
                  << num_working_channels << " channel_shift=" << channel_shift << std::endl;
        exit(-1);
    }
    if ((allo_per_pim % z_per_pim) != 0)
    {
        std::cerr << "ERROR: allo_per_pim must be divisible by z_per_pim. allo_per_pim="
                  << allo_per_pim << " z_per_pim=" << z_per_pim << std::endl;
        exit(-1);
    }

    // Transfer: Halo fills

    int num_channels_per_row = num_working_channels / channel_shift;
    setControl(&bst_hab_nom_, true, getToggleCond(), false, false, false, true, true);
    issueConvHaloFills(in_starting_row, in_ending_row, channel_shift, num_channels_per_row,
                       num_working_channels, use_nom);

    // Kernel: Compute partial results
    vector<PIMCmd> pim_cmds;
    pim_cmds = PIMCmdGen::getPIMCmds(KernelType::CONV, 0, z_per_pim);
    setControl(&bst_hab_pim_, true, getToggleCond(), false, true, true);
    setControl(&bst_rst_grf_, true, getToggleCond(), false, true, false, false);
    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB, num_working_channels);
    programCrf(pim_cmds, 0, num_working_channels);
    crf_bst_[1][1] = crf_bst_[0][1];
    crf_bst_[1][1].u32Data_[1] = PIMCmd(PIMCmdType::FILL, PIMOpdType::GRF_A, PIMOpdType::GRF_C, 14 - 1, 1, 0, 0, 0, 0, 1).toInt();
    int crf_idx = 0;
    for (int i = 0; i < y_per_pim; i++){
        for (int j = 0; j < 3; j++){
            for (int k = 0; k < iter_count_1; k++){
                row = in_ending_row + output_rows_per_pim + 3 * iter_count_1 * i + 3 * k + j;
                for (int m = 0; m < 4; m++) {
                    if (m == 2) { // Partial result use the opposite parity when relu needed for final result
                        is_odd = false;
                        crf_idx = 1;
                    } else if (m == 0) {
                        is_odd = true;
                        crf_idx = 0;
                    }
                    if ((m == 0) || (m == 2))
                        addTransactionAll(true, 0, 1, pim_reg1_ra_, 0x4 + 1, "PROGRAM_CRF", &(crf_bst_[crf_idx][1]), true, 1, num_working_channels);

                    col = (m % 2 == 0) ? 1 : 17;
                    changePIMMode(dramMode::HAB, dramMode::HAB_PIM, num_working_channels);  // PC reset.
                    computeConv(w_data, current_row, i, j, k, m, z_per_pim, num_working_channels);
                    // TODO: Check whether direct GRF_C to bank is possible
                    addTransactionAll(false, 0, 0, 0, 1, "FILL_GRF_*_FROM_GRF_C", &null_bst_, true, num_grfB_-2, num_working_channels);
                    addTransactionAll(true, is_odd, 0, row, col, "GRF_*_TO_BANK", &null_bst_, true, num_grfB_-2, num_working_channels);
                    changePIMMode(dramMode::HAB_PIM, dramMode::HAB, num_working_channels);  // for grfBReset 
                }
            }
        }
        current_row = current_row + z_per_pim;
    }
    changePIMMode(dramMode::HAB, dramMode::SB, num_working_channels);
    parkOut();

    // Transfer: Align partial results
    issueConvAlignTransfers(in_ending_row, output_rows_per_pim, y_per_pim, iter_count_1,
                            channel_shift, num_channels_per_row, num_working_channels, use_nom);

    // Kernel: Accumulate partial results
    pim_cmds = PIMCmdGen::getPIMCmds(KernelType::CONV, 1, output_rows_per_pim);
    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB, num_working_channels);
    programCrf(pim_cmds, 2, num_working_channels);
    changePIMMode(dramMode::HAB, dramMode::HAB_PIM, num_working_channels);
    for (int i = 0; i < 2; i++) {
        is_odd = (i == 0);
        for (int y = 0; y < y_per_pim; y++){
            for (int k = 0; k < iter_count_1; k++){
                int out_row_idx = y * iter_count_1 + k;
                int prev_row_idx = ((y == 0) ? (y_per_pim - 1) : (y - 1)) * iter_count_1 + k;
                int next_row_idx = ((y == (y_per_pim - 1)) ? 0 : (y + 1)) * iter_count_1 + k;

                // TODO: Check whether read/write has difference in pim 
                row = in_ending_row + output_rows_per_pim + out_row_idx * 3 + 1;
                addTransactionAll(false, is_odd, 0, row, 1, "FILL_GRF_C_FROM_BANK", &null_bst_, true, num_grfB_-2, num_working_channels);
                addTransactionAll(false, is_odd, 0, row, 17, "FILL_GRF_*_FROM_BANK", &null_bst_, true, num_grfB_-2, num_working_channels);
                row = in_ending_row + output_rows_per_pim + prev_row_idx * 3;
                addTransactionAll(false, is_odd, 0, row, 1, "ADD", &null_bst_, true, num_grfB_-2, num_working_channels);
                addTransactionAll(false, is_odd, 0, row, 17, "ADD", &null_bst_, true, num_grfB_-2, num_working_channels);
                row = in_ending_row + output_rows_per_pim + next_row_idx * 3 + 2;
                addTransactionAll(false, is_odd, 0, row, 1, "ADD", &null_bst_, true, num_grfB_-2, num_working_channels);
                addTransactionAll(false, is_odd, 0, row, 17, "ADD", &null_bst_, true, num_grfB_-2, num_working_channels);
                row = in_ending_row + out_row_idx;
                addTransactionAll(false, 0, 0, 0, 1, "FILL_GRF_*_FROM_GRF_C", &null_bst_, true, num_grfB_-2, num_working_channels);
                addTransactionAll(true, !is_odd, 0, row, 1, "GRF_*_TO_BANK", &null_bst_, true, num_grfB_-2, num_working_channels);
                addTransactionAll(false, 0, 0, 0, 1, "FILL_GRF_*_FROM_GRF_*", &null_bst_, true, num_grfB_-2, num_working_channels);
                addTransactionAll(true, !is_odd, 0, row, 17, "GRF_*_TO_BANK", &null_bst_, true, num_grfB_-2, num_working_channels);
            }
        }
    }
    changePIMMode(dramMode::HAB_PIM, dramMode::HAB, num_working_channels);
    changePIMMode(dramMode::HAB, dramMode::SB, num_working_channels);
    parkOut();
}

void PIMKernel::executeGemv(NumpyBurstType* w_data, NumpyBurstType* i_data, bool is_tree)
{
    int num_output_tiles = ceil(((double)w_data->bShape[0] / (num_total_pim_blocks_)) / num_grfB_);
    int num_input_tiles = ceil((double)w_data->bShape[1] / (double)num_grfA_);
    int num_batch = i_data->bShape[0];
    int zero_row = 1000;

    if (is_tree)
    {
        for (int ch = 0; ch < num_pim_chans_; ch++)
        {
            for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
            {
                for (int ba = 0; ba < num_banks_per_bg_; ba++)
                {
                    for (int ca = 0; ca < num_grfA_; ca++)
                    {
                        uint64_t addr = pim_addr_mgr_->addrGen(ch, 0, bg_idx, ba, zero_row, ca);
                        mem_->addTransaction(true, addr, &null_bst_);
                    }
                }
            }
        }
    }

    vector<PIMCmd> pim_cmds;
    if (is_tree)
    {
        int num_jump = ceil((double)num_input_tiles / 2) - 1;
        pim_cmds = PIMCmdGen::getPIMCmds(KernelType::GEMVTREE, num_jump, 0, 0);
    }
    else
    {
        int num_jump_of_even_bank = num_grfB_ * ceil((double)num_input_tiles / 2) - 1;
        int num_jump_of_odd_bank = num_grfB_ * floor(num_input_tiles / 2) - 1;
        pim_cmds =
            PIMCmdGen::getPIMCmds(KernelType::GEMV, 0, num_jump_of_odd_bank, num_jump_of_even_bank);
    }
    setControl(&bst_hab_pim_, true, getToggleCond(), false, true);
    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB);
    programCrf(pim_cmds);

    for (int j = 0; j < num_output_tiles; j++)
    {
        for (int b = 0; b < num_batch; b++)
        {
            changePIMMode(dramMode::HAB, dramMode::HAB_PIM);  // PC reset.

            int col = num_output_tiles * num_input_tiles / 2 * num_grfA_ * num_grfB_ +
                      (j + b) * num_grfB_;
            if (is_tree)
            {
                for (int i = 0; i < num_input_tiles; i++, col += num_grfB_)
                {
                    computeGemv(i_data, num_input_tiles, num_output_tiles, i, j, b,
                                (i % 2 == 0) ? pimBankType::EVEN_BANK : pimBankType::ODD_BANK);
                    addTransactionAll(true, 1, 0, 0, col, "GRFB_TO_BANK_", &null_bst_, true,
                                      num_grf_);
                    addTransactionAll(false, 0, 0, zero_row, 0, "RESET_GRF_B", &null_bst_, true,
                                      num_grfB_);
                }
            }
            else
            {
                for (int i = 0; i < num_input_tiles; i += 2)
                    computeGemv(i_data, num_input_tiles, num_output_tiles, i, j, b,
                                pimBankType::EVEN_BANK);
                for (int i = 1; i < num_input_tiles; i += 2)
                    computeGemv(i_data, num_input_tiles, num_output_tiles, i, j, b,
                                pimBankType::ODD_BANK);
                addTransactionAll(true, 1, 0, 0, col, "GRFB_TO_BANK_", &null_bst_, true, num_grf_);
            }
            changePIMMode(dramMode::HAB_PIM, dramMode::HAB);  // for grfBReset
        }
    }
    changePIMMode(dramMode::HAB, dramMode::SB);
    parkOut();
    runPIM();
}

void PIMKernel::computeConv(NumpyBurstType* data, unsigned starting_row, int activation_row, int kernel_row, 
                            int k1, int m1, int z_per_pim, unsigned num_working_channels)
{
    unsigned row = starting_row;
    pimBankType pb_type;        
    for (int i = 0; i < z_per_pim; i++) // TODO: Need per_pim GRF_A control for z_per_pim < z_count
    {
        for (int j = 15; j >= 0; j--)
        {     
            for (int ch_idx = 0; ch_idx < num_working_channels; ch_idx++)
            {
                for (int ra_idx = 0; ra_idx < num_pim_ranks_; ra_idx++)
                {   
                    for (int k = 0; k < 2; k++)
                    {
                        // input weights to GRF_A
                        for (int gidx = 0; gidx < 6; gidx++)
                        {
                            string str = "WRIO_TO_GRF_";
                            uint64_t addr = pim_addr_mgr_->addrGen(ch_idx, ra_idx, 0, 0, pim_reg1_ra_, 0x10 + gidx + k*8);

                            mem_->addTransaction(true, addr, str, &data->getBurst(64*k1 + 16*m1 + j, i*4 + k*2 + gidx/3, kernel_row, gidx%3));
                        }
                    }
                }
            }
            addBarrier(num_working_channels);

            for (int k = 0; k < 2; k++)
            {       
                pb_type = (k == 0) ? pimBankType::EVEN_BANK : pimBankType::ODD_BANK;    
                
                for (int c_idx = 18; c_idx >= 16; c_idx--)
                {
                    addTransactionAll(false, (int)pb_type, 0, row, c_idx, "MACX_", &null_bst_, true, num_grfB_-2, num_working_channels);
                }

                for (int c_idx = 2; c_idx >= 0; c_idx--)
                {
                    addTransactionAll(false, (int)pb_type, 0, row, c_idx, "MACX_", &null_bst_, true, num_grfB_-2, num_working_channels);
                }
            }
            addTransactionAll(false, 0, 0, row, 0, "REDUCE_", &null_bst_, true, 1, num_working_channels);
            addTransactionAll(true, 0, 0, pim_reg1_ra_, 0x0, "RESET_GRF_B_", &bst_rst_grf_, true, 1, num_working_channels);
        }
        row++;
    }
}

void PIMKernel::computeGemv(NumpyBurstType* data, int num_input_tiles, int num_output_tiles,
                            int inputTile, int outputTile, int batchIdx, pimBankType pb_type)
{
    for (int ch_idx = 0; ch_idx < num_pim_chans_; ch_idx++)
    {
        for (int ra_idx = 0; ra_idx < num_pim_ranks_; ra_idx++)
        {
            // input upload to GRF
            for (int gidx = 0; gidx < num_grfA_; gidx++)
            {
                string str = "WRIO_TO_GRF_";
                uint64_t addr =
                    pim_addr_mgr_->addrGen(ch_idx, ra_idx, 0, 1, pim_reg1_ra_, 0x10 + gidx);
                int input_idx =
                    batchIdx * num_grfA_ * num_input_tiles + inputTile * num_grfA_ + gidx;
                mem_->addTransaction(true, addr, str, &data->bData[input_idx]);
            }
            mem_->addBarrier(ch_idx);
        }
    }

    unsigned row = 0;
    unsigned col = (num_grfA_ * num_grfB_) * (inputTile / 2 + outputTile * num_input_tiles / 2);

    for (int c_idx = 0; c_idx < 64; c_idx += 8)
        addTransactionAll(false, (int)pb_type, 0, row, col + c_idx, "MAC_", &null_bst_, true,
                          num_grfA_);
}

void PIMKernel::readResult(BurstType* resultBst, pimBankType pb_type, int output_dim,
                           uint64_t base_addr, unsigned starting_row, unsigned starting_col)
{
    int ch_idx = 0;
    int ra_idx = 0;
    int bg_idx = 0;
    int bank_idx = 0;
    int bank_offset = (int)pb_type % 2;
    uint64_t addr;

    for (int x = 0; x < output_dim; x += num_grf_)
    {
        unsigned row = starting_row;
        unsigned col = starting_col;

        for (int grf_idx = 0; grf_idx < num_grf_; grf_idx++)
        {
            addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx + bank_offset, bank_idx, row,
                                              col);
            mem_->addTransaction(false, base_addr + addr, "output", &resultBst[x + grf_idx]);
            col++;
        }
        changeBank(pb_type, ch_idx, ra_idx, bg_idx, bank_idx, starting_row, starting_col, row, col);
    }
}

void PIMKernel::readResultConv(BurstType* resultBst, NumpyBurstType* result, unsigned starting_row, 
                               unsigned ending_row, unsigned unused_channels) // TODO: Combine with preloadConv, remove ending_row
{
    int ch_idx = 0, ra_idx = 0, bg_idx = 0, bank_idx = 0;
    unsigned row = starting_row, col = 0, starting_col = 0;
    uint64_t addr;

    unsigned num_working_channels = num_pim_chans_ - unused_channels;
    unsigned num_working_pim_blocks = num_working_channels * num_total_pim_blocks_ / num_pim_chans_;

    int z_count = result->bShape[0] / 4; //TODO: Check whether ceil is needed
    int allo_per_pim = (z_count * result->bShape[1] * result->bShape[2]) / (14 * num_working_pim_blocks); // Doesn't need ceil_div since it is handled earlier
    int z_per_pim = (z_count <= allo_per_pim) ? z_count : allo_per_pim;
    int y_per_pim = allo_per_pim / z_per_pim;

    for (int x = 0; x < result->bShape[2]; x += 28)
    {
        int y1 = 0, y2 = 0; // TODO: Need extra code to support more than 4 bank groups
        int y = 0;
        while (y2 < result->bShape[1])
        {   
            for (int z = 0; z < z_count; z += 1)
            {
                y = (bg_idx < 2) ? y1 : y2;

                for (int i = 0; i < 4; i++)
                {
                    if (((4*z)+i) >= result->bShape[0]) break;
                    bool is_odd = (i < 2) ? false : true;
                    int col_shift = (bg_idx < 2) ? 0 : 14;

                    for (int j = 0; j < 14; j++)
                    {
                        col = (i % 2) * 16 + j + 1;
                        addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx + is_odd, bank_idx,
                                                        row, col);

                        int out_idx =
                            ((4*z) + i) * result->bShape[1] * result->bShape[2] +
                            y * result->bShape[2] +
                            (x + col_shift + j);

                        if (out_idx < 0 || out_idx >= result->bData.size())
                        {
                            std::cerr << "readResultConv OOB: "
                                    << "out_idx=" << out_idx
                                    << " size=" << result->bData.size()
                                    << " z=" << z << " i=" << i
                                    << " y=" << y
                                    << " x=" << x
                                    << " col_shift=" << col_shift
                                    << " j=" << j
                                    << std::endl;
                            exit(-1);
                        }
                        mem_->addTransaction(false, addr, "output", &resultBst[out_idx]);

                    }
                }
                row++;
                if(((z % z_per_pim) == (z_per_pim - 1)) && (y_per_pim == 1))
                {
                    changeBank(pimBankType::EVEN_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                            starting_row, starting_col, row, col);
                    row = starting_row;
                }
                if (((((z + 1) / z_per_pim) % 4) == 0) && (bg_idx == 2))
                {
                    z -= (z_per_pim * 4);
                } 
                else if ((((z + 1) / z_per_pim) == 8) && (bg_idx == 0))
                {
                    y2++;
                } 
            }
             
            if (bg_idx < 2) y1++;
            else y2++;

            if(((y % y_per_pim) == (y_per_pim - 1)) && (y_per_pim != 1))
            {
                changeBank(pimBankType::EVEN_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                        starting_row, starting_col, row, col);
                row = starting_row;
            }
        }
    }
}

void PIMKernel::executeEltwise(int dim, pimBankType pb_type, KernelType ktype, int input0_row,
                               int result_row, int input1_row)
{
    int num_tile = dim / (num_banks_ * num_pim_chans_ * num_pim_ranks_ * num_grf_);
    int num_jump_to_be_taken = num_tile - 1;
    vector<PIMCmd> pim_cmds = PIMCmdGen::getPIMCmds(ktype, num_jump_to_be_taken, 0, 0);

    setControl(&bst_hab_pim_, true, getToggleCond(pb_type), false, false);
    setControl(&bst_hab_, false, getToggleCond(pb_type), false, false);

    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB);
    programCrf(pim_cmds);
    changePIMMode(dramMode::HAB, dramMode::HAB_PIM);

    if (ktype == KernelType::ADD || ktype == KernelType::MUL)
        computeAddOrMul(num_tile, input0_row, result_row, input1_row);
    else if (ktype == KernelType::RELU)
        computeRelu(num_tile, input0_row, result_row);
    /*
       else if (ktype == KernelType::BN)
       computeBn(num_tile, input0_row, result_row);
     */

    changePIMMode(dramMode::HAB_PIM, dramMode::HAB);
    changePIMMode(dramMode::HAB, dramMode::SB);
    parkOut();
}

void PIMKernel::computeAddOrMul(int num_tile, int input0_row, int result_row, int input1_row)
{
    for (int i = 0; i < num_tile; i++)
    {
        int c = num_grf_ * i;
        for (int b = 0; b < 2; b++)  // for even/odd banks, respectively
        {
            addTransactionAll(false, b, 0, input0_row, c, "BANK_TO_GRF_", &null_bst_, true,
                              num_grf_);
            addTransactionAll(false, b, 0, input1_row, c, "ADD", &null_bst_, true, num_grf_);
            addTransactionAll(true, b, 0, result_row, c, "GRF_TO_BANK", &null_bst_, true, num_grf_);
        }
    }
}

/*
void PIMKernel::computeBn(int num_tile, int input0_row, int result_row)
{
    for (int ch_idx = 0; ch_idx < num_pim_chans_; ch_idx++)
    {
        for (int ra_idx = 0; ra_idx < num_pim_ranks_; ra_idx++)
        {
            int srf_bst_num = (input0_row != result_row)? (ch_idx * num_pim_ranks_ + ra_idx) : 0;
            mem_->addTransaction(true, pim_addr_mgr_->addrGen(ch_idx, ra_idx, 0, 0, pim_reg1_ra_,
                                       0x1), &srf_bst_[srf_bst_num]);
        }
    }
    addBarrier();

    if (input0_row != result_row)
        input0_row = result_row = 0;
    for (int i = 0; i < num_tile; i++)
    {
        for (int b = 0; b < 2; b++) // for even/ddd banks, respectively
        {
            addTransactionAll(false, b, 0, input0_row, num_grf_ * i, "MAD1", &null_bst_,
                              true, num_grf_);
            addTransactionAll(false, b, 0, input0_row, num_grf_ * i, "MAD2", &null_bst_,
                              true, num_grf_);
            addTransactionAll(true , b, 0, result_row, num_grf_ * i, "GRF_TO_BANK", &null_bst_,
                              true, num_grf_);
        }
    }
}
*/

void PIMKernel::computeRelu(int num_tile, int input0_row, int result_row)
{
    for (int i = 0; i < num_tile; i++)
    {
        int c = num_grf_ * i;
        addTransactionAll(false, 0, 0, input0_row, c, "FILL&ReLU", &null_bst_, true, num_grf_);
        addTransactionAll(true, 0, 0, result_row, c, "GRF_A_TO_EVEN_BANK", &null_bst_, true,
                          num_grf_);
        addTransactionAll(false, 1, 0, input0_row, c, "FILL&ReLU", &null_bst_, true, num_grf_);
        addTransactionAll(true, 1, 0, result_row, c, "GRF_B_TO_ODD_BANK", &null_bst_, true,
                          num_grf_);
    }
}

void PIMKernel::readData(BurstType* bst_data, size_t bst_cnt, unsigned starting_row,
                         unsigned starting_col)
{
    uint64_t init_addr = pim_addr_mgr_->addrGenSafe(0, 0, 0, 0, starting_row, starting_col);

    for (uint64_t addr = init_addr, i = 0; i < bst_cnt; addr += transaction_size_, i++)
    {
        mem_->addTransaction(false, addr, &bst_data[i]);
    }
}

void PIMKernel::adderTree(BurstType* result, int output_dim, int num_tile, int step, fp16* temp)
{
    if (num_tile == 1)
        return;

    int iter = num_tile / 2;
    if (step == 0)
    {
        for (int i = 0; i < iter; i++)
        {
            temp[i] = result[2 * i * output_dim].fp16AdderTree() +
                      result[(2 * i + 1) * output_dim].fp16AdderTree();
        }
    }
    else
    {
        for (int i = 0; i < iter; i++) temp[i] = temp[i * 2] + temp[i * 2 + 1];

        if (num_tile % 2 == 1)
            temp[iter] = temp[num_tile];
    }

    adderTree(result, output_dim, ceil(double(num_tile) / (double)2), step + 1, temp);

    return;
}