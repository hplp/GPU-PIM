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

#include "PIMCmd.h"

namespace DRAMSim
{
bool operator==(const PIMCmd& lhs, const PIMCmd& rhs)
{
    return lhs.toInt() == rhs.toInt();
}

bool operator!=(const PIMCmd& lhs, const PIMCmd& rhs)
{
    return lhs.toInt() != rhs.toInt();
}

void PIMCmd::fromInt(uint32_t val)
{
    type_ = PIMCmdType(fromBit(val, 4, 28));
    switch (type_)
    {
        case PIMCmdType::EXIT:
            break;

        case PIMCmdType::NOP:
            loopCount_ = fromBit(val, 15, 13);
            grfFull_ = fromBit(val, 1, 12);
            break;

        case PIMCmdType::JUMP:
            loopCount_ = fromBit(val, 15, 13); // TODO: Not consistent with paper
            loopCounterIdx_ = fromBit(val, 2, 8);
            loopOffset_ = fromBit(val, 5, 0); // TODO: Check correct? (only need to jump 32)
            break;

        case PIMCmdType::REDUCE: // TODO: Fit this to some category
            dst_ = PIMOpdType(fromBit(val, 3, 25));
            src0_ = PIMOpdType(fromBit(val, 3, 22));
            loopCounterIdx_ = fromBit(val, 2, 8);
            break;

        case PIMCmdType::FILL:
        case PIMCmdType::MOV:
            dst_ = PIMOpdType(fromBit(val, 3, 25));
            src0_ = PIMOpdType(fromBit(val, 3, 22));
            isRelu_ = fromBit(val, 1, 17);
            loopCount_ = fromBit(val, 4, 13);
            grfFull_ = fromBit(val, 1, 12);
            dstIdx_ = fromBit(val, 4, 8);
            src0Idx_ = fromBit(val, 4, 4);
            src1Idx_ = fromBit(val, 4, 0);
            break;

        case PIMCmdType::NOM1:
        case PIMCmdType::NOM2:
            direction_ = fromBit(val, 1, 17);
            isAuto_ = fromBit(val, 1, 15);
            dstIdx_ = fromBit(val, 4, 8);
            src0Idx_ = fromBit(val, 4, 4);
            break;

        case PIMCmdType::MAD:
            src2_ = PIMOpdType(fromBit(val, 3, 16));
        case PIMCmdType::ADD:
        case PIMCmdType::MUL:
        case PIMCmdType::MAC:
        case PIMCmdType::MACX:
            dst_ = PIMOpdType(fromBit(val, 3, 25));
            src0_ = PIMOpdType(fromBit(val, 3, 22));
            src1_ = PIMOpdType(fromBit(val, 3, 19));
            isAuto_ = fromBit(val, 1, 15);
            grfFull_ = fromBit(val, 1, 12);
            dstIdx_ = fromBit(val, 4, 8);
            src0Idx_ = fromBit(val, 4, 4);
            src1Idx_ = fromBit(val, 4, 0);
            break;

        default:
            break;
    }
}

void PIMCmd::validationCheck() const
{
    if (type_ == PIMCmdType::MOV || type_ == PIMCmdType::FILL)
    {
        if (dst_ == PIMOpdType::EVEN_BANK || dst_ == PIMOpdType::ODD_BANK)
        {
            if (src0_ == PIMOpdType::GRF_A || src0_ == PIMOpdType::GRF_B || src0_ == PIMOpdType::GRF_C ||
                src1_ == PIMOpdType::GRF_A || src1_ == PIMOpdType::GRF_B || src1_ == PIMOpdType::GRF_C ||
                src2_ == PIMOpdType::GRF_A || src2_ == PIMOpdType::GRF_B || src2_ == PIMOpdType::GRF_C)
            {
                std::cerr << "ERROR) Invalid in ISA 1.0 " << toStr() << std::endl;
                exit(-1);
            }
        }
        /*
           if (src0_ == PIMOpdType::EVEN_BANK || src0_ == PIMOpdType::ODD_BANK ||
           src1_ == PIMOpdType::EVEN_BANK || src1_ == PIMOpdType::ODD_BANK ||
           src2_ == PIMOpdType::EVEN_BANK || src2_ == PIMOpdType::ODD_BANK){
           if (dst_ == PIMOpdType::GRF_A || dst_ == PIMOpdType::GRF_B){
           std::cerr << "ERROR) Invalid in ISA 1.0 " << toStr() << std::endl;
           exit(-1);
           }
           }
         */
    }
    else if (type_ == PIMCmdType::REDUCE){
        if (dst_ == PIMOpdType::EVEN_BANK || dst_ == PIMOpdType::ODD_BANK || 
            src0_ == PIMOpdType::EVEN_BANK || src0_ == PIMOpdType::ODD_BANK ||
            src0_ == PIMOpdType::SRF_M || src0_ == PIMOpdType::SRF_A)
        {
            std::cerr << "ERROR) Invalid in ISA 1.0 " << toStr() << std::endl;
            exit(-1);
        }

    }
}

uint32_t PIMCmd::toInt() const
{
    validationCheck();
    uint32_t val = toBit(int(type_), 4, 28);
    switch (type_)
    {
        case PIMCmdType::EXIT:
            break;

        case PIMCmdType::NOP:
            val |= toBit(grfFull_, 1, 12);
            val |= toBit(loopCount_, 15, 13);
            break;

        case PIMCmdType::JUMP:
            val |= toBit(loopCount_, 15, 13);
            val |= toBit(loopCounterIdx_, 2, 8);
            val |= toBit(loopOffset_, 5, 0);
            break;

        case PIMCmdType::REDUCE: // TODO: Fit this to some category
            val |= toBit(int(dst_), 3, 25);
            val |= toBit(int(src0_), 3, 22);
            val |= toBit(loopCounterIdx_, 2, 8);
            break;


        case PIMCmdType::FILL:
        case PIMCmdType::MOV:
            val |= toBit(int(dst_), 3, 25);
            val |= toBit(int(src0_), 3, 22);
            val |= toBit(dstIdx_, 4, 8);
            val |= toBit(src0Idx_, 4, 4);
            val |= toBit(src1Idx_, 4, 0);
            val |= toBit(grfFull_, 1, 12);
            val |= toBit(loopCount_, 4, 13);
            val |= toBit(isRelu_, 1, 17);
            break;

        case PIMCmdType::NOM1:
        case PIMCmdType::NOM2:
            val |= toBit(dstIdx_, 4, 8);
            val |= toBit(src0Idx_, 4, 4);
            val |= toBit(isAuto_, 1, 15);
            val |= toBit(direction_, 1, 17);
            break;

        case PIMCmdType::MAD:
            val |= toBit(int(src2_), 3, 16);
        case PIMCmdType::ADD:
        case PIMCmdType::MUL:
        case PIMCmdType::MAC:
        case PIMCmdType::MACX:
            val |= toBit(int(dst_), 3, 25);
            val |= toBit(int(src0_), 3, 22);
            val |= toBit(int(src1_), 3, 19);
            val |= toBit(isAuto_, 1, 15);
            val |= toBit(grfFull_, 1, 12);
            val |= toBit(dstIdx_, 4, 8);
            val |= toBit(src0Idx_, 4, 4);
            val |= toBit(src1Idx_, 4, 0);
            break;

        default:
            break;
    }

    return val;
}

std::string PIMCmd::toStr() const
{
    stringstream ss;
    ss << cmdToStr(type_) << " ";
    switch (type_)
    {
        case PIMCmdType::EXIT:
            break;

        case PIMCmdType::NOP:
            ss << loopCount_ + 1 << "x";
            if (grfFull_)
                ss << ", 16 GRF";
            break;

        case PIMCmdType::JUMP:
            ss << "Jump[" << loopCounterIdx_ << "]: ";
            ss << loopCount_ << "x ";
            ss << "(PC - " << loopOffset_ << ")";
            break;

        case PIMCmdType::REDUCE: // TODO: Add the correct printing
        case PIMCmdType::FILL:
        case PIMCmdType::MOV:
            ss << opdToStr(dst_, dstIdx_) << ", ";
            ss << opdToStr(src0_, src0Idx_) << ", ";
            ss << loopCount_ + 1 << "x";
            if (isRelu_)
                ss << ", relu";
            if (grfFull_)
                ss << ", 16 GRF";
            break;

        case PIMCmdType::NOM1: // TODO: Add the correct printing
        case PIMCmdType::NOM2:
            ss << "col ";
            if (direction_)
                ss << src0Idx_ << " -> col " << dstIdx_;
            else
                ss << dstIdx_ << " <- col " << src0Idx_;
            break;

        case PIMCmdType::ADD:
        case PIMCmdType::MUL:
        case PIMCmdType::MAC:
        case PIMCmdType::MACX: // TODO: Add the custom indexing details here as well
            ss << opdToStr(dst_, dstIdx_) << ", ";
            ss << opdToStr(src0_, src0Idx_) << ", ";
            ss << opdToStr(src1_, src1Idx_);
            break;

        case PIMCmdType::MAD:
            ss << opdToStr(dst_, dstIdx_) << ", ";
            ss << opdToStr(src0_, src0Idx_) << ", ";
            ss << opdToStr(src1_, src1Idx_) << ", ";
            ss << opdToStr(src2_, src1Idx_);
            break;

        default:
            break;
    }
    if (isAuto_)
    {
        ss << ", auto";
    }
    return ss.str();
}
}  // namespace DRAMSim