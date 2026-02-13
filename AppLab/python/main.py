# SPDX-FileCopyrightText: Copyright (C) 2025 ARDUINO SA
# SPDX-License-Identifier: MPL-2.0

from arduino.app_utils import *
import math
import cmath

# ==========================================
# 1. USER CONFIGURATION
# ==========================================
L = 4
M_COLS = 16
PHI_VALUES = [0, 180]
PHI_RAND = [90, 0, 90, 0, 90, 90, 0, 90, 0, 90, 0, 90, 0]
USE_PREPHASE = False

# The desired Beam Direction (Gradient)
STEERING_ANGLE = 30  

# The 4 QPSK Symbols (Modulation Phases)
# Matches "With_QPSK_L_4_1.m" logic for bit_level=2
QPSK_PHASES = [0, 90, 180, 270]

# ==========================================
# 2. STATE
# ==========================================
state = {
    'cb': [],
    'symbol_idx': 0  # Start at Index 0 (45 degrees)
}

# ==========================================
# 3. MATH HELPERS
# ==========================================
def sinc(m): return 1.0 if m==0 else math.sin(math.pi*m/L)/(math.pi*m/L)

def generate_codebook():
    cb = []
    base = len(PHI_VALUES)
    for i in range(base**L):
        seq = [(i // (base**n)) % base for n in range(L)]
        gamma_seq = [PHI_VALUES[x] for x in seq]
        a_m = complex(0,0)
        for n in range(1, L+1):
            term = cmath.exp(1j*math.radians(gamma_seq[n-1])) * sinc(1) * \
                   cmath.exp(-1j*math.pi*1*(2*n-1)/L)
            a_m += term / L
        if abs(a_m) > 0.6:
            ph = math.degrees(cmath.phase(a_m)) % 360
            cb.append({'Gamma': gamma_seq, 'Phase': ph})
    return cb

def get_pattern(phase, cb):
    best = min(cb, key=lambda x: abs((x['Phase'] - phase + 180) % 360 - 180))
    val = 0
    for t, p in enumerate(best['Gamma']):
        if p > 90: val |= (1 << t)
    return val

# ==========================================
# 4. API FUNCTION (Called by Arduino)
# ==========================================
def get_ris_frame():
    # Init Codebook
    if not state['cb']:
        print("Generating Codebook...", flush=True)
        state['cb'] = generate_codebook()

    # 1. Get Current QPSK Phase (Base)
    idx = state['symbol_idx']
    base_phase = QPSK_PHASES[idx]

    # 2. Calculate Gradient (Delta)
    delta = math.degrees(2 * math.pi * 0.5 * math.sin(math.radians(STEERING_ANGLE)))
    
    pat_list = []
    col0_debug = 0
    
    # 3. Generate Patterns for all columns
    for k in range(M_COLS):
        # Formula: Base + Gradient (Matches MATLAB line 92)
        phi_req = (base_phase + k * delta) % 360
        
        if USE_PREPHASE: 
            phi_req = (phi_req - PHI_RAND[k]) % 360
            
        pat = get_pattern(phi_req, state['cb'])
        pat_list.append(str(pat))
        
        if k == 0: col0_debug = pat

    result_str = ",".join(pat_list)
    
    print(f"Sent Symbol {idx+1}/4: Phase={base_phase}° | Col0: {col0_debug}", flush=True)

    # 4. Cycle to NEXT Symbol for the next call
    state['symbol_idx'] = (idx + 1) % 4
    
    return result_str

# ==========================================
# 5. RUN SERVER
# ==========================================
print("QPSK Server Ready. Waiting for Arduino...", flush=True)
Bridge.provide("get_ris_frame", get_ris_frame)
App.run()
