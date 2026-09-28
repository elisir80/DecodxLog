# voacapl in DecoDXLog

VOACAP (Voice of America Coverage Analysis Program) is the HF propagation
engine of NTIA/ITS. It was written by an agency of the U.S. Government and is
not subject to copyright in the U.S.; the changes made by J. A. Watson for the
gfortran port (voacapl) are released under CC0. See `LICENSE`.

Source: https://github.com/jawatson/voacapl, commit
c12a98b348c39f36f2659b8b3aa97b86284c8a1d (29 March 2026), version 0.7.7.

Only what DecoDXLog needs is kept here:

- `src/` — the sources listed in the `Makefile.am` of each part (modules,
  voa_lib, hfmufesw, wp10dwin, voacapw), unchanged;
- `itshfbc/coeffs` — the CCIR/URSI coefficients in ASCII, with the two
  programs that turn them into the binary files VOACAP reads;
- `itshfbc/database` and `itshfbc/antennas/default` — the files VOACAP opens;
- `sample/voacapx.dat` — the example input deck of voacapl, used by the tests.

`sources.cmake` is generated from the `Makefile.am` files; `voacap.cmake`
builds everything with CMake instead of autotools. Nothing in the VOACAP
sources has been modified.
