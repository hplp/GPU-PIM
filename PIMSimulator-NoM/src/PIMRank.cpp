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

#include "PIMRank.h"

#include <bitset>
#include <iostream>

#include "AddressMapping.h"
#include "PIMCmd.h"

using namespace std;
using namespace DRAMSim;

PIMRank::PIMRank(ostream& simLog, Configuration& configuration)
    : chanId(-1),
      rankId(-1),
      dramsimLog(simLog),
      pimPC_(0),
      lastRepeatIdx_(-1),
      numRepeatToBeDone_(-1),
      crfExit_(false),
      config(configuration),
      pimBlocks(getConfigParam(UINT, "NUM_PIM_BLOCKS"),
                PIMBlock(PIMConfiguration::getPIMPrecision()))
{
    for (int i = 0; i < 4; i++)
    {
        lastJumpIdx_[i] = numJumpToBeTaken_[i] = -1;
    }
    currentClockCycle = 0;
}

void PIMRank::attachRank(Rank* r)
{
    this->rank = r;
}

void PIMRank::setChanId(int id)
{
    this->chanId = id;
}

void PIMRank::setRankId(int id)
{
    this->rankId = id;
}

int PIMRank::getChanId() const
{
    return this->chanId;
}

int PIMRank::getRankId() const
{
    return this->rankId;
}

void PIMRank::update() {}

void PIMRank::controlPIM(BusPacket* packet)
{
    uint8_t grf_a_zeroize = packet->data->u8Data_[20];
    if (grf_a_zeroize)
    {
        if (DEBUG_CMD_TRACE)
        {
            PRINTC(RED, OUTLOG_CH_RA("GRF_A_ZEROIZE"));
        }
        BurstType burst_zero;
        for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
        {
            for (int i = 0; i < 16; i++) pimBlocks[pb].grfA[i] = burst_zero;
        }
    }
    uint8_t grf_b_zeroize = packet->data->u8Data_[21];
    if (grf_b_zeroize)
    {
        if (DEBUG_CMD_TRACE)
        {
            PRINTC(RED, OUTLOG_CH_RA("GRF_B_ZEROIZE"));
        }
        BurstType burst_zero;
        for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
        {
            for (int i = 0; i < 16; i++) pimBlocks[pb].grfB[i] = burst_zero;
        }
    }
    uint8_t grf_c_zeroize = packet->data->u8Data_[22];
    if (grf_c_zeroize)
    {
        if (DEBUG_CMD_TRACE)
        {
            PRINTC(RED, OUTLOG_CH_RA("GRF_C_ZEROIZE"));
        }
        BurstType burst_zero;
        for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
        {
            for (int i = 0; i < 16; i++) pimBlocks[pb].grfC[i] = burst_zero;
        }
    }
    if (packet->data->u8Data_[0] & 1)
    {
        pimOpMode_ = packet->data->u8Data_[0] & 2;
        pimOrNom_ = packet->data->u8Data_[0] & 4;
        toggleEvenBank_ = !(packet->data->u8Data_[16] & 1);
        toggleOddBank_ = !(packet->data->u8Data_[16] & 2);
        toggleRa13h_ = (packet->data->u8Data_[16] & 4);

        if (pimOpMode_)
        {
            if(pimOrNom_)
            {
                rank->mode_ = dramMode::HAB_NOM;
                PRINTC(RED, OUTLOG_CH_RA("HAB_NOM"));
            }
            else
            {
                rank->mode_ = dramMode::HAB_PIM;
                PRINTC(RED, OUTLOG_CH_RA("HAB_PIM"));
            }
            pimPC_ = 0;
            for (int i = 0; i < 4; i++)
            {
                lastJumpIdx_[i] = numJumpToBeTaken_[i] = -1;
            }
            lastRepeatIdx_ = numRepeatToBeDone_ = -1;
            crfExit_ = false;
        }
        else
        {
            rank->mode_ = dramMode::HAB;
            PRINTC(RED, OUTLOG_CH_RA("HAB mode"));
        }
    }
}

