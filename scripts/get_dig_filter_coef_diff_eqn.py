import sympy as sp
import re
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

def extract_filter_coefficients(equation_str):
    """Parses text equation and extracts standard b and a filter arrays."""
    # 1. Standardize text format
    eq_clean = equation_str.replace('=', '-(') + ')'
    eq_clean = re.sub(r'([xy])\s*[\(\[]\s*n(?:T)?\s*-\s*(\d+)(?:T)?\s*[\)\]]', r'\1_\2', eq_clean)
    eq_clean = re.sub(r'([xy])\s*[\(\[]\s*n(?:T)?\s*[\)\]]', r'\1_0', eq_clean)
    
    # 2. Extract with SymPy
    var_names = set(re.findall(r'[xy]_\d+', eq_clean))
    symbols = {name: sp.Symbol(name) for name in var_names}
    expr = sp.sympify(eq_clean, locals=symbols)
    expr = sp.expand(expr)
    
    x_dict, y_dict = {}, {}
    for name, sym in symbols.items():
        coeff = expr.coeff(sym)
        signal_type, delay = name.split('_')
        delay = int(delay)
        
        if signal_type == 'x':
            x_dict[delay] = -float(coeff)
        elif signal_type == 'y':
            y_dict[delay] = float(coeff)
            
    # 3. Fill missing delays with zeros
    max_b = max(x_dict.keys()) if x_dict else 0
    max_a = max(y_dict.keys()) if y_dict else 0
    
    b = [x_dict.get(i, 0.0) for i in range(max_b + 1)]
    a = [y_dict.get(i, 0.0) for i in range(max_a + 1)]
    
    # Normalize by a_0
    if a and a[0] != 0:
        a_0 = a[0]
        b = [x / a_0 for x in b]
        a = [x / a_0 for x in a]
        
    return b, a

def visualize_filter(b, a):
    """Generates Frequency Response and Pole-Zero plots for the filter coefficients."""
    # Compute Frequency Response
    w, h = signal.freqz(b, a, worN=2000)
    
    # Compute Poles and Zeros
    zeros, poles, k = signal.tf2zpk(b, a)
    
    # Create Layout (2 rows, 2 columns; bottom right stays empty or combines info)
    fig = plt.figure(figsize=(12, 10))
    
    # 1. Magnitude Response Plot
    ax1 = fig.add_subplot(2, 2, 1)
    ax1.plot(w / np.pi, 20 * np.log10(np.abs(h) + 1e-12), 'b')
    ax1.set_title("Magnitude Response")
    ax1.set_xlabel("Normalized Frequency (×π rad/sample)")
    ax1.set_ylabel("Amplitude (dB)")
    ax1.grid(True)
    
    # 2. Phase Response Plot
    ax2 = fig.add_subplot(2, 2, 2)
    ax2.plot(w / np.pi, np.angle(h), 'g')
    ax2.set_title("Phase Response")
    ax2.set_xlabel("Normalized Frequency (×π rad/sample)")
    ax2.set_ylabel("Phase (radians)")
    ax2.grid(True)
    
    # 3. Pole-Zero Plot
    ax3 = fig.add_subplot(2, 2, 3)
    # Draw Unit Circle
    uc = plt.Circle((0,0), radius=1, color='gray', fill=False, linestyle='--')
    ax3.add_patch(uc)
    # Plot Zeros (o) and Poles (x)
    ax3.plot(np.real(zeros), np.imag(zeros), 'go', label='Zeros', markersize=8, fillstyle='none')
    ax3.plot(np.real(poles), np.imag(poles), 'rx', label='Poles', markersize=8)
    
    # Check Stability
    is_stable = all(np.abs(p) < 1 for p in poles) if len(poles) > 0 else True
    stability_text = "Stable" if is_stable else "UNSTABLE"
    
    ax3.set_title(f"Pole-Zero Map ({stability_text})")
    ax3.set_xlabel("Real Part")
    ax3.set_ylabel("Imaginary Part")
    ax3.grid(True)
    ax3.axhline(0, color='black', lw=0.5)
    ax3.axvline(0, color='black', lw=0.5)
    ax3.axis('equal')
    ax3.legend()
    
    # 4. Info Panel
    ax4 = fig.add_subplot(2, 2, 4)
    ax4.axis('off')
    info_text = (
        f"Filter Summary:\n\n"
        f"Num of Coefficients (b): {len(b)}\n"
        f"Den of Coefficients (a): {len(a)}\n"
        f"Number of Zeros: {len(zeros)}\n"
        f"Number of Poles: {len(poles)}\n\n"
        f"System Status: {stability_text.upper()}"
    )
    ax4.text(0.1, 0.5, info_text, fontsize=12, family='monospace', va='center')

    plt.tight_layout()
    plt.show()

# --- Execution ---
equation = "y(nT) = 2*y(nT-T) - y(nT-2T) + x(nT) - 2*x(nT-6T) + x(nT-12T)"
b, a = extract_filter_coefficients(equation)

print(f"b: {b}")
print(f"a: {a}")

# Call the visualization function
visualize_filter(b, a)
