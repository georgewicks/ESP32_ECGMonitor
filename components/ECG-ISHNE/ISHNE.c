/**
 * @file ISHNE.c
 * @brief ISHNE writer implementation (see ISHNE.h).
 *
 * Pure C (stdio only) so it can be built and unit-tested on a host as well as
 * on the ESP32. The checksum algorithm and header layout are verified against
 * the reference file S1123.ecg.
 */

/* fileno() needs this before the first system header on POSIX hosts. */
#define _POSIX_C_SOURCE 200809L

#include "ISHNE.h"

#include <string.h>
#include <unistd.h>

#define ISHNE_HEADER_FILE_OFFSET (8 + 2)   /* magic + checksum               */
#define ISHNE_ECG_OFFSET         (ISHNE_HEADER_FILE_OFFSET + ISHNE_FIXED_HEADER_SIZE) /* 522 */

uint16_t ishne_crc_ccitt(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

void ishne_header_init(ishne_header_t *hdr)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->version = 1;
    for (int i = 0; i < ISHNE_MAX_LEADS; i++) {
        hdr->lead_spec[i]  = ISHNE_LEAD_ABSENT;
        hdr->lead_qual[i]  = ISHNE_LEAD_ABSENT;
        hdr->resolution[i] = 0;
    }
}

int ishne_open(ishne_writer_t *w, const char *path, const ishne_header_t *hdr)
{
    if (w == NULL || path == NULL || hdr == NULL) {
        return -1;
    }
    memset(w, 0, sizeof(*w));

    w->fp = fopen(path, "wb");
    if (w->fp == NULL) {
        return -2;
    }
    /* Full buffering: avoid a filesystem write per sample. */
    setvbuf(w->fp, NULL, _IOFBF, 8192);

    /* Copy the caller's header and enforce this implementation's layout:
     * no variable-length block, so both offsets are 522 and the ECG follows
     * the fixed header immediately. */
    w->hdr = *hdr;
    w->hdr.var_block_size   = 0;
    w->hdr.offset_var_block = ISHNE_ECG_OFFSET;
    w->hdr.offset_ecg_block = ISHNE_ECG_OFFSET;
    w->hdr.sample_size_ecg  = 0;   /* patched at close */
    w->nleads = (hdr->nleads > 0 && hdr->nleads <= ISHNE_MAX_LEADS) ? hdr->nleads : 0;

    /* magic (8 bytes) + zero checksum placeholder (2 bytes) */
    if (fwrite(ISHNE_MAGIC, 1, 8, w->fp) != 8) {
        fclose(w->fp);
        return -3;
    }
    uint16_t zero = 0;
    if (fwrite(&zero, sizeof(zero), 1, w->fp) != 1) {
        fclose(w->fp);
        return -4;
    }

    /* fixed 512-byte header */
    if (fwrite(&w->hdr, ISHNE_FIXED_HEADER_SIZE, 1, w->fp) != 1) {
        fclose(w->fp);
        return -5;
    }

    return 0;
}

int ishne_write(ishne_writer_t *w, const int16_t *interleaved, uint32_t nsamples)
{
    if (w == NULL || w->fp == NULL || (interleaved == NULL && nsamples > 0) ||
        w->nleads <= 0) {
        return -1;
    }

    size_t values = (size_t)nsamples * (size_t)w->nleads;
    if (fwrite(interleaved, sizeof(int16_t), values, w->fp) != values) {
        return -2;
    }
    w->sample_count += nsamples;
    return 0;
}

int ishne_close(ishne_writer_t *w)
{
    if (w == NULL || w->fp == NULL) {
        return -1;
    }

    /* 1. Commit the data and patch the final sample count into the header. */
    fflush(w->fp);
    w->hdr.sample_size_ecg = (int32_t)w->sample_count;

    /* 2. Rewrite the (now final) header, then the checksum over it. */
    if (fseek(w->fp, ISHNE_HEADER_FILE_OFFSET, SEEK_SET) != 0) {
        fclose(w->fp);
        return -2;
    }
    if (fwrite(&w->hdr, ISHNE_FIXED_HEADER_SIZE, 1, w->fp) != 1) {
        fclose(w->fp);
        return -3;
    }

    uint16_t crc = ishne_crc_ccitt((const uint8_t *)&w->hdr, ISHNE_FIXED_HEADER_SIZE);
    if (fseek(w->fp, 8, SEEK_SET) != 0) {
        fclose(w->fp);
        return -4;
    }
    if (fwrite(&crc, sizeof(crc), 1, w->fp) != 1) {
        fclose(w->fp);
        return -5;
    }

    /* 3. Flush everything, sync to media, close. */
    fflush(w->fp);
    fsync(fileno(w->fp));
    fclose(w->fp);
    w->fp = NULL;
    return 0;
}
