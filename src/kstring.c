#include "heap.h"
#include "kstring.h"
#include "memfun.h"

void string_to_upper(char *p_str) {
    while ((*p_str) != 0) {
        if (('a' <= (*p_str)) && ((*p_str) <= 'z')) { *p_str += ('A' - 'a'); }
        p_str++;
    }
}

bool string_equals(char const *p_a, char const *p_b) {
    while (((*p_a) != 0) && ((*p_b) != 0)) {
        if ((*p_a) != (*p_b)) { return false; }

        p_a++;
        p_b++;
    }
    return (*p_a) == (*p_b);
}

size_t string_len(char const *p_str) {
    size_t len = 0;
    while (*p_str) {
        len++;
        p_str++;
    }
    return len;
}

size_t string_split(char const *p_str, char ch, bool b_ignore_empty,
                    char **pp_res, size_t res_len) {
    size_t res_idx = 0; // next ptr in pp_res to be set

    size_t idx = 0;          // current char idx
    size_t substr_len = 0;   // num of chars from previous separator
    size_t after_ch_idx = 0; // previous separator idx + 1

    while (true) {
        if (((ch == p_str[idx]) || (0 == p_str[idx]))) {
            // A separator or end of p_str.

            if ((!b_ignore_empty) || ((b_ignore_empty && (substr_len != 0)))) {
                // Allocate a string to store the substring.
                char *p_substr = heap_alloc(substr_len + 1);

                // Copy the substring.
                __builtin_memcpy(p_substr, &p_str[after_ch_idx], substr_len);
                p_substr[substr_len] = 0;

                if (res_idx == res_len) {
                    // pp_res is full, while the string has not been fully
                    // parsed.
                    return res_idx + 1;
                }

                // Update pp_res.
                pp_res[res_idx] = p_substr;
                res_idx++;
            }

            // Update counters.
            after_ch_idx = (idx + 1);
            substr_len = 0;

            if (0 == p_str[idx]) { break; }
        } else {
            // In else block, because otherwise it would be set to 0 above and
            // immediately incremented.
            substr_len++;
        }

        idx++;
    }

    return res_idx;
}

bool string_to_uint32(char const *p_str, uint32_t *p_num, int base) {
    // Construct an uppercase copy of p_str.
    char *const p_strup = heap_alloc(string_len(p_str) + 1);
    char *p_iter = p_strup;
    __builtin_memcpy(p_strup, p_str, string_len(p_str) + 1);
    string_to_upper(p_strup);

    // Reset *p_num.
    *p_num = 0;

    bool b_ok = true;
    while (*p_iter) {
        int digit;
        if (('0' <= (*p_iter)) && ((*p_iter) <= '9')) {
            digit = ((*p_iter) - '0');
        } else if (('A' <= (*p_iter)) && ((*p_iter) <= 'Z')) {
            digit = (((*p_iter) - 'A') + 10);
        } else {
            // Cannot convert char to a digit.
            *p_num = 0;
            b_ok = false;
            break;
        }

        if (digit >= base) {
            // The digit is greater than the base.
            *p_num = 0;
            b_ok = false;
            break;
        }

        uint64_t check_overflow = (((uint64_t)(*p_num)) * base + digit);
        if (check_overflow > UINT32_MAX) {
            *p_num = 0;
            b_ok = false;
            break;
        }

        *p_num *= base;
        *p_num += digit;

        p_iter++;
    }

    heap_free(p_strup);
    return b_ok;
}

size_t string_itoa(unsigned int num, bool b_signed, char *p_buf,
                   unsigned int base) {
    size_t buf_pos = 0;
    bool b_negative = false;

    if (0 == num) {
        p_buf[buf_pos++] = '0';
    } else if (b_signed && (((int)num) < 0)) {
        b_negative = true;
        num *= (-1);
    }

    while (num > 0) {
        int rem = (num % base);
        char ch;
        if (rem < 10) {
            ch = (48 + rem);
        } else {
            ch = ('a' + (rem - 10));
        }

        p_buf[buf_pos++] = ch;
        num /= base;
    }

    if (b_negative) { p_buf[buf_pos++] = '-'; }

    // Reverse the string.
    for (size_t idx = 0; idx < (buf_pos / 2); idx++) {
        char tmp;
        tmp = p_buf[(buf_pos - 1) - idx];
        p_buf[(buf_pos - 1) - idx] = p_buf[idx];
        p_buf[idx] = tmp;
    }

    p_buf[buf_pos++] = 0;
    return buf_pos;
}

char *string_dup(char const *p_str) {
    const size_t len = string_len(p_str);
    char *const dst = heap_alloc(len + 1);
    kmemcpy(dst, p_str, len + 1);
    return dst;
}
