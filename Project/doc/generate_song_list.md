# Generating the Song List

## Requirements
 - Python 3.7
 - Desired music files (.mp3, .m4a, .wav formats)

### Steps
1. Create a folder in the user’s Music directory called **to_SD**.
2. Copy desired .mp3/.m4a/.wav files into **to_SD**.
3. In the embsys105/Project directory, run the [generateSongList.py](../generateSongList.py) python script.

        a. Renames files so they can be read within the MP3 Player application.
        b.  Creates a file, songs.txt, that will be read by the MP3 Player application to populate a linked list of track information.
        c. Line format within songs.txt (for .mp3 files):
        
            “track<file number>.mp3, <song name>.mp3”
            Ex: track004.mp3, Involuntary Doppelganger.mp3
4. Copy the contents of the **to_SD** directory into the root directory of the SD card.