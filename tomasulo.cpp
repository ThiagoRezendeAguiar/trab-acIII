#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

using namespace std;

struct Instruction
{
    string op;
    string rd;
    string rs1;
    string rs2;
    string imm;

    Instruction(string op, string rd, string rs1, string rs2, string imm)
        : op(op), rd(rd), rs1(rs1), rs2(rs2), imm(imm) {}
};

struct InstructionStatus
{
    Instruction instruction;
    int issueTime, execCompleteTime, writeResultTime, commitTime;

    InstructionStatus(Instruction instruction)
        : instruction(instruction), issueTime(-1), execCompleteTime(-1), writeResultTime(-1), commitTime(-1) {}
};

struct ROBEntry
{
    bool busy;
    bool ready;
    string destination;
    int value;
    string type;
    int instructionIndex;
};

struct ReservationStation
{
    string name;
    string op;
    int qj, qk;
    int vj, vk;
    int destRobTag;
    string addr;
    string imm;
    bool busy;
    int instructionIndex;
    int remainingCycles;

    ReservationStation(string name)
        : name(name), op(""), qj(-1), qk(-1), vj(0), vk(0), destRobTag(-1), addr(""), imm(""),
          busy(false), instructionIndex(-1), remainingCycles(-1) {}

    void reset()
    {
        op = "";
        qj = qk = -1;
        vj = vk = 0;
        destRobTag = -1;
        addr = "";
        imm = "";
        busy = false;
        instructionIndex = -1;
        remainingCycles = -1;
    }
};

struct Register
{
    int robTag;
    int value;

    Register() : robTag(-1), value(0) {}
};

class Tomasulo
{
private:
    queue<Instruction> instructions;
    vector<InstructionStatus> instructionsStatus;
    vector<ReservationStation> addRS;
    vector<ReservationStation> mulRS;
    vector<ReservationStation> loadStoreRS;
    vector<ROBEntry> rob;
    unordered_map<string, Register> registers;
    unordered_map<string, int> memory;
    unordered_set<string> usedRegisters;

    int cycle;
    bool isCompleted;
    int robSize;
    int robHead, robTail;

    static const int ADD_SUB_LATENCY = 2;
    static const int MUL_LATENCY = 10;
    static const int DIV_LATENCY = 40;
    static const int LOAD_STORE_LATENCY = 2;

public:
    Tomasulo(int addQuantity, int mulQuantity, int loadStoreQuantity, int robSize = 6)
        : cycle(0), isCompleted(false), robSize(robSize), robHead(0), robTail(0)
    {
        rob.resize(robSize);
        addRS.reserve(addQuantity);
        mulRS.reserve(mulQuantity);
        loadStoreRS.reserve(loadStoreQuantity);

        for (int i = 0; i < addQuantity; i++)
        {
            addRS.emplace_back("ADD" + to_string(i + 1));
        }

        for (int i = 0; i < mulQuantity; i++)
        {
            mulRS.emplace_back("MUL" + to_string(i + 1));
        }

        for (int i = 0; i < loadStoreQuantity; i++)
        {
            loadStoreRS.emplace_back("LOAD" + to_string(i + 1));
        }

        for (int i = 0; i < 32; i++)
        {
            string name = "R" + to_string(i);
            registers[name] = Register();
        }
    }

