#include "heap.h"
#include "kprintf.h"
#include "kshell/ksharg.h"
#include "kshell/kshscan.h"
#include "kstring.h"
#include "memfun.h"

static ksharg_err_t
prv_ksharg_check_name(const ksharg_parser_inst_t *parser_inst,
                      const char *name);

static ksharg_err_t
prv_ksharg_inst_posarg(ksharg_parser_inst_t *parser_inst,
                       const ksharg_posarg_desc_t *posarg_desc,
                       ksharg_posarg_inst_t *posarg_inst);
static ksharg_err_t prv_ksharg_inst_flag(ksharg_parser_inst_t *parser_inst,
                                         const ksharg_flag_desc_t *flag_desc,
                                         ksharg_flag_inst_t *flag_inst);

static ksharg_err_t
prv_ksharg_parse_posarg_str(ksharg_posarg_inst_t *posarg_inst,
                            const char *arg_str);
static ksharg_err_t prv_ksharg_parse_flag_seq(ksharg_parser_inst_t *parser_inst,
                                              const char *arg_str,
                                              const char *next_arg,
                                              bool *out_skip_next);

static ksharg_err_t
prv_ksharg_parse_long_flag(ksharg_parser_inst_t *parser_inst,
                           const char *arg_str, const char *next_arg,
                           bool *out_skip_next);
static ksharg_err_t
prv_ksharg_parse_short_flag(ksharg_parser_inst_t *parser_inst, char flag_ch,
                            bool last_in_seq, const char *next_arg,
                            bool *out_skip_next);
static ksharg_err_t prv_ksharg_find_flag(ksharg_parser_inst_t *parser_inst,
                                         const char *name,
                                         ksharg_flag_inst_t **out_flag_inst);

static ksharg_err_t prv_ksharg_parse_typed_val(const char *str,
                                               ksharg_val_type_t val_type,
                                               ksharg_val_t *val);

ksharg_err_t ksharg_inst_parser(const ksharg_parser_desc_t *desc,
                                ksharg_parser_inst_t **out_inst) {
    ksharg_err_t err;

    ksharg_parser_inst_t *const inst = heap_alloc(sizeof(*inst));
    kmemset(inst, 0, sizeof(*inst));
    inst->desc = desc;
    inst->num_posargs = desc->num_posargs;
    inst->num_flags = desc->num_flags;

    if (inst->num_posargs > 0) {
        inst->posargs =
            heap_alloc(inst->num_posargs * sizeof(ksharg_posarg_inst_t));
        kmemset(inst->posargs, 0,
                inst->num_posargs * sizeof(ksharg_posarg_inst_t));
    }
    if (inst->num_flags > 0) {
        inst->flags = heap_alloc(inst->num_flags * sizeof(ksharg_flag_inst_t));
        kmemset(inst->flags, 0, inst->num_flags * sizeof(ksharg_flag_inst_t));
    }

    bool now_required = true;
    for (size_t idx_posarg = 0; idx_posarg < desc->num_posargs; idx_posarg++) {
        const ksharg_posarg_desc_t *const posarg_desc =
            &desc->posargs[idx_posarg];
        if (posarg_desc->required) {
            if (!now_required) {
                if (inst->posargs) { heap_free(inst->posargs); }
                if (inst->flags) { heap_free(inst->flags); }
                heap_free(inst);
                kprintf("ksharg: required positional argument '%s' follows "
                        "optional arguments\n",
                        posarg_desc->name);
                return KSHARG_ERR_REQUIRED_FOLLOWS_OPTIONAL;
            }
        } else {
            now_required = false;
        }

        ksharg_posarg_inst_t *const posarg_inst = &inst->posargs[idx_posarg];
        err = prv_ksharg_inst_posarg(inst, posarg_desc, posarg_inst);
        if (err != KSHARG_ERR_NONE) {
            if (inst->posargs) { heap_free(inst->posargs); }
            if (inst->flags) { heap_free(inst->flags); }
            heap_free(inst);
            return err;
        }
    }
    for (size_t idx_flag = 0; idx_flag < desc->num_flags; idx_flag++) {
        const ksharg_flag_desc_t *const flag_desc = &desc->flags[idx_flag];
        ksharg_flag_inst_t *const flag_inst = &inst->flags[idx_flag];
        err = prv_ksharg_inst_flag(inst, flag_desc, flag_inst);
        if (err != KSHARG_ERR_NONE) {
            if (inst->posargs) { heap_free(inst->posargs); }
            if (inst->flags) { heap_free(inst->flags); }
            heap_free(inst);
            return err;
        }
    }

    *out_inst = inst;
    return KSHARG_ERR_NONE;
}