bool PIMRank::isToggleCond(BusPacket* packet)
{
    if (pimOpMode_ && !crfExit_)
    {
        if (toggleRa13h_)
        {
            if (toggleEvenBank_ && (config.addrMapping.bankgroupParity(packet->bank) == 0))
                return true;
            else if (toggleOddBank_ && (config.addrMapping.bankgroupParity(packet->bank) == 1))
                return true;
            return false;
        }
        else if (!toggleRa13h_ && !isReservedRA(packet->row))
        {
            if (toggleEvenBank_ && (config.addrMapping.bankgroupParity(packet->bank) == 0))
                return true;
            else if (toggleOddBank_ && (config.addrMapping.bankgroupParity(packet->bank) == 1))
                return true;
            return false;
        }
        return false;
    }
    else
    {
        return false;
    }
}

void PIMRank::readHab(BusPacket* packet)
{
    if (isReservedRA(packet->row))  // ignored
    {
        PRINTC(GRAY, OUTLOG_ALL("READ"));
    }
    else
    {
        PRINTC(GRAY, OUTLOG_ALL("BANK_TO_PIM"));
#ifndef NO_STORAGE
        int grf_id = getGrfIdx16(packet->column);
        for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
        {
            rank->banks[config.bankId(pb, config.addrMapping.bankgroupParity(packet->bank))].read(packet);
            pimBlocks[pb].grfB[grf_id] = *(packet->data);
        }
#endif
    }
}

void PIMRank::writeHab(BusPacket* packet)
{
    if (packet->row == config.PIM_REG1_RA)  // WRIO to PIM Broadcasting
    {
        if (packet->column == 0x00)
            controlPIM(packet);
        if (0x10 <= packet->column && packet->column <= 0x1f)
        {
            if (DEBUG_CMD_TRACE)
            {
                if (packet->column - 16 < 16)
                    PRINTC(GREEN, OUTLOG_B_GRF_A("BWRITE_GRF_A"));
            }
#ifndef NO_STORAGE
            for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
            {
                if (packet->column - 16 < 16)
                    pimBlocks[pb].grfA[packet->column - 0x10] = *(packet->data);
            }
#endif
        }
        else if (0x04 <= packet->column && packet->column <= 0x07)
        {
            if (DEBUG_CMD_TRACE)
                PRINTC(GREEN, OUTLOG_B_CRF("BWRITE_CRF"));
            crf.bst[packet->column - 0x04] = *(packet->data);
        }
        else if (packet->column == 0x1)
        {
            if (DEBUG_CMD_TRACE)
                PRINTC(GREEN, OUTLOG_CH_RA("BWRITE_SRF"));
            for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++) pimBlocks[pb].srf = *(packet->data);
        }
    }
    else if (packet->row == config.PIM_REG2_RA)  // WRIO to PIM Broadcasting
    {
        if ((0x0 <= packet->column && packet->column <= 0xf) ||
            (0x10 <= packet->column && packet->column <= 0x1f))
        {
            if (DEBUG_CMD_TRACE)
            {
                if (packet->column < 16)
                    PRINTC(GREEN, OUTLOG_B_GRF_B("BWRITE_GRF_B"));
                else if (packet->column - 16 < 16)
                    PRINTC(GREEN, OUTLOG_B_GRF_C("BWRITE_GRF_C"));
            }
#ifndef NO_STORAGE
            for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
            {
                if (packet->column < 16)
                    pimBlocks[pb].grfB[packet->column] = *(packet->data);
                else if (packet->column - 16 < 16)
                    pimBlocks[pb].grfC[packet->column - 0x10] = *(packet->data);
            }
#endif
        }
    }
    else if (isReservedRA(packet->row))
    {
        PRINTC(GRAY, OUTLOG_ALL("WRITE"));
    }
    else  // PIM (only GRF) to Bank Move
    {
        PRINTC(GREEN, OUTLOG_ALL("PIM_TO_BANK"));

#ifndef NO_STORAGE
        int grf_id = getGrfIdx16(packet->column);
        for (int pb = 0; pb < config.NUM_PIM_BLOCKS; pb++)
        {
            if (config.addrMapping.bankgroupParity(packet->bank) == 0)
            {
                *(packet->data) = pimBlocks[pb].grfA[grf_id];
                rank->banks[config.bankId(pb, false)].write(packet);  // basically read from bank;
            }
            else if (config.addrMapping.bankgroupParity(packet->bank) == 1)
            {
                *(packet->data) = pimBlocks[pb].grfB[grf_id];
                rank->banks[config.bankId(pb, true)].write(packet);  // basically read from bank.
            }
        }
#endif
    }
}

