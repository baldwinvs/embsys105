#ifndef TASKS_H_
#define TASKS_H_

// Task prototypes
void TouchPollingTask(void* pData);
void StreamingTask(void* pData);
void LcdHandlerTask(void* pData);
void CommandHandlerTask(void* pData);

void TaskRxFlags(void* pData);

#endif