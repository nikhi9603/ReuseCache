
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <string>

#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

#define SIZE_OF_CALL_INSTRUCTION 5
#define MAX_MEMORY_SIZE 64

#define debug false

typedef struct trace_instr_format
{
    unsigned long long int ip; // instruction pointer (program counter) value

    unsigned char has_mem_is_branch; // does this have memory operands(second bit), is this branch(LSB bit)
    unsigned char branch_taken;      // if so, is this taken

    unsigned char destination_register[NUM_INSTR_DESTINATIONS]; // output registers
    unsigned char source_register[NUM_INSTR_SOURCES];           // input registers

    unsigned long long int *destination_memory_address; // output memory
    unsigned long long int *source_memory_address;      // input memory

    uint8_t *destination_memory_size; // output memory sizes
    uint8_t *source_memory_size;      // input memory sizes

    unsigned char **destination_memory_value; // output memory values
    unsigned char **source_memory_value;      // input memory values
} trace_instr_format_t;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 instrCount = 0;

FILE *out;

bool output_file_closed = false;
bool tracing_on = false;

trace_instr_format_t curr_instr;
bool skip_curr_instr = false;

int memoryWriteIndex;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace",
                                 "specify file name for Champsim tracer output");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "2000000000",
                                  "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "2000000000",
                                   "How many instructions to trace");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    std::cerr << "This tool creates a register and memory access trace" << std::endl
              << "Specify the output trace file with -o" << std::endl
              << "Specify the number of instructions to skip before tracing with -s" << std::endl
              << "Specify the number of instructions to trace with -t" << std::endl
              << std::endl;

    std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

void BeginInstruction(VOID *ip, uint32_t op_code, uint32_t numMemOperands, VOID *opstring)
{
    instrCount++;

    if (instrCount > KnobSkipInstructions.Value())
    {
        tracing_on = true;

        if (instrCount > (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()))
            tracing_on = false;
    }

    if (!tracing_on)
        return;

    curr_instr.ip = (unsigned long long int)ip;

    bool hasMemory = (numMemOperands != 0);

    if (hasMemory)
    {
        curr_instr.has_mem_is_branch = 0b00000010;
    }
    else
    {
        curr_instr.has_mem_is_branch = 0;
    }
    curr_instr.branch_taken = 0;

    for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
    {
        curr_instr.destination_register[i] = 0;
    }

    for (int i = 0; i < NUM_INSTR_SOURCES; i++)
    {
        curr_instr.source_register[i] = 0;
    }

    if (hasMemory)
    {
        curr_instr.destination_memory_address = new unsigned long long int[NUM_INSTR_DESTINATIONS];
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
            curr_instr.destination_memory_address[i] = 0;

        curr_instr.source_memory_address = new unsigned long long int[NUM_INSTR_SOURCES];
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
            curr_instr.source_memory_address[i] = 0;

        curr_instr.destination_memory_size = new uint8_t[NUM_INSTR_DESTINATIONS];
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
            curr_instr.destination_memory_size[i] = 0;

        curr_instr.source_memory_size = new uint8_t[NUM_INSTR_SOURCES];
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
            curr_instr.source_memory_size[i] = 0;

        curr_instr.destination_memory_value = new unsigned char *[NUM_INSTR_DESTINATIONS];
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
            curr_instr.destination_memory_value[i] = nullptr;

        curr_instr.source_memory_value = new unsigned char *[NUM_INSTR_SOURCES];
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
            curr_instr.source_memory_value[i] = nullptr;
    }
    else
    {
        curr_instr.destination_memory_address = nullptr;
        curr_instr.destination_memory_size = nullptr;
        curr_instr.destination_memory_value = nullptr;

        curr_instr.source_memory_address = nullptr;
        curr_instr.source_memory_size = nullptr;
        curr_instr.source_memory_value = nullptr;
    }

    memoryWriteIndex = -1;
}

