#include <bits/stdc++.h>
// #include "utils.h"
#include "regex.h"
#include "DJIT.h"
using namespace std;

void parseLog(const string &filename, const string &algo)
{
    Algo *algoObj;
    if (algo == "DJIT")
        algoObj = new DJIT();
    else
    {
        cerr << "FastTrack not implemented yet." << endl;
        return;
    }

    ifstream file(filename);
    if (!file.is_open())
    {
        cout << "Unable to open file.";
        return;
    }

    string line;

    while (getline(file, line))
    {
        smatch match;

        if (regex_match(line, match, threadBeginRegex))
        {
            algoObj->append_slot();
        }
        else if (regex_match(line, match, memoryOpRegex))
        {
            LogEntry entry;
            entry.threadId = stoi(match[1]);
            entry.ip = match[2];
            entry.addr = match[3];
            entry.size = stoi(match[4]);
            entry.isRead = stoi(match[5]) == 1 ? true : false;
            algoObj->first_access(entry);
        }
        else if (regex_match(line, match, afterLockReleaseRegex))
        {
            LogEntry entry;
            entry.threadId = stoi(match[1]);
            entry.lockAddress = match[2];
            algoObj->release_sync(entry);
        }
        else if (regex_match(line, match, afterLockAcquireRegex))
        {
            LogEntry entry;
            entry.threadId = stoi(match[1]);
            entry.lockAddress = match[2];
            algoObj->acquire_sync(entry);
        }
        else if (regex_match(line, match, threadParentRegex))
        {
            algoObj->set_parent(stoi(match[1]));
        }
        else if (!regex_match(line, match, beforeLockReleaseRegex) && !regex_match(line, match, beforeLockAcquireRegex) && !regex_match(line, match, threadEnd) && !line.empty())
        {
            cout << "Extra Line: " << line << endl;
        }
    }

    file.close();
    for (auto ele : algoObj->getAnswer())
    {
        cout << ele.first << " Count: " << ele.second << endl;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Filename and Algo required." << endl;
        return 1;
    }

    if (argv[1] != string("DJIT") && argv[1] != string("FastTrack"))
    {
        cerr << "Invalid Algorithm." << endl;
        return 1;
    }

    string logFilename = argv[2];
    parseLog(logFilename, argv[1]);

    return 0;
}
