import struct
import numpy as np
import matplotlib.pyplot as plt

class IshneParser:
    def __init__(self, file_path):
        self.file_path = file_path
        self.header = {}
        self.signals = None
        self.time_axis = None

    def parse(self):
        with open(self.file_path, "rb") as f:
            # 1. Verify Magic Number
            magic = f.read(8).decode("ascii", errors="ignore")
            if "ISHNE" not in magic:
                raise ValueError("Not a valid ISHNE file.")

            # 2. Skip Checksum (2 bytes)
            f.seek(10)

            # 3. Read specific Fixed Header bounds (Example subsets parsed via struct)
            # ISHNE layout uses exact byte offsets. Let's pull core variables:
            f.seek(10 + 4)  # Skip block sizes to get sample count
            self.header['num_samples'] = struct.unpack("<I", f.read(4))[0]
            
            f.seek(10 + 142) # Jump to Subject Name block
            self.header['first_name'] = f.read(40).decode("ascii").strip("\x00")
            self.header['last_name'] = f.read(40).decode("ascii").strip("\x00")
            
            f.seek(10 + 234) # Jump to Sampling and Lead Info
            self.header['sample_rate'] = struct.unpack("<H", f.read(2))[0]
            self.header['num_leads'] = struct.unpack("<H", f.read(2))[0]

            # Read Lead Specifications (up to 12 leads, 2 bytes each)
            lead_specs = struct.unpack(f"<{self.header['num_leads']}H", f.read(2 * self.header['num_leads']))
            self.header['leads'] = [self._map_lead_name(code) for code in lead_specs]

            # Read Resolutions (nV per ADC unit, 4 bytes each)
            f.seek(10 + 286)
            self.header['resolutions'] = struct.unpack(f"<{self.header['num_leads']}I", f.read(4 * self.header['num_leads']))

            # 4. Jump past the 512-byte header + variable block to read Data Block
            f.seek(10 + 512) 
            
            # Read all 16-bit signed integer samples
            raw_data = np.fromfile(f, dtype=np.int16)
            
            # Reshape data into an array: (Samples, Leads)
            total_expected = self.header['num_samples'] * self.header['num_leads']
            raw_data = raw_data[:total_expected] # Truncate if padding exists
            self.signals = raw_data.reshape(-1, self.header['num_leads']).astype(np.float32)

            # Convert raw ADC steps to physical Millivolts (mV)
            # nV to mV requires dividing by 1,000,000
            for i in range(self.header['num_leads']):
                res_mv = self.header['resolutions'][i] / 1000000.0
                self.signals[:, i] *= res_mv

            # Generate Time Axis in Seconds
            self.time_axis = np.arange(len(self.signals)) / self.header['sample_rate']

    def _map_lead_name(self, code):
        # ISHNE standard mappings (1=Generic, 5-10 = I-aVF, 11-16 = V1-V6)
        mapping = {0: "Unknown", 1: "Generic", 5: "I", 6: "II", 7: "III", 
                   11: "V1", 12: "V2", 13: "V3", 14: "V4", 15: "V5", 16: "V6"}
        return mapping.get(code, f"Lead-{code}")

    def plot(self, start_sec=0, duration_sec=5):
        """ Plots a snippet of the parsed ECG data """
        if self.signals is None:
            print("No data parsed yet!")
            return

        # Calculate sample slices
        fs = self.header['sample_rate']
        start_idx = int(start_sec * fs)
        end_idx = int((start_sec + duration_sec) * fs)

        t_slice = self.time_axis[start_idx:end_idx]
        sig_slice = self.signals[start_idx:end_idx, :]

        # Create stacked subplots for each lead
        num_leads = self.header['num_leads']
        fig, axes = plt.subplots(num_leads, 1, figsize=(12, 2 * num_leads), sharex=True)
        if num_leads == 1: axes = [axes]

        fig.suptitle(f"ECG Holter - {self.header['first_name']} {self.header['last_name']}", fontsize=14)

        for i in range(num_leads):
            axes[i].plot(t_slice, sig_slice[:, i], color='crimson', lw=1)
            axes[i].set_ylabel(f"{self.header['leads'][i]} (mV)")
            axes[i].grid(True, which='both', color='lightgray', linestyle='--')
            
        axes[-1].set_xlabel("Time (seconds)")
        plt.tight_layout()
        plt.show()

# --- Execution Example ---
# parser = IshneParser("patient_data.ecg")
# parser.parse()
# parser.plot(start_sec=60, duration_sec=10) # Plot 10 seconds starting at minute 1
