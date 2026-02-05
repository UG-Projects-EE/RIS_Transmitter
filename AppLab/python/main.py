import numpy as np
import scipy.io as sio
import time
import sys
import math
from arduino import bridge  # AppLab Bridge Library

# --- PHYSICS & CODEBOOK FUNCTIONS ---
def force_flat_float(data):
    if np.ndim(data) == 0:
        try: return np.array([float(data)])
        except: return np.array([])
    flat_list = []
    try: iterator = data.flat
    except AttributeError: iterator = data
    for item in iterator:
        if isinstance(item, (int, float, np.number)): flat_list.append(float(item))
        else: flat_list.extend(force_flat_float(item))
    return np.array(flat_list)

def parse_codebook(file_name, m_sel, L=4):
    mat = sio.loadmat(file_name)
    data = mat['results']
    while data.shape == (1, 1): data = data[0, 0]
    center_idx = (len(data) - 1) // 2
    idx = center_idx + m_sel
    harmonic_data = data[idx]
    
    # Nuclear Extraction
    raw_p = force_flat_float(harmonic_data['Phase'])
    raw_g = force_flat_float(harmonic_data['Gamma'])
    
    num_entries = len(raw_p)
    gammas_reshaped = raw_g.reshape((num_entries, L))
    
    return [{'Phase': raw_p[i], 'Gamma': gammas_reshaped[i, :]} for i in range(num_entries)]

def local_map(phase_needed, gamma_list):
    best_k = 0
    best_diff = float('inf')
    for k in range(len(gamma_list)):
        diff = np.abs(np.mod(gamma_list[k]['Phase'] - phase_needed + 180, 360) - 180)
        if diff < best_diff:
            best_diff = diff
            best_k = k
    return np.where(np.isclose(gamma_list[best_k]['Gamma'], 180), 1, 0)

# --- MAIN EXECUTION ---
def main():
    print("============================================================")
    print("🚀 Unified RIS Modulation & Beam Steering System")
    
    # 1. User Input (Simulating the interactive part of the modulation code)
    m_sel = 1      # Default Harmonic
    bit_res = 1    # 1-bit resolution
    theta_des = 45 # Default angle
    
    # 2. Physics Params
    M, N, L = 16, 16, 4
    d_lam = 0.5
    Phi_rand_1D = 90 * np.array([1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1])
    Delta_phi_deg = np.rad2deg(2 * np.pi * d_lam * np.sin(np.deg2rad(theta_des)))

    # 3. Load Codebook
    print("📂 Loading Codebook...")
    try:
        GammaList = parse_codebook('One_bit_Code_Book.mat', m_sel, L=4)
    except Exception as e:
        print(f"❌ Error loading .mat file: {e}")
        return

    print("📡 Calculating Beam Pattern...")
    while True:
        # Calculate Phase Requirements
        Phi_req = np.mod(np.arange(M) * Delta_phi_deg, 360)
        
        # Generate RIS Matrix (16x4)
        RIS_matrix = np.zeros((M, L))
        for m in range(M):
            RIS_matrix[m, :] = local_map(Phi_req[m], GammaList)

        # Pack row-major flatten to 20 bytes (160 bits)
        flat_bits = RIS_matrix.flatten().astype(np.uint8)
        packed_bytes = np.packbits(flat_bits)
        hex_str = ''.join([f'{b:02X}' for b in packed_bytes[:20]])

        # 4. BRIDGE: Send to Arduino
        try:
            print(f"📤 Sending Hex Beam to MCU: {hex_str}")
            response = bridge.call('set_ris_beam', hex_str)
            print(f"📥 MCU Response: {response}")
        except Exception as e:
            print(f"⏳ Waiting for Bridge connection... {e}")

        time.sleep(3) # Update every 3 seconds

if __name__ == "__main__":
    main()





main.py