void PIMRank::readOpd(int pb, BurstType& bst, PIMOpdType type, BusPacket* packet, int idx,
                      bool is_auto, bool grf_full, bool is_mac, bool special_indexing)
{
    idx = grf_full ? getGrfIdx16(idx) : getGrfIdx(idx);

    switch (type)
    {
        case PIMOpdType::A_OUT:
            bst = pimBlocks[pb].aOut;
            return;
        case PIMOpdType::M_OUT:
            bst = pimBlocks[pb].mOut;
            return;
        case PIMOpdType::EVEN_BANK:
            if (config.addrMapping.bankgroupParity(packet->bank) != 0)
                PRINT("Warning, CRF bank coding and bank id from packet are inconsistent");
            rank->banks[config.bankId(pb, false)].read(packet);  // basically read from bank.
            bst = *(packet->data);
            return;
        case PIMOpdType::ODD_BANK:
            if (config.addrMapping.bankgroupParity(packet->bank) == 0)
                PRINT("Warning, CRF bank coding and bank id from packet are inconsistent");
            rank->banks[config.bankId(pb, true)].read(packet);  // basically read from bank.
            bst = *(packet->data);
            return;
        case PIMOpdType::GRF_A:
            if (is_auto)
                bst = pimBlocks[pb].grfA[(grf_full) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column)];
            else
                bst = pimBlocks[pb].grfA[idx];
            return;
        case PIMOpdType::GRF_B:
            if (is_auto)
                if (is_mac)
                    bst = pimBlocks[pb].grfB[(special_indexing) ? getGrfIdx16(packet->column - idx + 1)
                                                                : getGrfIdxHigh(packet->row, packet->column)];
                else
                    bst = pimBlocks[pb].grfB[(grf_full) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column)];
            else
                bst = pimBlocks[pb].grfB[idx];
            return;
        case PIMOpdType::GRF_C:
            if (is_auto)
                bst = pimBlocks[pb].grfC[(grf_full) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column)];
            else
                bst = pimBlocks[pb].grfC[idx];
            return;
        case PIMOpdType::SRF_M:
            bst.set(pimBlocks[pb].srf.fp16Data_[idx]);
            return;
        case PIMOpdType::SRF_A:
            bst.set(pimBlocks[pb].srf.fp16Data_[idx + 8]);
            return;
    }
}

void PIMRank::readOpdTrans(int pb, BurstType& bst, PIMOpdType type, BusPacket* packet, int idx)
{
    idx = getGrfIdx16(idx);

    switch (type)
    {
        case PIMOpdType::GRF_A:
            for (int i = 0; i < 16; i++)
            {
                bst.fp16Data_[i] = pimBlocks[pb].grfA[i].fp16Data_[idx];
            }
            return;
        case PIMOpdType::GRF_B:
            for (int i = 0; i < 16; i++)
            {
                bst.fp16Data_[i] = pimBlocks[pb].grfB[i].fp16Data_[idx];
            }
            return;
        case PIMOpdType::GRF_C:
            for (int i = 0; i < 16; i++)
            {
                bst.fp16Data_[i] = pimBlocks[pb].grfC[i].fp16Data_[idx];
            }
            return;
        case PIMOpdType::A_OUT:
        case PIMOpdType::M_OUT:
        case PIMOpdType::EVEN_BANK:
        case PIMOpdType::ODD_BANK:
        case PIMOpdType::SRF_M:
        case PIMOpdType::SRF_A:
            ERROR("== Unsupported Operand Type for REDUCE");
            exit(-1);
            return;
    }
}

