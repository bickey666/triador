#undef NDEBUG
#include <cassert>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <array>
#include <vector>
#include <utility>
#include <cstdlib>
#include <ctime>

#include "triador.h"

using namespace std;

const array<string, 9> allowed_opcodes = {"EX", "JP", "SK", "OP", "RR", "R1", "R2", "R3", "R4"};

Triador::Triador() {
    PC = -364; // the program counter is initalized
    // N.B. the rest of the memory is not guaranteed to be initialized!
    std::srand(std::time(nullptr));
    for (int &r : R)
        r = std::rand()%27 - 13;
    C = std::rand()%2 - 1;

    fHalt = false;
}

void Triador::assert_memory_state() {
    for (const int &r : R)
        assert(abs(r)<=13);
    assert(abs(C)<=1);
    assert(abs(PC)<=364);
}

void Triador::display_memory_state() {
    for (int i=0; i<13; i++) {
        if (i<9) cout << " ";
        cout << "R" << (i+1) << " ";
    }
    cout << " C   PC      opcode" << endl;
    for (const int &r : R)
        cout << setw(3) << r << " ";
    cout << setw(2) << C  << "  " << setw(4) << PC << "     ";
    if (program.size()) cout << allowed_opcodes[program[PC+364].first+4] << " " << program[PC+364].second;
    else cout << "-- ---";
    cout << endl << endl;
}

void Triador::load_program(const char *filename) {
    // The program file must contain a single instruction per line.
    // An instruction is a two-character opcode followed by an integer number;
    // any character beyond the instruction is discarded (considered to be a comment).
    // Therefore, each line must contain one of the following instructions,
    // where arg means an integer number with values from -13 to +13:
    // EX arg
    // JP arg
    // SK arg
    // OP arg
    // RR arg
    // R1 arg
    // R2 arg
    // R3 arg
    // R4 arg
    program = std::vector<std::pair<int, int> >();
    ifstream in;
    in.open(filename, ifstream::in);
    if (in.fail()) return;
    string line;
    while (!in.eof()) {
        getline(in, line);
        if (!line.length()) break;

        istringstream iss(line.c_str());
        string opcode("trash"); // invalid value initialization
        int arg = 14;           // invalid value initialization
        iss >> opcode >> arg;

        int instruction = -5; // out of bounds initialization
        for (int i=0; i<9; i++) {
            if (opcode!=allowed_opcodes[i]) continue;
            instruction = i-4;
            break;
        }

        if (!iss || abs(instruction)>4 || abs(arg)>13) {  // is it a valid instruction?
            cerr << "Warning: bad program formatting" << endl;
            break;
        }

        program.push_back(make_pair(instruction, arg));
    }
}

// compute ttt[3] such that value = ttt[0] + 3*ttt[1] + 9*ttt[2]
void binary_to_ternary(const int value, int ttt[3]) {
    assert(abs(value)<=13);
    int n = value;
    bool neg = n < 0;
    if (neg) n = -n;

    for (int i=0; i<3; i++) {
        int r = n % 3; // remainder operator over negative values is implementation-defined, thus bool neg
        if (r == 0)
            ttt[i] = 0;
        else if (r == 1)
            ttt[i] = 1;
        else {
            ttt[i] = -1;
            n++;
        }
        n /= 3;
    }
    if (neg) for (int i=0; i<3; i++)
        ttt[i] = -ttt[i];
}

void Triador::cycle() {
    if (!program.size()) return;
    int opcode = program[PC+364].first;
    int arg    = program[PC+364].second;
    assert(abs(opcode)<=4 && abs(arg)<=13);

    switch (opcode) {
        case -4: { // EX: halt and catch fire if not processed
                     if (!do_ex(arg))
                     {
                         fHalt = true;
                         return;
                     }
                 } break;
        case -3: { // JP: jump instruction
                     PC = R[12]*27 + arg;
                     return;
                 } break;
        case -2: { // SK: conditional skips of the next operation
                     if (abs(arg)>1) { // skip w.r.t R1-R4 values
                         int reg = R[(abs(arg)-2)/3]; // register value
                         int cmp = (abs(arg)-2)%3;    // comp operation
                         if (arg>0) {
                             if (cmp==0 && reg <0) PC++;
                             if (cmp==1 && reg==0) PC++;
                             if (cmp==2 && reg >0) PC++;
                         } else {
                             if (cmp==0 && reg>=0) PC++;
                             if (cmp==1 && reg!=0) PC++;
                             if (cmp==2 && reg<=0) PC++;
                         }
                     } else { // skip w.r.t C value
                         if (arg==-1 && C==-1) PC++;
                         if (arg== 0 && C== 0) PC++;
                         if (arg== 1 && C== 1) PC++;
                     }
                 } break;
        case -1: { // OP: unary tritwise operation
                     int ttt_mem[3] = {0,0,0};
                     int ttt_arg[3] = {0,0,0};
                     int ttt_res[3] = {0,0,0};
                     binary_to_ternary(R[0], ttt_mem);
                     binary_to_ternary(arg,  ttt_arg);
                     for (int i=0; i<3; i++)
                         ttt_res[i] = ttt_arg[1-ttt_mem[i]];
                     R[0] = ttt_res[0] + 3*ttt_res[1] + 9*ttt_res[2];
                 } break;
        case 0: { // RR: copying between registers
                    if (!arg) break; // "RR OOO" means "do nothing"
                    if (abs(arg)==1) { // RR with -1 or +1 argument is an increment/decrement
                        R[0] += arg;
                        if (abs(R[0])<=13) C = 0;
                        else if (R[0]> 13) { C =  1; R[0] -= 27; } // emulate the overflow
                        else if (R[0]<-13) { C = -1; R[0] += 27; } // and set the borrow/carry flag
                    } else
                        R[arg<0 ? -arg-1 : 0] = R[arg<0 ? 0 : arg-1]; // RR with argument >1 or <-1 is a copy
                } break;
        case 1: { // R1: write arg to the register R1
                    R[0] = arg;
                } break;
        case 2: { // R2: write arg to the register R2
                    R[1] = arg;
                } break;
        case 3: { // R3: write arg to the register R3
                    R[2] = arg;
                } break;
        case 4: { // R4: write arg to the register R4
                    R[3] = arg;
                } break;
    }

    PC++; // advance the program counter
}

void Triador::run(bool verbose) {
    assert_memory_state();
    if (verbose) display_memory_state();
    if (!program.size()) return;
    while (1) {
        cycle();
        assert_memory_state();

        if (PC+364>=(int)program.size()) {
            std::cerr << "Warning: PC points outside the program, halting Triador" << std::endl;
            break;
        }
        if (fHalt) break; // halt and catch fire
        if (verbose) display_memory_state(); // moved here to prevent memory state printing twice on halt
    }
}

