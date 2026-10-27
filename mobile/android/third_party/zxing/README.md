# ZXing core 3.5.3 (QR decoding subset)

Used by `ScanActivity` (扫一扫) to read the desktop sign-in QR code from camera frames on the phone.

- Source: Maven Central `com.google.zxing:core:3.5.3`, `core-3.5.3-sources.jar`
  (SHA-1 `e842d62431675510e2b2116bbc8f75a8b1f6d1ca`, matching the published `.sha1`).
- License: Apache License 2.0, see `LICENSE`. Copyright notices are kept in every source file.
- Included unchanged: the `com.google.zxing` root package except `MultiFormatReader` and
  `MultiFormatWriter`, plus `common/**`, `qrcode/QRCodeReader.java`, `qrcode/decoder/**` and
  `qrcode/detector/**`. Other barcode formats and all encoders are left out.

`scripts/build_android.py` compiles these files together with the app; no Gradle or network step is
needed at build time. To update, replace the files from a newer verified sources jar with the same subset.
