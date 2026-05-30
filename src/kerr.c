#include "kerr.h"

const char *kerr_str(kerr_t err) {
    switch (err) {
    case KERR_NONE:          return "KERR_NONE";
    case KERR_NO_SPACE:      return "KERR_NO_SPACE";
    case KERR_NAME_TOO_LONG: return "KERR_NAME_TOO_LONG";
    case KERR_PATH_TOO_DEEP: return "KERR_PATH_TOO_DEEP";
    case KERR_EMPTY_PATH:    return "KERR_EMPTY_PATH";
    case KERR_NOT_ABSOLUTE:  return "KERR_NOT_ABSOLUTE";
    case KERR_BAD_NODE:      return "KERR_BAD_NODE";
    case KERR_NOT_SUPP:      return "KERR_NOT_SUPP";
    case KERR_BAD_ARGS:      return "KERR_BAD_ARGS";
    case KERR_NO_FS:         return "KERR_NO_FS";
    case KERR_NO_FS_DATA:    return "KERR_NO_FS_DATA";
    case KERR_EXISTS:        return "KERR_EXISTS";
    case KERR_NOT_FOUND:     return "KERR_NOT_FOUND";
    case KERR_NOT_EMPTY:     return "KERR_NOT_EMPTY";
    case KERR_IO:            return "KERR_IO";
    case KERR_IN_USE:        return "KERR_IN_USE";
    case KERR_BAD_OFFSET:    return "KERR_BAD_OFFSET";
    case KERR_NOT_MOUNTED:   return "KERR_NOT_MOUNTED";
    case KERR_BAD_FLAGS:     return "KERR_BAD_FLAGS";
    case KERR_NOT_OPENED:    return "KERR_NOT_OPENED";
    }
    return "<unknown kerr_t value>";
}
