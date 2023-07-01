#include <printf.h>
#include <vga.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

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
