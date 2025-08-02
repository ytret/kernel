#include <gtest/gtest.h>

extern "C" {
#include "kshell/ksharg.h"
#include "list.h"
}

class KshargTest : public testing::Test {
  protected:
    void SetUp() override {
        list_init(&empty_list, NULL);

        desc.num_posargs = 0;
        desc.num_flags = 0;
    }

    void TearDown() override {
        if (desc.num_posargs > 0) { free(desc.posargs); }
        if (desc.num_flags > 0) { free(desc.flags); }
    }

    void set_posargs(const std::vector<ksharg_posarg_desc_t> &posargs) {
        desc.num_posargs = posargs.size();
        desc.posargs = static_cast<ksharg_posarg_desc_t *>(
            malloc(posargs.size() * sizeof(ksharg_posarg_desc_t)));
        size_t idx = 0;
        for (const ksharg_posarg_desc_t &posarg : posargs) {
            desc.posargs[idx] = posarg;
            idx++;
        }
    }

    void set_flags(const std::vector<ksharg_flag_desc_t> &flags) {
        desc.num_flags = flags.size();
        desc.flags = static_cast<ksharg_flag_desc_t *>(
            malloc(flags.size() * sizeof(ksharg_flag_desc_t)));
        size_t idx = 0;
        for (const ksharg_flag_desc_t &flag : flags) {
            desc.flags[idx] = flag;
            idx++;
        }
    }

    list_t empty_list;
    ksharg_err_t err;
    ksharg_parser_desc_t desc = {};
};

