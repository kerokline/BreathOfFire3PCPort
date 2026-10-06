"""Coarse alignment on log band energies (100 frames a second), robust to small rate and timbre differences."""
import numpy as np
from scipy import signal

HOP = 441  # 10 ms at 44.1 kHz


def bands(x, sr=44100):
    f, t, S = signal.stft(x, sr, nperseg=2048, noverlap=2048 - HOP, boundary=None, padded=False)
    P = np.abs(S) ** 2
    edges = np.geomspace(60, 12000, 25)
    B = np.stack([P[(f >= lo) & (f < hi)].sum(axis=0) for lo, hi in zip(edges[:-1], edges[1:])])
    L = np.log10(B + 1e-10)
    # onset-ish: positive differences, normalised per band
    D = np.maximum(np.diff(L, axis=1, prepend=L[:, :1]), 0)
    return L, D


def best_lag(Da, Db, min_lag=None, max_lag=None):
    """Lag (frames) of Da inside Db maximising the summed band correlation, normalised."""
    na, nb = Da.shape[1], Db.shape[1]
    nf = 1 << (na + nb).bit_length()
    acc = np.zeros(nf)
    for i in range(Da.shape[0]):
        a = Da[i] - Da[i].mean(); b = Db[i] - Db[i].mean()
        acc += np.fft.irfft(np.fft.rfft(b, nf) * np.conj(np.fft.rfft(a, nf)), nf)
    c = acc[:nb - na + 1]
    # normalise by window energies
    ea = sum(np.sum((Da[i] - Da[i].mean()) ** 2) for i in range(Da.shape[0]))
    cs = np.zeros(nb - na + 1)
    for i in range(Db.shape[0]):
        b = Db[i] - Db[i].mean()
        q = np.concatenate([[0], np.cumsum(b ** 2)])
        cs += q[na:] - q[:-na]
    c = c / np.sqrt(np.maximum(cs, 1e-12) * ea)
    lo = 0 if min_lag is None else max(0, min_lag)
    hi = len(c) if max_lag is None else min(len(c), max_lag)
    k = lo + int(np.argmax(c[lo:hi]))
    return k, float(c[k]), c
