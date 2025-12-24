#include <jescore.h>
#include "sdcard.h"
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"
#include "fsm.h"

static sdmmc_host_t host = SDSPI_HOST_DEFAULT();
static sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
static esp_vfs_fat_sdmmc_mount_config_t mount_config;
static sdmmc_card_t* card;
static uint8_t mounted = 0;
static spi_bus_config_t bus_cfg;
static SemaphoreHandle_t stream_lock;

e_syserr_t sd_init(int32_t max_files, uint32_t max_freq_khz){
    if(max_freq_khz > SDMMC_FREQ_52M) return e_syserr_param;
    if(max_freq_khz != 0){
        host.max_freq_khz = max_freq_khz;
    }
    else{
        host.max_freq_khz = SDCARD_MAX_FREQ_BUS_DEFAULT;
    }

    bus_cfg.mosi_io_num = PIN_SDSPI_MOSI;
    bus_cfg.miso_io_num = PIN_SDSPI_MISO;
    bus_cfg.sclk_io_num = PIN_SDSPI_SCK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    mount_config.format_if_mount_failed = false;
    mount_config.max_files = max_files;
    mount_config.allocation_unit_size = 16 * 1024;

    slot_config.gpio_cs = PIN_SDSPI_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return e_syserr_driver_fail;
    }
    stream_lock = xSemaphoreCreateMutex();
    jes_err_t je;
    je = jes_register_job(SDCARD_SERVER_JOB_NAME, 2*4096, 1, sd_job, 0);
    if(je != e_err_no_err) { jes_throw_error(je); return (e_syserr_t)je;}
    return e_syserr_none;
}

e_syserr_t sd_init_default(){
    return sd_init(SDCARD_MAX_FILES_DEFAULT, SDCARD_MAX_FREQ_BUS_DEFAULT);
}

e_syserr_t sd_mnt(){
    if (mounted) return e_syserr_none;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(
        SDCARD_BASE_PATH,
        &host,
        &slot_config,
        &mount_config,
        &card
    );
    if (ret != ESP_OK) return e_syserr_driver_fail;
    mounted = 1;
    return e_syserr_none;
}

e_syserr_t sd_unmnt(){
    if (!mounted) return e_syserr_none;
    esp_err_t stat = esp_vfs_fat_sdcard_unmount(SDCARD_BASE_PATH, card);
    if(stat != ESP_OK) return e_syserr_driver_fail;
    // stat = spi_bus_free((spi_host_device_t)host.slot);
    // if(stat != ESP_OK) return e_syserr_driver_fail;
    mounted = 0;
    return e_syserr_none;
}

uint8_t sd_is_mounted(void){
    return mounted;
}

e_syserr_t sd_get_free_kbytes(uint32_t* free_bytes, uint32_t* all_bytes){
    if (!mounted) return e_syserr_sdcard_unmnted;
    FATFS *fs;
    DWORD fre_clust;
    
    FRESULT res = f_getfree("0:", &fre_clust, &fs);
    if(res != FR_OK) return e_syserr_driver_fail;
    
    uint32_t bytes_per_cluster = fs->csize * card->csd.sector_size;
    *free_bytes = (uint32_t)fre_clust * bytes_per_cluster / 1024;
    *all_bytes = (uint32_t)(fs->n_fatent - 2) * bytes_per_cluster / 1024;
    return e_syserr_none;
}

e_syserr_t sd_create_file(const char* path) {
    if (!mounted) return e_syserr_sdcard_unmnted;
    FILE* file = fopen(path, "w");
    if (file == NULL) return e_syserr_file_generic;
    fclose(file);
    return e_syserr_none;
}

e_syserr_t sd_delete_file(const char* path) {
    if (!mounted) return e_syserr_sdcard_unmnted;
    if (remove(path) != 0) return e_syserr_file_generic;
    return e_syserr_none;
}

e_syserr_t sd_write(void* data, uint16_t type_size, uint32_t len, const char* fname, const char* mode, uint32_t pos, uint32_t* points_w) {
    if (!mounted) return e_syserr_sdcard_unmnted;
    FILE* f = fopen(fname, mode);
    if (f == NULL) return e_syserr_file_generic;
    if(fseek(f, pos, SEEK_SET) != 0){
        fclose(f);
        return e_syserr_file_generic;
    }
    *points_w = fwrite(data, type_size, len, f);
    if(*points_w != len) { 
        fclose(f);
        return e_syserr_oom; 
    }
    fclose(f);
    return e_syserr_none;
}

e_syserr_t sd_write_txt(char* data, uint32_t len, const char* fname, uint32_t pos, uint32_t* points_w){
    return sd_write((void*)data, sizeof(char), len-1, fname, "w", pos, points_w);
}