void PIMRank::writeOpd(int pb, BurstType& bst, PIMOpdType type, BusPacket* packet, int idx,
                       bool is_auto, bool grf_full, bool is_mac, bool special_indexing)
{
    idx = grf_full ? getGrfIdx16(idx) : getGrfIdx(idx);

    switch (type)
    {
        case PIMOpdType::A_OUT:
            pimBlocks[pb].aOut = bst;
            return;
        case PIMOpdType::M_OUT:
            pimBlocks[pb].mOut = bst;
            return;
        case PIMOpdType::EVEN_BANK:
            if (config.addrMapping.bankgroupParity(packet->bank) != 0)
            {
                PRINT("CRF bank coding and bank id from packet are inconsistent");
            }
            *(packet->data) = bst;
            rank->banks[config.bankId(pb, false)].write(packet);  // basically read from bank.
            return;
        case PIMOpdType::ODD_BANK:
            if (config.addrMapping.bankgroupParity(packet->bank) == 0)
            {
                PRINT("CRF bank coding and bank id from packet are inconsistent");
                exit(-1);
            }
            *(packet->data) = bst;
            rank->banks[config.bankId(pb, true)].write(packet);  // basically read from bank.
            return;
        case PIMOpdType::GRF_A:
            if (is_auto)
                pimBlocks[pb].grfA[(grf_full) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column)] = bst;
            else
                pimBlocks[pb].grfA[idx] = bst;
            return;
        case PIMOpdType::GRF_B:
            if (is_auto)
                if (is_mac)
                    pimBlocks[pb].grfB[(special_indexing) ? getGrfIdx16(packet->column - idx + 1)
                                                          : getGrfIdxHigh(packet->row, packet->column)] = bst;
                else
                    pimBlocks[pb].grfB[(grf_full) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column)] = bst;
            else
                pimBlocks[pb].grfB[idx] = bst;
            return;
        case PIMOpdType::GRF_C:
            if (is_auto)
                pimBlocks[pb].grfC[(grf_full) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column)] = bst;
            else
                pimBlocks[pb].grfC[idx] = bst;
            return;
        case PIMOpdType::SRF_M:
            pimBlocks[pb].srf = bst;
            return;
        case PIMOpdType::SRF_A:
            pimBlocks[pb].srf = bst;
            return;
    }
}

void PIMRank::writeOpdTrans(int pb, BurstType& bst, PIMOpdType type, BusPacket* packet, int idx)
{
    idx = getGrfIdx16(idx);

    switch (type)
    {
        case PIMOpdType::GRF_A:
            for (int i = 0; i < 16; i++)
            {
                pimBlocks[pb].grfA[i].fp16Data_[idx] = bst.fp16Data_[i];
            }
            return;
        case PIMOpdType::GRF_B:
            for (int i = 0; i < 16; i++)
            {
                pimBlocks[pb].grfB[i].fp16Data_[idx] = bst.fp16Data_[i];
            }
            return;
        case PIMOpdType::GRF_C:
            for (int i = 0; i < 16; i++)
            {
                pimBlocks[pb].grfC[i].fp16Data_[idx] = bst.fp16Data_[i];
            }
            return;
        
        case PIMOpdType::SRF_M:
            pimBlocks[pb].srf = bst;
            return;
        case PIMOpdType::SRF_A:
            pimBlocks[pb].srf = bst;
            return;

        case PIMOpdType::A_OUT:
        case PIMOpdType::M_OUT:
        case PIMOpdType::EVEN_BANK:
        case PIMOpdType::ODD_BANK:
            ERROR("== Unsupported Operand Type for REDUCE");
            exit(-1);
            return;
    }
}

