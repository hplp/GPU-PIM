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

#include "tests/PIMBenchTestCases.h"

#include <cstdlib>

#include "gtest/gtest.h"

/*
 * PIMBenchTest:
 * micro benchmark for performance comparison between w/ PIM and w/o PIM
 */

using namespace DRAMSim;

static unsigned getEnvUnsigned(const char* name, unsigned default_value)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
        return default_value;
    return static_cast<unsigned>(std::strtoul(value, nullptr, 10));
}

TEST_F(PIMBenchFixture, conv)
{
    unsigned input_height = getEnvUnsigned("PIM_CONV_H", 224);
    unsigned input_width = getEnvUnsigned("PIM_CONV_W", 224);
    unsigned input_depth = getEnvUnsigned("PIM_CONV_D", 64);
    unsigned kernel_size = getEnvUnsigned("PIM_CONV_K", 3);
    unsigned kernel_count = getEnvUnsigned("PIM_CONV_COUT", input_depth);

    setPIMBenchTestCase(KernelType::CONV, input_height, input_width, input_depth,
                        kernel_size, kernel_count);
                    // (KernelType, in_h, in_w, in_d, ker_s, ker_c)
    executeKernel();                                    // execute w/o PIM
    executePIMKernel();                                 // execute w/ PIM
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, gemv)
{
    setPIMBenchTestCase(KernelType::GEMV, 4096, 4096);  // (KernelType, out_vec, in_vec)
    executeKernel();                                    // execute w/o PIM
    executePIMKernel();                                 // execute w/ PIM
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, mul)
{
    setPIMBenchTestCase(KernelType::MUL, 2 * 1024 * 1024, 2 * 1024 * 1024);
    executeKernel();
    executePIMKernel();
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, add)
{
    setPIMBenchTestCase(KernelType::ADD, 1024 * 1024, 1024 * 1024);
    executeKernel();
    executePIMKernel();
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, relu)
{
    setPIMBenchTestCase(KernelType::RELU, 4 * 1024 * 1024, 4 * 1024 * 1024);
    executeKernel();
    executePIMKernel();
    expectPIMBench(2.0);
}