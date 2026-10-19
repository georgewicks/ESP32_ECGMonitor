"""
ISHNE Holter file parser (v1.0), corrected to match the published spec.

Header offsets verified against:
  - "The ISHNE Holter Standard Output File Format", Badilini, A.N.E. 1998
  - components/ECG-ISHNE/ISHNE.h (the matching C writer)

Layout:
  [0:8)    magic "ISHNE1.0"
  [8:10)   CRC-CCITT checksum over bytes [10 .. offset-ecg-block)
  [10:522) fixed 512-byte header
  [522:...) variable comment block (0 bytes in this writer)
  [offset-ecg-block:...) interleaved little-endian int16 samples, nleads per sample
"""

import struct

import numpy as np

MAGIC = b"ISHNE1.0"

# Fixed-header field offsets relative to the file start.
_OFF = {
    "var_block_size": 10,
    "sample_size": 14,
    "offset_var_block": 18,
    "offset_ecg_block": 22,
    "version": 26,
    "first_name": 28,
    "last_name": 68,
    "patient_id": 108,
    "sex": 128,
    "race": 130,
    "birth_date": 132,
    "record_date": 138,
    "file_date": 144,
    "start_time": 150,
    "nleads": 156,
    "lead_spec": 158,
    "lead_qual": 182,
    "resolution": 206,
    "pacemaker": 230,
    "recorder": 232,
    "sample_rate": 272,
    "proprietary": 274,
    "copyright": 354,
    "reserved": 434,
}

_LEAD_NAMES = {
    0: "Unknown", 1: "Generic", 2: "X", 3: "Y", 4: "Z",
    5: "I", 6: "II", 7: "III", 8: "aVR", 9: "aVL", 10: "aVF",
    11: "V1", 12: "V2", 13: "V3", 14: "V4", 15: "V5", 16: "V6",
    17: "ES", 18: "AS", 19: "AI",
}


def _crc_ccitt(buf):
    """CRC-CCITT (poly 0x1021, init 0xFFFF), as used by the ISHNE checksum."""
    crc = 0xFFFF
    for b in buf:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _cstr(data, offset, length):
    return data[offset:offset + length].split(b"\x00", 1)[0].decode("ascii", "replace")


class IshneParser:
    def __init__(self, file_path):
        self.file_path = file_path
        self.header = {}
        self.signals = None          # (n_samples, n_leads) float mV
        self.time_axis = None

    def parse(self):
        with open(self.file_path, "rb") as f:
            data = f.read()

        if data[:8] != MAGIC:
            raise ValueError("Not a valid ISHNE file (bad magic number).")

        # Structural fields first, so we know where the data starts and ends.
        sample_size = struct.unpack_from("<i", data, _OFF["sample_size"])[0]
        nleads = struct.unpack_from("<h", data, _OFF["nleads"])[0]
        offset_ecg = struct.unpack_from("<i", data, _OFF["offset_ecg_block"])[0]

        # Verify the checksum over the header block.
        stored_crc = struct.unpack_from("<H", data, 8)[0]
        computed = _crc_ccitt(data[10:offset_ecg])
        self.header["checksum_ok"] = (stored_crc == computed)
        if not self.header["checksum_ok"]:
            raise ValueError(
                f"Checksum mismatch: stored 0x{stored_crc:04X}, computed 0x{computed:04X}."
            )

        h = self.header
        h["version"] = struct.unpack_from("<h", data, _OFF["version"])[0]
        h["first_name"] = _cstr(data, _OFF["first_name"], 40)
        h["last_name"] = _cstr(data, _OFF["last_name"], 40)
        h["patient_id"] = _cstr(data, _OFF["patient_id"], 20)
        h["sex"] = struct.unpack_from("<h", data, _OFF["sex"])[0]
        h["race"] = struct.unpack_from("<h", data, _OFF["race"])[0]
        h["birth_date"] = struct.unpack_from("<hhh", data, _OFF["birth_date"])
        h["record_date"] = struct.unpack_from("<hhh", data, _OFF["record_date"])
        h["file_date"] = struct.unpack_from("<hhh", data, _OFF["file_date"])
        h["start_time"] = struct.unpack_from("<hhh", data, _OFF["start_time"])
        h["nleads"] = nleads
        h["sample_size"] = sample_size
        h["sample_rate"] = struct.unpack_from("<h", data, _OFF["sample_rate"])[0]
        h["recorder"] = _cstr(data, _OFF["recorder"], 40)
        h["pacemaker"] = struct.unpack_from("<h", data, _OFF["pacemaker"])[0]

        lead_spec = struct.unpack_from(f"<{12}h", data, _OFF["lead_spec"])
        resolution = struct.unpack_from(f"<{12}h", data, _OFF["resolution"])
        h["leads"] = [self._map_lead_name(c) for c in lead_spec[:nleads]]
        h["resolutions"] = list(resolution[:nleads])   # nV per LSB

        # Data block: interleaved little-endian int16.
        total = sample_size * nleads
        raw = np.frombuffer(data, dtype="<i2", count=total, offset=offset_ecg)
        self.signals = raw.reshape(-1, nleads).astype(np.float64)

        # Convert raw ADC units to millivolts: 1 LSB = resolution nV.
        for i, res_nv in enumerate(h["resolutions"]):
            self.signals[:, i] *= res_nv / 1_000_000.0

        self.time_axis = np.arange(self.signals.shape[0]) / h["sample_rate"]
        return self

    @staticmethod
    def _map_lead_name(code):
        return _LEAD_NAMES.get(code, f"Lead-{code}")

    def summary(self):
        h = self.header
        lines = [
            f"file:       {self.file_path}",
            f"subject:    {h['first_name']} {h['last_name']} ({h['patient_id']})",
            f"recorded:   {h['record_date'][2]}-{h['record_date'][1]:02d}-{h['record_date'][0]:02d} "
            f"{h['start_time'][0]:02d}:{h['start_time'][1]:02d}:{h['start_time'][2]:02d}",
            f"sample rate: {h['sample_rate']} Hz",
            f"leads:       {h['nleads']} -> {h['leads']}",
            f"resolutions: {h['resolutions']} nV/LSB",
            f"samples:     {h['sample_size']} per lead ({self.signals.shape[0] / h['sample_rate']:.1f} s)",
            f"checksum:    {'OK' if h['checksum_ok'] else 'FAIL'}",
        ]
        return "\n".join(lines)

    def plot(self, start_sec=0, duration_sec=5):
        import matplotlib.pyplot as plt   # lazy: parse() works without matplotlib

        if self.signals is None:
            print("No data parsed yet!")
            return

        fs = self.header["sample_rate"]
        a = int(start_sec * fs)
        b = min(int((start_sec + duration_sec) * fs), self.signals.shape[0])
        t = self.time_axis[a:b]
        sig = self.signals[a:b, :]
        n = self.header["nleads"]

        fig, axes = plt.subplots(n, 1, figsize=(12, 2 * n), sharex=True)
        if n == 1:
            axes = [axes]
        fig.suptitle(f"ECG Holter — {self.header['first_name']} {self.header['last_name']}",
                     fontsize=14)
        for i in range(n):
            axes[i].plot(t, sig[:, i], color="crimson", lw=0.8)
            axes[i].set_ylabel(f"{self.header['leads'][i]} (mV)")
            axes[i].grid(True, which="both", color="lightgray", linestyle="--")
        axes[-1].set_xlabel("Time (seconds)")
        plt.tight_layout()
        plt.show()


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <file.ecg>")
        sys.exit(1)
    p = IshneParser(sys.argv[1]).parse()
    print(p.summary())
