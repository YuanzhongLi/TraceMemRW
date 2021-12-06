/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include "pin.H"

using namespace std;


FILE* trace;
PIN_LOCK pinLock;

ADDRINT write_address;
ADDRINT global_printf_address;
bool printf_flag = false;
bool is_written = false;

bool GC_flag = false;
string GC_start = "GC";
string GC_end = "GCEND";

string tmp = "";

// #define FILE
#define OUT

/**
 * R address, value : Read memory
 * W address, value : Write memory
 * S                : system call start
 * E                : systemm call end
 * G                : GC dump
 * #                : execution end
 */

string trimString(string str) {
    string ret = "";
    for (char ch: str) {
        ret += ch;
        if (ch == 't' || ch == 'o') {
            return ret;
        }
    }
    return ret;
};

vector<string> splitString(string str, string delimiter = " ")
{
    int start = 0;
    int end = str.find(delimiter);
    vector<string> ret;
    while (end != -1) {
        ret.emplace_back(str.substr(start, end - start));
        start = end + delimiter.size();
        end = str.find(delimiter, start);
    }
    ret.emplace_back(str.substr(start, end - start));
    return ret;
};

VOID Read(THREADID tid, ADDRINT addr) {
    PIN_GetLock(&pinLock, 1);
    ADDRINT * addr_ptr = (ADDRINT*)addr;
    ADDRINT value;
    PIN_SafeCopy(&value, addr_ptr, sizeof(ADDRINT));

//     if (!printf_flag) {
// #ifdef FILE
//     fprintf(trace, "R %lx %lx\n", addr, value);
// #endif
// #ifdef OUT
//     printf("R %lx %lx\n", addr, value);
// #endif
//     }

    PIN_ReleaseLock(&pinLock);
};

VOID Write(THREADID tid, ADDRINT addr) {
    PIN_GetLock(&pinLock, 1);
    write_address = addr;
    is_written = true;
    PIN_ReleaseLock(&pinLock);
};

// insert before Read and Write to get the previous written data
void WriteData(THREADID tid, ADDRINT addr) {
  PIN_GetLock(&pinLock, 1);
  //Reading from memory
  if (is_written) {
    ADDRINT * addr_ptr = (ADDRINT*)write_address;
    ADDRINT value;
    PIN_SafeCopy(&value, addr_ptr, sizeof(ADDRINT));

    if (write_address == global_printf_address)  { // hook printf
        if (value == 1ll) {
            printf_flag = true;
        } else {
            printf_flag = false;
        }
    } else {
//         if (!printf_flag) {
// #ifdef FILE
//             fprintf(trace, "W %lx %lx\n", write_address, value);
// #endif
// #ifdef OUT
//             printf("W %lx %lx\n", write_address, value);
// #endif
//         }
    }


    is_written = false;
  }

  PIN_ReleaseLock(&pinLock);
};


VOID check(string str) {
    if (str == GC_start) {
        GC_flag = true;
        return;
    } else if (str == GC_end) {
        GC_flag = false;
        return;
    }

    if (str[0] == 'P') {
        vector<string> words = splitString(str);
        global_printf_address = (ADDRINT)(strtoul(words[1].c_str(), 0, 16));
    }

    if (GC_flag) {
        vector<string> words = splitString(str);
        long type = strtol(words[0].c_str(), 0, 10);
        long bytes = strtol(words[1].c_str(), 0, 10);
        long original_bytes = strtol(words[2].c_str(), 0, 10);
        unsigned long address = strtoul(words[3].c_str(), 0, 16);
        unsigned long hidden_class = strtoul(words[4].c_str(), 0, 16);

#ifdef FILE
        fprintf(trace, "D %ld %ld %ld %lx %lx\n", type, bytes, original_bytes, address, hidden_class);
        for (long i = 0; i < bytes; i += 8) {
            PIN_GetLock(&pinLock, 1);
            ADDRINT * adr_ptr = (ADDRINT*)(address + i);
            ADDRINT value;
            PIN_SafeCopy(&value, adr_ptr, sizeof(ADDRINT));
            fprintf(trace, "%s %lx\n", words[5+(i/8)].c_str(), value);
            PIN_ReleaseLock(&pinLock);
        }
#endif
#ifdef OUT
        printf("D %ld %ld %ld %lx %lx\n", type, bytes, original_bytes, address, hidden_class);
        for (long i = 0; i < bytes; i += 8) {
            PIN_GetLock(&pinLock, 1);
            ADDRINT * adr_ptr = (ADDRINT*)(address + i);
            ADDRINT value;
            PIN_SafeCopy(&value, adr_ptr, sizeof(ADDRINT));
            printf("%s %lx\n", words[5+(i/8)].c_str(), value);
            PIN_ReleaseLock(&pinLock);
        }
#endif
    }
};

