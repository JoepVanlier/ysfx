# ysfx

Hosting library and audio plugin for JSFX

![capture](docs/capture.png)

## Description

This package provides support for audio and MIDI effects developed with the JSFX
language. These effects exist in source code form, and they are compiled and ran
natively by hosting software.

This contains a hosting library, providing a JSFX compiler and runtime.
In addition, there is an audio plugin which can act as a JSFX host in a digital
audio workstation.

# Helping out

The best way to help out is by using the plugin and reporting bugs you run into. 
While I do make an effort to try builds prior to release in different DAWs, 
it is pretty likely that certain things get missed or overlooked. If you run 
into incorrect behavior, please report it using the issue tracker above and 
I will do my best to fix it. Thank you!

# Forked Project

Note, this is a fork of YSFX with a few modifications to support newer JSFX
features and fix some existing bugs.

## Remarks

Despite what the name might suggest, JSFX is not JavaScript.
These technologies are unrelated to each other.

This project is not the work of Cockos, Inc; however, it is based on several
free and open source components from the WDL. Originally, this project was based
on jsusfx by Pascal Gauthier, which was then rewritten by jpcima.

Some time after the realization of this project, Cockos announced the release of
JSFX as open source under the LGPL. This does not affect the development of this
project, which remains a custom implementation based on the liberally licensed
bits of the WDL.

Unfortunately, the original maintainer (jpcima) seems to have disappeared, so 
I (Joep Vanlier) have decided to fork the project to update it with some more 
recent JSFX features and bugfixes.

# Usage notes

The audio plugin will initially present the JSFX effects available in the REAPER
user folders, if these exist.

The effects are source code files which end with the extension `.jsfx`, or with
no extension at all.

Note that, unlike REAPER, ysfx will let you install JSFX wherever you want.
If you use effects from a custom folder, ysfx will usually figure things out, but
not always.

The ideal hierarchy is one where there exists at least a pair of folders named
"Effects" and "Data" side-by-side, which receive the code files and resource files
respectively.

Example:
- `My JSFX/Effects/guitar/amp-model`
- `My JSFX/Data/amp_models/Marshall JCM800 - Marshall Stock 70.wav`

## Differences to JSFX

There are still quite a few differences between `ysfx` and `jsfx` and some things may not work as well.
Please report bugs and missing features, but also be mindful of the limited time I can spend on fixing them.

I will keep a list of differences I have noticed so far here:

- Do not load images in `@init`, it is terrible for performance in `ysfx`.
While it will work, it will reload those images on every `@init`.
Better is to either use the file syntax at the top of the script, e.g.

```
  filename:0,img/background.png
  filename:1,img/poti_low_gain.png
```

and use those values there.
Or load them in `@gfx` on the first pass.
So something like:

```
@gfx
(!imgs_loaded) ? (
    // load images here
    imgs_loaded = 1;
);
```

## Download development builds

You can find the most recent builds [here](https://github.com/JoepVanlier/ysfx/releases).
Note that I don't know how to do code signing yet, so the Mac builds are not signed.

## Building

To build the project, one must first set up a C++ development environment
equipped with Git and CMake. One can then build the library and the audio
plugin, by entering commands as follows:

```
git clone https://github.com/JoepVanlier/ysfx.git
cd ysfx
git submodule update --init --recursive
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .
```

## JSFX resources

There are many JSFX out there!

- Saike (my own): https://github.com/JoepVanlier/JSFX
- Geraint Luff: https://github.com/geraintluff/jsfx
- tilr JSFX: https://github.com/tiagolr/tilr_jsfx
- Mawi JSFX: https://github.com/mawi-design/JSFX
- Justin Johnson: https://github.com/Justin-Johnson/ReJJ
- Tukan Studios: https://github.com/TukanStudios/TUKAN_STUDIOS_PLUGINS
- Jozmac: https://github.com/jozmac/reapack-jm
- JClones: https://github.com/JClones/JSFXClones
- Sonic Anomaly: https://github.com/Sonic-Anomaly/Sonic-Anomaly-JSFX

For a list with many more, please see: https://www.keithhaydon.com/Reaper/JSFX2.pdf
