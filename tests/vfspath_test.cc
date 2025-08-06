#include "vfs/vfs_node.h"
#include <gtest/gtest.h>

extern "C" {
#include "vfs/vfs_path.h"
}

class VfsPathTest : public testing::Test {
  protected:
    void SetUp() override {
    }

    void TearDown() override {
        vfs_path_free(&path);
    }

    void expect_part_eq(size_t part_idx, const std::string &eq) {
        bool found_idx = false;
        size_t idx = 0;
        for (list_node_t *node = path.parts.p_first_node; node != NULL;
             node = node->p_next, idx++) {
            if (idx == part_idx) {
                found_idx = true;
                vfs_path_part_t *const part =
                    LIST_NODE_TO_STRUCT(node, vfs_path_part_t, list_node);
                EXPECT_EQ(std::string(part->name), eq);
                break;
            }
        }
        ASSERT_TRUE(found_idx) << "no part at index " << part_idx;
    }

    vfs_path_t path;
    vfs_err_t err;
};

TEST_F(VfsPathTest, EmptyString) {
    err = vfs_path_from_str("", &path);
    ASSERT_EQ(err, VFS_ERR_PATH_EMPTY);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_TRUE(list_is_empty(&path.parts));
}

TEST_F(VfsPathTest, SingleSlashRoot) {
    err = vfs_path_from_str("/", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_TRUE(list_is_empty(&path.parts));
}

TEST_F(VfsPathTest, MultipleSlashesRoot) {
    err = vfs_path_from_str("///", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_TRUE(list_is_empty(&path.parts));
}

TEST_F(VfsPathTest, AbsolutePath1Part) {
    err = vfs_path_from_str("/foo", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 1);
    expect_part_eq(0, "foo");
}

TEST_F(VfsPathTest, AbsolutePath2Parts) {
    err = vfs_path_from_str("/foo/bar", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 2);
    expect_part_eq(0, "foo");
    expect_part_eq(1, "bar");
}

TEST_F(VfsPathTest, AbsolutePath3Parts) {
    err = vfs_path_from_str("/a/bb/cdefgh", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 3);
    expect_part_eq(0, "a");
    expect_part_eq(1, "bb");
    expect_part_eq(2, "cdefgh");
}

TEST_F(VfsPathTest, AbsolutePath3PartsMultipleSlashes) {
    err = vfs_path_from_str("/////a/////bb///cdefgh", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 3);
    expect_part_eq(0, "a");
    expect_part_eq(1, "bb");
    expect_part_eq(2, "cdefgh");
}

TEST_F(VfsPathTest, AbsolutePath3PartsTrailingSlash) {
    err = vfs_path_from_str("/0/1/2/", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 3);
    expect_part_eq(0, "0");
    expect_part_eq(1, "1");
    expect_part_eq(2, "2");
}

TEST_F(VfsPathTest, AbsolutePath3PartsTrailingSlashes) {
    err = vfs_path_from_str("/0/1/2////", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_TRUE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 3);
    expect_part_eq(0, "0");
    expect_part_eq(1, "1");
    expect_part_eq(2, "2");
}

TEST_F(VfsPathTest, RelativePath1Part) {
    err = vfs_path_from_str("foo-bar", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 1);
    expect_part_eq(0, "foo-bar");
}
TEST_F(VfsPathTest, RelativePath1PartTrailingSlash) {
    err = vfs_path_from_str("foo-bar/", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 1);
    expect_part_eq(0, "foo-bar");
}
TEST_F(VfsPathTest, RelativePath1PartTrailingSlashes) {
    err = vfs_path_from_str("foo-bar///", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 1);
    expect_part_eq(0, "foo-bar");
}
TEST_F(VfsPathTest, RelativePath2Parts) {
    err = vfs_path_from_str("foo/bar", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 2);
    expect_part_eq(0, "foo");
    expect_part_eq(1, "bar");
}
TEST_F(VfsPathTest, RelativePath3Parts) {
    err = vfs_path_from_str("foo/bar/xyz", &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 3);
    expect_part_eq(0, "foo");
    expect_part_eq(1, "bar");
    expect_part_eq(2, "xyz");
}

TEST_F(VfsPathTest, EqMaxNameLen) {
    const std::string part1_str = "foo";
    std::string part2_str;
    for (size_t idx = 0; idx < VFS_NODE_MAX_NAME_SIZE - 1; idx++) {
        part2_str += "a";
    }
    const std::string path_str = part1_str + "/" + part2_str;

    err = vfs_path_from_str(path_str.c_str(), &path);
    ASSERT_EQ(err, VFS_ERR_NONE);
    EXPECT_FALSE(path.is_absolute);
    EXPECT_EQ(list_count(&path.parts), 2);
    expect_part_eq(0, part1_str);
    expect_part_eq(1, part2_str);
}
TEST_F(VfsPathTest, JustAboveMaxNameLen) {
    const std::string part1_str = "foo";
    std::string part2_str;
    for (size_t idx = 0; idx < VFS_NODE_MAX_NAME_SIZE; idx++) {
        part2_str += "a";
    }
    const std::string path_str = part1_str + "/" + part2_str;

    err = vfs_path_from_str(path_str.c_str(), &path);
    ASSERT_EQ(err, VFS_ERR_PATH_PART_TOO_LONG);
}
