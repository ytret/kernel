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