    bool loadInstructions(const string &filePath)
    {
        ifstream file(filePath);

        if (!file.is_open())
        {
            cerr << "Error opening file: " << filePath << endl;
            return false;
        }

        string line;
        while (getline(file, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            istringstream iss(line);
            string op, rd, rs1, rs2, imm;
            iss >> op;

            if (op == "ADD" || op == "SUB" || op == "MUL" || op == "DIV")
            {
                iss >> rd >> rs1 >> rs2;
                imm = "";

                usedRegisters.insert(rd);
                usedRegisters.insert(rs1);
                usedRegisters.insert(rs2);
            }
            else if (op == "LW")
            {
                iss >> rd >> rs1 >> imm;
                rs2 = "";

                usedRegisters.insert(rd);
                usedRegisters.insert(rs1);
            }
            else if (op == "SW")
            {
                iss >> rs2 >> rs1 >> imm;
                rd = rs2;

                usedRegisters.insert(rd);
                usedRegisters.insert(rs1);
            }
            else
            {
                cerr << "Unknown instruction: " << op << endl;
                continue;
            }

            instructions.push(Instruction(op, rd, rs1, rs2, imm));
        }

        file.close();
        return true;
    }

    void run()
    {
        while (!isCompleted)
        {
            cout << "\n=== Cycle " << cycle + 1 << " ===" << endl;

            commit();
            writeResults();
            executeInstructions();
            issueInstruction();

            printState();
            cycle++;
            isCompleted = checkSimulationComplete();
        }

        printFinalResults();
    }

    void setRegister(string index, int value) 
    {
        registers[index].value = value;
    }


    void setMemory(string index, int value)
    {
        memory[index] = value;
    }

private:
    void issueInstruction()
    {
        if (instructions.empty())
            return;

        if (rob[robTail].busy)
            return;

        Instruction instruction = instructions.front();
        ReservationStation *rs = getAvailableReservationStation(instruction.op);
        if (rs == nullptr)
            return;

        instructions.pop();

        instructionsStatus.push_back(InstructionStatus(instruction));
        int instructionIndex = instructionsStatus.size() - 1;

        auto &status = instructionsStatus[instructionIndex];
        status.issueTime = cycle + 1;

        string type;
        string destination;
        if (instruction.op == "LW")
        {
            type = "load";
            destination = instruction.rd;
        }
        else if (instruction.op == "SW")
        {
            type = "store";
            destination = "";
        }
        else
        {
            type = "arith";
            destination = instruction.rd;
        }

        rob[robTail] = {true, false, destination, 0, type, instructionIndex};

        rs->busy = true;
        rs->op = instruction.op;
        rs->instructionIndex = instructionIndex;
        rs->destRobTag = robTail;

        if (instruction.op == "ADD" || instruction.op == "SUB")
        {
            rs->remainingCycles = ADD_SUB_LATENCY;
        }
        else if (instruction.op == "MUL")
        {
            rs->remainingCycles = MUL_LATENCY;
        }
        else if (instruction.op == "DIV")
        {
            rs->remainingCycles = DIV_LATENCY;
        }
        else if (instruction.op == "LW" || instruction.op == "SW")
        {
            rs->remainingCycles = LOAD_STORE_LATENCY;
            rs->imm = instruction.imm;
        }

        if (instruction.op == "LW")
        {
            registers[instruction.rd].robTag = robTail;

            if (registers[instruction.rs1].robTag == -1)
            {
                rs->vj = registers[instruction.rs1].value;
                rs->addr = to_string(stoi(instruction.imm) + rs->vj);
            }
            else
            {
                ROBEntry &entry = rob[registers[instruction.rs1].robTag];
                rs->qj = entry.ready ? -1 : registers[instruction.rs1].robTag;
                if (entry.ready)
                {
                    rs->vj = entry.value;
                    rs->addr = to_string(stoi(instruction.imm) + rs->vj);
                }
            }
        }
        else if (instruction.op == "SW")
        {
            if (registers[instruction.rs1].robTag == -1)
            {
                rs->vj = registers[instruction.rs1].value;
                int base = rs->vj;
                int offset = stoi(instruction.imm);
                rob[robTail].destination = to_string(base + offset);
            }
            else
            {
                ROBEntry &entry = rob[registers[instruction.rs1].robTag];
                rs->qj = entry.ready ? -1 : registers[instruction.rs1].robTag;
                if (entry.ready)
                {
                    rs->vj = entry.value;
                    int base = rs->vj;
                    int offset = stoi(instruction.imm);
                    rob[robTail].destination = to_string(base + offset);
                }
            }

            if (registers[instruction.rd].robTag == -1)
            {
                rs->vk = registers[instruction.rd].value;
            }
            else
            {
                ROBEntry &entry = rob[registers[instruction.rd].robTag];
                rs->qk = entry.ready ? -1 : registers[instruction.rd].robTag;
                if (entry.ready)
                    rs->vk = entry.value;
            }
        }
        else
        {
            registers[instruction.rd].robTag = robTail;

            if (registers[instruction.rs1].robTag == -1)
            {
                rs->vj = registers[instruction.rs1].value;
            }
            else
            {
                ROBEntry &entry = rob[registers[instruction.rs1].robTag];
                rs->qj = entry.ready ? -1 : registers[instruction.rs1].robTag;
                if (entry.ready)
                    rs->vj = entry.value;
            }

            if (registers[instruction.rs2].robTag == -1)
            {
                rs->vk = registers[instruction.rs2].value;
            }
            else
            {
                ROBEntry &entry = rob[registers[instruction.rs2].robTag];
                rs->qk = entry.ready ? -1 : registers[instruction.rs2].robTag;
                if (entry.ready)
                    rs->vk = entry.value;
            }
        }

        robTail = (robTail + 1) % robSize;
    }

    void executeInstructions()
    {
        for (auto &rs : addRS)
        {
            if (rs.busy && rs.qj == -1 && rs.qk == -1 && rs.remainingCycles > 0)
            {
                rs.remainingCycles--;

                if (rs.remainingCycles == 0)
                {
                    instructionsStatus.at(rs.instructionIndex).execCompleteTime = cycle + 1;
                }
            }
        }

        for (auto &rs : mulRS)
        {
            if (rs.busy && rs.qj == -1 && rs.qk == -1 && rs.remainingCycles > 0)
            {
                rs.remainingCycles--;

                if (rs.remainingCycles == 0)
                {
                    instructionsStatus.at(rs.instructionIndex).execCompleteTime = cycle + 1;
                }
            }
        }

        for (auto &rs : loadStoreRS)
        {
            if (rs.busy && rs.qj == -1 && (rs.op != "SW" || rs.qk == -1) && rs.remainingCycles > 0)
            {
                if (rs.op == "SW" && rs.addr.empty() && rob[rs.destRobTag].destination != "")
                {
                    rs.addr = rob[rs.destRobTag].destination;
                }

                rs.remainingCycles--;

                if (rs.remainingCycles == 0)
                {
                    instructionsStatus.at(rs.instructionIndex).execCompleteTime = cycle + 1;
                }
            }
        }
    }

    void writeResults()
    {
        for (auto &rs : addRS)
        {
            if (rs.busy && rs.remainingCycles == 0)
            {
                int result = 0;

                if (rs.op == "ADD")
                {
                    result = rs.vj + rs.vk;
                }
                else if (rs.op == "SUB")
                {
                    result = rs.vj - rs.vk;
                }

                rob[rs.destRobTag].value = result;
                rob[rs.destRobTag].ready = true;

                broadcastResult(rs.destRobTag, result);

                auto &status = instructionsStatus.at(rs.instructionIndex);
                status.writeResultTime = cycle + 1;

                rs.reset();
            }
        }

        for (auto &rs : mulRS)
        {
            if (rs.busy && rs.remainingCycles == 0)
            {
                int result = 0;

                if (rs.op == "MUL")
                {
                    result = rs.vj * rs.vk;
                }
                else if (rs.op == "DIV")
                {
                    if (rs.vk != 0)
                    {
                        result = rs.vj / rs.vk;
                    }
                    else
                    {
                        cerr << "Warning: Division by zero detected!" << endl;
                        result = 0;
                    }
                }

                rob[rs.destRobTag].value = result;
                rob[rs.destRobTag].ready = true;
                broadcastResult(rs.destRobTag, result);

                auto &status = instructionsStatus.at(rs.instructionIndex);
                status.writeResultTime = cycle + 1;

                rs.reset();
            }
        }

        for (auto &rs : loadStoreRS)
        {
            if (rs.busy && rs.remainingCycles == 0)
            {
                if (rs.op == "LW")
                {
                    rob[rs.destRobTag].value = memory[rs.addr];
                    rob[rs.destRobTag].ready = true;
                    broadcastResult(rs.destRobTag, rob[rs.destRobTag].value);
                }
                else if (rs.op == "SW")
                {
                    rob[rs.destRobTag].value = rs.vk;
                    rob[rs.destRobTag].ready = true;
                }

                auto &status = instructionsStatus.at(rs.instructionIndex);
                status.writeResultTime = cycle + 1;

                rs.reset();
            }
        }
    }

    void commit()
    {
        if (!rob[robHead].busy)
            return;

        ROBEntry &entry = rob[robHead];
        if (entry.ready)
        {
            if (entry.type == "arith" || entry.type == "load")
            {
                if (registers[entry.destination].robTag == robHead)
                {
                    registers[entry.destination].value = entry.value;
                    registers[entry.destination].robTag = -1;
                }
            }
            else if (entry.type == "store")
            {
                memory[entry.destination] = entry.value;
            }

            instructionsStatus[entry.instructionIndex].commitTime = cycle + 1;

            rob[robHead].busy = false;
            robHead = (robHead + 1) % robSize;
        }
    }

    void broadcastResult(int robTag, int value)
    {
        for (auto &rs : addRS)
        {
            if (rs.busy)
            {
                if (rs.qj == robTag)
                {
                    rs.vj = value;
                    rs.qj = -1;
                }
                if (rs.qk == robTag)
                {
                    rs.vk = value;
                    rs.qk = -1;
                }
            }
        }

        for (auto &rs : mulRS)
        {
            if (rs.busy)
            {
                if (rs.qj == robTag)
                {
                    rs.vj = value;
                    rs.qj = -1;
                }
                if (rs.qk == robTag)
                {
                    rs.vk = value;
                    rs.qk = -1;
                }
            }
        }

        for (auto &rs : loadStoreRS)
        {
            if (rs.busy)
            {
                if (rs.qj == robTag)
                {
                    rs.vj = value;
                    rs.qj = -1;

                    if (rs.op == "SW" && rs.qj == -1)
                    {
                        int base = rs.vj;
                        int offset = stoi(rs.imm);
                        rob[rs.destRobTag].destination = to_string(base + offset);
                    }
                }
                if (rs.qk == robTag)
                {
                    rs.vk = value;
                    rs.qk = -1;
                }
            }
        }
    }

    ReservationStation *getAvailableReservationStation(const string &op)
    {
        if (op == "ADD" || op == "SUB")
        {
            for (auto &rs : addRS)
            {
                if (!rs.busy)
                    return &rs;
            }
        }
        else if (op == "MUL" || op == "DIV")
        {
            for (auto &rs : mulRS)
            {
                if (!rs.busy)
                    return &rs;
            }
        }
        else if (op == "LW" || op == "SW")
        {
            for (auto &rs : loadStoreRS)
            {
                if (!rs.busy)
                    return &rs;
            }
        }

        return nullptr;
    }

    bool checkSimulationComplete()
    {
        if (!instructions.empty())
            return false;

        for (const auto &rs : addRS)
        {
            if (rs.busy)
                return false;
        }
        for (const auto &rs : mulRS)
        {
            if (rs.busy)
                return false;
        }
        for (const auto &rs : loadStoreRS)
        {
            if (rs.busy)
                return false;
        }

        for (const auto &entry : rob)
        {
            if (entry.busy)
                return false;
        }

        return true;
    }

    void printState()
    {
        cout << "\nReservation Station ADD/SUB:" << endl;
        cout << "Name\tBusy\tOp\tVj\tVk\tQj\tQk\tDestROB" << endl;
        for (const auto &rs : addRS)
        {
            cout << rs.name << "\t"
                 << (rs.busy ? "Yes" : "No") << "\t"
                 << (rs.busy ? rs.op : "-") << "\t"
                 << rs.vj << "\t"
                 << rs.vk << "\t"
                 << (rs.qj != -1 ? "ROB" + to_string(rs.qj) : "-") << "\t"
                 << (rs.qk != -1 ? "ROB" + to_string(rs.qk) : "-") << "\t"
                 << (rs.destRobTag != -1 ? "ROB" + to_string(rs.destRobTag) : "-") << endl;
        }

        cout << "\nReservation Station MUL/DIV:" << endl;
        cout << "Name\tBusy\tOp\tVj\tVk\tQj\tQk\tDestROB" << endl;
        for (const auto &rs : mulRS)
        {
            cout << rs.name << "\t"
                 << (rs.busy ? "Yes" : "No") << "\t"
                 << (rs.busy ? rs.op : "-") << "\t"
                 << rs.vj << "\t"
                 << rs.vk << "\t"
                 << (rs.qj != -1 ? "ROB" + to_string(rs.qj) : "-") << "\t"
                 << (rs.qk != -1 ? "ROB" + to_string(rs.qk) : "-") << "\t"
                 << (rs.destRobTag != -1 ? "ROB" + to_string(rs.destRobTag) : "-") << endl;
        }

        cout << "\nReservation Station LOAD/STORE:" << endl;
        cout << "Name\tBusy\tOp\tA\tVj\tVk\tQj\tQk\tDestROB" << endl;
        for (const auto &rs : loadStoreRS)
        {
            cout << rs.name << "\t"
                 << (rs.busy ? "Yes" : "No") << "\t"
                 << (rs.busy ? rs.op : "-") << "\t"
                 << (!rs.addr.empty() ? rs.addr : "-") << "\t"
                 << rs.vj << "\t"
                 << rs.vk << "\t"
                 << (rs.qj != -1 ? "ROB" + to_string(rs.qj) : "-") << "\t"
                 << (rs.qk != -1 ? "ROB" + to_string(rs.qk) : "-") << "\t"
                 << (rs.destRobTag != -1 ? "ROB" + to_string(rs.destRobTag) : "-") << endl;
        }

        cout << "\nROB Status:" << endl;
        cout << "Entry\tBusy\tReady\tType\tDest\tValue" << endl;
        for (int i = 0; i < robSize; i++)
        {
            auto &entry = rob[i];
            cout << i << "\t"
                 << (entry.busy ? "Yes" : "No") << "\t"
                 << (entry.ready ? "Yes" : "No") << "\t"
                 << (entry.busy ? entry.type : "-") << "\t"
                 << (entry.busy ? entry.destination : "-") << "\t"
                 << (entry.busy ? to_string(entry.value) : "-") << endl;
        }

        cout << "\nRegisters Status:" << endl;
        cout << "Reg\tValue\tROB Tag" << endl;

        vector<string> sortedRegs(usedRegisters.begin(), usedRegisters.end());
        sort(sortedRegs.begin(), sortedRegs.end(), [](const string &a, const string &b)
             { return stoi(a.substr(1)) < stoi(b.substr(1)); });

        for (const auto &regName : sortedRegs)
        {
            const auto &reg = registers[regName];
            cout << regName << "\t"
                 << reg.value << "\t"
                 << (reg.robTag != -1 ? "ROB" + to_string(reg.robTag) : "-") << endl;
        }
    }

    void printFinalResults()
    {
        cout << "\n=== Final Results ===" << endl;

        cout << "\nInstruction Timeline:" << endl;
        cout << "Instruction\t\tIssue\tExecComp\tWriteRes\tCommit" << endl;
        for (size_t i = 0; i < instructionsStatus.size(); i++)
        {
            const auto &instr = instructionsStatus[i].instruction;
            cout << instr.op << " "
                 << instr.rd << " " << instr.rs1;
            if (!instr.rs2.empty())
                cout << " " << instr.rs2;
            else if (!instr.imm.empty())
                cout << " " << instr.imm;

            cout << "\t\t" << instructionsStatus[i].issueTime << "\t"
                 << instructionsStatus[i].execCompleteTime << "\t\t"
                 << instructionsStatus[i].writeResultTime << "\t\t"
                 << instructionsStatus[i].commitTime << endl;
        }

        cout << "\nFinal Register Values:" << endl;
        vector<string> sortedRegs(usedRegisters.begin(), usedRegisters.end());
        sort(sortedRegs.begin(), sortedRegs.end(), [](const string &a, const string &b)
             { return stoi(a.substr(1)) < stoi(b.substr(1)); });

        for (const auto &regName : sortedRegs)
        {
            const auto &reg = registers[regName];
            cout << regName << " = " << reg.value << endl;
        }

        cout << "\nFinal Memory Contents:" << endl;
        vector<pair<string, int>> sortedMem;
        for (const auto &mem : memory)
        {
            if (mem.second != 0)
            {
                sortedMem.push_back(mem);
            }
        }

        sort(sortedMem.begin(), sortedMem.end(), [](const auto &a, const auto &b)
             { return stoi(a.first) < stoi(b.first); });

        for (const auto &mem : sortedMem)
        {
            cout << "Memory[" << mem.first << "] = " << mem.second << endl;
        }
    }
};

int main()
{
    Tomasulo simulator(3, 2, 3, 6);

    string filePath;
    cout << "Enter the file path:" << endl;
    cin >> filePath;
    cin.ignore();

    if (!simulator.loadInstructions(filePath))
    {
        cerr << "Error loading instructions.";
        return 1;
    }

    simulator.setRegister("R0", 5);   
    simulator.setRegister("R1", 3);   
    simulator.setRegister("R2", 2);   
    simulator.setRegister("R3", 3);   
    simulator.setRegister("R4", 2);  
    simulator.setRegister("R5", 5); 
    
    simulator.setMemory("105", 10); 
    simulator.setMemory("203", 0);  

    simulator.run();

    return 0;
}