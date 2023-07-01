#include <printf.h>
#include <string.h>
#include <vga.h>

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

// Maximum lengths of unsigned and signed ints in base 10 and/or base 16.
#define MAX_INT_LEN_10          11
#define MAX_UINT_LEN_10         10
#define MAX_UINT_LEN_16         8

static void itoa(unsigned int num, bool b_signed, char * p_buf,
                 unsigned int base);

void
printf (char const * restrict p_format, ...)
{
    va_list args;
    va_start(args, p_format);

    // A string not containing '%'.
    //
    char const * p_pure_str = p_format;
    size_t pure_str_len = 0;

    // Next character is a special one (is preceded by '%'.)
    //
    bool b_next_spec = false;

    while ((*p_format) != 0)
    {
        if (b_next_spec)
        {
            // A special character.
            //
            b_next_spec  = false;
            p_pure_str   = (p_format + 1);
            pure_str_len = 0;

            if ('%' == (*p_format))
            {
                vga_print_str("%");
            }
            else if ('s' == (*p_format))
            {
                char const * p_arg_str = va_arg(args, char const *);
                vga_print_str(p_arg_str);
            }
            else if ('d' == (*p_format))
            {
                int arg_num = va_arg(args, int);
                char p_itoa_str[MAX_INT_LEN_10 + 1];
                itoa(arg_num, true, p_itoa_str, 10);
                vga_print_str(p_itoa_str);
            }
            else if ('u' == (*p_format))
            {
                unsigned int arg_num = va_arg(args, unsigned int);
                char p_itoa_str[MAX_UINT_LEN_10 + 1];
                itoa(arg_num, false, p_itoa_str, 10);
                vga_print_str(p_itoa_str);
            }
            else if (('x' == (*p_format)) || ('X' == (*p_format)))
            {
                unsigned int arg_num = va_arg(args, unsigned int);
                char p_itoa_str[MAX_UINT_LEN_16 + 1];
                itoa(arg_num, false, p_itoa_str, 16);

                if ('X' == (*p_format))
                {
                    string_to_upper(p_itoa_str);
                }

                vga_print_str(p_itoa_str);
            }
            else if (('p' == (*p_format)) || ('P' == (*p_format)))
            {
                vga_print_str("0x");

                void const * p_ptr = va_arg(args, void const *);
                char p_itoa_str[MAX_UINT_LEN_16 + 1];
                itoa((unsigned int) p_ptr, false, p_itoa_str, 16);

                if ('P' == (*p_format))
                {
                    string_to_upper(p_itoa_str);
                }

                vga_print_str(p_itoa_str);
            }
            else
            {
                // Unknown special character, just print it with the '%' sign.
                //
                char const * p_spec_str = (p_format - 1);
                vga_print_str_len(p_spec_str, 2);
            }
        }
        else if ((*p_format) == '%')
        {
            // A percent sign possibly followed by a special character.
            //
            b_next_spec = true;

            // Print the preceding non-special characters, if there were any.
            //
            if (pure_str_len > 0)
            {
                vga_print_str_len(p_pure_str, pure_str_len);
            }
        }
        else
        {
            pure_str_len++;
        }

        p_format++;
    }

    // If the format string ends with '%', print it.
    //
    if (b_next_spec)
    {
        vga_print_str("%");
    }
    else if (pure_str_len > 0)
    {
        // Otherwise, print the last sequence of non-special characters.
        //
        vga_print_str_len(p_pure_str, pure_str_len);
    }

    va_end(args);
}

static void
itoa (unsigned int num, bool b_signed, char * p_buf, unsigned int base)
{
    size_t buf_pos  = 0;
    bool b_negative = false;

    if (0 == num)
    {
        p_buf[buf_pos++] = '0';
    }
    else if (b_signed && (((int) num) < 0))
    {
        b_negative = true;
        num *= (-1);
    }

    while (num > 0)
    {
        int rem = (num % base);
        char ch;
        if (rem < 10)
        {
            ch = (48 + rem);
        }
        else
        {
            ch = ('a' + (rem - 10));
        }

        p_buf[buf_pos++] = ch;
        num /= base;
    }

    if (b_negative)
    {
        p_buf[buf_pos++] = '-';
    }

    // Reverse the string.
    //
    for (size_t idx = 0; idx < (buf_pos / 2); idx++)
    {
        char tmp;
        tmp                        = p_buf[(buf_pos - 1) - idx];
        p_buf[(buf_pos - 1) - idx] = p_buf[idx];
        p_buf[idx]                 = tmp;
    }

    p_buf[buf_pos++] = 0;
}
