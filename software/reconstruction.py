"""
Reconstruct the angular irradiance function from 49 detector measurements.

Companion code for "An Eternal Irradiance Camera" (SIGGRAPH Asia 2026).
Project page: https://cave.cs.columbia.edu/projects/categories/project?cid=Computational+Imaging&pid=An+Eternal+Irradiance+Camera

Notation follows Section S2 of the supplemental material:
https://www.cs.columbia.edu/~jklotz/papers/irradiance-cam/irradiance-cam-supp.pdf

The 49 detectors point along the directions n_1, ..., n_49 of the optimal
Tammes packing of the sphere. Each detector measures irradiance E(n_i) (here,
illuminance in lux). We model the irradiance function as bandlimited in real
spherical harmonics Y_ℓm up to degree ℓ = L - 1 and solve for its coefficients
E_ℓm with a least-squares fit that is constrained to be non-negative over the
whole sphere.

Irradiance is the radiance convolved with a clamped-cosine kernel. That
kernel's spherical harmonic coefficients are zero for odd degrees ℓ > 1, so we
drop those basis functions. This leaves 1 + 3 + 5 + 9 + 13 = 31 unknowns
E_ℓ1m1, ..., E_ℓ31m31 for L = 7, which improves the conditioning of the system.

Requirements: numpy, scipy (>= 1.15), healpy, matplotlib, qpsolvers[quadprog]

Usage:
    python reconstruction.py                            # reconstruct from the average of all frames of the example capture
    python reconstruction.py --data data/capture-2.npz  # other outdoor captures in data/
"""
import argparse
from pathlib import Path

import numpy as np
import scipy.special
import healpy as hp
import matplotlib.pyplot as plt
from qpsolvers import solve_ls

L = 7 # Bandlimit: spherical harmonics of degree ℓ = 0, ..., L - 1
NSIDE_CONSTRAINT = 20 # HEALPix resolution at which non-negativity is enforced (K = 12 * 20^2 = 4800)
FPS = 30 # Frame rate of the example captures


def real_sph_harm(ell_max: int, theta: np.ndarray, phi: np.ndarray):
    """
    Real spherical harmonics Y_ℓm up to degree ell_max (supplemental eq. (1)).

    theta: zenith angle in [0, pi], phi: azimuth in [0, 2*pi)
    Returns Y ((ell_max + 1)^2 x N), ell, m ((ell_max + 1)^2,) for N directions
    """
    Y_complex = scipy.special.sph_harm_y_all(ell_max, ell_max, theta, phi)

    # scipy includes the Condon-Shortley phase, so (-1)^m cancels it
    Y, ell, m = [], [], []
    for ell_curr in range(ell_max + 1):
        for m_curr in range(-ell_curr, ell_curr + 1):
            if m_curr == 0:
                Y_curr = np.real(Y_complex[ell_curr, 0])
            elif m_curr > 0:
                Y_curr = np.sqrt(2) * (-1)**m_curr * np.real(Y_complex[ell_curr, m_curr])
            else:
                Y_curr = np.sqrt(2) * (-1)**m_curr * np.imag(Y_complex[ell_curr, -m_curr])
            Y.append(Y_curr)
            ell.append(ell_curr)
            m.append(m_curr)

    return np.stack(Y), np.asarray(ell), np.asarray(m)


def irradiance_basis(theta: np.ndarray, phi: np.ndarray, ell_max: int=L - 1):
    """
    Spherical harmonic basis for the irradiance function, without odd degrees
    ℓ > 1. Returns a matrix (N x 31 for ell_max = 6) whose row i is
    [Y_ℓ1m1(n_i), ..., Y_ℓ31m31(n_i)] for the direction n_i = (theta_i, phi_i),
    and the degree/order (ℓ_j, m_j) of each column.
    """
    Y, ell, m = real_sph_harm(ell_max, theta, phi)
    keep = ~((ell > 1) & (ell % 2 == 1))
    return Y[keep].T, ell[keep], m[keep]


def reconstruct_coefficients(
        b: np.ndarray,
        detector_theta: np.ndarray,
        detector_phi: np.ndarray,
        mask: np.ndarray=None,
        ell_max: int=L - 1,
        nside_constraint: int=NSIDE_CONSTRAINT):
    """
    Solve (supplemental eq. (3))

        min_x || A x - b ||_2^2  subject to  G x >= 0,

    where
        A = [Y_ℓjmj(n_i)]   (49 x 31, eq. (4)): basis sampled at the detector directions n_1, ..., n_49
        x = [E_ℓjmj]        (31,      eq. (5)): spherical harmonic coefficients of the irradiance function
        b = [E(n_i)]        (49,      eq. (6)): irradiance measured by each detector
        G = [Y_ℓjmj(n'_k)]  (K x 31,  eq. (7)): basis sampled at K HEALPix directions n'_1, ..., n'_K,
                                                so that the reconstruction is non-negative everywhere on the sphere

    b: measurements (49,), one per detector
    mask: boolean (49,), detectors to use (e.g., to exclude broken or saturated
        ones). Masked-out detectors are dropped from the rows of A and b.
    Returns x and the degree/order (ℓ_j, m_j) of each coefficient.
    """
    if mask is None:
        mask = np.ones(len(b), dtype=np.bool_)

    A, ell, m = irradiance_basis(detector_theta[mask], detector_phi[mask], ell_max)

    theta_grid, phi_grid = hp.pix2ang(nside_constraint, np.arange(hp.nside2npix(nside_constraint)))
    G, _, _ = irradiance_basis(theta_grid, phi_grid, ell_max)

    # qpsolvers expects G x <= h, so negate G x >= 0
    x = solve_ls(A, b[mask], G=-G, h=np.zeros(G.shape[0]), solver="quadprog")
    if x is None:
        raise RuntimeError("Non-negative least squares solve failed")

    return x, ell, m