e_syserr_t sd_append(void* data, uint16_t type_size, uint32_t len, const char* fname, uint32_t* points_w){
    return sd_write(data, type_size, len, fname, "ab", 0, points_w);
}

e_syserr_t sd_append_txt(void* data, uint32_t len, const char* fname, uint32_t* points_w){
    return sd_write(data, 1, len-1, fname, "a", 0, points_w);
}

e_syserr_t sd_read(void* data, uint16_t type_size, uint32_t len, const char* fname, const char* mode, uint32_t pos, uint32_t* points_r) {
    if (!mounted) return e_syserr_sdcard_unmnted;
    FILE* f = fopen(fname, mode);
    if (f == NULL) return e_syserr_file_generic;
    if (pos > 0){
        if(fseek(f, pos, SEEK_SET) != 0){
            fclose(f);
            return e_syserr_file_generic;
        }
    }
    *points_r = fread(data, type_size, len, f);
    if(*points_r != len) {
        fclose(f);
        return e_syserr_file_eof; 
    }
    fclose(f);
    return e_syserr_none;
}

e_syserr_t sd_read_txt(char* data, uint32_t len, const char* fname, uint32_t pos, uint32_t* points_r){
    e_syserr_t e = sd_read(data, sizeof(char), len-1, fname, "r", pos, points_r);
    data[len-1] = '\0';
    return e;
}

e_syserr_t sd_stream_in(stereo_sample_t* data, uint32_t len, FILE* f, uint32_t* points_w){
    if (!mounted) return e_syserr_sdcard_unmnted;    // TODO: should this be checked every time?
    if (f == NULL) return e_syserr_file_generic;    // TODO: should this be checked every time?
    xSemaphoreTake(stream_lock, portMAX_DELAY);
    *points_w = fwrite(data, sizeof(stereo_sample_t), len, f);
    xSemaphoreGive(stream_lock);
    if(*points_w != len) { 
        return e_syserr_oom; 
    }
    return e_syserr_none;
}

e_syserr_t sd_stream_out(stereo_sample_t* data, uint32_t len, FILE* f, uint32_t* points_r){
    if (!mounted) return e_syserr_sdcard_unmnted;    // TODO: should this be checked every time?
    if (f == NULL) return e_syserr_file_generic;    // TODO: should this be checked every time?
    xSemaphoreTake(stream_lock, portMAX_DELAY);
    *points_r = fread(data, sizeof(stereo_sample_t), len, f);
    xSemaphoreGive(stream_lock);
    if(*points_r != len) { 
        return e_syserr_oom; 
    }
    return e_syserr_none;
}

FILE* sd_stream_open(const char* fname, const char* mode){
    if (!mounted) return NULL;
    FILE* f = fopen(fname, mode);
    return f;
}

FILE* sd_stream_read_open(const char* fname){
    if (!mounted) return NULL;
    FILE* f = fopen(fname, "rb");
    return f;
}

FILE* sd_stream_write_open(const char* fname){
    if (!mounted) return NULL;
    FILE* f = fopen(fname, "ab");
    return f;
}

void sd_stream_close(FILE* f){
    xSemaphoreTake(stream_lock, portMAX_DELAY);
    fclose(f);
    xSemaphoreGive(stream_lock);
}

e_syserr_t sd_ls(const char *dirname, char* pret, uint16_t len) {
    if (!mounted) return e_syserr_sdcard_unmnted;
    DIR *dir = opendir(dirname);
    if (!dir) {
        return e_syserr_file_generic;
    }
    struct dirent *entry;
    uint16_t i = 0;
    while ((entry = readdir(dir)) != NULL) {
        char* pr = entry->d_name;
        while (*pr != '\0') {
            if (i == len - 1) {
                closedir(dir);
                return e_syserr_oom;
            }
            pret[i++] = *pr++;
        }
        if (i == len - 1) {
            closedir(dir);
            return e_syserr_oom;
        }
        pret[i++] = '\n';
    }
    if (i < len) {
        pret[i] = '\0';
    } else {
        closedir(dir);
        return e_syserr_oom;
    }
    closedir(dir);
    strremove(pret, "System Volume Information\n");
    strremove(pret, ".Trash-1000\n");
    return e_syserr_none;
}


e_syserr_t sd_cat(const char *fname, char* pret, uint16_t len) {
    if (!mounted) return e_syserr_sdcard_unmnted;
    uint32_t points_r = 0;
    e_syserr_t e = sd_read_txt(pret,
                               len,
                               fname,
                               0,
                               &points_r);
    
    if(e == e_syserr_file_eof){
        // in case of e_syserr_file_eof, the file has less content than buf_size,
        // which is not an issue when calling this function. It either returns all content
        // of a small file with less than buf_size or buf_size characters.
        return e_syserr_none;
    }
    return e;
}