void ksharg_free_parser_inst(ksharg_parser_inst_t *inst) {
    for (size_t idx = 0; idx < inst->num_posargs; idx++) {
        ksharg_posarg_inst_t *const posarg = &inst->posargs[idx];

        switch (posarg->desc->val_type) {
        case KSHARG_VAL_STR:
            if (posarg->val.val_str) { heap_free(posarg->val.val_str); }
            break;
        }

        if (posarg->given_str) { heap_free(posarg->given_str); }
    }

    for (size_t idx = 0; idx < inst->num_flags; idx++) {
        ksharg_flag_inst_t *const flag = &inst->flags[idx];

        if (flag->find_name) { heap_free(flag->find_name); }
        if (flag->given_str) { heap_free(flag->given_str); }

        if (flag->desc->has_val) {
            if (flag->val_str) { heap_free(flag->val_str); }

            switch (flag->desc->val_type) {
            case KSHARG_VAL_STR:
                if (flag->val.val_str) { heap_free(flag->val.val_str); }
                break;
            }
        }
    }

    if (inst->num_posargs > 0) { heap_free(inst->posargs); }
    if (inst->num_flags > 0) { heap_free(inst->flags); }

    heap_free(inst);
}

ksharg_err_t ksharg_parse_str(ksharg_parser_inst_t *inst, const char *arg_str) {
    list_t arg_list;
    list_init(&arg_list, NULL);

    kshscan_err_t scan_err = kshscan_str(arg_str, &arg_list);
    if (scan_err.err_type != KSHSCAN_ERR_NONE) {
        kshscan_free_arg_list(&arg_list);
        kprintf("ksharg: kshscan failed with error code %u\n",
                scan_err.err_type);
        return KSHARG_ERR_SCAN_FAILED;
    }

    ksharg_err_t err = ksharg_parse_list(inst, &arg_list);
    kshscan_free_arg_list(&arg_list);
    return err;
}

ksharg_err_t ksharg_parse_list(ksharg_parser_inst_t *inst,
                               const list_t *arg_list) {
    ksharg_err_t err;
    size_t posarg_idx = 0;
    bool skip_arg = false;
    for (list_node_t *arg_node = arg_list->p_first_node; arg_node != NULL;
         arg_node = arg_node->p_next) {
        if (skip_arg) {
            skip_arg = false;
            continue;
        }

        const kshscan_arg_t *arg =
            LIST_NODE_TO_STRUCT(arg_node, kshscan_arg_t, list_node);
        const kshscan_arg_t *next_arg =
            arg_node->p_next ? LIST_NODE_TO_STRUCT(arg_node->p_next,
                                                   kshscan_arg_t, list_node)
                             : NULL;

        const bool is_flag =
            string_len(arg->arg_str) > 0 && arg->arg_str[0] == '-';
        if (is_flag) {
            err = prv_ksharg_parse_flag_seq(inst, arg->arg_str,
                                            next_arg ? next_arg->arg_str : NULL,
                                            &skip_arg);
            if (err != KSHARG_ERR_NONE) { return err; }
        } else {
            if (posarg_idx >= inst->num_posargs) {
                kprintf("ksharg: too many positional arguments were given\n");
                return KSHARG_ERR_TOO_MANY_POSARGS;
            }
            ksharg_posarg_inst_t *const posarg_inst =
                &inst->posargs[posarg_idx];
            err = prv_ksharg_parse_posarg_str(posarg_inst, arg->arg_str);
            if (err != KSHARG_ERR_NONE) { return err; }
            posarg_idx++;
        }
    }

    for (size_t idx = 0; idx < inst->num_posargs; idx++) {
        const ksharg_posarg_inst_t *posarg = &inst->posargs[idx];
        if (posarg->desc->required && posarg->given_str == NULL) {
            kprintf("ksharg: missing required positional argument '%s'\n",
                    posarg->desc->name);
            return KSHARG_ERR_MISSING_REQUIRED_POSARG;
        }
    }

    return KSHARG_ERR_NONE;
}

