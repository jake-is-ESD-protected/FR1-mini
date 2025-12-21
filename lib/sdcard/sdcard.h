/// @file sdcard.h
/// @brief
/*
Combined abstraction for ESP32-SDMMC driver code and file-IO.
Provides functionalites to mount, unmount, create, delete,
write to, read from, append to *files* on the SD card.

Additionally, a jescore compatible SD card job can be invoked
by registering the function `sd_job` as a job.
*/
/// @author jake-is-ESD-protected. jesdev.io

#ifndef _SDCARD_H_
#define _SDCARD_H_

#include "esp_err.h"
#include "syserr.h"
#include "audio.h"
#include "sdmmc_cmd.h"

#define SDCARD_BASE_PATH            "/sdcard"
#define SDCARD_PAGE_SIZE_BYTE       512
#define SDCARD_SERVER_JOB_NAME      "sdcard"
#define SDCARD_MAX_FILES_DEFAULT    5
#define SDCARD_MAX_FREQ_BUS_DEFAULT SDMMC_FREQ_52M // SDMMC_FREQ_DEFAULT

#define PIN_SDSPI_CS     (gpio_num_t)13
#define PIN_SDSPI_MOSI   (gpio_num_t)15
#define PIN_SDSPI_MISO   (gpio_num_t)2
#define PIN_SDSPI_SCK    (gpio_num_t)14

#define SDCARD_LS_MAX_CHAR      256
#define SDCARD_CAT_MAX_CHAR     256
#define SDCARD_PATH_MAX_CHAR    64

#define SDCARD_DEFAULT_FNAME_WAV    "fr1_rec_0000.wav"

/// @deprecated
/// @enum SD card control commands.
typedef enum {
    sd_cmd_act_none,
    sd_cmd_mnt,
    sd_cmd_unmnt,
    sd_cmd_write_chunk,
    sd_cmd_read_chunk,
    NUM_SD_ACTIONS
} sd_cmd_t;

/// @brief Initialize the SDMMC config struct.
/// @param max_files Max amount of files in FS.
/// @param max_freq_khz Max bus transfer speed. Pass 0 to use the default value.
/// @return FR1 error code. Either returns `e_syserr_none` or `e_syserr_param`.
/// @note Does not perform calls to ESP-IDF functions, only sets a struct.
e_syserr_t sd_init(int32_t max_files, uint32_t max_freq_khz);

/// @brief Initialize the SDMMC config struct.
/// @return FR1 error code. Either returns `e_syserr_none` or `e_syserr_param`.
/// @note Is part of the common signature interface for the init routine.
e_syserr_t sd_init_default(void);

/// @brief Mount the SD card.
/// @return FR1 error code. 
/// @note Immideatly returns with `e_syserr_none` if already mounted. Calls `esp_vfs_fat_sdmmc_mount`.
e_syserr_t sd_mnt(void);

/// @brief Unmount the SD card.
/// @return FR1 error code.
/// @note Immideatly returns with `e_syserr_none` if already unmounted. Calls `esp_vfs_fat_sdmmc_unmount`.
e_syserr_t sd_unmnt(void);

/// @brief Checks the mounting state of the SD card.
/// @return Mounting state expressed as 0 (umounted) and 1 (mounted).
uint8_t sd_is_mounted(void);

/// @brief Get the number of free and total kilobytes in the FS.
/// @param free_kbytes Pointer to variable to hold the free kbyte number.
/// @param all_kbytes Pointer to variable to hold the total kbyte number.
/// @return FR1 error code.
e_syserr_t sd_get_free_kbytes(uint32_t* free_kbytes, uint32_t* all_kbytes);

/// @brief Create a file on the FS.
/// @param path Absolute path to a file.
/// @return FR1 error code.
/// @note The absolute path has to be given with `SDCARD_BASE_PATH` as base.
e_syserr_t sd_create_file(const char* path);

/// @brief Delete file on the FS.
/// @param path Absolute path to a file.
/// @return FR1 error code. Only returns `e_syserr_file_generic` in case of file-IO errors.
e_syserr_t sd_delete_file(const char* path);

/// @brief Write content to a file.
/// @param data Pointer to content to be written.
/// @param type_size Size of data type in byte.
/// @param len Length of data (not in byte).
/// @param fname Name of the file. File is created if it does not exist.
/// @param mode IO mode. Text and binary forms supported (w, wb)
/// @param pos Position to start writing from. Uses `fseek`. This parameter is ignored when using mode "a"/"ab".
/// @param  points_w Amount of data points that were actually written to the file.
/// @return FR1 error code.
e_syserr_t sd_write(void* data, uint16_t type_size, uint32_t len, const char* fname, const char* mode, uint32_t pos, uint32_t*  points_w);

/// @brief Write text (char*) to a file.
/// @param data Pointer to c-string.
/// @param len Length of string. This must include the trailing "\0"!
/// @param fname Name of the file. File is created if it does not exist.
/// @param pos IO mode. Text forms supported (r, w, a).
/// @param  points_w Amount of chars that were actually written to the file. For text, this will usually be len-1, because the trailing "\0" is not written.
/// @return FR1 error code.
e_syserr_t sd_write_txt(char* data, uint32_t len, const char* fname, uint32_t pos, uint32_t*  points_w);

