#ifndef _SYSERR_H_
#define _SYSERR_H_

#include <jes_err.h>

#define FR1_DEBUG_MSG_FATAL "[FTAL] "
#define FR1_DEBUG_MSG_WARN  "[WARN] "
#define FR1_DEBUG_MSG_INFO  "[INFO] "

/*
This error enum is intended to work with the established jescore
errors. jescore errors can be cast into sys-errors, completing
a fully defined set of errors for the project.
*/
typedef enum{
    e_syserr_none = e_err_NUM_ERR, // no err, all fine
    e_syserr_null,          // pointer is NULL but shouldn't be
    e_syserr_zero,          // value is zero but shouldn't be
    e_syserr_oom,           // out of memory
    e_syserr_uninitialized, // an init operation was not called
    e_syserr_param,         // illegal parameter
    e_syserr_locked,        // resource is locked at access time
    e_syserr_prohibited,    // action is logically forbidden
    e_syserr_driver_fail,   // underlying driver code failed (ESP-IDF) 
    e_syserr_sdcard_unmnted,// SD card is not mounted
    e_syserr_file_generic,  // generic error with a part of the file IO
    e_syserr_file_missing,  // targeted file does not exist
    e_syserr_file_duplicate,// targeted file already exists
    e_syserr_file_eof,      // EOF
    e_syserr_too_long,      // string is too long for a buffer
    e_syserr_audio_dead     // Audio loop died during runtime.    
}e_syserr_t;

#endif // _SYSERR_H_