ksharg_err_t ksharg_get_posarg_inst(ksharg_parser_inst_t *inst,
                                    const char *name,
                                    ksharg_posarg_inst_t **out_posarg) {
    for (size_t idx = 0; idx < inst->num_posargs; idx++) {
        ksharg_posarg_inst_t *const posarg = &inst->posargs[idx];
        if (string_equals(name, posarg->desc->name)) {
            *out_posarg = posarg;
            return KSHARG_ERR_NONE;
        }
    }
    *out_posarg = nullptr;
    kprintf("ksharg: positional argument '%s' is not found\n", name);
    return KSHARG_ERR_POSARG_NOT_FOUND;
}

ksharg_err_t ksharg_get_flag_inst(ksharg_parser_inst_t *inst, const char *name,
                                  ksharg_flag_inst_t **out_flag) {
    for (size_t idx = 0; idx < inst->num_flags; idx++) {
        ksharg_flag_inst_t *const flag = &inst->flags[idx];
        if (string_equals(name, flag->find_name)) {
            *out_flag = flag;
            return KSHARG_ERR_NONE;
        }
    }
    *out_flag = nullptr;
    kprintf("ksharg: flag '%s' is not found\n", name);
    return KSHARG_ERR_FLAG_NOT_FOUND;
}

static ksharg_err_t
prv_ksharg_inst_posarg(ksharg_parser_inst_t *parser_inst,
                       const ksharg_posarg_desc_t *posarg_desc,
                       ksharg_posarg_inst_t *posarg_inst) {
    if (!posarg_desc->name || string_len(posarg_desc->name) == 0) {
        kprintf("ksharg: missing positional argument name\n");
        return KSHARG_ERR_NO_POSARG_NAME_GIVEN;
    }

    ksharg_err_t err = prv_ksharg_check_name(parser_inst, posarg_desc->name);
    if (err != KSHARG_ERR_NONE) { return err; }

    posarg_inst->desc = posarg_desc;
    if (!posarg_desc->required && posarg_desc->val_type == KSHARG_VAL_STR &&
        posarg_desc->default_val.val_str) {
        posarg_inst->val.val_str = string_dup(posarg_desc->default_val.val_str);
    } else {
        posarg_inst->val = posarg_desc->default_val;
    }
    return KSHARG_ERR_NONE;
}

static ksharg_err_t prv_ksharg_inst_flag(ksharg_parser_inst_t *parser_inst,
                                         const ksharg_flag_desc_t *flag_desc,
                                         ksharg_flag_inst_t *flag_inst) {
    ksharg_err_t err;

    const bool has_short_name =
        flag_desc->short_name && string_len(flag_desc->short_name) > 0;
    const bool has_long_name =
        flag_desc->long_name && string_len(flag_desc->long_name) > 0;
    if (!has_short_name && !has_long_name) {
        kprintf("ksharg: missing flag name\n");
        return KSHARG_ERR_NO_FLAG_NAME_GIVEN;
    }

    if (has_short_name) {
        err = prv_ksharg_check_name(parser_inst, flag_desc->short_name);
        if (err != KSHARG_ERR_NONE) { return err; }
    }
    if (has_long_name) {
        err = prv_ksharg_check_name(parser_inst, flag_desc->long_name);
        if (err != KSHARG_ERR_NONE) { return err; }
    }

    if (has_long_name) {
        flag_inst->find_name = string_dup(flag_desc->long_name);
    } else {
        flag_inst->find_name = string_dup(flag_desc->short_name);
    }

    flag_inst->desc = flag_desc;
    if (flag_desc->has_val && flag_desc->val_type == KSHARG_VAL_STR &&
        flag_desc->default_val.val_str) {
        flag_inst->val.val_str = string_dup(flag_desc->default_val.val_str);
    } else {
        flag_inst->val = flag_desc->default_val;
    }
    return KSHARG_ERR_NONE;
}

