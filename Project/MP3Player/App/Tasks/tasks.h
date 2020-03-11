#ifndef TASKS_H_
#define TASKS_H_

/** 
 * @defgroup tasks MP3 Player Tasks
 * The tasks that make up the entirety of the MP3 player after startup.
 * 
 * @{
 */

/** The TouchPollingTask polls for a touch input and waits for the user
 * to release the touch before sending a message to the CommandHandlerTask.
 * 
 * @param pData Pointer to input data for the task.
 */
void TouchPollingTask(void* pData);

/** The StreamingTask streams music files from the SD
 * card while being able to respond to user commands.
 * 
 * @param pData Pointer to input data for the task.
 */
void StreamingTask(void* pData);

/** The LcdHandlerTask handles displaying the current track, the progress of
 * the current track, and the current state of player.
 * 
 * @param pData Pointer to input data for the task.
 */
void LcdHandlerTask(void* pData);

/** The CommandHandlerTask receives input from the TouchPollingTask and then
 * converts that input into a command value that is comprised of the state
 * to change to and the control to perform.
 * 
 * @param pData Pointer to the input data for the task.
 */
void CommandHandlerTask(void* pData);

/** The InitializationTask synchronizes the initialization of tasks and then
 * wakes up TouchPollingTask and LcdHandlerTask using event flags.
 * 
 * @param pData Pointer to input data for the task.
 */
void InitializationTask(void* pData);

/** @} */

#endif
