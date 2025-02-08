#include <bits/stdc++.h>
using namespace std;
struct LogEntry
{
    string type;
    int threadId;
    string ip; // Unnecessary but kept for completeness
    string addr;
    int size;
    bool isRead;
    string lockAddress;
};

string intToHex(unsigned long long decimalValue)
{
    stringstream ss;
    ss << "0x" << hex << decimalValue;
    return ss.str();
}

unsigned long long hexToInt(const string &hexStr)
{
    return stoull(hexStr, nullptr, 16);
}

class Algo
{
public:
    virtual void append_slot() = 0;
    virtual void release_sync(LogEntry entry) = 0;
    virtual void acquire_sync(LogEntry entry) = 0;
    virtual void first_access(LogEntry entry) = 0;
    virtual void set_parent(int parent) = 0;
    virtual unordered_map<string, int> getAnswer() = 0;
};

