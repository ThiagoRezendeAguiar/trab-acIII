#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

using namespace std;

struct Instruction {
    string op;
    string rd;
    string rs1;
    string rs2;
    string imm;
    
    Instruction(string op, string rd, string rs1, string rs2, string imm)
        : op(op), rd(rd), rs1(rs1), rs2(rs2), imm(imm) {}
};

struct InstructionStatus {
    Instruction instruction;
    int issueTime, execCompleteTime, writeResultTime;
    
    InstructionStatus(Instruction instruction) 
        : instruction(instruction), issueTime(-1), execCompleteTime(-1), writeResultTime(-1) {}
};

struct ReservationStation {
    string name;
    string op;
    string qj, qk;
    string vj, vk;
    string rd;
    string A;
    bool busy;
    int instructionIndex;
    int remainingCycles;

    ReservationStation(string name) 
        : name(name), op(""), qj(""), qk(""), vj(""), vk(""), rd(""), A(""), busy(false), instructionIndex(-1), remainingCycles(-1) {}
    
    void reset() {
        op = qj = qk = vj = vk = rd = A = "";
        busy = false;
        instructionIndex = -1;
        remainingCycles = -1;
    }
};

struct Register {
    string qi;
    int value;

    Register() : qi(""), value(0) {}
};

class Tomasulo {
    private:
        queue<Instruction> instructions;
        vector<InstructionStatus> instructionsStatus;
        vector<ReservationStation> addRS;
        vector<ReservationStation> mulRS;
        vector<ReservationStation> loadStoreRS;
        unordered_map<string, Register> registers;
        unordered_map<string, int> memory;
        unordered_set<string> usedRegisters;

        int cycle;
        bool isCompleted;

        static const int ADD_SUB_LATENCY = 2;
        static const int MUL_LATENCY = 10;
        static const int DIV_LATENCY = 40;
        static const int LOAD_STORE_LATENCY = 2;

    public:
        Tomasulo(int addQuantity, int mulQuantity, int loadStoreQuantity): cycle(0), isCompleted(false) {
            addRS.reserve(addQuantity);
            mulRS.reserve(mulQuantity);
            loadStoreRS.reserve(loadStoreQuantity);

            for (int i = 0; i < addQuantity; i++) {
                addRS.emplace_back("ADD" + to_string(i+1));
            }

            for (int i = 0; i < mulQuantity; i++) {
                mulRS.emplace_back("MUL" + to_string(i+1));
            }

            for (int i = 0; i < loadStoreQuantity; i++) {
                loadStoreRS.emplace_back("LOAD" + to_string(i+1));
            }

            for (int i = 0; i < 32; i++) {
                string name = "R" + to_string(i);
                registers[name] = Register();
            }
        }

        bool loadInstructions(const string &filePath) {
            ifstream file(filePath);

            if (!file.is_open()) {
                cerr << "Error opening file: " << filePath << endl;
                return false;
            }

            string line;
            while (getline(file, line)) {
                if (line.empty() || line[0] == '#') continue;

                istringstream iss(line);
                string op, rd, rs1, rs2, imm;
                iss >> op;

                if (op == "ADD" || op == "SUB" || op == "MUL" || op == "DIV") {
                    iss >> rd >> rs1 >> rs2;
                    imm = "";
                    
                    usedRegisters.insert(rd);
                    usedRegisters.insert(rs1);
                    usedRegisters.insert(rs2);
                }
                else if (op == "LW") {
                    iss >> rd >> rs1 >> imm;
                    rs2 = "";
                    
                    usedRegisters.insert(rd);
                    usedRegisters.insert(rs1);
                }
                else if (op == "SW") {
                    iss >> rs2 >> rs1 >> imm;
                    rd = rs2;
                    
                    usedRegisters.insert(rd);
                    usedRegisters.insert(rs1);
                }
                else {
                    cerr << "Unknown instruction: " << op << endl;
                    continue;
                }
                
                instructions.push(Instruction(op, rd, rs1, rs2, imm));
            }

            file.close();
            return true;
        }

        void run() {
            while (!isCompleted) {
                cout << "\n=== Cycle " << cycle + 1 << " ===" << endl;

                writeResults();
                executeInstructions();

                if (!instructions.empty()) {
                    issueInstruction();
                }

                printState();
                cycle++;
                isCompleted = checkSimulationComplete();

                // cout << "Press Enter to advance to the next cycle...";
                // cin.get();
            }
            
            printFinalResults();
        }

