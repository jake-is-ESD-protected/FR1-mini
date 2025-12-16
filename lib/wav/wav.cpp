#include "jescore.h"
#include "audio.h"
#include "wav.h"
#include "sdcard.h"
#include "syserr.h"
#include <string.h>
#include "uart_unif.h"

wav_hdr_t wav_create_header(uint16_t numChannels, uint32_t sampleRate, uint16_t bitsPerSample) {
    wav_hdr_t header;
    memset(&header, 0, sizeof(header));

    // RIFF header
    memcpy(header.chunkID, "RIFF", 4);
    memcpy(header.format, "WAVE", 4);

    // Format chunk
    memcpy(header.subchunk1ID, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1; // PCM
    header.numChannels = numChannels;
    header.sampleRate = sampleRate;
    header.bitsPerSample = bitsPerSample;
    header.byteRate = sampleRate * numChannels * bitsPerSample / 8;
    header.blockAlign = numChannels * bitsPerSample / 8;

    // Data chunk
    memcpy(header.subchunk2ID, "data", 4);

    return header;
}

e_syserr_t wav_open_for_write(wav_file_t* wav, const char* filename, uint16_t numChannels, uint32_t sampleRate, uint16_t bitsPerSample) {
    if (wav == NULL || filename == NULL) {
        return e_syserr_param;
    }

    // Create the header
    wav->header = wav_create_header(numChannels, sampleRate, bitsPerSample);
    wav->samples_transfered = 0;
    strncpy(wav->filename, filename, sizeof(wav->filename) - 1);
    wav->filename[sizeof(wav->filename) - 1] = '\0';

    // Create and open the file
    e_syserr_t e = sd_create_file(filename);
    if (e != e_syserr_none) {
        return e;
    }

    // Write the initial header
    uint32_t points_written;
    e = sd_write(&wav->header, sizeof(wav_hdr_t), 1, filename, "wb", 0, &points_written);
    if (e != e_syserr_none || points_written != 1) {
        return e_syserr_file_generic;
    }

    // Open the file for appending data
    wav->file = sd_stream_write_open(filename);
    if (wav->file == NULL) {
        return e_syserr_file_generic;
    }
    return e_syserr_none;
}

e_syserr_t wav_open_for_read(wav_file_t* wav, const char* filename){
    if (filename == NULL || strlen(filename) > __WAV_FN_LEN) {
        return e_syserr_param;
    }

    // set parameters to struct
    wav->samples_transfered = 0;
    strncpy(wav->filename, filename, sizeof(wav->filename) - 1);
    
    // read into struct from saved file
    uint32_t points_read;
    e_syserr_t e = sd_read(&wav->header, sizeof(wav_hdr_t), 1, filename, "rb", 0, &points_read);
    if (e != e_syserr_none || points_read != 1) {
        return e_syserr_file_generic;
    }
    vTaskDelay(5 / portTICK_PERIOD_MS); /// TODO: Without this delay, the UT crashes (?)
    // Open the file for reading data
    wav->file = sd_stream_read_open(filename);
    if (wav->file == NULL) {
        return e_syserr_file_generic;
    }
    fseek(wav->file, WAV_HEADER_SIZE_TOTAL, SEEK_SET); // jump over header
    return e_syserr_none;
}

e_syserr_t wav_write_samples(wav_file_t* wav, const void* samples, uint32_t sample_count) {
    if (wav == NULL || samples == NULL || wav->file == NULL) {
        return e_syserr_param;
    }
    uint32_t points_written;
    e_syserr_t e = sd_stream_in((stereo_sample_t*)samples, sample_count, wav->file, &points_written);
    if (e != e_syserr_none) {
        return e;
    }
    if(points_written != sample_count) {
        return e_syserr_file_generic;
    }
    wav->samples_transfered += points_written;
    return e_syserr_none;
}

e_syserr_t wav_read_samples(wav_file_t* wav, const void* samples, uint32_t sample_count){
    if (wav == NULL || samples == NULL || wav->file == NULL) {
        return e_syserr_param;
    }
    uint32_t points_read = 0;
    e_syserr_t e = sd_stream_out((stereo_sample_t*)samples, sample_count, wav->file, &points_read);
    if (e != e_syserr_none) {
        return e;
    }
    if(points_read != sample_count) {
        return e_syserr_file_generic;
    }
    wav->samples_transfered += points_read;
    return e_syserr_none;
}

e_syserr_t wav_update_header(wav_file_t* wav) {
    if (wav == NULL) {
        return e_syserr_param;
    }

    // Calculate data size in bytes
    uint32_t data_size = wav->samples_transfered * wav->header.numChannels * (wav->header.bitsPerSample / 8);

    // Update header fields
    wav->header.chunkSize = 36 + data_size;
    wav->header.subchunk2Size = data_size;

    // Write the updated header
    uint32_t points_written;
    e_syserr_t e = sd_write(&wav->header, sizeof(wav->header), 1, wav->filename, "r+b", 0, &points_written);
    if (e != e_syserr_none || points_written != 1) {
        return e != e_syserr_none ? e : e_syserr_file_generic;
    }

    return e_syserr_none;
}

e_syserr_t wav_close_for_write(wav_file_t* wav) {
    if (wav == NULL) {
        return e_syserr_param;
    }
    if (wav->file == NULL){
        return e_syserr_null;
    }
    sd_stream_close(wav->file);
    e_syserr_t e = wav_update_header(wav);
    wav->file = NULL;
    return e;
}

e_syserr_t wav_close_for_read(wav_file_t* wav) {
    if (wav == NULL) {
        return e_syserr_param;
    }
    if (wav->file == NULL){
        return e_syserr_null;
    }
    sd_stream_close(wav->file);
    wav->file = NULL;
    return e_syserr_none;
}