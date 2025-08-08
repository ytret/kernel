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

    void add_nodes(const std::vector<list_node_t *> &nodes) {
        for (auto node : nodes) {
            list_append(&list, node);
        }
    }

    void exp_nodes(std::vector<list_node_t *> nodes) {
        size_t idx;

        idx = 0;
        for (list_node_t *node = list.p_first_node; node != NULL;
             node = node->p_next) {
            ASSERT_LT(idx, nodes.size()) << "too many nodes in the list";
            EXPECT_EQ(node, nodes.at(idx))
                << "node " << idx << " during forward traversal";
            idx++;
        }

        idx = nodes.size();
        for (list_node_t *node = list.p_last_node; node != NULL;
             node = node->p_prev) {
            ASSERT_GT(idx, 0) << "too many nodes in the list";
            idx--;
            EXPECT_EQ(node, nodes.at(idx))
                << "node " << idx << " during backward traversal";
        }
    }

    list_t list;
    list_node_t node1;
    list_node_t node2;
    list_node_t node3;
};

TEST_F(ListTest, TestEmpty) {
    EXPECT_TRUE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 0);
    EXPECT_EQ(list_pop_first(&list), nullptr);
    exp_nodes({});
}

TEST_F(ListTest, TestAppendOne) {
    add_nodes({&node1});
    EXPECT_FALSE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 1);
    exp_nodes({&node1});
}
TEST_F(ListTest, TestAppendTwo) {
    add_nodes({&node1, &node2});
    EXPECT_FALSE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 2);
    exp_nodes({&node1, &node2});
}

TEST_F(ListTest, TestClear) {
    add_nodes({&node1, &node2});
    list_clear(&list);
    EXPECT_TRUE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 0);
    exp_nodes({});
}

TEST_F(ListTest, TestRemoveFromStart) {
    add_nodes({&node1, &node2, &node3});
    list_remove(&list, &node1);
    EXPECT_EQ(list_count(&list), 2);
    exp_nodes({&node2, &node3});
}
TEST_F(ListTest, TestRemoveFromMiddle) {
    add_nodes({&node1, &node2, &node3});
    list_remove(&list, &node2);
    EXPECT_EQ(list_count(&list), 2);
    exp_nodes({&node1, &node3});
}
TEST_F(ListTest, TestRemoveFromEnd) {
    add_nodes({&node1, &node2, &node3});
    list_remove(&list, &node3);
    EXPECT_EQ(list_count(&list), 2);
    exp_nodes({&node1, &node2});
}

TEST_F(ListTest, TestPopFirstEmpty) {
    list_pop_first(&list);
    EXPECT_TRUE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 0);
}
TEST_F(ListTest, TestPopFirstOneTime) {
    add_nodes({&node1, &node2, &node3});
    list_pop_first(&list);
    EXPECT_EQ(list_count(&list), 2);
    exp_nodes({&node2, &node3});
}
TEST_F(ListTest, TestPopFirstTwoTimes) {
    add_nodes({&node1, &node2, &node3});
    list_pop_first(&list);
    list_pop_first(&list);
    EXPECT_EQ(list_count(&list), 1);
    exp_nodes({&node3});
}
TEST_F(ListTest, TestPopFirstThreeTimes) {
    add_nodes({&node1, &node2, &node3});
    list_pop_first(&list);
    list_pop_first(&list);
    list_pop_first(&list);
    EXPECT_TRUE(list_is_empty(&list));
    EXPECT_EQ(list_count(&list), 0);
    exp_nodes({});
}