        void setMemory(string index, int value) {
            memory[index] = value;
        }

    private:
        void issueInstruction() {
            if (instructions.empty()) return;

            Instruction instruction = instructions.front();
            ReservationStation* rs = getAvailableReservationStation(instruction.op);
            if (rs == nullptr) return;

            instructions.pop();

            instructionsStatus.push_back(InstructionStatus(instruction));
            int instructionIndex = instructionsStatus.size() - 1;
            
            auto& status = instructionsStatus[instructionIndex];
            status.issueTime = cycle + 1;

            rs->busy = true;
            rs->op = instruction.op;
            rs->instructionIndex = instructionIndex;

            if (instruction.op == "ADD" || instruction.op == "SUB") {
                rs->remainingCycles = ADD_SUB_LATENCY;
            } else if (instruction.op == "MUL") {
                rs->remainingCycles = MUL_LATENCY;
            } else if (instruction.op == "DIV") {
                rs->remainingCycles = DIV_LATENCY;
            } else if (instruction.op == "LW" || instruction.op == "SW") {
                rs->remainingCycles = LOAD_STORE_LATENCY;
            }

            if (instruction.op == "LW") {
                rs->rd = instruction.rd;
                if (registers[instruction.rs1].qi.empty()) {
                    rs->vj = instruction.rs1;
                    rs->qj = "";
                    rs->A = to_string(stoi(instruction.imm) + registers[instruction.rs1].value);
                } else {
                    rs->vj = instruction.rs1;
                    rs->qj = registers[instruction.rs1].qi;
                    rs->A = "";
                }
                rs->vk = instruction.imm;
                registers[instruction.rd].qi = rs->name;
            } else if (instruction.op == "SW") {
                rs->rd = instruction.rd;
                if (registers[instruction.rs1].qi.empty()) {
                    rs->vj = instruction.rs1;
                    rs->qj = "";
                    rs->A = to_string(stoi(instruction.imm) + registers[instruction.rs1].value); 
                } else {
                    rs->vj = instruction.rs1;
                    rs->qj = registers[instruction.rs1].qi;
                    rs->A = ""; 
                }
                
                if (registers[instruction.rd].qi.empty()) {
                    rs->vk = instruction.rd;
                    rs->qk = "";
                } else {
                    rs->vk = instruction.rd;
                    rs->qk = registers[instruction.rd].qi;
                }
            } else {
                if (registers[instruction.rs1].qi.empty()) {
                    rs->vj = instruction.rs1;
                    rs->qj = "";
                } else {
                    rs->vj = instruction.rs1;
                    rs->qj = registers[instruction.rs1].qi;
                }

                if (registers[instruction.rs2].qi.empty()) {
                    rs->vk = instruction.rs2;
                    rs->qk = "";
                } else {
                    rs->vk = instruction.rs2;
                    rs->qk = registers[instruction.rs2].qi;
                }

                rs->rd = instruction.rd; 
                registers[instruction.rd].qi = rs->name;
            }
        }

        void executeInstructions() {
            for(auto& rs: addRS) {
                if (rs.busy && rs.qj.empty() && rs.qk.empty() && rs.remainingCycles > 0) {
                    rs.remainingCycles--;
                    
                    if (rs.remainingCycles == 0) {
                        instructionsStatus.at(rs.instructionIndex).execCompleteTime = cycle + 1;
                    }
                }
            }

            for(auto& rs: mulRS) {
                if (rs.busy && rs.qj.empty() && rs.qk.empty() && rs.remainingCycles > 0) {
                    rs.remainingCycles--;
                    
                    if (rs.remainingCycles == 0) {
                        instructionsStatus.at(rs.instructionIndex).execCompleteTime = cycle + 1;
                    }
                }
            }

            for(auto& rs: loadStoreRS) {
                if (rs.busy && rs.qj.empty() && (rs.op != "SW" || rs.qk.empty()) && rs.remainingCycles > 0) {
                    if (rs.A.empty()) {
                        rs.A = to_string(stoi(rs.vk) + registers[rs.vj].value);
                    }

                    rs.remainingCycles--;
                    
                    if (rs.remainingCycles == 0) {
                        instructionsStatus.at(rs.instructionIndex).execCompleteTime = cycle + 1;
                    }
                }
            }
        }

