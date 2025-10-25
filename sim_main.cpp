#include <bits/stdc++.h>
using namespace std;

// ====== CACHE MODEL ======
struct CacheStats { size_t hits=0, misses=0; };

class Cache {
    size_t size, line_size, assoc, num_sets;
    vector<list<uint64_t>> sets;
    vector<unordered_map<uint64_t, list<uint64_t>::iterator>> index;
public:
    Cache(size_t sz, size_t line, size_t a)
        : size(sz), line_size(line), assoc(a) {
        num_sets = (size / line_size) / assoc;
        sets.resize(num_sets);
        index.resize(num_sets);
    }

    bool access(uint64_t addr, CacheStats &s) {
        uint64_t tag = addr / line_size;
        size_t set = tag % num_sets;
        auto &m = index[set];
        auto &lst = sets[set];
        if (m.find(tag) != m.end()) {
            // hit → move to front (LRU)
            s.hits++;
            lst.erase(m[tag]);
            lst.push_front(tag);
            m[tag] = lst.begin();
            return true;
        }
        // miss
        s.misses++;
        if (lst.size() >= assoc) {
            uint64_t old = lst.back();
            lst.pop_back();
            m.erase(old);
        }
        lst.push_front(tag);
        m[tag] = lst.begin();
        return false;
    }
};

// ====== PIPELINE MODEL ======
enum OpType { LOAD, STORE, ALU, BRANCH, NOP };

struct Instruction {
    OpType type;
    uint64_t addr;
    int rd, rs1, rs2;
};

struct PipelineReg {
    bool valid = false;
    Instruction inst;
};

int main() {
    ios::sync_with_stdio(false);

    // --- Load trace ---
    vector<Instruction> trace;
    string op; uint64_t addr;
    ifstream fin("simple.trace");
    while (fin >> op >> hex >> addr) {
        Instruction I;
        if (op=="LOAD") I.type=LOAD;
        else if (op=="STORE") I.type=STORE;
        else if (op=="ALU") I.type=ALU;
        else if (op=="BRANCH") I.type=BRANCH;
        else I.type=NOP;
        I.addr = addr;
        trace.push_back(I);
    }

    Cache L1(8*1024, 64, 2);
    CacheStats cs;

    // --- Pipeline State ---
    PipelineReg IF, ID, EX, MEM, WB;
    size_t pc=0, cycle=0, stalls=0;
    size_t retired=0;
    const size_t n = trace.size();

    while (retired < n) {
        cycle++;

        // WB stage
        if (WB.valid) retired++, WB.valid=false;

        // MEM stage
        if (MEM.valid) {
            auto t = MEM.inst.type;
            if (t==LOAD || t==STORE) L1.access(MEM.inst.addr, cs);
            WB = MEM; WB.valid=true;
            MEM.valid=false;
        }

        // EX stage (simulate 1-cycle ALU)
        if (EX.valid) { MEM=EX; MEM.valid=true; EX.valid=false; }

        // ID stage (simple hazard: stall if LOAD before dependent ALU)
        bool hazard=false;
        if (ID.valid && ID.inst.type==LOAD && EX.valid)
            hazard=true;
        if (hazard) stalls++;
        else { if (ID.valid) { EX=ID; EX.valid=true; ID.valid=false; } }

        // IF stage
        if (pc<n && !hazard) {
            IF.inst = trace[pc++];
            IF.valid=true;
        }
        if (IF.valid) { ID=IF; ID.valid=true; IF.valid=false; }
    }

    double cpi = double(cycle)/retired;
    cout << "Instructions: " << retired << "\n";
    cout << "Cycles: " << cycle << "\n";
    cout << "Stalls: " << stalls << "\n";
    cout << "CPI: " << fixed << setprecision(2) << cpi << "\n";
    cout << "Cache hits: " << cs.hits << "  misses: " << cs.misses << "\n";
    cout << "Hit rate: " << (100.0*cs.hits)/(cs.hits+cs.misses) << "%\n";
    return 0;
}
