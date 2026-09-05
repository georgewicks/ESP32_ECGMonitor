/**
 * @file ISHNE.h
 * @author your name (you@domain.com)
 * @brief  standard structure
 * @version 0.1
 * @date 2026-09-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdint.h>

#pragma pack(push, 1) // Ensures tight byte packing without compiler padding
typedef struct {
    char     magic_number[8];    // Must be "ISHNE1.0"
    uint16_t checksum;           // 16-bit CRC of the file
    
    // Patient Information
    char     patient_id[40];     // Patient identification code
    char     last_name[40];      // Patient last name
    char     first_name[40];     // Patient first name
    
    // Acquisition Data
    uint8_t  day;                // Date of recording
    uint8_t  month;
    uint16_t year;
    uint8_t  hour;               // Start time of recording
    uint8_t  minute;
    uint8_t  second;
    
    // Signal Specification
    uint32_t sample_rate;        // Sampling rate in Hz
    uint16_t total_leads;        // Number of ECG leads recorded
    uint16_t resolution[12];     // Resolution per lead in nanovolts/bit
    
    // Formatting & Lengths
    uint32_t total_samples;      // Number of samples per lead
    uint16_t proprietary_len;    // Length of custom proprietary block (bytes)
    uint32_t comment_len;        // Length of comment text block (bytes)
} IshneEcgHeader;
#pragma pack(pop)
