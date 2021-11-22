/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include "pin.H"

FILE* trace;
PIN_LOCK pinLock;

ADDRINT write_address;
bool is_written = false;

VOID Read(THREADID tid, ADDRINT addr) {
    PIN_GetLock(&pinLock, 1);
    ADDRINT * addr_ptr = (ADDRINT*)addr;
    ADDRINT value;
    PIN_SafeCopy(&value, addr_ptr, sizeof(ADDRINT));
    fprintf(trace, "R %lx %lx\n", addr, value);
    PIN_ReleaseLock(&pinLock);
}

VOID Write(THREADID tid, ADDRINT addr) {
    PIN_GetLock(&pinLock, 1);

    // ADDRINT * addr_ptr = (ADDRINT*)addr;
    // ADDRINT value;
    // PIN_SafeCopy(&value, addr_ptr, sizeof(ADDRINT));
    // fprintf(trace,"W %lx, %lx\n", addr, value);

    write_address = addr;
    is_written = true;
    // wirte_data_size = size;

    PIN_ReleaseLock(&pinLock);
}

// insert before Read and Write to get the previous written data
void WriteData(THREADID tid, ADDRINT addr) {
  PIN_GetLock(&pinLock, 1);
  //Reading from memory
  if (is_written) {

    ADDRINT * addr_ptr = (ADDRINT*)write_address;
    ADDRINT value;
    PIN_SafeCopy(&value, addr_ptr, sizeof(ADDRINT));

    fprintf(trace, "W %lx %lx\n", write_address, value);
    is_written = false;
  }

  PIN_ReleaseLock(&pinLock);
}

VOID SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v) {
    int syscall_number = PIN_GetSyscallNumber(ctxt, std);
    fprintf(trace, "S %d\n", syscall_number);
}

// VOID SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v) {
//     int syscall_number = PIN_GetSyscallNumber(ctxt, std);
//     fprintf(trace, "S %d END\n", syscall_number);
// }

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
    if (INS_IsSyscall(ins)) {
        // fprintf(trace, "Syscall\n");
    } else {
        // Instruments memory accesses using a predicated call, i.e.
        // the instrumentation is called iff the instruction will actually be executed.
        //
        // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
        // prefixed instructions appear as predicated instructions in Pin.
        UINT32 memOperands = INS_MemoryOperandCount(ins);

        // Iterate over each memory operand of the instruction.
        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {
            if (INS_MemoryOperandIsRead(ins, memOp))
            {
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
                (AFUNPTR)WriteData, IARG_THREAD_ID, IARG_MEMORYOP_EA,
                memOp, IARG_END);

                INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
                (AFUNPTR)Read, IARG_THREAD_ID, IARG_MEMORYOP_EA,
                memOp, IARG_END);
            }
            // Note that in some architectures a single memory operand can be
            // both read and written (for instance incl (%eax) on IA-32)
            // In that case we instrument it once for read and once for write.
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
                (AFUNPTR)WriteData, IARG_THREAD_ID, IARG_MEMORYOP_EA,
                memOp, IARG_END);

                INS_InsertPredicatedCall(ins, IPOINT_BEFORE,
                (AFUNPTR)Write, IARG_THREAD_ID, IARG_MEMORYOP_EA,
                memOp, IARG_END);
            }
        }
    }
}

VOID Fini(INT32 code, VOID* v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints a trace of memory addresses\n" + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    PIN_InitLock(&pinLock);
    trace = fopen("trace_rw.log", "w");

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    // PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
