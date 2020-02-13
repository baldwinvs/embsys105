
    PUBLIC TestAndSet

    SECTION .text:CODE:REORDER:NOROOT(2)
    THUMB

/*
 * void TestAndSet(uint32_t* lock)
 */
TestAndSet
load:
    LDREX   R1, [R0]
    CBZ     R1, store       ; compare and branch if R1 is zero
    B       load            ; try again
store:
    MOV     R2, #1
    STREX   R1, R2, [R0]    ; store 1 into address of R0
    CBZ     R1, done        ; compare and branch if R1 is zero
    B       load            ; failed, try again
done:
    BX      LR
    END