void EndInstruction()
{
    if (debug)
        std::cout << "Done with instruction no: " << instrCount << std::endl;

    if (instrCount > KnobSkipInstructions.Value())
    {
        tracing_on = true;

        if (instrCount <= (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()))
        {
            if (!skip_curr_instr)
            {
                fwrite(&curr_instr.ip, sizeof(unsigned long long int), 1, out);
                fwrite(&curr_instr.has_mem_is_branch, sizeof(unsigned char), 1, out);
                fwrite(&curr_instr.branch_taken, sizeof(unsigned char), 1, out);
                fwrite(curr_instr.destination_register, sizeof(unsigned char), NUM_INSTR_DESTINATIONS, out);
                fwrite(curr_instr.source_register, sizeof(unsigned char), NUM_INSTR_SOURCES, out);

                if ((curr_instr.has_mem_is_branch & 0b00000010) == 2)
                {

                    fwrite(curr_instr.destination_memory_address, sizeof(unsigned long long int), NUM_INSTR_DESTINATIONS, out);
                    fwrite(curr_instr.source_memory_address, sizeof(unsigned long long int), NUM_INSTR_SOURCES, out);
                    fwrite(curr_instr.destination_memory_size, sizeof(uint8_t), NUM_INSTR_DESTINATIONS, out);
                    fwrite(curr_instr.source_memory_size, sizeof(uint8_t), NUM_INSTR_SOURCES, out);

                    for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
                    {
                        if (curr_instr.destination_memory_size[i] > 0 && curr_instr.destination_memory_value[i] != nullptr)
                        {
                            fwrite(curr_instr.destination_memory_value[i], curr_instr.destination_memory_size[i], 1, out);
                            delete curr_instr.destination_memory_value[i];
                            curr_instr.destination_memory_value[i] = nullptr;
                        }
                    }

                    for (int i = 0; i < NUM_INSTR_SOURCES; i++)
                    {
                        if (curr_instr.source_memory_size[i] > 0 && curr_instr.source_memory_value[i] != nullptr)
                        {
                            fwrite(curr_instr.source_memory_value[i], curr_instr.source_memory_size[i], 1, out);
                            delete curr_instr.source_memory_value[i];
                            curr_instr.source_memory_value[i] = nullptr;
                        }
                    }

                    delete[] curr_instr.destination_memory_address;
                    curr_instr.destination_memory_address = nullptr;
                    delete[] curr_instr.source_memory_address;
                    curr_instr.source_memory_address = nullptr;
                    delete[] curr_instr.destination_memory_size;
                    curr_instr.destination_memory_size = nullptr;
                    delete[] curr_instr.source_memory_size;
                    curr_instr.source_memory_size = nullptr;
                }
            }
        }
        else
        {
            tracing_on = false;

            if (!output_file_closed)
            {
                fclose(out);
                output_file_closed = true;
            }

            exit(0);
        }

        memoryWriteIndex = -1;
        skip_curr_instr = false;

        if ((curr_instr.has_mem_is_branch & 0b00000010) == 2)
        {
            for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
            {
                if (curr_instr.destination_memory_size[i] > 0 && curr_instr.destination_memory_value[i] != nullptr)
                {
                    delete curr_instr.destination_memory_value[i];
                    curr_instr.destination_memory_value[i] = nullptr;
                }
            }

            for (int i = 0; i < NUM_INSTR_SOURCES; i++)
            {
                if (curr_instr.source_memory_size[i] > 0 && curr_instr.source_memory_value[i] != nullptr)
                {
                    delete curr_instr.source_memory_value[i];
                    curr_instr.source_memory_value[i] = nullptr;
                }
            }

            delete[] curr_instr.destination_memory_address;
            curr_instr.destination_memory_address = nullptr;
            delete[] curr_instr.source_memory_address;
            curr_instr.source_memory_address = nullptr;
            delete[] curr_instr.destination_memory_size;
            curr_instr.destination_memory_size = nullptr;
            delete[] curr_instr.source_memory_size;
            curr_instr.source_memory_size = nullptr;
        }
    }
}

void BranchOrNot(uint32_t taken)
{
    curr_instr.has_mem_is_branch |= 0b00000001;
    if (taken != 0)
    {
        curr_instr.branch_taken = 1;
    }
}

void RegRead(uint32_t i, uint32_t index)
{
    if (!tracing_on)
        return;

    REG r = (REG)i;

    int already_found = 0;
    for (int i = 0; i < NUM_INSTR_SOURCES; i++)
    {
        if (curr_instr.source_register[i] == ((unsigned char)r))
        {
            already_found = 1;
            break;
        }
    }
    if (already_found == 0)
    {
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
        {
            if (curr_instr.source_register[i] == 0)
            {
                curr_instr.source_register[i] = (unsigned char)r;
                break;
            }
        }
    }
}

