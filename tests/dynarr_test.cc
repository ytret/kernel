/*
 * NOTE: these tests are AI-generated.
 */

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>

extern "C" {
#include "dynarr.h"
#include "heap.h"
}

class DynarrTest : public testing::Test {
  protected:
    void TearDown() override {
        if (arr.buf) {
            heap_free(arr.buf);
            arr.buf = nullptr;
        }
    }

    void init_u32(size_t init_cap = 4, size_t item_align = alignof(uint32_t)) {
        dynarr_init(&arr, sizeof(uint32_t), item_align, init_cap);
    }

    void push_items(std::initializer_list<uint32_t> items) {
        for (uint32_t item : items) {
            size_t idx = 0;
            ASSERT_TRUE(dynarr_push(&arr, &item, &idx));
            EXPECT_EQ(idx, arr.num_items - 1);
        }
    }

    void expect_items(std::initializer_list<uint32_t> expected) {
        ASSERT_EQ(arr.num_items, expected.size());

        size_t idx = 0;
        for (uint32_t want : expected) {
            uint32_t got = 0;
            ASSERT_TRUE(dynarr_get_at(&arr, idx, &got, sizeof(got)));
            EXPECT_EQ(got, want) << "item " << idx;
            idx++;
        }
    }

    dynarr_t arr{};
};

TEST_F(DynarrTest, InitRoundsItemSizeAndAlignsBuffer) {
    dynarr_init(&arr, 3, 8, 2);

    EXPECT_NE(arr.buf, nullptr);
    EXPECT_EQ(arr.buf_size, 16);
    EXPECT_EQ(arr.item_real_size, 3);
    EXPECT_EQ(arr.item_size, 8);
    EXPECT_EQ(arr.item_align, 8);
    EXPECT_EQ(arr.num_items, 0);
    EXPECT_EQ(arr.cap_items, 2);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(arr.buf) % 8, 0U);
}

TEST_F(DynarrTest, PushStoresItemsAndReturnsIndexes) {
    init_u32(2);

    uint32_t item1 = 11;
    uint32_t item2 = 22;
    size_t idx1 = 99;
    size_t idx2 = 99;

    EXPECT_TRUE(dynarr_push(&arr, &item1, &idx1));
    EXPECT_TRUE(dynarr_push(&arr, &item2, &idx2));

    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(arr.num_items, 2);
    EXPECT_EQ(arr.cap_items, 2);
    expect_items({11, 22});
}

TEST_F(DynarrTest, PushGrowsCapacityAndPreservesItems) {
    init_u32(1);

    push_items({1, 2, 3});

    EXPECT_EQ(arr.num_items, 3);
    EXPECT_EQ(arr.cap_items, 9);
    expect_items({1, 2, 3});
}

TEST_F(DynarrTest, InsertAtBeginningShiftsExistingItems) {
    init_u32();
    push_items({10, 20, 30});

    const uint32_t item = 5;
    ASSERT_TRUE(dynarr_insert_at(&arr, 0, &item));

    EXPECT_EQ(arr.num_items, 4);
    expect_items({5, 10, 20, 30});
}

TEST_F(DynarrTest, InsertBeforeLastElementPreservesTail) {
    init_u32();
    push_items({10, 20, 30});

    const uint32_t item = 25;
    ASSERT_TRUE(dynarr_insert_at(&arr, 2, &item));

    EXPECT_EQ(arr.num_items, 4);
    expect_items({10, 20, 25, 30});
}

TEST_F(DynarrTest, TakeAtReturnsRemovedItemAndClosesGap) {
    init_u32();
    push_items({10, 20, 30, 40});

    uint32_t taken = 0;
    ASSERT_TRUE(dynarr_take_at(&arr, 1, &taken, sizeof(taken)));

    EXPECT_EQ(taken, 20U);
    EXPECT_EQ(arr.num_items, 3);
    expect_items({10, 30, 40});
}

TEST_F(DynarrTest, PopReturnsLastItem) {
    init_u32();
    push_items({10, 20, 30});

    uint32_t popped = 0;
    ASSERT_TRUE(dynarr_pop(&arr, &popped, sizeof(popped)));

    EXPECT_EQ(popped, 30U);
    EXPECT_EQ(arr.num_items, 2);
    expect_items({10, 20});
}

TEST_F(DynarrTest, RemoveAtDeletesWithoutOutputBuffer) {
    init_u32();
    push_items({10, 20, 30});

    ASSERT_TRUE(dynarr_remove_at(&arr, 1));

    EXPECT_EQ(arr.num_items, 2);
    expect_items({10, 30});
}

TEST_F(DynarrTest, ReadAndRemovalFailuresDoNotMutateArray) {
    init_u32();
    push_items({10, 20});

    uint16_t too_small = 0;
    uint32_t value = 0;

    EXPECT_FALSE(dynarr_get_at(&arr, 2, &value, sizeof(value)));
    EXPECT_FALSE(dynarr_get_at(&arr, 0, nullptr, sizeof(value)));
    EXPECT_FALSE(dynarr_get_at(&arr, 0, &too_small, sizeof(too_small)));
    EXPECT_FALSE(dynarr_take_at(&arr, 2, &value, sizeof(value)));
    EXPECT_FALSE(dynarr_take_at(&arr, 0, &too_small, sizeof(too_small)));
    EXPECT_FALSE(dynarr_pop(&arr, &too_small, sizeof(too_small)));

    EXPECT_EQ(arr.num_items, 2);
    expect_items({10, 20});
}

TEST_F(DynarrTest, PushAndInsertRejectNullItems) {
    init_u32();

    EXPECT_FALSE(dynarr_push(&arr, nullptr, nullptr));
    EXPECT_FALSE(dynarr_insert_at(&arr, 0, nullptr));
    EXPECT_EQ(arr.num_items, 0);
}