void PIMRank::doPIM(BusPacket* packet)
{
    PIMCmd cCmd;
    packet->row = masked2accessibleRA(packet->row);
    do
    {
        cCmd.fromInt(crf.data[pimPC_]);
        if (DEBUG_CMD_TRACE)
        {
            PRINTC(CYAN, string((packet->busPacketType == READ) ? "READ ch" : "WRITE ch")
                             << getChanId() << " ra" << getRankId() << " bg"
                             << config.addrMapping.bankgroupId(packet->bank) << " b" << packet->bank
                             << " r" << packet->row << " c" << packet->column << "|| [" << pimPC_
                             << "] " << cCmd.toStr() << " @ " << currentClockCycle);
        }

        if (cCmd.type_ == PIMCmdType::EXIT)
        {
            crfExit_ = true;
            break;
        }
        else if (cCmd.type_ == PIMCmdType::JUMP)
        {
            if (lastJumpIdx_[cCmd.loopCounterIdx_] != pimPC_)
            {
                if (cCmd.loopCount_ > 0)
                {
                    lastJumpIdx_[cCmd.loopCounterIdx_] = pimPC_;
                    numJumpToBeTaken_[cCmd.loopCounterIdx_] = cCmd.loopCount_;
                }
            }
            if (numJumpToBeTaken_[cCmd.loopCounterIdx_] > 0)
            {
                pimPC_ -= cCmd.loopOffset_;
                numJumpToBeTaken_[cCmd.loopCounterIdx_]--;
            }
            else {
                numJumpToBeTaken_[cCmd.loopCounterIdx_] = cCmd.loopCount_;
            }
        }
        else
        {
            if (cCmd.type_ == PIMCmdType::FILL || cCmd.isAuto_ || cCmd.type_ == PIMCmdType::NOP)
            {
                if (lastRepeatIdx_ != pimPC_)
                {
                    lastRepeatIdx_ = pimPC_;
                    numRepeatToBeDone_ = cCmd.loopCount_;
                }

                if (numRepeatToBeDone_ > 0)
                {
                    pimPC_ -= 1;
                    numRepeatToBeDone_--;
                }
                else
                    lastRepeatIdx_ = -1;
            }

            if (cCmd.type_ == PIMCmdType::NOM1 || cCmd.type_ == PIMCmdType::NOM2) {
                doNOM(packet, cCmd);
            }
            else
            {
                for (int pimblock_id = 0; pimblock_id < config.NUM_PIM_BLOCKS; pimblock_id++)
                {
                    doPIMBlock(packet, cCmd, pimblock_id);

                    if (DEBUG_PIM_BLOCK && pimblock_id == 0)
                    {
                        PRINT("[BANK_R]" << packet->data->fp16ToStr());
                        PRINT("[CMD]" << bitset<32>(cCmd.toInt()) << "(" << cCmd.toStr() << ")");
                        PRINT(pimBlocks[pimblock_id].print());
                        PRINT("----------");
                    }
                }
            }
        }
        pimPC_++;
        // EXIT check
        PIMCmd next_cmd;
        next_cmd.fromInt(crf.data[pimPC_]);
        if (next_cmd.type_ == PIMCmdType::EXIT)
            crfExit_ = true;
    } while (cCmd.type_ == PIMCmdType::JUMP);
}

