/**
 * @file ISHNE.c
 * @brief ISHNE writer implementation (see ISHNE.h).
 *
 * Pure C (stdio only) so it can be built and unit-tested on a host as well as
 * on the ESP32. The checksum algorithm and header layout are verified against
 * the reference file S1123.ecg.
 */

/* fileno()/usleep() need these before the first system header on POSIX hosts. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "ISHNE.h"

#include <string.h>
#include <unistd.h>

#define ISHNE_HEADER_FILE_OFFSET (8 + 2)   /* magic + checksum               */
#define ISHNE_ECG_OFFSET         (ISHNE_HEADER_FILE_OFFSET + ISHNE_FIXED_HEADER_SIZE) /* 522 */

/* Write-retry policy: a transient SD-card glitch (marginal wiring) can fail a
 * single fwrite and succeed moments later. Retry a few times before giving up. */
#define ISHNE_WRITE_RETRIES     (3)
#define ISHNE_CLOSE_RETRIES     (3)
#define ISHNE_RETRY_DELAY_US    (5000)   /* 5 ms between attempts */

/**
 * @brief fwrite that retries on a short/zero write.
 *
 * Advances past any bytes already written, so a retry never duplicates data.
 * @return 0 on success, -1 after exhausting retries.
 */
static int fwrite_retry(FILE *fp, const void *buf, size_t size, size_t count)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t remaining = size * count;
    int attempts = 0;

    while (remaining > 0) {
        size_t n = fwrite(p, 1, remaining, fp);
        if (n > 0) {
            p += n;
            remaining -= n;
            attempts = 0;
        } else {
            if (++attempts > ISHNE_WRITE_RETRIES) {
                return -1;
            }
            clearerr(fp);
            usleep(ISHNE_RETRY_DELAY_US);
        }
    }
    return 0;
}

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
    if (fwrite_retry(w->fp, ISHNE_MAGIC, 1, 8) != 0) {
        fclose(w->fp);
        return -3;
    }
    uint16_t zero = 0;
    if (fwrite_retry(w->fp, &zero, sizeof(zero), 1) != 0) {
        fclose(w->fp);
        return -4;
    }

    /* fixed 512-byte header */
    if (fwrite_retry(w->fp, &w->hdr, 1, ISHNE_FIXED_HEADER_SIZE) != 0) {
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
    if (fwrite_retry(w->fp, interleaved, sizeof(int16_t), values) != 0) {
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

    /* 2. Rewrite the (now final) header, then the checksum over it. Retry the
     * whole seek+write sequence a few times to survive a transient card glitch. */
    int rc = -1;
    for (int attempt = 0; attempt < ISHNE_CLOSE_RETRIES && rc != 0; attempt++) {
        if (fseek(w->fp, ISHNE_HEADER_FILE_OFFSET, SEEK_SET) != 0) {
            rc = -2;
        } else if (fwrite_retry(w->fp, &w->hdr, 1, ISHNE_FIXED_HEADER_SIZE) != 0) {
            rc = -3;
        } else {
            uint16_t crc = ishne_crc_ccitt((const uint8_t *)&w->hdr, ISHNE_FIXED_HEADER_SIZE);
            if (fseek(w->fp, 8, SEEK_SET) != 0) {
                rc = -4;
            } else if (fwrite_retry(w->fp, &crc, sizeof(crc), 1) != 0) {
                rc = -5;
            } else {
                rc = 0;
            }
        }
        if (rc != 0) {
            clearerr(w->fp);
            usleep(ISHNE_RETRY_DELAY_US);
        }
    }

    /* 3. Flush everything, sync to media, close. */
    fflush(w->fp);
    fsync(fileno(w->fp));
    fclose(w->fp);
    w->fp = NULL;
    return rc;
}