static ksharg_err_t
prv_ksharg_check_name(const ksharg_parser_inst_t *parser_inst,
                      const char *name) {
    for (size_t idx = 0; idx < parser_inst->num_posargs; idx++) {
        ksharg_posarg_inst_t *const posarg_inst = &parser_inst->posargs[idx];
        if (!posarg_inst->desc) { continue; }
        if (string_equals(posarg_inst->desc->name, name)) {
            kprintf("ksharg: argument name conflict: '%s' is already used for "
                    "a positional argument\n",
                    name);
            return KSHARG_ERR_ARG_NAME_TAKEN;
        }
    }
    for (size_t idx = 0; idx < parser_inst->num_flags; idx++) {
        ksharg_flag_inst_t *const flag_inst = &parser_inst->flags[idx];
        if (!flag_inst->desc) { continue; }

        const bool eq_short_name =
            flag_inst->desc->short_name &&
            string_equals(flag_inst->desc->short_name, name);
        const bool eq_long_name =
            flag_inst->desc->long_name &&
            string_equals(flag_inst->desc->long_name, name);

        if (eq_short_name || eq_long_name) {
            kprintf("ksharg: argument name conflict: '%s' is already used for "
                    "a flag\n",
                    name);
            return KSHARG_ERR_ARG_NAME_TAKEN;
        }
    }
    return KSHARG_ERR_NONE;
}

static ksharg_err_t
prv_ksharg_parse_posarg_str(ksharg_posarg_inst_t *posarg_inst,
                            const char *arg_str) {
    ksharg_err_t err = prv_ksharg_parse_typed_val(
        arg_str, posarg_inst->desc->val_type, &posarg_inst->val);
    if (err != KSHARG_ERR_NONE) { return err; }
    posarg_inst->given_str = string_dup(arg_str);
    return KSHARG_ERR_NONE;
}

static ksharg_err_t prv_ksharg_parse_flag_seq(ksharg_parser_inst_t *parser_inst,
                                              const char *arg_str,
                                              const char *next_arg,
                                              bool *out_skip_next) {
    const bool is_long =
        string_len(arg_str) > 2 && (arg_str[0] == '-' && arg_str[1] == '-');
    if (is_long) {
        // FIXME: parse '--'

        return prv_ksharg_parse_long_flag(parser_inst, arg_str, next_arg,
                                          out_skip_next);
    } else {
        // FIXME: parse '-'
        ksharg_err_t err;

        for (size_t ch_idx = 1; ch_idx < string_len(arg_str); ch_idx++) {
            const char flag_ch = arg_str[ch_idx];
            const bool last_in_seq = ch_idx == string_len(arg_str) - 1;
            err = prv_ksharg_parse_short_flag(parser_inst, flag_ch, last_in_seq,
                                              next_arg, out_skip_next);
            if (err != KSHARG_ERR_NONE) { return err; }
        }
    }

    return KSHARG_ERR_NONE;
}