        void writeResults() {
            for(auto& rs: addRS) {
                if (rs.busy && rs.remainingCycles == 0) {
                    int result = 0;

                    if (rs.op == "ADD") {
                        result = registers[rs.vj].value + registers[rs.vk].value;
                    } else if (rs.op == "SUB") {
                        result = registers[rs.vj].value - registers[rs.vk].value;
                    }

                    registers[rs.rd].value = result;
                    
                    if (registers[rs.rd].qi == rs.name) {
                        registers[rs.rd].qi = "";
                    }

                    auto& status = instructionsStatus.at(rs.instructionIndex);
                    status.writeResultTime = cycle + 1;

                    updateStations(rs.name);
                    rs.reset();
                }
            }

            for(auto& rs: mulRS) {
                if (rs.busy && rs.remainingCycles == 0) {
                    int result = 0;

                    if (rs.op == "MUL") {
                        result = registers[rs.vj].value * registers[rs.vk].value;
                    } else if (rs.op == "DIV") {
                        if (registers[rs.vk].value != 0) {
                            result = registers[rs.vj].value / registers[rs.vk].value;
                        } else {
                            cerr << "Warning: Division by zero detected!" << endl;
                            result = 0;
                        }
                    }

                    registers[rs.rd].value = result;
                    
                    if (registers[rs.rd].qi == rs.name) {
                        registers[rs.rd].qi = "";
                    }

                    auto& status = instructionsStatus.at(rs.instructionIndex);
                    status.writeResultTime = cycle + 1;

                    updateStations(rs.name);
                    rs.reset();
                }
            }

            for(auto& rs: loadStoreRS) {
                if (rs.busy && rs.remainingCycles == 0) {
                    if (rs.op == "LW") {
                        registers[rs.rd].value = memory[rs.A];
                        
                        if (registers[rs.rd].qi == rs.name) {
                            registers[rs.rd].qi = "";
                        }
                    } else if (rs.op == "SW") {
                        memory[rs.A] = registers[rs.rd].value;
                    }

                    auto& status = instructionsStatus.at(rs.instructionIndex);
                    status.writeResultTime = cycle + 1;

                    updateStations(rs.name);
                    rs.reset();
                }
            }
        }

        ReservationStation* getAvailableReservationStation(const string& op) {
            if (op == "ADD" || op == "SUB") {
                for (auto& rs : addRS) {
                    if (!rs.busy) return &rs;
                }
            } else if (op == "MUL" || op == "DIV") {
                for (auto& rs : mulRS) {
                    if (!rs.busy) return &rs;
                }
            } else if (op == "LW" || op == "SW") {
                for (auto& rs : loadStoreRS) {
                    if (!rs.busy) return &rs;
                }
            }

            return nullptr;
        }

        void updateStations(const string& name) {
            for(auto& rs : addRS) {
                if (rs.busy) {
                    if (rs.qj == name) rs.qj = "";
                    if (rs.qk == name) rs.qk = "";
                }
            }

            for(auto& rs : mulRS) {
                if (rs.busy) {
                    if (rs.qj == name) rs.qj = "";
                    if (rs.qk == name) rs.qk = "";
                }
            }

            for(auto& rs : loadStoreRS) {
                if (rs.busy) {
                    if (rs.qj == name) rs.qj = "";
                    if (rs.qk == name) rs.qk = "";
                }
            }
        }

        bool checkSimulationComplete() {
            if (!instructions.empty()) return false;

            for (const auto& rs : addRS) {
                if (rs.busy) return false;
            }
            for (const auto& rs : mulRS) {
                if (rs.busy) return false;
            }
            for (const auto& rs : loadStoreRS) {
                if (rs.busy) return false;
            }
            
            return true;
        }

