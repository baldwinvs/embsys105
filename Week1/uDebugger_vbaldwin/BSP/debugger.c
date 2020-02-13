#include <stdint.h>
#include <string.h>
#include "print.h"

#define MAX_CHARS (64)
#define HEX_STR_LEN (10)

/*
 *
 * Reverse a given string based on the character count.
 * str: pointer to the string.
 * count: the number of expected characters in the string.
 *
 */
static void reverseString(char* str, const uint32_t count)
{
    int pos = 0;
    char temp;
    while(pos < (count/2)) {
        temp = str[pos];
        str[pos] = str[count - 1 - pos];
        str[count - 1 - pos] = temp;
        pos++;
    }
}

/*
 *
 * Create a hex string (including leading "0x") from a 32 bit unsigned value.
 * val: the value to represent in hexadecimal.
 * str: pointer to the string.
 *
 */
static void createHexStr(uint32_t val, char* str)
{
    const uint32_t max = 10; // 10 characters for a 32 bit hex value, "0x1234ABCD"
    uint32_t remainder = 0;
    uint32_t pos = 0;

    // Create the hex string
    while(val) {
        remainder = val % 16;
        str[pos] = (remainder < 10 ? '0' + remainder : 'A' + remainder - 10);
        val /= 16;
        pos++;
    }
    // Fill remaining characters as 0
    while(pos < (max - 2)) {
        str[pos] = '0';
        pos++;
    }
    // Add the 0x in reverse order
    str[pos++] = 'x';
    str[pos] = '0';

    reverseString(str, max);
}

/*
 *
 * Part of a fault exception handler. Prints the given register values.
 * pc: the value of the program counter when the fault occurred.
 * lr: the value of the link register when the fault occurred.
 *
 */
void FaultPrint(uint32_t pc, uint32_t lr)
{
    // Print an error message specifying the PC and LR values when the fault occurred
    char str[MAX_CHARS];
    char substr1[HEX_STR_LEN];
    char substr2[HEX_STR_LEN];

    createHexStr(pc, substr1);
    createHexStr(lr, substr2);

    strcpy(str, "Hard fault at PC=");
    strncat(str, substr1, HEX_STR_LEN);
    strcat(str, " LR=");
    strncat(str, substr2, HEX_STR_LEN);
    strcat(str, "\n");
    PrintString(str);
}

/*
 *
 * Print R0-R3, R12, LR, PC, and xPSR of the exception stack frame.
 * sp: the stack pointer initially pointing to R0 in the exception stack frame.
 *
 * NOTE: avoids using sprintf because it doesn't have the complete functionality.
 *
 */
void FaultPrintExceptionStack(uint32_t* sp)
{
    char str[MAX_CHARS];
    char substr1[HEX_STR_LEN];
    char substr2[HEX_STR_LEN];

    PrintString("Hard fault occurred:\n");

    // "        R0=0x1234ABCD   R1=0x1234ABCD"
    createHexStr(*sp++, substr1); // R0 string
    createHexStr(*sp++, substr2); // R1 string
    strcpy(str, "\tR0=");
    strncat(str, substr1, HEX_STR_LEN);
    strcat(str, "\tR1=");
    strncat(str, substr2, HEX_STR_LEN);
    strcat(str, "\n");
    PrintString(str);

    // "        R2=0x1234ABCD   R3=0x1234ABCD"
    createHexStr(*sp++, substr1); // R2 string
    createHexStr(*sp++, substr2); // R3 string
    strcpy(str, "\tR2=");
    strncat(str, substr1, HEX_STR_LEN);
    strcat(str, "\tR3=");
    strncat(str, substr2, HEX_STR_LEN);
    strcat(str, "\n");
    PrintString(str);

    // "        R12=0x1234ABCD  LR=0x1234ABCD"
    createHexStr(*sp++, substr1); // R12 string
    createHexStr(*sp++, substr2); // LR string
    strcpy(str, "\tR12=");
    strncat(str, substr1, HEX_STR_LEN);
    strcat(str, "\tLR=");
    strncat(str, substr2, HEX_STR_LEN);
    strcat(str, "\n");
    PrintString(str);

    // "        PC=0x1234ABCD   PSR=0x1234ABCD"
    createHexStr(*sp++, substr1); // PC string
    createHexStr(*sp, substr2);   // xPSR string
    strcpy(str, "\tPC=");
    strncat(str, substr1, HEX_STR_LEN);
    strcat(str, "\tPSR=");
    strncat(str, substr2, HEX_STR_LEN);
    strcat(str, "\n");
    PrintString(str);
}