void RegWrite(REG i, uint32_t index)
{
    if (!tracing_on)
        return;

    REG r = (REG)i;

    int already_found = 0;
    for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
    {
        if (curr_instr.destination_register[i] == ((unsigned char)r))
        {
            already_found = 1;
            break;
        }
    }
    if (already_found == 0)
    {
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
        {
            if (curr_instr.destination_register[i] == 0)
            {
                curr_instr.destination_register[i] = (unsigned char)r;
                break;
            }
        }
    }
}

void MemoryRead(VOID *addr, uint32_t index, uint32_t read_size)
{
    if (!tracing_on)
        return;

    bool already_found = false;
    for (int i = 0; i < NUM_INSTR_SOURCES; i++)
    {
        if (curr_instr.source_memory_address[i] == ((unsigned long long int)addr))
        {
            already_found = true;
            break;
        }
    }

    if (!already_found)
    {
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
        {
            if (curr_instr.source_memory_address[i] == 0)
            {
                curr_instr.source_memory_address[i] = (unsigned long long int)addr;
                curr_instr.source_memory_size[i] = read_size;
                curr_instr.source_memory_value[i] = new unsigned char[read_size];

                if (debug)
                    std::cout << "Instruction read " << read_size << " bytes at 0x" << std::hex << addr << std::endl;

                if (PIN_SafeCopy(curr_instr.source_memory_value[i], addr, read_size) == read_size)
                {
                    if (debug)
                    {
                        std::cout << "Obtained memory from 0x" << std::hex << addr << "with value ";
                        for (uint32_t j = 0; j < read_size; j++)
                        {
                            std::cout << std::hex << (int)curr_instr.source_memory_value[i][j];
                        }
                        std::cout << std::dec << std::endl;
                    }
                }
                else
                {
                    delete curr_instr.source_memory_value[i];
                    curr_instr.source_memory_value[i] = nullptr;
                    curr_instr.source_memory_size[i] = 0;

                    if (debug)
                        std::cout << "Failed to obtain memory from 0x" << std::hex << addr << std::dec << std::endl;
                }

                break;
            }
        }
    }
}

void MemoryWriteCaptureAddress(VOID *addr, uint32_t index)
{
    if (!tracing_on)
        return;

    bool already_found = false;
    for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
    {
        if (curr_instr.destination_memory_address[i] == ((unsigned long long int)addr))
        {
            already_found = true;
            break;
        }
    }

    if (!already_found)
    {
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
        {
            if (curr_instr.destination_memory_address[i] == 0)
            {
                curr_instr.destination_memory_address[i] = (unsigned long long int)addr;
                memoryWriteIndex = i;
                break;
            }
        }
    }
}

void MemoryWriteCaptureValue(uint32_t index, uint32_t write_size)
{
    if (!tracing_on)
        return;

    if (memoryWriteIndex != -1)
    {
        curr_instr.destination_memory_size[memoryWriteIndex] = write_size;
        curr_instr.destination_memory_value[memoryWriteIndex] = new unsigned char[write_size];
        unsigned long long int *addr = (unsigned long long int *)(curr_instr.destination_memory_address[memoryWriteIndex]);

        if (debug)
            std::cout << "Instruction wrote " << write_size << " bytes at 0x" << std::hex << addr << std::dec << std::endl;

        if (PIN_SafeCopy(curr_instr.destination_memory_value[memoryWriteIndex], addr, write_size) == write_size)
        {
            if (debug)
            {
                std::cout << "Obtained memory from " << std::hex << addr << "with value ";
                for (uint32_t j = 0; j < write_size; j++)
                {
                    std::cout << std::hex << (int)curr_instr.destination_memory_value[memoryWriteIndex][j];
                }
                std::cout << std::dec << std::endl;
            }
        }
        else
        {
            delete curr_instr.destination_memory_value[memoryWriteIndex];
            curr_instr.destination_memory_size[memoryWriteIndex] = 0;
            curr_instr.destination_memory_value[memoryWriteIndex] = nullptr;

            if (debug)
                std::cout << "Failed to obtain memory from 0x" << std::hex << addr << std::endl;
        }

        memoryWriteIndex = -1;
    }
    else
    {
        if (debug)
            std::cout << "Memory write value instrumentation failed to find a memory write address" << std::endl;
    }
}

