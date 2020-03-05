#!/usr/bin/env python3

import io, os, shutil
from pathlib import Path

# Read mp3/m4a/wav files from some directory (don't reorder)
# for each file
#   copy to output directory as: "trackXXX.<extension>, <fileName>.<extension>"

music_directory = str(Path.home()) + '\\Music\\to_SD\\'
output_directory = str(Path.home()) + '\\Music\\to_SD\\'

def _copy(filename):
    # create the full pathed file name
    file = music_directory + filename

    # open the file, copy to the output directory, and then close
    fd = open(file, "r")
    if not os.path.exists(output_directory + filename):
        shutil.copy2(file, output_directory)
    fd.close()

def _writeLine(fd, trackName, fileName):
    line = trackName + ', ' + fileName + '\n'
    fd.write(line)
    print (line)

def generateSongList():
    i = 0
    fd = open(os.getcwd() + '\\songs.txt', "w")
    if not os.path.exists(output_directory):
        os.mkdir(output_directory)
    for filename in os.listdir(music_directory):
        if filename.endswith(".mp3") or filename.endswith(".m4a") or filename.endswith(".wav"):
            i += 1

            # create the new track name in the format "trackXXX.<extension>"
            track = f'track{i:03}.' + filename.split('.')[-1]
            # the next line to write to songs.txt
            _writeLine(fd, track, filename)
            _copy(filename)

            # Rename the newly copied file to 'trackXXX.mXX'
            if not os.path.exists(output_directory + track):
                os.rename(output_directory + filename, output_directory + track)
    fd.close()

if __name__ == '__main__':
    generateSongList()
