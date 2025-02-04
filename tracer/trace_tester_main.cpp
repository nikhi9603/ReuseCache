#include <iostream>

#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

class input_instr
{
public:
    // instruction pointer or PC (Program Counter)
    uint64_t ip;

    // branch info
    uint8_t is_branch;
    uint8_t branch_taken;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    uint8_t source_registers[NUM_INSTR_SOURCES];           // input registers

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES];           // input memory

    uint8_t destination_memory_size[NUM_INSTR_DESTINATIONS]; // output memory sizes
    uint8_t source_memory_size[NUM_INSTR_SOURCES];           // input memory sizes

    unsigned char *destination_memory_value[NUM_INSTR_DESTINATIONS]; // output memory values
    unsigned char *source_memory_value[NUM_INSTR_SOURCES];           // input memory values

    input_instr()
    {
        ip = 0;
        is_branch = 0;
        branch_taken = 0;

        for (uint32_t i = 0; i < NUM_INSTR_SOURCES; i++)
        {
            source_registers[i] = 0;
            source_memory[i] = 0;
            source_memory_size[i] = 0;
            source_memory_value[i] = nullptr;
        }

        for (uint32_t i = 0; i < NUM_INSTR_DESTINATIONS; i++)
        {
            destination_registers[i] = 0;
            destination_memory[i] = 0;
            destination_memory_size[i] = 0;
            destination_memory_value[i] = nullptr;
        }
    }

    static size_t get_size()
    {
        return sizeof(ip) + sizeof(is_branch) + sizeof(branch_taken) +
               sizeof(destination_registers) + sizeof(source_registers);
    }
};

int main(int argc, char **argv)
{
    char *trace_file = argv[1];

    char *gunzip_command = new char[1024];
    sprintf(gunzip_command, "gunzip -c %s", trace_file);

    FILE *trace = popen(gunzip_command, "r");

    input_instr trace_read_instr;
    size_t instr_size = input_instr::get_size();

    while (fread(&trace_read_instr, instr_size, 1, trace))
    {
        bool hasMemOperands = ((trace_read_instr.is_branch & 0b00000010) == 2);
        if (hasMemOperands)
        {
            for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
            {
                if (trace_read_instr.destination_memory_size[i] > 0)
                {
                    trace_read_instr.destination_memory_value[i] = new unsigned char[trace_read_instr.destination_memory_size[i]];
                    fread(trace_read_instr.destination_memory_value[i], trace_read_instr.destination_memory_size[i], 1, trace);
                }
            }

            for (int i = 0; i < NUM_INSTR_SOURCES; i++)
            {
                if (trace_read_instr.source_memory_size[i] > 0)
                {
                    trace_read_instr.source_memory_value[i] = new unsigned char[trace_read_instr.source_memory_size[i]];
                    fread(trace_read_instr.source_memory_value[i], trace_read_instr.source_memory_size[i], 1, trace);
                }
            }
        }

        std::cout << "Instruction: " << trace_read_instr.ip << std::endl;
        std::cout << "Branch: " << trace_read_instr.is_branch << std::endl;
        std::cout << "Branch taken: " << trace_read_instr.branch_taken << std::endl;
        std::cout << "Destination registers: ";
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
            std::cout << (int)trace_read_instr.destination_registers[i] << " ";
        std::cout << std::endl;
        std::cout << "Source registers: ";
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
            std::cout << (int)trace_read_instr.source_registers[i] << " ";
        std::cout << std::endl;
        std::cout << "Source memory:" << std::endl;
        ;
        for (int i = 0; i < NUM_INSTR_SOURCES; i++)
        {
            if (trace_read_instr.source_memory_size[i] > 0)
            {
                std::cout << "Address: " << trace_read_instr.source_memory[i] << " ";
                std::cout << "Size: " << trace_read_instr.source_memory_size[i] << " Value: ";
                for (int j = 0; j < trace_read_instr.source_memory_size[i]; j++)
                    std::cout << (int)trace_read_instr.source_memory_value[i][j];
                std::cout << std::endl;
            }
        }
        std::cout << "Destination memory: " << std::endl;
        for (int i = 0; i < NUM_INSTR_DESTINATIONS; i++)
        {
            if (trace_read_instr.destination_memory_size[i] > 0)
            {
                std::cout << "Address: " << trace_read_instr.destination_memory[i] << " ";
                std::cout << "Size: " << trace_read_instr.destination_memory_size[i] << " Value: ";
                for (int j = 0; j < trace_read_instr.destination_memory_size[i]; j++)
                    std::cout << (int)trace_read_instr.destination_memory_value[i][j];
                std::cout << std::endl;
            }
        }
    }
}