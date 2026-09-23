# FLAC fixtures

`flac-pcm24.flac` was generated independently with the FLAC 1.5.0 CLI, mono
signed little-endian PCM24 at 48000 Hz, level 5, blocksize 1024, no padding or
seektable. Its 2053 integers are `[-8388608, -1, 0, 1, 8388607]` followed by
`i * 123 - 200000` for `i` in `range(2048)`. Output was written to a seekable
file so STREAMINFO contains the actual sample count and MD5. Python tests
compare decoded integers to that formula without depending on the encoder.

`golden-v4.json` uses the original v1 golden fixture's independent samples,
with manually constructed v4 raw/constant descriptors and an empty event
trailer. The original v1 files remain the reader compatibility fixture.