void PIMRank::doPIMBlock(BusPacket* packet, PIMCmd cCmd, int pimblock_id)
{
    if (cCmd.type_ == PIMCmdType::FILL || cCmd.type_ == PIMCmdType::MOV)
    {
        BurstType bst;
        bool is_auto = (cCmd.type_ == PIMCmdType::FILL) ? true : false;

        readOpd(pimblock_id, bst, cCmd.src0_, packet, cCmd.src0Idx_, is_auto, cCmd.grfFull_, false);
        if (cCmd.isRelu_)
        {
            for (int i = 0; i < 16; i++)
                bst.u16Data_[i] = (bst.u16Data_[i] & (1 << 15)) ? 0 : bst.u16Data_[i];
        }
        writeOpd(pimblock_id, bst, cCmd.dst_, packet, cCmd.dstIdx_, is_auto, cCmd.grfFull_, false);
    }
    else if (cCmd.type_ == PIMCmdType::REDUCE)
    {
        BurstType dstBst;
        BurstType src0Bst;

        int jump_count = numJumpToBeTaken_[cCmd.loopCounterIdx_];
        if (jump_count < 0)
            jump_count = 16 - 1;

        readOpdTrans(pimblock_id, dstBst, cCmd.dst_, packet, jump_count);
        for (int i = 0; i < 16; i++)
        {
            readOpdTrans(pimblock_id, src0Bst, cCmd.src0_, packet, i);
            pimBlocks[pimblock_id].add(dstBst, src0Bst, dstBst);
        }
        writeOpdTrans(pimblock_id, dstBst, cCmd.dst_, packet, jump_count);
    }
    else if (cCmd.type_ == PIMCmdType::ADD || cCmd.type_ == PIMCmdType::MUL) // TODO: Add 16 GRF support
    {
        BurstType dstBst;
        BurstType src0Bst;
        BurstType src1Bst;
        readOpd(pimblock_id, src0Bst, cCmd.src0_, packet, cCmd.src0Idx_, cCmd.isAuto_, cCmd.grfFull_, false);
        readOpd(pimblock_id, src1Bst, cCmd.src1_, packet, cCmd.src1Idx_, cCmd.isAuto_, cCmd.grfFull_, false);

        if (cCmd.type_ == PIMCmdType::ADD)
            // dstBst = src0Bst + src1Bst;
            pimBlocks[pimblock_id].add(dstBst, src0Bst, src1Bst);
        else if (cCmd.type_ == PIMCmdType::MUL)
            // dstBst = src0Bst * src1Bst;
            pimBlocks[pimblock_id].mul(dstBst, src0Bst, src1Bst);

        writeOpd(pimblock_id, dstBst, cCmd.dst_, packet, cCmd.dstIdx_, cCmd.isAuto_, cCmd.grfFull_, false);
    }
    else if (cCmd.type_ == PIMCmdType::MAC || cCmd.type_ == PIMCmdType::MAD) // TODO: Add 16 GRF support
    {
        BurstType dstBst;
        BurstType src0Bst;
        BurstType src1Bst;
        bool is_mac = (cCmd.type_ == PIMCmdType::MAC) ? true : false;

        readOpd(pimblock_id, src0Bst, cCmd.src0_, packet, cCmd.src0Idx_, cCmd.isAuto_, false, is_mac);
        readOpd(pimblock_id, src1Bst, cCmd.src1_, packet, cCmd.src1Idx_, cCmd.isAuto_, false, is_mac);
        if (is_mac)
        {
            readOpd(pimblock_id, dstBst, cCmd.dst_, packet, cCmd.dstIdx_, cCmd.isAuto_, false, is_mac);
            // dstBst = src0Bst * src1Bst + dstBst;
            pimBlocks[pimblock_id].mac(dstBst, src0Bst, src1Bst);
        }
        else
        {
            BurstType src2Bst;
            readOpd(pimblock_id, src2Bst, cCmd.src2_, packet, cCmd.src1Idx_, cCmd.isAuto_, false, is_mac);
            // dstBst = src0Bst * src1Bst + src2Bst;
            pimBlocks[pimblock_id].mad(dstBst, src0Bst, src1Bst, src2Bst);
        }

        writeOpd(pimblock_id, dstBst, cCmd.dst_, packet, cCmd.dstIdx_, cCmd.isAuto_, false, is_mac);
    }
    else if (cCmd.type_ == PIMCmdType::MACX)
    {
        BurstType dstBst;
        BurstType src0Bst;
        BurstType src1Bst;
        int jump_count = numJumpToBeTaken_[cCmd.dstIdx_];
        if (jump_count < 0)
            jump_count = cCmd.src1Idx_ * 2 - 1;

        readOpd(pimblock_id, src0Bst, cCmd.src0_, packet, cCmd.src0Idx_ + jump_count, false, true, true, true);
        readOpd(pimblock_id, src1Bst, cCmd.src1_, packet, cCmd.src1Idx_, cCmd.isAuto_, true, true, true);
        readOpd(pimblock_id, dstBst, cCmd.dst_, packet, jump_count % cCmd.src1Idx_, cCmd.isAuto_, true, true, true);
        pimBlocks[pimblock_id].mac(dstBst, src0Bst, src1Bst);

        writeOpd(pimblock_id, dstBst, cCmd.dst_, packet, jump_count % cCmd.src1Idx_, cCmd.isAuto_, true, true, true);
    }
    else if (cCmd.type_ == PIMCmdType::NOP && packet->busPacketType == WRITE)
    {
        int grf_id = (cCmd.grfFull_) ? getGrfIdx16(packet->column) : getGrfIdx(packet->column);
        if (config.addrMapping.bankgroupParity(packet->bank) == 0)
        {
            *(packet->data) = pimBlocks[pimblock_id].grfA[grf_id];
            rank->banks[config.bankId(pimblock_id, false)].write(packet);  // basically read from bank;
        }
        else if (config.addrMapping.bankgroupParity(packet->bank) == 1)
        {
            *(packet->data) = pimBlocks[pimblock_id].grfB[grf_id];
            rank->banks[config.bankId(pimblock_id, true)].write(packet);  // basically read from bank.
        }
    }
}

