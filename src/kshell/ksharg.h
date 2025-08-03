#pragma once

#include <stddef.h>

#include "list.h"

typedef struct ksharg_parser_desc ksharg_parser_desc_t;
typedef struct ksharg_parser_inst ksharg_parser_inst_t;

typedef enum {
    KSHARG_VAL_STR,
} ksharg_val_type_t;

typedef union {
    int val_int;
    char *val_str;
    ksharg_parser_inst_t *val_parser;
} ksharg_val_t;

typedef struct {
    const char *name;
    const char *help_str;
    ksharg_val_type_t val_type;
    ksharg_val_t default_val;
    bool required;
} ksharg_posarg_desc_t;

typedef struct {
    const char *short_name;
    const char *long_name;
    const char *help_str;

    bool has_val;
    ksharg_val_type_t val_type;
    ksharg_val_t default_val;
    const char *val_name;
} ksharg_flag_desc_t;

struct ksharg_parser_desc {
    const char *name;
    const char *description;
    const char *epilog;

    size_t num_posargs;
    ksharg_posarg_desc_t *posargs;
    size_t num_flags;
    ksharg_flag_desc_t *flags;
};

typedef struct {
    const ksharg_posarg_desc_t *desc;
    ksharg_val_t val;

    char *given_str;
} ksharg_posarg_inst_t;

typedef struct {
    const ksharg_flag_desc_t *desc;
    char *find_name;
    ksharg_val_t val;

    char *given_str;
    char *val_str;
} ksharg_flag_inst_t;

struct ksharg_parser_inst {
    const ksharg_parser_desc_t *desc;
    size_t num_posargs;
    ksharg_posarg_inst_t *posargs;
    size_t num_flags;
    ksharg_flag_inst_t *flags;
};

typedef enum {
    KSHARG_ERR_NONE,
    KSHARG_ERR_INTERNAL,

    KSHARG_ERR_SCAN_FAILED,
    KSHARG_ERR_ARG_NAME_TAKEN,

    KSHARG_ERR_NO_POSARG_NAME_GIVEN,
    KSHARG_ERR_REQUIRED_FOLLOWS_OPTIONAL,
    KSHARG_ERR_TOO_MANY_POSARGS,
    KSHARG_ERR_MISSING_REQUIRED_POSARG,

    KSHARG_ERR_NO_FLAG_NAME_GIVEN,
    KSHARG_ERR_UNRECOGNIZED_FLAG,
    KSHARG_ERR_FLAG_REQUIRES_ARG,
    KSHARG_ERR_FLAG_SPECIFICED_TWICE,
    KSHARG_ERR_SHORT_FLAG_WITH_ARG_NOT_LAST,

    KSHARG_ERR_POSARG_NOT_FOUND,
    KSHARG_ERR_FLAG_NOT_FOUND,
} ksharg_err_t;

ksharg_err_t ksharg_inst_parser(const ksharg_parser_desc_t *desc,
                                ksharg_parser_inst_t **out_inst);
void ksharg_free_parser_inst(ksharg_parser_inst_t *inst);
void ksharg_print_help(const ksharg_parser_desc_t *desc);

ksharg_err_t ksharg_parse_str(ksharg_parser_inst_t *inst, const char *arg_str);
ksharg_err_t ksharg_parse_list(ksharg_parser_inst_t *inst,
                               const list_t *arg_list);

ksharg_err_t ksharg_get_posarg_inst(ksharg_parser_inst_t *inst,
                                    const char *name,
                                    ksharg_posarg_inst_t **out_posarg);
ksharg_err_t ksharg_get_flag_inst(ksharg_parser_inst_t *inst, const char *name,
                                  ksharg_flag_inst_t **out_flag);
