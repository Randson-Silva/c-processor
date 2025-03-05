#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define MEMORY_SIZE 255
#define STACK_POINTER_BASE 0x8200

typedef struct Processor {
    uint16_t Registers[8];
    uint16_t ProgramCounter;
    uint32_t StackPointer;
    uint16_t InstructionRegister;
    int Carry, Overflow, Zero, Sign;
} Processor;

Processor proc = {{0}, 0x0000, 0x8200, 0x0000, 0, 0, 0, 0};
uint8_t mainMemory[MEMORY_SIZE];
uint8_t dataMemory[MEMORY_SIZE];
uint8_t accessedMemory[MEMORY_SIZE] = {0};
uint8_t stack[MEMORY_SIZE];
bool stackAccessed[MEMORY_SIZE] = {false};

#define STACK_END (proc.StackPointer - STACK_POINTER_BASE)

uint16_t highestAddress = 0;

void LoadFile(const char *fileName) {
    char buffer[20];
    uint16_t address, instruction;

    FILE *file = fopen(fileName, "r");
    if (!file) {
        printf("Erro ao abrir o arquivo!");
        exit(1);
    }

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (sscanf(buffer, "%4hx: 0x%4hx", &address, &instruction) == 2) {
            mainMemory[address] = instruction & 0xFF;
            mainMemory[address + 1] = (instruction >> 8) & 0xFF;
            if (address > highestAddress) {
                highestAddress = address;
            }
        }
    }

    fclose(file);
}

void PrintRegisters() {
    for (int i = 0; i < 8; i++) {
        printf("R%d: 0x%04X\n", i, proc.Registers[i]);
    }
}

void PrintStack() {
    while (proc.StackPointer > 0x8100) {
        proc.StackPointer -= 2;

        uint16_t value = stack[STACK_END] | (stack[STACK_END + 1] << 8);
        if (stackAccessed[STACK_END]) {
            printf("0x%04X: 0x%04X\n", proc.StackPointer, value);
        }
    }
}

void PrintMem() {
    for (int i = 0; i < MEMORY_SIZE; i += 1) {
        if (accessedMemory[i]) {
            uint16_t value = dataMemory[i] | (dataMemory[i + 1] << 8);
            printf("0x%04X: 0x%04X\n", i, value);
        }
    }
}

void DisplayState() {
    printf("REGISTRADORES:\n");
    PrintRegisters();
    printf("PC: 0x%04X SP: 0x%04X\n", proc.ProgramCounter, proc.StackPointer);
    printf("FLAGS:\n");
    printf("Carry: %d\nOverflow: %d\nZero: %d\nSign: %d\n", proc.Carry, proc.Overflow, proc.Zero, proc.Sign);
    printf("MEMÓRIA DE DADOS:\n");
    PrintMem();
    printf("PILHA:\n");
    PrintStack();
}

