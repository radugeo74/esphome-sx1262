# C1 CRC investigation — sanitized summary

User-specific radio data and identifiers are excluded from this document.
The observed C1 packet had coherent length, but interpreting the final two bytes as a whole-frame EN13757 CRC did not match. A C1 format-specific CRC layout must be verified before accepting or rejecting radio frames.

The synthetic `test_native_c1_integrity.cpp` exercises both valid CRC and CRC mismatch without using real meter telegrams.

The hardware experiments must still distinguish radio framing, CRC layout, and decryption. Never commit user radio samples or identifiers.