        void printState() {
            cout << "\nReservation Station ADD/SUB:" << endl;
            cout << "Name\tBusy\tOp\tVj\tVk\tQj\tQk\tRD" << endl;
            for (const auto& rs : addRS) {
                cout << rs.name << "\t" 
                     << (rs.busy ? "Yes" : "No") << "\t"
                     << (rs.busy ? rs.op : "-") << "\t"
                     << (!rs.vj.empty() ? rs.vj : "-") << "\t"
                     << (!rs.vk.empty() ? rs.vk : "-") << "\t"
                     << (!rs.qj.empty() ? rs.qj : "-") << "\t"
                     << (!rs.qk.empty() ? rs.qk : "-") << "\t"
                     << (!rs.rd.empty() ? rs.rd : "-") << endl;
            }
            
            cout << "\nReservation Station MUL/DIV:" << endl;
            cout << "Name\tBusy\tOp\tVj\tVk\tQj\tQk\tRD" << endl;
            for (const auto& rs : mulRS) {
                cout << rs.name << "\t" 
                     << (rs.busy ? "Yes" : "No") << "\t"
                     << (rs.busy ? rs.op : "-") << "\t"
                     << (!rs.vj.empty() ? rs.vj : "-") << "\t"
                     << (!rs.vk.empty() ? rs.vk : "-") << "\t"
                     << (!rs.qj.empty() ? rs.qj : "-") << "\t"
                     << (!rs.qk.empty() ? rs.qk : "-") << "\t"
                     << (!rs.rd.empty() ? rs.rd : "-") << endl;
            }
            
            cout << "\nReservation Station LOAD/STORE:" << endl;
            cout << "Name\tBusy\tOp\tA\tVj\tVk\tQj\tQk\tRD" << endl;
            for (const auto& rs : loadStoreRS) {
                cout << rs.name << "\t" 
                     << (rs.busy ? "Yes" : "No") << "\t"
                     << (rs.busy ? rs.op : "-") << "\t"
                     << (!rs.A.empty() ? rs.A : "-") << "\t"
                     << (!rs.vj.empty() ? rs.vj : "-") << "\t"
                     << (!rs.vk.empty() ? rs.vk : "-") << "\t"
                     << (!rs.qj.empty() ? rs.qj : "-") << "\t"
                     << (!rs.qk.empty() ? rs.qk : "-") << "\t"
                     << (!rs.rd.empty() ? rs.rd : "-") << endl;
            }
            
            cout << "\nRegisters Status:" << endl;
            cout << "Reg\tValor\tQi" << endl;
            
            vector<string> sortedRegs(usedRegisters.begin(), usedRegisters.end());
            sort(sortedRegs.begin(), sortedRegs.end(), [](const string& a, const string& b) {
                return stoi(a.substr(1)) < stoi(b.substr(1));
            });
            
            for (const auto& regName : sortedRegs) {
                const auto& reg = registers[regName];
                cout << regName << "\t" 
                     << reg.value << "\t"
                     << (reg.qi.empty() ? "-" : reg.qi) << endl;
            }
        }

        void printFinalResults() {
            cout << "\n=== Final Results ===" << endl;
            
            cout << "\nCronograma de Execução das Instruções:" << endl;
            cout << "Instrução\t\tIssue\t\tExecComp\tWriteResult" << endl;
            for (size_t i = 0; i < instructionsStatus.size(); i++) {
                const auto& instr = instructionsStatus[i].instruction;
                cout << instr.op << " " 
                     << instr.rd << " " << instr.rs1;
                if (!instr.rs2.empty()) cout << " " << instr.rs2;
                else if (!instr.imm.empty()) cout << " " << instr.imm;
                
                cout << "\t\t" << instructionsStatus[i].issueTime << "\t\t"
                     << instructionsStatus[i].execCompleteTime << "\t\t"
                     << instructionsStatus[i].writeResultTime << endl;
            }
            
            cout << "\nValores Finais dos Registradores Utilizados:" << endl;
            vector<string> sortedRegs(usedRegisters.begin(), usedRegisters.end());
            sort(sortedRegs.begin(), sortedRegs.end(), [](const string& a, const string& b) {
                return stoi(a.substr(1)) < stoi(b.substr(1));
            });
            
            for (const auto& regName : sortedRegs) {
                const auto& reg = registers[regName];
                cout << regName << " = " << reg.value << endl;
            }
            
            cout << "\nValores Finais da Memória (apenas posições utilizadas):" << endl;
            vector<pair<string, int>> sortedMem;
            for (const auto& mem : memory) {
                if (mem.second != 0) {
                    sortedMem.push_back(mem);
                }
            }
            
            sort(sortedMem.begin(), sortedMem.end(), [](const auto& a, const auto& b) {
                return stoi(a.first) < stoi(b.first);
            });
            
            for (const auto& mem : sortedMem) {
                cout << "Memory[" << mem.first << "] = " << mem.second << endl;
            }
        }
};

int main() {
    Tomasulo simulator(3, 2, 3);

    string filePath;
    cout << "Enter the file path:" << endl;
    cin >> filePath;
    cin.ignore();

    if (!simulator.loadInstructions(filePath)) {
        cerr << "Error loading instructions.";
        return 1;
    }

    simulator.setMemory("10", 5);
    simulator.setMemory("20", 10);
    simulator.setMemory("30", 3);

    simulator.run();
    
    return 0;
}