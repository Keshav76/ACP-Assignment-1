#include <bits/stdc++.h>
#include "utils.h"

class DJIT : public Algo
{
private:
    int num_threads = 0;
    unordered_map<string, vector<int>> Rd, Wr, Lo;
    unordered_map<int, vector<int>> Th;
    int parent_thread = -1;
    unordered_map<string, int> ans;

public:
    void init_vc(unordered_map<string, vector<int>> &a, string name)
    {
        a[name] = vector<int>(num_threads, 0);
    }
    void set_parent(int parent)
    {
        parent_thread = parent;
    }
    void append_slot() {
        int new_tid = num_threads;

        if (parent_thread == -1) Th[new_tid] = vector<int>(new_tid, 0);
        else Th[new_tid] = Th[parent_thread];

        num_threads++;
        for (auto ele : Th) Th[ele.first].push_back(1);
        for (auto ele : Rd) Rd[ele.first].push_back(0);
        for (auto ele : Wr) Wr[ele.first].push_back(0);
        for (auto ele : Lo) Lo[ele.first].push_back(0);
    }
    void release_sync(LogEntry entry)
    {
        int thread = entry.threadId;
        Th[thread][thread]++;
        string lock = entry.lockAddress;
        if (Lo[lock].size() == 0)
            init_vc(Lo, lock);
        for (int i = 0; i < num_threads; i++)
            Lo[lock][i] = max(Lo[lock][i], Th[thread][i]);
    }
    void acquire_sync(LogEntry entry)
    {
        int thread = entry.threadId;
        string lock = entry.lockAddress;
        if (Lo[lock].size() == 0)
            init_vc(Lo, lock);
        for (int i = 0; i < num_threads; i++)
            Th[thread][i] = max(Th[thread][i], Lo[lock][i]);
    }
    void first_access(LogEntry entry)
    {
        int thread = entry.threadId;
        int raceLen = 0;
        for (int i = 0; i < 1; i++)
        {
            string v = intToHex(hexToInt(entry.addr) + i);
            if (Rd[v].size() == 0 || Wr[v].size() == 0)
            {
                init_vc(Rd, v);
                init_vc(Wr, v);
            }

            if (entry.isRead)
            {
                Rd[v][thread] = Th[thread][thread];
                for (int i = 0; i < num_threads; i++)
                {
                    if (Wr[v][i] > Th[thread][i] && i != thread)
                        report(i, thread, "W-R", v);
                }
            }
            else
            {
                Wr[v][thread] = Th[thread][thread];
                for (int i = 0; i < num_threads; i++)
                {
                    if (i != thread && (Wr[v][i] > Th[thread][i]))
                        report(i, thread, "W-W", v);
                    else if (i != thread && (Rd[v][i] > Th[thread][i]))
                        report(i, thread, "R-W", v);
                }
            }
        }
    }
    void report(int t1, int t2, string type, string addr, int size = 1)
    {
        if (t1 > t2)
            swap(t1, t2);
        string s = addr + " " + type + " TID:" + to_string(t1) + " TID:" + to_string(t2) + " Size:" + to_string(size);
        ans[s]++;
    }
    unordered_map<string, int> getAnswer()
    {
        return ans;
    }
};
