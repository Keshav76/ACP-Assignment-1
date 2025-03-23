#include <bits/stdc++.h>

using namespace std;

// All the Regexes to match in the logs
regex threadParentRegex(R"(^Before pthread_create\(\): Parent: (\d+)$)");
regex threadBeginRegex("Thread begin: (\\d+)");
regex memoryOpRegex("TID: (\\d+), IP: (0x[0-9a-fA-F]+), ADDR: (0x[0-9a-fA-F]+), Size \\(B\\): (\\d+), isRead: (\\d+)");
regex beforeLockReleaseRegex("Before lock release: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex afterLockReleaseRegex("After lock release: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex beforeLockAcquireRegex("Before lock acquire: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex afterLockAcquireRegex("After lock acquire: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex threadEnd(R"(^Thread ended: (\d+)$)");

/// DS to store each line of log
struct LogEntry {
    string type;
    int threadId;
    string ip; // Unnecessary but kept for completeness
    string addr;
    int size;
    bool isRead;
    string lockAddress;
};

// Fasttrack VC Union {Thread, clock}
struct REntry {
    bool is_epoch;
    int clock;
    int thread;
    vector < int > vc;
    REntry(): is_epoch(true), clock(0), thread(0) {}
};


// Utils for using address as int 
string intToHex(unsigned long long decimalValue) {
    stringstream ss;
    ss << "0x" << hex << decimalValue;
    return ss.str();
}

unsigned long long hexToInt(const string & hexStr) {
    return stoull(hexStr, nullptr, 16);
}


// I have used an abstract class Algo to make sure both implementations are identical for traces
class Algo {
    public: virtual void append_slot() = 0;
    virtual void release_sync(LogEntry entry) = 0; // Lock release
    virtual void acquire_sync(LogEntry entry) = 0; // Lock acquire
    virtual void first_access(LogEntry entry) = 0; // Memory access
    virtual void set_parent(int parent) = 0; // Set parent thread
    virtual unordered_map <string, int> getAnswer() = 0;
};

// FastTrack Algorithm
class FastTrack: public Algo {
    private: int num_threads = 0,
    parentTid = -1;
    unordered_map <string, pair<int, int>> Wr; // (clock, thread)
    unordered_map <string, REntry> Rd;
    unordered_map <string, vector<int>> Lo; // Lock vector clocks
    unordered_map <int, vector<int>> Th; // Thread vector clocks
    unordered_map <string, int > ans;

    // Helper to compare epoch <= vector clock
    bool epoch_le(pair < int, int > epoch,
        const vector < int > & vc) {
        return epoch.first <= vc[epoch.second];
    }

    // Helper to compare vector clock <= vector clock
    bool vc_le(const vector < int > & a,
        const vector < int > & b) {
        for (int i = 0; i < num_threads; i++)
            if (a[i] > b[i])
                return false;
        return true;
    }

    void init_thread(int tid) {
        if (parentTid != -1)
            Th[tid] = Th[parentTid];
        else
            Th[tid] = vector < int > (num_threads, 0);
        Th[tid][tid] = 1; // Initial clock
    }

    public: void set_parent(int parent) {
        parentTid = parent;
    }

    void append_slot() {
        int new_tid = num_threads++;
        init_thread(new_tid);
    }

    void release_sync(LogEntry entry) {
        int t = entry.threadId;
        string lock = entry.lockAddress;
        Th[t][t]++; // Increment thread clock

        if (!Lo.count(lock))
            Lo[lock] = vector < int > (num_threads, 0);

        for (int i = 0; i < num_threads; i++)
            Lo[lock][i] = max(Lo[lock][i], Th[t][i]);
    }

    void acquire_sync(LogEntry entry) {
        int t = entry.threadId;
        string lock = entry.lockAddress;

        if (Lo.count(lock)) {
            for (int i = 0; i < num_threads; i++)
                Th[t][i] = max(Th[t][i], Lo[lock][i]);
        }
    }

    void handle_read(string addr, int thread, int & raceLen, string & prevRace, bool & racy) {
        if (Th[thread][thread] == Rd[addr].clock)
            return;
        // Check write epoch
        if (Wr.count(addr)) {
            auto & wx = Wr[addr];
            if (!epoch_le(wx, Th[thread])) {
                string s = " W-R TID:" + to_string(min(wx.second, thread)) + " TID:" + to_string(max(wx.second, thread));
                if (prevRace == s)
                    raceLen++;
                else {
                    if (raceLen > 0) {
                        string fin = intToHex(hexToInt(addr) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                        report(fin);
                    }
                    prevRace = s;
                    raceLen = 1;
                }
                racy = true;
            }
        }

        // Handle read state
        if (!Rd.count(addr)) {
            Rd[addr].is_epoch = true;
            Rd[addr].clock = Th[thread][thread];
            Rd[addr].thread = thread;
            return;
        }

        REntry & rx = Rd[addr];
        if (rx.is_epoch) {

            // FT READ EXCLUSIVE
            if (epoch_le({
                    rx.clock,
                    rx.thread
                }, Th[thread])) {
                rx.clock = Th[thread][thread];
                rx.thread = thread;
            }
            // FT READ SHARE
            else {
                // Convert to VC
                rx.is_epoch = false;
                if (rx.vc.size() == 0)
                    rx.vc = vector < int > (num_threads, 0);
                rx.vc[rx.thread] = rx.clock;
                rx.vc[thread] = Th[thread][thread];
            }
        } else {
            // FT READ SHARED
            rx.vc[thread] = Th[thread][thread];
        }
    }

    void handle_write(string addr, int thread, int & raceLen, string & prevRace, bool & racy) {
        //Same Epoch
        if (Wr.count(addr) && Wr[addr].second == thread)
            return;
        // Check read state
        if (Rd.count(addr)) {
            REntry & rx = Rd[addr];
            if (rx.is_epoch) {
                if (!epoch_le({
                        rx.clock,
                        rx.thread
                    }, Th[thread])) {
                    string s = " R-W TID:" + to_string(min(rx.thread, thread)) + " TID:" + to_string(max(rx.thread, thread));
                    if (prevRace == s)
                        raceLen++;
                    else {
                        if (raceLen > 0) {
                            string fin = intToHex(hexToInt(addr) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                            report(fin);
                        }
                        prevRace = s;
                        raceLen = 1;
                    }
                    racy = true;
                }
            } else {
                if (!vc_le(rx.vc, Th[thread])) {
                    // Find conflicting threads
                    for (int i = 0; i < num_threads; i++) {
                        if (rx.vc[i] > Th[thread][i]) {
                            string s = " R-W TID:" + to_string(min(i, thread)) + " TID:" + to_string(max(i, thread));
                            if (prevRace == s)
                                raceLen++;
                            else {
                                if (raceLen > 0) {
                                    string fin = intToHex(hexToInt(addr) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                                    report(fin);
                                }
                                prevRace = s;
                                raceLen = 1;
                            }
                            racy = true;
                        }
                    }
                }
            }
        }

        // Check previous write W-W Race checking
        if (Wr.count(addr)) {
            auto & wx = Wr[addr];
            if (!epoch_le(wx, Th[thread])) {
                string s = " W-W TID:" + to_string(min(wx.second, thread)) + " TID:" + to_string(max(wx.second, thread));
                if (prevRace == s)
                    raceLen++;
                else {
                    if (raceLen > 0) {
                        string fin = intToHex(hexToInt(addr) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                        report(fin);
                    }
                    prevRace = s;
                    raceLen = 1;
                }
                racy = true;
            }
        }

        // Update write epoch
        Wr[addr] = {
            Th[thread][thread],
            thread
        };

        // Reset read state if needed
        if (Rd.count(addr) && !Rd[addr].is_epoch) {
            Rd.erase(addr); // Reset to epoch mode
        }
    }

    void first_access(LogEntry entry) {
        int thread = entry.threadId;
        int raceLen = 0;
        string prevRace = "";
        bool racy = false;

        for (int i = 0; i < entry.size; i++) {
            string addr = intToHex(hexToInt(entry.addr) + i);
            if (entry.isRead) {
                handle_read(addr, thread, raceLen, prevRace, racy);
            } else {
                handle_write(addr, thread, raceLen, prevRace, racy);
            }

            if (!racy && raceLen > 0) {
                string s = prevRace + " Size:" + to_string(raceLen);
                string fin = intToHex(hexToInt(addr) - raceLen) + s;
                report(fin);
                raceLen = 0;
                prevRace = "";
            }
            racy = false;
        }

        if (raceLen > 0) {
            string s = prevRace + " Size:" + to_string(raceLen);
            string fin = intToHex(hexToInt(entry.addr) + entry.size - raceLen) + s;
            report(fin);
        }
    }

    void report(string s) {
        ans[s]++;
    }

    unordered_map < string,
    int > getAnswer() {
        return ans;
    }
};

class DJIT: public Algo {
    private: int num_threads = 0;
    unordered_map <string, vector<int>> Rd, Wr, Lo;
    unordered_map <int, vector<int>> Th;
    unordered_map <string, int > ans;
    int parent_thread = -1;

    public: void init_vc(unordered_map < string, vector < int >> & a, string name) {
        a[name] = vector < int > (num_threads, 0);
    }
    void set_parent(int parent) {
        parent_thread = parent;
    }
    void append_slot() {
        int new_tid = num_threads;

        // Changed 0 -> 1 for DJIT
        if (parent_thread == -1)
            Th[new_tid] = vector < int > (new_tid, 0);
        else
            Th[new_tid] = Th[parent_thread];

        num_threads++;
        for (auto ele: Th)
            Th[ele.first].push_back(1);
        for (auto ele: Rd)
            Rd[ele.first].push_back(0);
        for (auto ele: Wr)
            Wr[ele.first].push_back(0);
        for (auto ele: Lo)
            Lo[ele.first].push_back(0);
    }
    void release_sync(LogEntry entry) {
        int thread = entry.threadId;
        Th[thread][thread]++;
        string lock = entry.lockAddress;
        if (Lo[lock].size() == 0)
            init_vc(Lo, lock);
        for (int i = 0; i < num_threads; i++)
            Lo[lock][i] = max(Lo[lock][i], Th[thread][i]);
    }
    void acquire_sync(LogEntry entry) {
        int thread = entry.threadId;
        string lock = entry.lockAddress;
        if (Lo[lock].size() == 0)
            init_vc(Lo, lock);
        for (int i = 0; i < num_threads; i++)
            Th[thread][i] = max(Th[thread][i], Lo[lock][i]);
    }
    void first_access(LogEntry entry) {
        int thread = entry.threadId;
        int raceLen = 0;
        string prevRace = "";
        bool racy = false;
        for (int i = 0; i < entry.size; i++) {
            string v = intToHex(hexToInt(entry.addr) + i);
            if (Rd[v].size() == 0 || Wr[v].size() == 0) {
                init_vc(Rd, v);
                init_vc(Wr, v);
            }

            if (entry.isRead) {
                Rd[v][thread] = Th[thread][thread];
                for (int i = 0; i < num_threads; i++) {
                    if (Wr[v][i] > Th[thread][i] && i != thread) {
                        racy = true;
                        string s = " W-R TID:" + to_string(min(i, thread)) + " TID:" + to_string(max(i, thread));
                        // report(i, thread, "W-R", v)                                                                                                                                     ;
                        if (prevRace == s)
                            raceLen++;
                        else {
                            if (raceLen > 0) {
                                string fin = intToHex(hexToInt(v) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                                report(fin);
                            }
                            prevRace = s;
                            raceLen = 1;
                        }
                    }
                }
            } else {
                Wr[v][thread] = Th[thread][thread];
                for (int i = 0; i < num_threads; i++) {
                    if (i != thread && (Wr[v][i] > Th[thread][i])) {
                        racy = true;
                        string s = " W-W TID:" + to_string(min(i, thread)) + " TID:" + to_string(max(i, thread));
                        if (prevRace == s)
                            raceLen++;
                        else {
                            if (raceLen > 0) {
                                string fin = intToHex(hexToInt(v) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                                report(fin);
                            }
                            prevRace = s;
                            raceLen = 1;
                        }
                    } else if (i != thread && (Rd[v][i] > Th[thread][i])) {
                        racy = true;
                        string s = " R-W TID:" + to_string(min(i, thread)) + " TID:" + to_string(max(i, thread));
                        // report(i, thread, "R-W", v)                                                                                                                                     ;
                        if (prevRace == s)
                            raceLen++;
                        else {
                            if (raceLen > 0) {
                                string fin = intToHex(hexToInt(v) - raceLen) + prevRace + " Size:" + to_string(raceLen);
                                report(fin);
                            }
                            prevRace = s;
                            raceLen = 1;
                        }
                    }
                }
            }
            if (!racy && raceLen > 0) {
                string s = prevRace + " Size:" + to_string(raceLen);
                string addr = intToHex(hexToInt(v) - raceLen);
                report(addr + s);
                raceLen = 0;
                prevRace = "";
            }
            racy = false;
        }
        if (raceLen > 0) {
            string s = prevRace + " Size:" + to_string(raceLen);
            string addr = intToHex(hexToInt(entry.addr) + entry.size - raceLen);
            report(addr + s);
        }
    }
    void report(string s) {
        ans[s]++;
    }
    unordered_map <string, int > getAnswer() {
        return ans;
    }
};

void parseLog(const string & filename,
    const string & algo) {
    Algo * algoObj;
    if (algo == "DJIT")
        algoObj = new DJIT();
    else
        algoObj = new FastTrack();

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Unable to open file.";
        return;
    }

    string line;
    while (getline(file, line)) {
        smatch match;

        if (regex_match(line, match, threadBeginRegex)) {
            algoObj -> append_slot();
        } else if (regex_match(line, match, memoryOpRegex)) {
            LogEntry entry;
            entry.threadId = stoi(match[1]);
            entry.ip = match[2];
            entry.addr = match[3];
            entry.size = stoi(match[4]);
            entry.isRead = stoi(match[5]) == 1 ? true : false;
            algoObj -> first_access(entry);
        } else if (regex_match(line, match, afterLockReleaseRegex)) {
            LogEntry entry;
            entry.threadId = stoi(match[1]);
            entry.lockAddress = match[2];
            algoObj -> release_sync(entry);
        } else if (regex_match(line, match, afterLockAcquireRegex)) {
            LogEntry entry;
            entry.threadId = stoi(match[1]);
            entry.lockAddress = match[2];
            algoObj -> acquire_sync(entry);
        } else if (regex_match(line, match, threadParentRegex)) {
            algoObj -> set_parent(stoi(match[1]));
        } else if (!regex_match(line, match, beforeLockReleaseRegex) && !regex_match(line, match, beforeLockAcquireRegex) && !regex_match(line, match, threadEnd) && !line.empty()) {
            cout << "Extra Line: " << line << endl;
        }
    }

    file.close();
    for (auto ele: algoObj -> getAnswer()) {
        // cout << ele.first << " Count: " << ele.second << endl;
    }
}

int main(int argc, char * argv[]) {
    if (argc < 3) {
        cerr << "Filename and Algo required. \nFormat ./exec <DJIT/FastTrack> <filename>" << endl;
        return 1;
    }

    if (argv[1] != string("DJIT") && argv[1] != string("FastTrack")) {
        cerr << "Invalid Algorithm." << endl;
        return 1;
    }

    string logFilename = argv[2];
    parseLog(logFilename, argv[1]);
    return 0;
}