void ExecuteInstructions() {
    while (1) {
        proc.InstructionRegister = mainMemory[proc.ProgramCounter] | (mainMemory[proc.ProgramCounter + 1] << 8);
        proc.ProgramCounter += 2;
        uint8_t opcode = (proc.InstructionRegister & 0xF000) >> 12;

        if (proc.InstructionRegister == 0xFFFF) {
            break;
        }

        if ((proc.InstructionRegister & 0xF800) >> 11 == 0 && (proc.InstructionRegister & 0x0003) == 0 &&
            (proc.InstructionRegister & 0x00FC) >> 2 != 0) {
            break;
        }

        if (proc.InstructionRegister == 0x0000) {
            DisplayState();
        }

        switch (opcode) {
            case 0x1: {
                uint16_t bit11 = (proc.InstructionRegister & 0x0800) >> 11;
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;

                if (bit11) {
                    uint8_t immediate = (proc.InstructionRegister & 0x00FF);
                    proc.Registers[destReg] = immediate;
                } else {
                    uint16_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;
                    proc.Registers[destReg] = proc.Registers[sourceReg];
                }
            }
            break;
            case 0x2: {
                uint8_t bit11 = (proc.InstructionRegister & 0x0800) >> 11;
                uint8_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;

                if (bit11) {
                    uint8_t immediate = ((proc.InstructionRegister & 0x0700) >> 3) | (proc.InstructionRegister & 0x001F);

                    dataMemory[proc.Registers[sourceReg]] = immediate;
                    accessedMemory[proc.Registers[sourceReg]] = 1;
                } else {
                    uint8_t tempReg = (proc.InstructionRegister & 0x001C) >> 2;
                    dataMemory[proc.Registers[sourceReg]] = proc.Registers[tempReg] & 0xFF;
                    dataMemory[proc.Registers[sourceReg] + 1] = (proc.Registers[tempReg] >> 8) & 0xFF;
                    accessedMemory[proc.Registers[sourceReg]] = 1;
                }
            }
            break;
            case 0x3: {
                uint8_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint8_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;
                proc.Registers[destReg] = dataMemory[proc.Registers[sourceReg]] | (dataMemory[proc.Registers[sourceReg] + 1] << 8);
            }
            break;
            case 0x4: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

                proc.Registers[destReg] = proc.Registers[sourceReg1] + proc.Registers[sourceReg2];

                uint32_t result = proc.Registers[sourceReg1] + proc.Registers[sourceReg2];

                proc.Carry = (result > 0xFFFF) ? 1 : 0;

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0x5: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

                proc.Registers[destReg] = proc.Registers[sourceReg1] - proc.Registers[sourceReg2];

                uint32_t result = proc.Registers[sourceReg1] - proc.Registers[sourceReg2];

                proc.Carry = (result > 0xFFFF) ? 1 : 0;

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0x6: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

                proc.Registers[destReg] = proc.Registers[sourceReg1] * proc.Registers[sourceReg2];

                uint32_t result = proc.Registers[sourceReg1] * proc.Registers[sourceReg2];

                proc.Carry = (result > 0xFFFF) ? 1 : 0;

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0x7: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

                proc.Registers[destReg] = proc.Registers[sourceReg1] & proc.Registers[sourceReg2];

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0x8: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

                proc.Registers[destReg] = proc.Registers[sourceReg1] | proc.Registers[sourceReg2];

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0x9: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;

                proc.Registers[destReg] = ~proc.Registers[sourceReg];

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0xA: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

                proc.Registers[destReg] = proc.Registers[sourceReg1] ^ proc.Registers[sourceReg2];

                proc.Zero = (proc.Registers[destReg] == 0) ? 1 : 0;
                proc.Sign = (proc.Registers[destReg] & 0x8000) ? 1 : 0;
            }
            break;
            case 0xB: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t immediate = proc.InstructionRegister & 0x001F;

                proc.Registers[destReg] = proc.Registers[sourceReg] >> immediate;
            }
            break;
            case 0xC: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t immediate = proc.InstructionRegister & 0x001F;

                proc.Registers[destReg] = proc.Registers[sourceReg] << immediate;
            }
            break;
            case 0xD: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t leastSignificantBit = proc.Registers[sourceReg] & 0x0001;

                proc.Registers[destReg] = (proc.Registers[sourceReg] >> 1) | (leastSignificantBit << 15);
            }
            break;
            case 0xE: {
                uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;
                uint16_t sourceReg = (proc.InstructionRegister & 0x00E0) >> 5;
                uint16_t mostSignificantBit = (proc.Registers[sourceReg] & 0x8000) >> 15;

                proc.Registers[destReg] = (proc.Registers[sourceReg] << 1) | mostSignificantBit;
            }
            break;
            default:
                break;
        }

        if ((proc.InstructionRegister & 0xF800) == 0x0000 && (proc.InstructionRegister & 0x0003) == 0x0003) {
            uint16_t sourceReg1 = (proc.InstructionRegister & 0x00E0) >> 5;
            uint16_t sourceReg2 = (proc.InstructionRegister & 0x001C) >> 2;

            int16_t result = proc.Registers[sourceReg1] - proc.Registers[sourceReg2];

            proc.Zero = (result == 0) ? 1 : 0;
            proc.Sign = (proc.Registers[sourceReg1] < proc.Registers[sourceReg2]) ? 1 : 0;
        }

        if ((proc.InstructionRegister & 0xF800) == 0x0000 && (proc.InstructionRegister & 0x0003) == 0x0001) {
            uint16_t sourceReg = (proc.InstructionRegister & 0x001C) >> 2;

            proc.StackPointer -= 2;

            stack[STACK_END] = proc.Registers[sourceReg] & 0x00FF;
            stack[STACK_END + 1] = (proc.Registers[sourceReg] & 0xFF00) >> 8;
            stackAccessed[STACK_END] = true;
        }

        if ((proc.InstructionRegister & 0xF800) == 0x0000 && (proc.InstructionRegister & 0x0003) == 0x0002) {
            uint16_t destReg = (proc.InstructionRegister & 0x0700) >> 8;

            proc.Registers[destReg] = stack[STACK_END] | (stack[STACK_END + 1] << 8);

            proc.StackPointer += 2;
        }

        if ((proc.InstructionRegister & 0xF000) == 0x0000 && (proc.InstructionRegister & 0x0800) == 0x0800) {
            uint16_t immediate = (proc.InstructionRegister & 0x07FC) >> 2;

            if (immediate & 0x0100) {
                immediate |= 0xFE00;
            }

            uint8_t branchType = (proc.InstructionRegister & 0x0003);

            if (branchType == 0x0) {
                proc.ProgramCounter += immediate;
                if (proc.ProgramCounter == highestAddress) {
                    break;
                }
            } else if (branchType == 0x1) {
                if (proc.Zero == 1 && proc.Sign == 0) {
                    proc.ProgramCounter += immediate;
                    if (proc.ProgramCounter == highestAddress) {
                        break;
                    }
                }
            } else if (branchType == 0x2) {
                if (proc.Zero == 0 && proc.Sign == 1) {
                    proc.ProgramCounter += immediate;
                    if (proc.ProgramCounter == highestAddress) {
                        break;
                    }
                }
            } else if (branchType == 0x3) {
                if (proc.Zero == 0 && proc.Sign == 0) {
                    proc.ProgramCounter += immediate;
                    if (proc.ProgramCounter == highestAddress) {
                        break;
                    }
                }
            }
        }

        if (proc.ProgramCounter > highestAddress) {
            break;
        }
    }
}

int main() {
    LoadFile("./teste1.txt");

    ExecuteInstructions();
    DisplayState();

    return 0;
}

// TODO: manage utf8 encoding error