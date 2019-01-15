How to run the program:
Extract all and run MLDX12VideoCapture.exe

Version 1.0

-Initial release
-Only YUV422 10bit can be recorded as AVI
-AVI file does not have index chunk. use mencoder to fix the index problem. Example:

> mencoder -idx output.avi -ovc copy -oac copy -o fixed.avi
