/**
 * @file ISHNE.h
 * @brief ISHNE Holter Standard Output File Format (v1.0) — writer.
 *
 * Layout verified against the published spec ("The ISHNE Holter Standard
 * Output File Format", Badilini, A.N.E. 1998) and against the reference
 * recording docs/ECG-File-examples/10-minute-Holter-style-ISCHNE/S1123/S1123.ecg:
 *
 *   offset  size  field
 *     0       8   magic number "ISHNE1.0"
 *     8       2   CRC-CCITT checksum (poly 0x1021, init 0xFFFF), little-endian,
 *                 computed over the header bytes [10 .. offset-ECG-block)
 *    10     512   fixed-length header (ishne_header_t)
 *   522     var   variable-length comment block (0 bytes in this implementation)
 *   522+var ...   ECG data: interleaved little-endian int16, nleads values per sample
 *
 * A single file = one recording. Open, stream samples, close (close patches the
 * sample count and checksum into the header).
 */

#pragma once

#include <stdint.h>
#include <stdio.h>

#define ISHNE_MAGIC              "ISHNE1.0"
#define ISHNE_FIXED_HEADER_SIZE  (512)
#define ISHNE_MAX_LEADS          (12)
#define ISHNE_LEAD_ABSENT        (-9)

/* Lead specification codes (spec Table 1). */
enum {
    ISHNE_LEAD_UNKNOWN    = 0,
    ISHNE_LEAD_GENERIC    = 1,
    ISHNE_LEAD_X          = 2,
    ISHNE_LEAD_Y          = 3,
    ISHNE_LEAD_Z          = 4,
    ISHNE_LEAD_I          = 5,
    ISHNE_LEAD_II         = 6,
    ISHNE_LEAD_III        = 7,
    ISHNE_LEAD_AVR        = 8,
    ISHNE_LEAD_AVL        = 9,
    ISHNE_LEAD_AVF        = 10,
    ISHNE_LEAD_V1         = 11,
    ISHNE_LEAD_V2         = 12,
    ISHNE_LEAD_V3         = 13,
    ISHNE_LEAD_V4         = 14,
    ISHNE_LEAD_V5         = 15,
    ISHNE_LEAD_V6         = 16,
};

/* Subject sex / race codes. */
enum {
    ISHNE_SEX_UNKNOWN     = 0,
    ISHNE_SEX_MALE        = 1,
    ISHNE_SEX_FEMALE      = 2,

    ISHNE_RACE_UNKNOWN    = 0,
    ISHNE_RACE_CAUCASIAN  = 1,
    ISHNE_RACE_BLACK      = 2,
    ISHNE_RACE_ORIENTAL   = 3,
};

#pragma pack(push, 1)
typedef struct {
    int32_t var_block_size;      /* size in bytes of variable-length block     */
    int32_t sample_size_ecg;     /* samples per lead (patched at close)        */
    int32_t offset_var_block;    /* byte offset of variable block (=522)       */
    int32_t offset_ecg_block;    /* byte offset of ECG block (=522)            */
    int16_t version;             /* file version (=1)                          */

    char    first_name[40];
    char    last_name[40];
    char    patient_id[20];

    int16_t sex;                 /* 0 unknown, 1 male, 2 female               */
    int16_t race;                /* 0 unknown, 1 caucasian, 2 black, 3 oriental */
    int16_t birth_date[3];       /* day, month, year (use -9 for unknown)      */
    int16_t record_date[3];      /* day, month, year of the recording          */
    int16_t file_date[3];        /* day, month, year the file was created      */
    int16_t start_time[3];       /* hour, minute, second                       */

    int16_t nleads;              /* number of leads actually stored            */
    int16_t lead_spec[ISHNE_MAX_LEADS];   /* lead codes (see enum); -9 = absent */
    int16_t lead_qual[ISHNE_MAX_LEADS];   /* lead quality; 1 = present/normal   */
    int16_t resolution[ISHNE_MAX_LEADS];  /* amplitude resolution, nV per LSB   */

    int16_t pacemaker;           /* 0 none, else see spec                       */
    char    recorder[40];

    int16_t sample_rate;         /* Hz                                          */
    char    proprietary[80];
    char    copyright[80];
    char    reserved[88];
} ishne_header_t;
#pragma pack(pop)

/* Sanity: the fixed header must be exactly 512 bytes. */
_Static_assert(sizeof(ishne_header_t) == ISHNE_FIXED_HEADER_SIZE,
               "ishne_header_t must be 512 bytes");

/** Streaming writer state (one per open file). */
typedef struct {
    FILE          *fp;
    int            nleads;
    uint32_t       sample_count;   /* samples per lead written so far          */
    ishne_header_t hdr;            /* working header (patched on close)        */
} ishne_writer_t;

/** Zero the header and set version + "lead absent" defaults. */
void ishne_header_init(ishne_header_t *hdr);

/**
 * @brief Create a new ISHNE file and write the magic + header.
 *
 * @param w     writer state (uninitialized is fine)
 * @param path  absolute output path
 * @param hdr   caller-filled header (names, dates, leads, resolution, rate).
 *              Structural fields (offsets, sample count) are set here.
 * @return 0 on success, negative on failure.
 */
int ishne_open(ishne_writer_t *w, const char *path, const ishne_header_t *hdr);

/**
 * @brief Append @p nsamples complete samples (interleaved nleads x int16).
 *
 * @param interleaved  buffer of nsamples * nleads little-endian int16 values,
 *                     in lead order (lead[0] first within each sample).
 * @return 0 on success, negative on failure.
 */
int ishne_write(ishne_writer_t *w, const int16_t *interleaved, uint32_t nsamples);

/**
 * @brief Patch the sample count and checksum into the header and close.
 * @return 0 on success, negative on failure.
 */
int ishne_close(ishne_writer_t *w);

/** CRC-CCITT (poly 0x1021, init 0xFFFF) used for the header checksum. */
uint16_t ishne_crc_ccitt(const uint8_t *buf, size_t len);