e_syserr_t sd_rm(const char* fname){
    if (!mounted) return e_syserr_sdcard_unmnted;
    if(!sd_file_exists(fname)){
        return e_syserr_file_missing;
    }
    return sd_delete_file(fname);
}

uint8_t sd_file_exists(const char *fname){
    if (!mounted) return 0;
    FILE* f = fopen(fname, "r");
    if(f == NULL) return 0;
    fclose(f);
    return 1;
}

e_syserr_t sd_get_unique_fname(char* proposed){
    if (!mounted) return e_syserr_sdcard_unmnted;
    uint8_t len = strlen(SDCARD_BASE_PATH "/" SDCARD_DEFAULT_FNAME_WAV);
    if(strlen(proposed) != len) return e_syserr_param;
    uint16_t idx = 0;
    e_syserr_t e = e_syserr_none;
    // the number in the fname is stored in index -5 to -8 -> fr2_rec_xxxx.wav
    // (see macro SDCARD_DEFAULT_FNAME_WAV)                        -> ^^^^ <-
    while(sd_file_exists(proposed)){                                            // keep looping until the given name does not exist
        char digits[5] = "xxxx";                                                // set up digit array
        memcpy(digits, &proposed[len - 8], 4);                                  // copy found digits into digits array
        if((e = str_to_4digit_uint(digits, &idx)) != e_syserr_none) return e;   // convert digit array to number and store in idx
        if((e = uint_to_4digit_str(++idx, digits)) != e_syserr_none) return e;  // convert the incremented idx to digits
        memcpy(&proposed[len - 8], digits, 4);                                  // put new digits in fname
    }
    return e;
}

void sd_job(void* p){
    char* args = jes_job_get_args();
    char* arg = strtok(args, " ");
    job_struct_t* pj = (job_struct_t*)p;
    e_syserr_t e;
    if(strcmp(arg, "mnt") == 0){
        if((e = sd_mnt()) != e_syserr_none){
            SCOPE_LOG_PJ(pj, "Unable to mount SD card!");
        }
        else{
            SCOPE_LOG_PJ(pj, "Mounted.");
        }
    }
    else if(strcmp(arg, "unmnt") == 0){
        if((e = sd_unmnt()) != e_syserr_none){
            SCOPE_LOG_PJ(pj, "Unable to unmount SD card!");
        }
        else{
            SCOPE_LOG_PJ(pj, "Unmounted.");
        }
    }
    else if(strcmp(arg, "ls") == 0){
        char* arg = strtok(NULL, " ");
        if(arg == NULL) { arg = (char*)"\0"; }
        char buf[SDCARD_PATH_MAX_CHAR];
        char ret[SDCARD_LS_MAX_CHAR];

        sprintf(buf, "%s/%s\0", SDCARD_BASE_PATH, arg);
        if((e = sd_ls(buf, ret, SDCARD_LS_MAX_CHAR)) != e_syserr_none){
            SCOPE_LOG_PJ(pj, "Error whlie listing files. (%d)", e);
            return;
        }
        uart_unif_write(ret);
    }
    else if(strcmp(arg, "cat") == 0){
        char* arg = strtok(NULL, " ");
        if(arg == NULL){
            SCOPE_LOG_PJ(pj, "cat error: specify a file to read.");
            return;
        }
        char buf[SDCARD_PATH_MAX_CHAR];
        char ret[SDCARD_CAT_MAX_CHAR];
        sprintf(buf, "%s/%s\0", SDCARD_BASE_PATH, arg);
        
        if((e = sd_cat(buf, ret, SDCARD_CAT_MAX_CHAR)) != e_syserr_none){
            SCOPE_LOG_PJ(pj, "Error whlie reading file. (%d)", e);
            return;
        }
        uart_unif_write(ret);
        
    }
    else if(strcmp(arg, "rm") == 0){
        char* arg = strtok(NULL, " ");
        if(arg == NULL){
            SCOPE_LOG_PJ(pj, "rm error: specify a file to delete.");
            return;
        }
        char buf[SDCARD_PATH_MAX_CHAR];
        sprintf(buf, "%s/%s\0", SDCARD_BASE_PATH, arg);
        if((e = sd_rm(buf)) != e_syserr_none){
            SCOPE_LOG_PJ(pj, "Error whlie deleting file. (%d)", e);
            return;
        }
    }
    else{
        SCOPE_LOG_PJ(pj, "Unknown SD command.");
    }
}