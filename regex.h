#include <regex>
using namespace std;

regex threadParentRegex(R"(^Before pthread_create\(\): Parent: (\d+)$)");
regex threadBeginRegex("Thread begin: (\\d+)");
regex memoryOpRegex("TID: (\\d+), IP: (0x[0-9a-fA-F]+), ADDR: (0x[0-9a-fA-F]+), Size \\(B\\): (\\d+), isRead: (\\d+)");
regex beforeLockReleaseRegex("Before lock release: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex afterLockReleaseRegex("After lock release: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex beforeLockAcquireRegex("Before lock acquire: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex afterLockAcquireRegex("After lock acquire: TID: (\\d+), Lock address: (0x[0-9a-fA-F]+)");
regex threadEnd(R"(^Thread ended: (\d+)$)");
