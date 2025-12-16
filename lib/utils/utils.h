#ifndef _UTILS_H_
#define _UTILS_H_

/// @brief 
/// @param str 
/// @param sub 
/// @return 
char *strremove(char *str, const char *sub);

/// @brief 
/// @param num 
/// @param buf 
/// @return 
e_syserr_t uint_to_4digit_str(uint16_t num, char* buf);

/// @brief 
/// @param str 
/// @param num 
/// @return 
e_syserr_t str_to_4digit_uint(const char* str, uint16_t* num);

#endif