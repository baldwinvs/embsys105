#ifndef LINKEDLIST_H_
#define LINKEDLIST_H_

//! Doubly linked list node containing the track name and the file name.
//! \todo rename this file to trackInfo.h
typedef struct track_info
{
    char trackName[SONGLEN];    //!< The ***actual*** name of the track.
    char fileName[13];          //!< The name of the file on the SD card.
    struct track_info* prev;    //!< Pointer to the previous track_info node.
    struct track_info* next;    //!< Pointer to the next track_info node.
} TRACK;

#endif /* LINKEDLIST_H_ */