TEST_F(KshargTest, NoArgs) {
    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_list(inst, &empty_list);
    ASSERT_EQ(err, KSHARG_ERR_NONE);

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, NoArgs_ButOneGiven) {
    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "arg");
    ASSERT_EQ(err, KSHARG_ERR_TOO_MANY_POSARGS);

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, OnePosArg_Req_Given) {
    set_posargs({{
        .name = "posarg",
        .help_str = nullptr,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = nullptr},
        .required = true,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "argval");
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_STREQ(inst->posargs[0].val.val_str, "argval");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, OnePosArg_Req_Missing) {
    set_posargs({{
        .name = "posarg",
        .help_str = nullptr,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = nullptr},
        .required = true,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_list(inst, &empty_list);
    EXPECT_EQ(err, KSHARG_ERR_MISSING_REQUIRED_POSARG);
    EXPECT_EQ(inst->posargs[0].val.val_str, nullptr);

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, OnePosArg_Req_ButTwoGiven) {
    set_posargs({{
        .name = "posarg",
        .help_str = nullptr,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = nullptr},
        .required = true,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "arg1 arg2");
    ASSERT_EQ(err, KSHARG_ERR_TOO_MANY_POSARGS);

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, OnePosArg_Opt_Given) {
    set_posargs({{
        .name = "posarg",
        .help_str = nullptr,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = strdup("default")},
        .required = true,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "given");
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->posargs[0].val.val_str, "given");

    ksharg_free_parser_inst(inst);
    free(desc.posargs[0].default_val.val_str);
}

TEST_F(KshargTest, OnePosArg_Opt_Missing) {
    set_posargs({{
        .name = "posarg",
        .help_str = nullptr,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = strdup("default")},
        .required = false,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_list(inst, &empty_list);
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->posargs[0].val.val_str, "default");

    ksharg_free_parser_inst(inst);
    free(desc.posargs[0].default_val.val_str);
}

TEST_F(KshargTest, TwoPosArgs_ReqReq_GivenGiven) {
    set_posargs({
        {
            .name = "arg1",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = true,
        },
        {
            .name = "arg2",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = true,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "val1 val2");
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->posargs[0].val.val_str, "val1");
    EXPECT_STREQ(inst->posargs[1].val.val_str, "val2");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, TwoPosArgs_OptReq) {
    set_posargs({
        {
            .name = "arg1",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = false,
        },
        {
            .name = "arg2",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = true,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    EXPECT_EQ(err, KSHARG_ERR_REQUIRED_FOLLOWS_OPTIONAL);
    EXPECT_EQ(inst, nullptr);
}

TEST_F(KshargTest, TwoPosArgs_ReqOpt_GivenGiven) {
    set_posargs({
        {
            .name = "arg1",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = true,
        },
        {
            .name = "arg2",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = strdup("default2")},
            .required = false,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "val1 val2");
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->posargs[0].val.val_str, "val1");
    EXPECT_STREQ(inst->posargs[1].val.val_str, "val2");

    ksharg_free_parser_inst(inst);
    free(desc.posargs[1].default_val.val_str);
}

TEST_F(KshargTest, TwoPosArgs_ReqOpt_GivenMissing) {
    set_posargs({
        {
            .name = "arg1",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = true,
        },
        {
            .name = "arg2",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = strdup("default2")},
            .required = false,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "val1");
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->posargs[0].val.val_str, "val1");
    EXPECT_STREQ(inst->posargs[1].val.val_str, "default2");

    ksharg_free_parser_inst(inst);
    free(desc.posargs[1].default_val.val_str);
}

TEST_F(KshargTest, OneFlag_Short_Given) {
    set_flags({ksharg_flag_desc_t{
        .short_name = "f",
        .long_name = nullptr,
        .help_str = nullptr,
        .has_val = false,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = nullptr},
        .val_name = nullptr,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-f");
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->flags[0].given_str, "f");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, OneFlag_Long_Given) {
    set_flags({ksharg_flag_desc_t{
        .short_name = nullptr,
        .long_name = "flag",
        .help_str = nullptr,
        .has_val = false,
        .val_type = KSHARG_VAL_STR,
        .default_val = {.val_str = nullptr},
        .val_name = nullptr,
    }});

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "--flag");
    EXPECT_EQ(err, KSHARG_ERR_NONE);
    EXPECT_STREQ(inst->flags[0].given_str, "--flag");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, FlagSeq_ABC) {
    set_flags({
        ksharg_flag_desc_t{
            .short_name = "a",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
        ksharg_flag_desc_t{
            .short_name = "b",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
        ksharg_flag_desc_t{
            .short_name = "c",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-abc");
    EXPECT_EQ(err, KSHARG_ERR_NONE);

    ksharg_flag_inst_t *flag = nullptr;

    err = ksharg_get_flag_inst(inst, "a", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "a");

    err = ksharg_get_flag_inst(inst, "b", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "b");

    err = ksharg_get_flag_inst(inst, "c", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "c");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, FlagSeq_CBA) {
    set_flags({
        ksharg_flag_desc_t{
            .short_name = "a",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
        ksharg_flag_desc_t{
            .short_name = "b",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
        ksharg_flag_desc_t{
            .short_name = "c",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-cba");
    EXPECT_EQ(err, KSHARG_ERR_NONE);

    ksharg_flag_inst_t *flag = nullptr;

    err = ksharg_get_flag_inst(inst, "a", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "a");

    err = ksharg_get_flag_inst(inst, "b", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "b");

    err = ksharg_get_flag_inst(inst, "c", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "c");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, FlagSeq_Unrecognized) {
    set_flags({
        ksharg_flag_desc_t{
            .short_name = "a",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-axy");
    EXPECT_EQ(err, KSHARG_ERR_UNRECOGNIZED_FLAG);

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, FlagSeq_WithVal_Ok) {
    set_flags({
        ksharg_flag_desc_t{
            .short_name = "a",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
        ksharg_flag_desc_t{
            .short_name = "v",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = true,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-av val1");
    EXPECT_EQ(err, KSHARG_ERR_NONE);

    ksharg_flag_inst_t *flag = nullptr;
    err = ksharg_get_flag_inst(inst, "v", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "v");
    EXPECT_STREQ(flag->val_str, "val1");
    EXPECT_STREQ(flag->val.val_str, "val1");

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, FlagSeq_WithVal_BadOrder) {
    set_flags({
        ksharg_flag_desc_t{
            .short_name = "a",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = false,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
        ksharg_flag_desc_t{
            .short_name = "v",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = true,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-va val1");
    EXPECT_EQ(err, KSHARG_ERR_SHORT_FLAG_WITH_ARG_NOT_LAST);

    ksharg_free_parser_inst(inst);
}

TEST_F(KshargTest, FlagWithValSkipsVal) {
    set_posargs({
        ksharg_posarg_desc_t{
            .name = "posarg",
            .help_str = nullptr,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .required = true,
        },
    });
    set_flags({
        ksharg_flag_desc_t{
            .short_name = "v",
            .long_name = nullptr,
            .help_str = nullptr,
            .has_val = true,
            .val_type = KSHARG_VAL_STR,
            .default_val = {.val_str = nullptr},
            .val_name = nullptr,
        },
    });

    ksharg_parser_inst_t *inst = nullptr;
    err = ksharg_inst_parser(&desc, &inst);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(inst, nullptr);

    err = ksharg_parse_str(inst, "-v val1 posarg1");
    EXPECT_EQ(err, KSHARG_ERR_NONE);

    ksharg_posarg_inst_t *posarg = nullptr;
    err = ksharg_get_posarg_inst(inst, "posarg", &posarg);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(posarg, nullptr);
    EXPECT_STREQ(posarg->given_str, "posarg1");
    EXPECT_STREQ(posarg->val.val_str, "posarg1");

    ksharg_flag_inst_t *flag = nullptr;
    err = ksharg_get_flag_inst(inst, "v", &flag);
    ASSERT_EQ(err, KSHARG_ERR_NONE);
    ASSERT_NE(flag, nullptr);
    EXPECT_STREQ(flag->given_str, "v");
    EXPECT_STREQ(flag->val_str, "val1");
    EXPECT_STREQ(flag->val.val_str, "val1");

    ksharg_free_parser_inst(inst);
}
