#ifndef LINKEDLIST_H_
#define LINKEDLIST_H_

typedef struct track_info
{
    char trackName[SONGLEN];
    char fileName[13];
    struct track_info* prev;
    struct track_info* next;
} TRACK;

#endif /* LINKEDLIST_H_ */