def evaluate(x: np.ndarray, theta: np.ndarray, phi: np.ndarray, ell_max: int=L - 1):
    """
    Evaluate the reconstructed irradiance function E(n) = sum_j E_ℓjmj Y_ℓjmj(n)
    at directions n = (theta, phi), given the coefficients x.
    """
    Y, _, _ = irradiance_basis(theta.ravel(), phi.ravel(), ell_max)
    E = Y @ x
    # The constraint is enforced on a finite grid, so clip any tiny negative
    # values between grid points
    return np.clip(E, 0, np.inf).reshape(theta.shape)


def equirectangular_directions(height: int, width: int, R: np.ndarray=np.eye(3)):
    """
    Directions (theta, phi) at the pixel centers of an equirectangular image.
    Row 0 is theta = 0 (+z). The directions are mapped through R, so pass the
    world-to-camera rotation to render a camera-frame function in world coordinates.
    """
    rr, cc = np.meshgrid(np.arange(height) + 0.5, np.arange(width) + 0.5, indexing="ij")
    theta = rr * np.pi / height
    phi = cc * 2 * np.pi / width

    d = np.stack([np.sin(theta) * np.cos(phi), np.sin(theta) * np.sin(phi), np.cos(theta)], axis=-1)
    d = d @ R.T
    theta = np.arccos(np.clip(d[..., 2], -1, 1))
    phi = np.mod(np.arctan2(d[..., 1], d[..., 0]), 2 * np.pi)

    return theta, phi


def plot_raw_measurements(lux: np.ndarray, sensor_mask: np.ndarray, saturated: np.ndarray):
    """
    Plot each detector's measurements over time, one panel per detector.
    Detectors excluded from the reconstruction are drawn in gray.

    lux: (frames, 49), sensor_mask: (49,), saturated: (frames, 49)
    """
    t = np.arange(lux.shape[0]) / FPS
    fig, axs = plt.subplots(7, 7, figsize=(16, 12), sharex=True)
    for i, ax in enumerate(axs.flat):
        if not sensor_mask[i]:
            note = " (bad)"
        elif saturated[:, i].any():
            note = " (saturated)"
        else:
            note = ""
        ax.plot(t, lux[:, i], color="0.6" if note else "C0", lw=1)
        ax.set_title("Sensor %d%s" % (i, note), fontsize=8)
        ax.tick_params(labelsize=6)
    for ax in axs[-1]:
        ax.set_xlabel("Time (s)", fontsize=8)
    for ax in axs[:, 0]:
        ax.set_ylabel("Lux", fontsize=8)
    fig.suptitle("Raw Sensor Measurements")
    fig.tight_layout()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", type=Path, default=Path(__file__).parent / "data" / "capture-5.npz")
    args = parser.parse_args()

    with np.load(args.data) as D:
        lux = D["lux"]                       # (frames, 49) illuminance per detector, in lux
        detector_theta = D["detector_theta"] # (49,) detector directions in the camera frame
        detector_phi = D["detector_phi"]
        sensor_mask = D["sensor_mask"]       # (49,) False for broken detectors
        saturated = D["saturated"]           # (frames, 49) True where a detector saturated
        R_to_world = D["R_to_world"]         # (3, 3) camera frame -> world frame (+z up)

    plot_raw_measurements(lux, sensor_mask, saturated)

    # Reconstruct from the average over all frames, excluding any detector
    # that saturated in any frame
    E = lux.mean(0)
    mask = sensor_mask & ~saturated.any(0)
    print("Using %d of %d detectors" % (mask.sum(), len(mask)))

    x, ell, m = reconstruct_coefficients(E, detector_theta, detector_phi, mask=mask)

    E_fit = evaluate(x, detector_theta, detector_phi)
    rms = np.sqrt(np.mean((E_fit - E)[mask]**2))
    print("RMS residual of measurements: %.1f lux (%.2f%% of mean)" % (rms, 100 * rms / E[mask].mean()))

    # Render as an equirectangular image in the world frame, so that up in the
    # image (theta = 0) is up in the world
    theta, phi = equirectangular_directions(256, 512, R_to_world.T)
    E_img = evaluate(x, theta, phi)

    plt.figure(figsize=(10, 5))
    plt.imshow(E_img, extent=[0, 360, 180, 0], cmap="inferno")
    plt.colorbar(label="Irradiance (lux)", fraction=0.025)
    plt.xlabel(r"$\phi$ (deg)")
    plt.ylabel(r"$\theta$ (deg)")
    plt.title("Reconstructed irradiance function")
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