/// @brief Append content to a file.
/// @param data Pointer to content to be appended.
/// @param type_size Size of data type in byte.
/// @param len Length of data (not in byte).
/// @param fname Name of the file. Has to exist.
/// @param  points_w Amount of bytes that were actually written to the file.
/// @return FR1 error code.
e_syserr_t sd_append(void* data, uint16_t type_size, uint32_t len, const char* fname, uint32_t*  points_w);

/// @brief Append text to a file.
/// @param data Pointer to c-string to be appended.
/// @param len Length of string. This must include the trailing "\0"!
/// @param fname Name of the file. Has to exist.
/// @param  points_w Amount of chars that were actually written to the file. For text, this will usually be len-1, because the trailing "\0" is not written.
/// @return FR1 error code.
e_syserr_t sd_append_txt(void* data, uint32_t len, const char* fname, uint32_t*  points_w);

/// @brief Read content from a file.
/// @param data Pointer to empty data array.
/// @param type_size Size of data type in byte.
/// @param len Length of data (not in byte).
/// @param fname Name of the file. Has to exist.
/// @param mode IO mode. Text and binary forms supported (r, rb)
/// @param pos Position to start reading from. Uses `fseek`.
/// @param  points_r Amount of data points that were actually read from the file.
/// @return FR1 error code.
e_syserr_t sd_read(void* data, uint16_t type_size, uint32_t len, const char* fname, const char* mode, uint32_t pos, uint32_t*  points_r);

/// @brief Read text from a file.
/// @param data Pointer to empty char array.
/// @param len Length of char array including the trailing "\0".
/// @param fname Name of the file. Has to exist.
/// @param pos Position to start reading from. Uses `fseek`.
/// @param  points_r Amount of chars that were actually read from the file.
/// @return FR1 error code.
e_syserr_t sd_read_txt(char* data, uint32_t len, const char* fname, uint32_t pos, uint32_t*  points_r);

/// @brief Write to SD with an open file pointer supplied from outside.
/// @param data Pointer to empty audio data array.
/// @param len Length of data (not in byte).
/// @param f Already opened file pointer.
/// @param points_w Amount of data points that were actually written to the file.
/// @return FR1 error code.
/// @note The file pointer **needs** to be opened in 'ab' mode, otherwise data will be destroyed.
/// Additionally, should an error occur, the function **does not** close the file. Close it from outside!
e_syserr_t sd_stream_in(stereo_sample_t* data, uint32_t len, FILE* f, uint32_t* points_w);

/// @brief Write to SD with an open file pointer supplied from outside.
/// @param data Pointer to audio to be written.
/// @param len Length of data (not in byte).
/// @param f Already opened file pointer.
/// @param points_r Amount of data points that were actually read from the file.
/// @return FR1 error code.
e_syserr_t sd_stream_out(stereo_sample_t* data, uint32_t len, FILE* f, uint32_t* points_r);

/// @brief Open file stream indefinetely for out-of-scope operations.
/// @param fname Name of the file. Has to exist.
/// @param mode IO mode. Use either "ab" or "rb" fo write or read streams
/// @return FILE pointer to opened file.
/// @note Needs to be closed in seperate call with `sd_stream_close()`.
FILE* sd_stream_open(const char* fname, const char* mode);

/// @brief Open file stream indefinetely for out-of-scope reading.
/// @param fname Name of the file. Has to exist.
/// @return FILE pointer to opened file.
/// @note Needs to be closed in seperate call with `sd_stream_close()`.
/// Acts as wrapper for `sd_stream_open` in "rb" mode.
FILE* sd_stream_read_open(const char* fname);

/// @brief Open file stream indefinetely for out-of-scope writing.
/// @param fname Name of the file. Has to exist.
/// @return FILE pointer to opened file.
/// @note Needs to be closed in seperate call with `sd_stream_close()`.
/// Acts as wrapper for `sd_stream_open` in "ab" mode.
FILE* sd_stream_write_open(const char* fname);

/// @brief Close an opened file stream.
/// @param f FILE pointer to opened file.
void sd_stream_close(FILE* f);

/// @brief List all files of a folder.
/// @param dirname Path to directory which contains entries to be listed.
/// @param pret Pointer to empty char array.
/// @param len Length of char array including the trailing "\0".
/// @return FR1 error code.
/// @note Single entries are delimited with a "\t" for easy printing.
e_syserr_t sd_ls(const char *dirname, char* pret, uint16_t len);

/// @brief List content of a file.
/// @param fname Name of the file. Has to exist.
/// @param pret Pointer to empty char array.
/// @param len Length of char array including the trailing "\0".
/// @return FR1 error code.
/// @note If the buffer is not large enough for all of the content in the file, only the initial values in the buffer will be shown (opposite of tail).
e_syserr_t sd_cat(const char *fname, char* pret, uint16_t len);

/// @brief Check wether a file exists or not.
/// @param fname Path to file.
/// @return 0 if not existing, 1 if existing.
/// @note Also returns 0 if the SD card is not mounted!
uint8_t sd_file_exists(const char *fname);

/// @brief Get a unique filename that does not yet exist in the FS.
/// @param proposed Proposed name of form "fr1_rec_xxxx.wav". Will be written to.
/// @return FR1 error code.
e_syserr_t sd_get_unique_fname(char* proposed);

/// @brief jescore CLI handler for the "sdcard" subcommand
/// @param p jescore job struct, set from outside.
/// @note This function only performs CLI responses and should not be called by user code.
void sd_job(void* p);

#endif // _SDCARD_H_