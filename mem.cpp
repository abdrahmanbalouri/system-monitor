#include "header.h"

namespace
{
unsigned long long parseValueKB(const map<string, unsigned long long> &values, const string &key)
{
    map<string, unsigned long long>::const_iterator it = values.find(key);
    if (it == values.end())
        return 0;
    return it->second;
}

char parseProcessState(const string &line, string &name, unsigned long long &utime, unsigned long long &stime, long long &rssPages)
{
    const size_t leftParen = line.find('(');
    const size_t rightParen = line.rfind(')');
    if (leftParen == string::npos || rightParen == string::npos || rightParen <= leftParen)
        return '?';

    name = line.substr(leftParen + 1, rightParen - leftParen - 1);
    if (rightParen + 2 >= line.size())
        return '?';

    const char state = line[rightParen + 2];
    istringstream rest(line.substr(rightParen + 4));
    string ignored;

    for (int field = 4; field <= 13; ++field)
        rest >> ignored;
    rest >> utime >> stime;
    for (int field = 16; field <= 23; ++field)
        rest >> ignored;
    rest >> rssPages;

    return state;
}
}

MemoryStats getMemoryStats()
{
    MemoryStats stats = {0, 0, 0, 0};
    ifstream file("/proc/meminfo");
    string key;
    unsigned long long value;
    string unit;
    map<string, unsigned long long> values;

    while (file >> key >> value >> unit)
    {
        if (!key.empty() && key[key.size() - 1] == ':')
            key.erase(key.size() - 1);
        values[key] = value;
    }

    stats.memTotalKB = parseValueKB(values, "MemTotal");
    const unsigned long long memAvailable = parseValueKB(values, "MemAvailable");
    stats.memUsedKB = stats.memTotalKB > memAvailable ? stats.memTotalKB - memAvailable : 0;
    stats.swapTotalKB = parseValueKB(values, "SwapTotal");
    const unsigned long long swapFree = parseValueKB(values, "SwapFree");
    stats.swapUsedKB = stats.swapTotalKB > swapFree ? stats.swapTotalKB - swapFree : 0;
    return stats;
}

DiskStats getDiskStats(const char *path)
{
    DiskStats stats = {0, 0, 0};
    struct statvfs fs;
    if (statvfs(path, &fs) != 0)
        return stats;

    const unsigned long long blockSize = fs.f_frsize;
    stats.totalBytes = blockSize * fs.f_blocks;
    stats.freeBytes = blockSize * fs.f_bavail;
    stats.usedBytes = stats.totalBytes > stats.freeBytes ? stats.totalBytes - stats.freeBytes : 0;
    return stats;
}

vector<ProcessInfo> getProcesses(unsigned long long totalCpuTicksDelta, const map<int, unsigned long long> &previousTicks, unsigned long long memTotalKB)
{
    vector<ProcessInfo> processes;
    DIR *dir = opendir("/proc");
    if (dir == NULL)
        return processes;

    const long pageSizeKB = sysconf(_SC_PAGESIZE) / 1024;
    dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isdigit(entry->d_name[0]))
            continue;

        const int pid = atoi(entry->d_name);
        const string statPath = string("/proc/") + entry->d_name + "/stat";
        ifstream file(statPath.c_str());
        if (!file.is_open())
            continue;

        string line;
        getline(file, line);

        string name;
        unsigned long long utime = 0;
        unsigned long long stime = 0;
        long long rssPages = 0;
        const char state = parseProcessState(line, name, utime, stime, rssPages);
        if (state == '?')
            continue;

        const unsigned long long totalTicks = utime + stime;
        map<int, unsigned long long>::const_iterator previous = previousTicks.find(pid);
        const unsigned long long processDelta = previous != previousTicks.end() && totalTicks >= previous->second ? totalTicks - previous->second : 0;

        ProcessInfo info;
        info.pid = pid;
        info.name = name;
        info.state = state;
        info.totalTimeTicks = totalTicks;
        info.cpuPercent = totalCpuTicksDelta > 0 ? static_cast<float>((100.0 * processDelta) / totalCpuTicksDelta) : 0.0f;

        const unsigned long long rssKB = rssPages > 0 ? static_cast<unsigned long long>(rssPages) * pageSizeKB : 0;
        info.memoryPercent = memTotalKB > 0 ? static_cast<float>((100.0 * rssKB) / memTotalKB) : 0.0f;
        processes.push_back(info);
    }

    closedir(dir);

    sort(processes.begin(), processes.end(), [](const ProcessInfo &left, const ProcessInfo &right) {
        if (left.cpuPercent == right.cpuPercent)
            return left.pid < right.pid;
        return left.cpuPercent > right.cpuPercent;
    });

    return processes;
}
