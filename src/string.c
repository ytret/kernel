#include <string.h>

void
string_to_upper (char * p_str)
{
    while ((*p_str) != 0)
    {
        if (('a' <= (*p_str)) && ((*p_str) <= 'z'))
        {
            *p_str += ('A' - 'a');
        }
        p_str++;
    }
}

bool
string_equals (char const * p_a, char const * p_b)
{
    while (((*p_a) != 0) && ((*p_b) != 0))
    {
        if ((*p_a) != (*p_b))
        {
            return (false);
        }

        p_a++;
        p_b++;
    }
    return ((*p_a) == (*p_b));
}

size_t
string_len (char const * p_str)
{
    size_t len = 0;
    while (*p_str)
    {
        len++;
        p_str++;
    }
    return (len);
}