VOID SysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5)
{
#if defined(TARGET_LINUX) && defined(TARGET_IA32)
    // On ia32 Linux, there are only 5 registers for passing system call arguments,
    // but mmap needs 6. For mmap on ia32, the first argument to the system call
    // is a pointer to an array of the 6 arguments
    if (num == SYS_mmap)
    {
        ADDRINT * mmapArgs = reinterpret_cast<ADDRINT *>(arg0);
        arg0 = mmapArgs[0];
        arg1 = mmapArgs[1];
        arg2 = mmapArgs[2];
        arg3 = mmapArgs[3];
        arg4 = mmapArgs[4];
        arg5 = mmapArgs[5];
    }
#endif
    // hook write system call
    if ((long) num == 1ll) {
        ADDRINT * addr_ptr = (ADDRINT*)arg1;
        // ADDRINT value;
        char value[(int)arg2];
        PIN_SafeCopy(value, addr_ptr, (int)arg2);
        string str = value;
        string trimedStr = trimString(str);
        int n = trimedStr.size();
        if (trimedStr[n-1] == 't') {
            tmp += string(trimedStr.begin(), trimedStr.end()-1);
        } else if (trimedStr[n-1] == 'o') {
            if (n == 1) {
                check(string(tmp.begin(), tmp.end()-1));
            } else {
                tmp += string(trimedStr.begin(), trimedStr.end()-2);
                check(tmp);
            }
            tmp = "";
        }
    }
}

VOID SysAfter(ADDRINT ret)
{

}

VOID SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v)
{
    SysBefore(PIN_GetContextReg(ctxt, REG_INST_PTR),
        PIN_GetSyscallNumber(ctxt, std),
        PIN_GetSyscallArgument(ctxt, std, 0),
        PIN_GetSyscallArgument(ctxt, std, 1),
        PIN_GetSyscallArgument(ctxt, std, 2),
        PIN_GetSyscallArgument(ctxt, std, 3),
        PIN_GetSyscallArgument(ctxt, std, 4),
        PIN_GetSyscallArgument(ctxt, std, 5));
}

VOID SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v) {
    SysAfter(PIN_GetSyscallReturn(ctxt, std));
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
    if (INS_IsSyscall(ins) && INS_IsValidForIpointAfter(ins)) {
        // Arguments and syscall number is only available before
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(SysBefore),
                       IARG_INST_PTR, IARG_SYSCALL_NUMBER,
                       IARG_SYSARG_VALUE, 0, IARG_SYSARG_VALUE, 1,
                       IARG_SYSARG_VALUE, 2, IARG_SYSARG_VALUE, 3,
                       IARG_SYSARG_VALUE, 4, IARG_SYSARG_VALUE, 5,
                       IARG_END);

        // return value only available after
        INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(SysAfter),
                       IARG_SYSRET_VALUE,
                       IARG_END);
    } else {
        // Instruments memory accesses using a predicated call, i.e.
        // the instrumentation is called iff the instruction will actually be executed.

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
#ifdef FILE
    fprintf(trace, "#\n");
    fclose(trace);
#endif
#ifdef OUT
    printf("#\n");
#endif
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
#ifdef FILE
    trace = fopen("check_rw.log", "w");
#endif

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
