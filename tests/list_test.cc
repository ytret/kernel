#include <gtest/gtest.h>

extern "C" {
#include "list.h"
}

class ListTest : public testing::Test {
  protected:
    void SetUp() override {
        list_init(&list, NULL);
    }
    void TearDown() override {
    }

    list_t list;
};

TEST_F(ListTest, TestEmpty) {
    EXPECT_TRUE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 0);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}

TEST_F(ListTest, TestAppendOne) {
    list_node_t node1;
    list_append(&list, &node1);

    EXPECT_FALSE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 1);
    EXPECT_EQ(list_pop_first(&list), &node1);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}

TEST_F(ListTest, TestAppendTwo) {
    list_node_t node1;
    list_node_t node2;
    list_append(&list, &node1);
    list_append(&list, &node2);

    EXPECT_FALSE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 2);
    EXPECT_EQ(list_pop_first(&list), &node1);
    EXPECT_EQ(list_pop_first(&list), &node2);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}

TEST_F(ListTest, TestClear) {
    list_node_t node1;
    list_node_t node2;
    list_append(&list, &node1);
    list_append(&list, &node2);

    list_clear(&list);

    EXPECT_TRUE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 0);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}

TEST_F(ListTest, TestRemoveFromStart) {
    list_node_t node1;
    list_node_t node2;
    list_node_t node3;
    list_append(&list, &node1);
    list_append(&list, &node2);
    list_append(&list, &node3);

    list_remove(&list, &node1);

    EXPECT_EQ(list_count(&list), 2);
    EXPECT_EQ(list_pop_first(&list), &node2);
    EXPECT_EQ(list_pop_first(&list), &node3);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}

TEST_F(ListTest, TestRemoveFromMiddle) {
    list_node_t node1;
    list_node_t node2;
    list_node_t node3;
    list_append(&list, &node1);
    list_append(&list, &node2);
    list_append(&list, &node3);

    list_remove(&list, &node2);

    EXPECT_EQ(list_count(&list), 2);
    EXPECT_EQ(list_pop_first(&list), &node1);
    EXPECT_EQ(list_pop_first(&list), &node3);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}

TEST_F(ListTest, TestRemoveFromEnd) {
    list_node_t node1;
    list_node_t node2;
    list_node_t node3;
    list_append(&list, &node1);
    list_append(&list, &node2);
    list_append(&list, &node3);

    list_remove(&list, &node3);

    EXPECT_EQ(list_count(&list), 2);
    EXPECT_EQ(list_pop_first(&list), &node1);
    EXPECT_EQ(list_pop_first(&list), &node2);
    EXPECT_EQ(list_pop_first(&list), nullptr);
}
