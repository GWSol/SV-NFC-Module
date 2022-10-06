# SV-NFC-Module
This repository is used to store the files used in the testing and development of the NFC module for Spring Valley Tech.
Included here are the folders for the original files and dependencies, the testing of HTTP posting (clearHTTP),
and the official HTTPS posting to the server via secure connection.

* clearHTTP
  * clearHTTP.bin (test .bin file for OTA updates)
  * clearHTTP.ino (test Arduino code for posting via HTTP)
  
* clearHTTPS
  * certs.h (header file containing libraries and SSL keys for OTA and HTTPS posting)
  * clearHTTPS.bin (official .bin file for OTA updates, currently v1.0.7)
  * clearHTTPS.ino (Arduino source code for NFC module)
  * version.txt (reference file for version checking during OTA)
