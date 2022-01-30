# FIT telemetry to SRT subtitles convertor

This tool can be used to create subtitles overlay (without video re-encoding) for your action video with the telemetry from .fit file recorded by FIT enabled device (Garmin / Suunto / Bryton etc.). Many video players accept subtitles in this format, you can even upload these subtitles to the [Youtube](https://support.google.com/youtube/answer/2734796) video as captions.

Usage:
```
usage: fitsrt <input> <output> <offset>

input - path to .fit file to read data from
output - path to .srt file to write subtitle to
offset - optional offset in seconds to sync video and .fit data
* if the offset is positive - 'offset' second of the data from .fit file will be displayed at the first second of the video.
    it is for situations when you started video after starting recording your activity(that generated .fit file)
* if the offset is negative - the first second of .fit data will be displayed at abs('offset') second of the video
    it is for situations when you started your activity (that generated .fit file) after starting the video

```

You can place subtitles to the same folder as the video with the same file name(but keep .srt extension) or embed subtitles into the video file (without re-encoding). You can use [FFMPEG tool](https://www.ffmpeg.org/download.html) for embedding:
```
ffmpeg -i infile.mp4 -i infile.srt -c copy -c:s mov_text outfile.mp4 
```

Tested in Visual Studio 2019(2022) (Open as Folder) and gcc under ubuntu. This project uses Conan as a dependency manager and CMake as a build system.

Build command:
```
cd repo
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

MIT License Copyright (c) 2022 pavel.sokolov@gmail.com / CEZEO software Ltd.. All rights reserved.

```
      .:+oooooooooooooooooooooooooooooooooooooo: `/ooooooooooo/` :ooooo+/-`
   `+dCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOEZshCEZEOCEZEOEZ#doCEZEOEZEZNs.
  :CEZEON#ddddddddddddddddddddddddddddddNCEZEO#h.:hdddddddddddh/.yddddCEZEO#N+
 :CEZEO+.        .-----------.`       `+CEZEOd/   .-----------.        `:CEZEO/
 CEZEO/         :CEZEOCEZEOEZNd.    `/dCEZEO+`   sNCEZEOCEZEO#Ny         -CEZEO
 CEZEO/         :#NCEZEOCEZEONd.   :hCEZEOo`     oNCEZEOCEZEO#Ny         -CEZEO
 :CEZEOo.`       `-----------.`  -yNEZ#Ns.       `.-----------.`       `/CEZEO/
  :CEZEONCEZEOd/.ydCEZEOCEZEOdo.sNCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOEZNEZEZN+
   `+dCEZEOEZEZdoCEZEOCEZEOEZ#N+CEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOCEZEOEZ#s.
      .:+ooooo/` :+oooooooooo+. .+ooooooooooooooooooooooooooooooooooooo+/.
 C E Z E O  S O F T W A R E (c) 2022   FIT telemetry to SRT subtitles converter
```

Part of this repository (fitsdk subfolder) contains 'Flexible and Interoperable Data Transfer (FIT) Protocol SDK' and these sources licensed under [FIT SDK license by Garmin](https://developer.garmin.com/fit/download/)
