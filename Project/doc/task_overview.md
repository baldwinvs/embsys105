# Task Overview

### Task Priorities

| Task               | Priority |
|:------------------:|:--------:|
| InitializationTask | 5        |
| TouchHandlingTask  | 6        |
| CommandHandlerTask | 7        |
| LcdHandlerTask     | 8        |
| SteamingTask       | 9        |

For intertask communication, all tasks (except for **InitializationTask**) use ***message mailboxes***.

### InitializationTask

The **InitializationTask** has the highest priority because it is a short lived task which has the single purpose of synchronizing the other tasks.

When each task has completed initialization they each will post an event bit which the InitializationTaks will be waiting for.  After all tasks have initialized then it will clear those event bits; of which, the TouchPollingTask and LcdHandlerTask functions are pending on.

### TouchHandlingTask

The **TouchHandlingTask** is responsible handling user touch input.

When the user touches the screen it is detected and checks to see if a button was pressed.  If a button was pressed it then checks to see if button is being held.  If a button is pressed and remains pressed the following table explains what happens:

| Button Pressed | Action |
|:------------|:-------|
| Volume Down | No action for holding. |
| Volume Up | No action for holding. |
| Restart | After 1 second of being pressed, a ***rewind*** commands begins to be issued at a frequency of 20 Hz to the CommandHandlerTask.</br>After the button is released, a ***play*** command is sent to the CommandHandlerTask. |
| Skip | After 1 second of being pressed, a ***fast forward*** command begins to be issued at a frequency of 20 Hz to the CommandHandlerTask.</br> After the button is released, a ***play*** command is sent to the CommandHandlerTask. |
| Play | After 1 second of being pressed, a ***stop progress*** command begins to be issued at a frequency of 20 Hz to the LcdHandlerTask.</br>After 2 additional seconds of being pressed, a ***stop*** command is sent to the CommandHandlerTask. *If the button is released before the last 2 seconds, no comamnd is issued.*|

Otherwise, if the button pressed is held for less than 1 second then that command is sent to the CommandHandlerTask via a message mailbox.

***Besides rewind/fast forward all buttons send a single command to the CommandHandlerTask when the button is released.***

### CommandHandlerTask

The ***CommandHandlerTask*** pends on a message mailbox from the TouchCommandHandler.  The state that the MP3 Player is currently in will dictate the resultant state.  The control, however, is independent of the state.

### LcdHandlerTask

The ***LcdHandlerTask*** polls a total of 4 message mailboxes: 1 from ***TouchPollingTask***, 1 from ***CommandHandlerTask***, and 2 from ***StreamingTask***.

- ***TouchPollingTask***: the message sent from this task contains the **stop progress** which is used for creating a simple animation on the screen to let the user know when the **play** button has been held for a sufficient amount of time.

- ***CommandHandlerTask***: the message sent from this task contains the **state and control**, though the control is not used.  The state received will display a corresponding state name on the GUI.

        | State        | Displayed State |
        |:------------:|:---------------:|
        | PLAY         | PLAYING         |
        | PAUSE        | PAUSED          |
        | STOP         | STOPPED         |
        | FAST FORWARD | FAST FORWARDING |
        | REWIND       | REWINDING       |

- ***StreamingTask***: the messages sent from this task include the **track name** and the **song progress**.  The track name can be up to 32 characters long and can be displayed on 1 or 2 lines, length dependent.  The song progress is periodically sent (once per 40 data writes to the MP3 driver).

### StreamingTask

The ***StreamingTask*** currently ***MUST*** read files from the SD card as it is not coded to do otherwise.  At initialization, this task will read from a file, **songs.txt**, from the SD card.  This file has songs mapped to a their respective file names on the SD card.  The format for each line looks like:

        .
        .
        .
        track004.mp3, Involuntary Doppelganger.mp3
        .
        track006.mp3, Killer Ball.mp3

When a line is read from the input file, a node for a doubly-linked list is created using **malloc()**.  After all lines have been read in and the entire linked list created, each file is then checked to see if it can be opened.  If it fails to open, then the respective node is removed from the linked list and deleted using **free()**.  Afterwards, it closes the doubly-linked list; tying the end to the beginning to create a circular doubly-linked list.

After initialization, it polls for a state/control value from the **CommandHandlerTask**.  When a play command is received, it sends the current song name to the **LcdHandlerTask** and then plays the current song by calling **Mp3StreamSDFile()**.

Inside of the streaming function, it polls just like **StreamingTask** did but now more states are handled; namely the rewind/fast forward/pause states.

 - **pause**: For this state a simple task delay is used.
 - **rewind**: For this state, it jumps backwards by 20 frames unless it is at the beginning.
 - **fast forward**: For this state, it jumps forwards by 20 frames unless it is at the end.
 