void MemoryWriteValueForCall()
{
    if (!tracing_on)
        return;

    if (memoryWriteIndex != -1)
    {
        curr_instr.destination_memory_size[memoryWriteIndex] = 0;
        curr_instr.destination_memory_value[memoryWriteIndex] = nullptr;
        memoryWriteIndex = -1;
    }
    else
    {
        curr_instr.destination_memory_size[memoryWriteIndex] = 8;
        curr_instr.destination_memory_value[memoryWriteIndex] = new unsigned char[8];
        unsigned long long int return_address = curr_instr.ip + SIZE_OF_CALL_INSTRUCTION;
        std::memcpy(curr_instr.destination_memory_value[memoryWriteIndex], &return_address, 8);

        memoryWriteIndex = -1;

        if (debug)
            std::cout << "Memory write value for call instruction at 0x" << std::hex << curr_instr.ip
                      << " with value " << return_address << std::dec << std::endl;
    }
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    uint32_t memOperands = INS_MemoryOperandCount(ins);

    // begin each instruction with this function
    uint32_t opcode = INS_Opcode(ins);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BeginInstruction,
                   IARG_INST_PTR,
                   IARG_UINT32, opcode,
                   IARG_UINT32, memOperands,
                   IARG_CALL_ORDER, CALL_ORDER_FIRST,
                   IARG_END);

    // instrument branch instructions
    if (INS_IsBranch(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot,
                       IARG_BRANCH_TAKEN,
                       IARG_END);

    // instrument register reads
    uint32_t readRegCount = INS_MaxNumRRegs(ins);
    for (uint32_t i = 0; i < readRegCount; i++)
    {
        uint32_t regNum = INS_RegR(ins, i);

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegRead,
                       IARG_UINT32, regNum,
                       IARG_UINT32, i,
                       IARG_END);
    }

    // instrument register writes
    uint32_t writeRegCount = INS_MaxNumWRegs(ins);
    for (uint32_t i = 0; i < writeRegCount; i++)
    {
        uint32_t regNum = INS_RegW(ins, i);

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegWrite,
                       IARG_UINT32, regNum,
                       IARG_UINT32, i,
                       IARG_END);
    }

    // instrument memory reads and writes

    // Iterate over each memory operand of the instruction.
    for (uint32_t memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            uint32_t read_size = INS_MemoryOperandSize(ins, memOp);
            if (read_size > MAX_MEMORY_SIZE)
            {
                skip_curr_instr = true;
                break;
            }

            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryRead,
                           IARG_MEMORYOP_EA, memOp,
                           IARG_UINT32, memOp,
                           IARG_UINT32, read_size,
                           IARG_END);
        }

        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryWriteCaptureAddress,
                           IARG_MEMORYOP_EA, memOp,
                           IARG_UINT32, memOp,
                           IARG_END);

            if (INS_HasFallThrough(ins))
            {
                uint32_t write_size = INS_MemoryOperandSize(ins, memOp);
                if (write_size > MAX_MEMORY_SIZE)
                {
                    skip_curr_instr = true;
                    break;
                }

                INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)MemoryWriteCaptureValue,
                               IARG_UINT32, memOp,
                               IARG_UINT32, write_size,
                               IARG_END);
            }
            else if (INS_IsCall(ins))
            {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryWriteValueForCall,
                               IARG_END);
            }
            else
            {
                if (debug)
                {
                    std::cout << "Warning: Memory write instrumentation at IPOINT_AFTER for instruction ** " << INS_Disassemble(ins) << " ** is invalid" << std::endl;
                    std::cout << "Opcode: " << std::dec << INS_Opcode(ins) << std::endl;
                }
            }
        }
    }

    // finalize each instruction with this function
    if (INS_HasFallThrough((ins)))
        INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)EndInstruction,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
    else
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EndInstruction,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
    // close the file if it hasn't already been closed
    if (!output_file_closed)
    {
        fclose(out);
        output_file_closed = true;
    }
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
        return Usage();

    const char *fileName = KnobOutputFile.Value().c_str();

    out = fopen(fileName, "wb");
    if (!out)
    {
        std::cout << "Couldn't open output trace file. Exiting." << std::endl;
        exit(1);
    }

    // Register function to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // cerr <<  "===============================================" << endl;
    // cerr <<  "This application is instrumented by the Champsim Trace Generator" << endl;
    // cerr <<  "Trace saved in " << KnobOutputFile.Value() << endl;
    // cerr <<  "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */