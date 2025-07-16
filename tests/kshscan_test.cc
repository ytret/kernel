#include <gtest/gtest.h>

extern "C" {
#include "kshell/kshscan.h"
}

class KshscanTest : public testing::Test {
  protected:
    void check_ok(const char *str, std::vector<std::string> exp_args) {
        list_t arg_list;
        list_init(&arg_list, NULL);

        kshscan_err_t err = kshscan_str(str, &arg_list);
        ASSERT_EQ(err.err_type, KSHSCAN_ERR_NONE);

        if (exp_args.size() == 0) {
            EXPECT_TRUE(list_is_empty(&arg_list))
                << "expected 0 arguments, got more";
        }

        for (const std::string &exp_arg : exp_args) {
            ASSERT_FALSE(list_is_empty(&arg_list)) << "expected more arguments";

            list_node_t *const node = list_pop_first(&arg_list);
            kshscan_arg_t *const arg =
                LIST_NODE_TO_STRUCT(node, kshscan_arg_t, list_node);

            EXPECT_EQ(std::string(arg->arg_str), exp_arg);

            kshscan_free_arg(arg);
        }
    }

    void check_bad(const char *str, kshscan_err_t exp_err) {
        list_t arg_list;
        list_init(&arg_list, NULL);

        kshscan_err_t err = kshscan_str(str, &arg_list);
        EXPECT_EQ(err.err_type, exp_err.err_type);
        EXPECT_EQ(err.char_pos, exp_err.char_pos);

        list_node_t *arg_node;
        while ((arg_node = list_pop_first(&arg_list))) {
            kshscan_arg_t *const arg =
                LIST_NODE_TO_STRUCT(arg_node, kshscan_arg_t, list_node);
            kshscan_free_arg(arg);
        }
    }
};

TEST_F(KshscanTest, TestEmptyString) {
    check_ok("", {""});
}

// Spaces.
TEST_F(KshscanTest, TestNumericString) {
    check_ok("4294967295", {"4294967295"});
}

TEST_F(KshscanTest, TestSignedNumericString) {
    check_ok("-4294967295", {"-4294967295"});
}

TEST_F(KshscanTest, TestAsciiString) {
    check_ok("LoremIpsum", {"LoremIpsum"});
}

TEST_F(KshscanTest, TestTwoAsciiStringsSingleSpace) {
    check_ok("Lorem Ipsum", {"Lorem", "Ipsum"});
}

TEST_F(KshscanTest, TestTwoAsciiStringsTwoSpaces) {
    check_ok("Lorem  Ipsum", {"Lorem", "", "Ipsum"});
}

// Single quotes.
TEST_F(KshscanTest, TestSingleQuotes) {
    check_ok("''", {""});
}
TEST_F(KshscanTest, TestSingleQuotesWithSpace) {
    check_ok("' '", {" "});
}
TEST_F(KshscanTest, TestSingleQuotedEmptyArg) {
    check_ok("arg1 '' arg3", {"arg1", "", "arg3"});
}
TEST_F(KshscanTest, TestSingleQuotesWithAscii) {
    check_ok("'Lorem Ipsum Dolor'", {"Lorem Ipsum Dolor"});
}
TEST_F(KshscanTest, TestSingleQuotesLeadingSpace) {
    check_ok("' Lorem Ipsum'", {" Lorem Ipsum"});
}
TEST_F(KshscanTest, TestSingleQuotesTrailingSpace) {
    check_ok("'Lorem ipsum '", {"Lorem ipsum "});
}
TEST_F(KshscanTest, TestUnnecessarySingleQuotes) {
    check_ok("'Lorem' 'ipsum'", {"Lorem", "ipsum"});
}
TEST_F(KshscanTest, TestSingleQuotesInString) {
    check_ok("Lorem''ips'u'm''", {"Loremipsum"});
}
TEST_F(KshscanTest, TestDoubleQuoteInSingleQuotes) {
    check_ok("'a\"bc'\"\"", {"a\"bc"});
}
TEST_F(KshscanTest, TestSingleQuotedMiddleArg) {
    check_ok("arg1 'quoted with spaces' arg2",
             {"arg1", "quoted with spaces", "arg2"});
}

// Double quotes.
TEST_F(KshscanTest, TestDoubleQuotes) {
    check_ok("\"\"", {""});
}
TEST_F(KshscanTest, TestDoubleQuotesWithSpace) {
    check_ok("\" \"", {" "});
}
TEST_F(KshscanTest, TestDoubleQuotedEmptyArg) {
    check_ok("arg1 \"\" arg3", {"arg1", "", "arg3"});
}
TEST_F(KshscanTest, TestDoubleQuotesWithAscii) {
    check_ok("\"Lorem Ipsum Dolor\"", {"Lorem Ipsum Dolor"});
}
TEST_F(KshscanTest, TestDoubleQuotesLeadingSpace) {
    check_ok("\" Lorem Ipsum\"", {" Lorem Ipsum"});
}
TEST_F(KshscanTest, TestDoubleQuotesTrailingSpace) {
    check_ok("\"Lorem ipsum \"", {"Lorem ipsum "});
}
TEST_F(KshscanTest, TestUnnecessaryDoubleQuotes) {
    check_ok("\"Lorem\" \"ipsum\"", {"Lorem", "ipsum"});
}
TEST_F(KshscanTest, TestDoubleQuotesInString) {
    check_ok("Lorem\"\"ips\"u\"m\"\"", {"Loremipsum"});
}
TEST_F(KshscanTest, TestSingleQuoteInDoubleQuotes) {
    check_ok("\"a'bc\"''", {"a'bc"});
}
TEST_F(KshscanTest, TestDoubleQuotedMiddleArg) {
    check_ok("arg1 \"quoted with spaces\" arg2",
             {"arg1", "quoted with spaces", "arg2"});
}

// Escaping.
TEST_F(KshscanTest, TestEscapedSpace1) {
    check_ok("Lorem\\ ipsum", {"Lorem ipsum"});
}
TEST_F(KshscanTest, TestEscapedSpace2) {
    check_ok("arg1\\ with\\ spaces arg2", {"arg1 with spaces", "arg2"});
}
TEST_F(KshscanTest, TestEscapedSpace3) {
    check_ok("\\ arg1", {" arg1"});
}
TEST_F(KshscanTest, TestEscapedSpace4) {
    check_ok("\\  arg2", {" ", "arg2"});
}
TEST_F(KshscanTest, TestEscapedSingleQuote) {
    check_ok("\\'", {"'"});
}
TEST_F(KshscanTest, TestQuotedEscapedSingleQuote) {
    check_ok("'\\''", {"'"});
}
TEST_F(KshscanTest, TestEscapedEscape) {
    check_ok("'\\\\'", {"\\"});
}
TEST_F(KshscanTest, TestEscapedGarbage) {
    check_ok("som\\e r\\andom\\ char\\s escaped",
             {"som\\e", "r\\andom char\\s", "escaped"});
}
TEST_F(KshscanTest, TestTwoEscapes) {
    check_ok("a\\\\b", {"a\\b"});
}
TEST_F(KshscanTest, TestFourEscapes) {
    check_ok("a\\\\\\\\b", {"a\\\\b"});
}
TEST_F(KshscanTest, TestEightEscapes) {
    check_ok("a\\\\\\\\\\\\\\\\b", {"a\\\\\\\\b"});
}

// Mixed.
TEST_F(KshscanTest, TestMix1) {
    check_ok("'start quoted' middle \"end quoted\"",
             {"start quoted", "middle", "end quoted"});
}

// Errors.
TEST_F(KshscanTest, TestErrExpQuote1) {
    check_bad("'", {.err_type = KSHSCAN_ERR_EXP_SINGLE_QUOTE, .char_pos = 1});
}
TEST_F(KshscanTest, TestErrExpQuote2) {
    check_bad("\"", {.err_type = KSHSCAN_ERR_EXP_DOUBLE_QUOTE, .char_pos = 1});
}
TEST_F(KshscanTest, TestErrExpQuote3) {
    check_bad("'Abc",
              {.err_type = KSHSCAN_ERR_EXP_SINGLE_QUOTE, .char_pos = 4});
}
TEST_F(KshscanTest, TestErrExpQuote4) {
    check_bad("'abc\\'",
              {.err_type = KSHSCAN_ERR_EXP_SINGLE_QUOTE, .char_pos = 6});
}
