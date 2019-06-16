#include <gtest/gtest.h>
#include <hpx/hpx_init.hpp>
#include <hpx/hpx_main.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::info);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}