static ksharg_err_t
prv_ksharg_parse_long_flag(ksharg_parser_inst_t *parser_inst,
                           const char *arg_str, const char *next_arg,
                           bool *out_skip_next) {
    ksharg_err_t err;

    ksharg_flag_inst_t *flag;
    err = prv_ksharg_find_flag(parser_inst, arg_str, &flag);
    if (err != KSHARG_ERR_NONE) { return err; }

    if (flag->given_str) {
        kprintf("ksharg: flag '%s' was specified twice\n", flag->find_name);
        return KSHARG_ERR_FLAG_SPECIFICED_TWICE;
    }
    flag->given_str = string_dup(arg_str);

    if (flag->desc->has_val) {
        if (next_arg) {
            err = prv_ksharg_parse_typed_val(next_arg, flag->desc->val_type,
                                             &flag->val);
            if (err != KSHARG_ERR_NONE) { return err; }
            flag->val_str = string_dup(next_arg);
            *out_skip_next = true;
        } else {
            kprintf("ksharg: flag '%s' requires an argument\n",
                    flag->find_name);
            return KSHARG_ERR_FLAG_REQUIRES_ARG;
        }
    } else {
        flag->val_str = NULL;
    }

    return KSHARG_ERR_NONE;
}

static ksharg_err_t
prv_ksharg_parse_short_flag(ksharg_parser_inst_t *parser_inst, char flag_ch,
                            bool last_in_seq, const char *next_arg,
                            bool *out_skip_next) {
    ksharg_err_t err;

    const char flag_str[2] = {flag_ch, 0};
    ksharg_flag_inst_t *flag;
    err = prv_ksharg_find_flag(parser_inst, flag_str, &flag);
    if (err != KSHARG_ERR_NONE) { return err; }

    if (flag->given_str) {
        kprintf("ksharg: flag '%s' was specified twice\n", flag->find_name);
        return KSHARG_ERR_FLAG_SPECIFICED_TWICE;
    }
    flag->given_str = string_dup(flag_str);

    if (flag->desc->has_val) {
        if (last_in_seq) {
            err = prv_ksharg_parse_typed_val(next_arg, flag->desc->val_type,
                                             &flag->val);
            if (err != KSHARG_ERR_NONE) { return err; }
            flag->val_str = string_dup(next_arg);
            *out_skip_next = true;
        } else {
            kprintf("ksharg: flag '%s' requires an argument, but is not last "
                    "in the flag sequence\n",
                    flag->find_name);
            return KSHARG_ERR_SHORT_FLAG_WITH_ARG_NOT_LAST;
        }
    } else {
        flag->val_str = NULL;
    }

    return KSHARG_ERR_NONE;
}

static ksharg_err_t prv_ksharg_find_flag(ksharg_parser_inst_t *parser_inst,
                                         const char *name,
                                         ksharg_flag_inst_t **out_flag_inst) {
    const bool is_long =
        string_len(name) > 2 && (name[0] == '-' && name[1] == '-');
    const char *nodash_name = is_long ? &name[2] : name;

    for (size_t idx = 0; idx < parser_inst->num_flags; idx++) {
        ksharg_flag_inst_t *const flag_inst = &parser_inst->flags[idx];

        if (is_long) {
            if (flag_inst->desc->long_name &&
                string_equals(nodash_name, flag_inst->desc->long_name)) {
                *out_flag_inst = flag_inst;
                return KSHARG_ERR_NONE;
            }
        } else if (flag_inst->desc->short_name &&
                   string_equals(nodash_name, flag_inst->desc->short_name)) {
            *out_flag_inst = flag_inst;
            return KSHARG_ERR_NONE;
        }
    }

    kprintf("ksharg: unrecognized flag '%s'\n", name);
    return KSHARG_ERR_UNRECOGNIZED_FLAG;
}

static ksharg_err_t prv_ksharg_parse_typed_val(const char *str,
                                               ksharg_val_type_t val_type,
                                               ksharg_val_t *val) {
    switch (val_type) {
    case KSHARG_VAL_STR:
        val->val_str = string_dup(str);
        break;
    }
    return KSHARG_ERR_NONE;
}
