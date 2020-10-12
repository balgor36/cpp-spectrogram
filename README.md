C++ Spectrogram
===============

A spectrogram written in C++.

You will need the FreeImage library and a C++11 compatible compiler to
compile the program.

Run with (assuming g++)
```
$ mkdir build & cd build
$ g++ -O2 -o spectro ../src/*.cpp -I ../include -std=c++11 -lfreeimage -lsndfile
$ ./spectro <filename>
```

The result files are the sequencies of spectrograms, slised by TIME_STEP
seconds (defined in Spectrograph.h). The filenames are scan0.png, scan1.png etc.
If TIME_STEP is zero, the result is a full spectrogram.

Currently there is no way to specify command line arguments, so you will
have to code them in if you would like to change settings.

Copying
-------
This source is released under the GPLv3 license.
Read the COPYING file for legal information.
