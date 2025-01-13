#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "folder_manager.h"



TEST(FoderManager, LoadFolder01) {
  EXPECT_EQ(1, 1);
  bool exist = folder_manager::get()->load_folder("/");
  EXPECT_EQ(exist, false);
}