void PIMRank::doNOM(BusPacket* packet, PIMCmd cCmd)
{
    if (cCmd.type_ == PIMCmdType::NOM1)
    {
        unsigned original_col = packet->column;
        int read_col = (packet->column & 0x10) + cCmd.src0Idx_;
        int write_col = (packet->column & 0x10) + cCmd.dstIdx_;
        int lower_bg_limit = config.NUM_BANKS_PER_BG * (config.NUM_BANK_GROUPS / 2);

        for (int lower_bg_base = 0; lower_bg_base < lower_bg_limit; lower_bg_base += config.NUM_BANKS_PER_BG)
        {
            int upper_bg_base = lower_bg_base + lower_bg_limit;
            int read_bg_base = cCmd.direction_ ? lower_bg_base : upper_bg_base;
            int write_bg_base = cCmd.direction_ ? upper_bg_base : lower_bg_base;

            for (int bank_idx = 0; bank_idx < config.NUM_BANKS_PER_BG; bank_idx++)
            {
                packet->column = read_col;
                rank->banks[read_bg_base + bank_idx].read(packet);

                packet->column = write_col;
                rank->banks[write_bg_base + bank_idx].write(packet);
            }
        }

        packet->column = original_col;
    }
    else if (cCmd.type_ == PIMCmdType::NOM2)
    {
        for (int bg_base = 0; bg_base < (config.NUM_BANKS_PER_BG * config.NUM_BANK_GROUPS); bg_base = bg_base + config.NUM_BANKS_PER_BG)
        {
            const int n = config.NUM_BANKS_PER_BG;

            if (cCmd.direction_) {
                // B0 -> B1 -> B2 -> B3 -> B0

                rank->banks[bg_base + n - 1].read(packet);
                BurstType saved_last = *(packet->data);

                for (int bank_idx = n - 1; bank_idx > 0; bank_idx--) {
                    rank->banks[bg_base + bank_idx - 1].read(packet);
                    BurstType bst = *(packet->data);

                    *(packet->data) = bst;
                    rank->banks[bg_base + bank_idx].write(packet);
                }

                *(packet->data) = saved_last;
                rank->banks[bg_base + 0].write(packet);
            }
            else {
                // B3 -> B2 -> B1 -> B0 -> B3

                rank->banks[bg_base + 0].read(packet);
                BurstType saved_first = *(packet->data);

                for (int bank_idx = 0; bank_idx < n - 1; bank_idx++) {
                    rank->banks[bg_base + bank_idx + 1].read(packet);
                    BurstType bst = *(packet->data);

                    *(packet->data) = bst;
                    rank->banks[bg_base + bank_idx].write(packet);
                }

                *(packet->data) = saved_first;
                rank->banks[bg_base + n - 1].write(packet);
            }
        }
    }
}