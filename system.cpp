#include "header.h"

namespace
{
string trim(const string &value)
{
    const string whitespace = " \t\r\n\"";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == string::npos)
        return "";
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

string readFirstLine(const string &path)
{
    ifstream file(path.c_str());
    string line;
    getline(file, line);
    return trim(line);
}

bool readUnsignedLongLong(const string &path, unsigned long long &value)
{
    ifstream file(path.c_str());
    if (!file.is_open())
        return false;
    file >> value;
    return !file.fail();
}

string readSysLabel(const string &basePath, const string &prefix, int index)
{
    const string labelPath = basePath + "/" + prefix + to_string(index) + "_label";
    string label = readFirstLine(labelPath);
    if (!label.empty())
        return label;
    return prefix + to_string(index);
}
}

// get cpu id and information, you can use `proc/cpuinfo`
string CPUinfo()
{
    char CPUBrandString[0x40];
    unsigned int CPUInfo[4] = {0, 0, 0, 0};

    // unix system
    // for windoes maybe we must add the following
    // __cpuid(regs, 0);
    // regs is the array of 4 positions
    __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
    unsigned int nExIds = CPUInfo[0];

    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);

        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    string str(CPUBrandString);
    return str;
}

// getOsName, this will get the OS of the current computer
string getOsName()
{
    ifstream file("/etc/os-release");
    string line;
    while (getline(file, line))
    {
        if (line.rfind("PRETTY_NAME=", 0) == 0)
            return trim(line.substr(strlen("PRETTY_NAME=")));
    }
    return "Linux";
}

SystemIdentity getSystemIdentity()
{
    SystemIdentity info;
    info.osName = getOsName();

    const char *login = getlogin();
    if (login != NULL)
        info.userName = login;
    else
    {
        const char *envUser = getenv("USER");
        info.userName = envUser != NULL ? envUser : "unknown";
    }

    char hostname[HOST_NAME_MAX + 1];
    memset(hostname, 0, sizeof(hostname));
    if (gethostname(hostname, sizeof(hostname) - 1) == 0)
        info.hostName = hostname;
    else
        info.hostName = "unknown";

    info.cpuName = trim(CPUinfo());
    return info;
}

CPUStats readCPUStats()
{
    CPUStats stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ifstream file("/proc/stat");
    string cpu;
    if (file.is_open())
    {
        file >> cpu >> stats.user >> stats.nice >> stats.system >> stats.idle >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal >> stats.guest >> stats.guestNice;
    }
    return stats;
}

float calculateCPUUsage(const CPUStats &previous, const CPUStats &current)
{
    const long long prevIdle = previous.idle + previous.iowait;
    const long long idle = current.idle + current.iowait;

    const long long prevNonIdle = previous.user + previous.nice + previous.system + previous.irq + previous.softirq + previous.steal;
    const long long nonIdle = current.user + current.nice + current.system + current.irq + current.softirq + current.steal;

    const long long prevTotal = prevIdle + prevNonIdle;
    const long long total = idle + nonIdle;
    const long long totalDelta = total - prevTotal;
    const long long idleDelta = idle - prevIdle;

    if (totalDelta <= 0)
        return 0.0f;
    return static_cast<float>(100.0 * (totalDelta - idleDelta) / totalDelta);
}

ProcessCounts getProcessCounts()
{
    ProcessCounts counts = {0, 0, 0, 0, 0, 0, 0};
    DIR *dir = opendir("/proc");
    if (dir == NULL)
        return counts;

    dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isdigit(entry->d_name[0]))
            continue;

        const string path = string("/proc/") + entry->d_name + "/stat";
        ifstream file(path.c_str());
        if (!file.is_open())
            continue;

        string line;
        getline(file, line);
        const size_t rightParen = line.rfind(')');
        if (rightParen == string::npos || rightParen + 2 >= line.size())
            continue;

        const char state = line[rightParen + 2];
        counts.total++;
        switch (state)
        {
        case 'R':
            counts.running++;
            break;
        case 'S':
        case 'I':
            counts.sleeping++;
            break;
        case 'D':
            counts.uninterruptible++;
            break;
        case 'Z':
            counts.zombie++;
            break;
        case 'T':
        case 't':
            counts.stopped++;
            break;
        default:
            counts.other++;
            break;
        }
    }

    closedir(dir);
    return counts;
}

FanInfo getFanInfo()
{
    FanInfo fan = {false, "Fan", "Unavailable", 0, 0, 0.0f};
    DIR *dir = opendir("/sys/class/hwmon");
    if (dir == NULL)
        return fan;

    dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "hwmon", 5) != 0)
            continue;

        const string basePath = string("/sys/class/hwmon/") + entry->d_name;
        for (int index = 1; index <= 8; ++index)
        {
            unsigned long long rpm = 0;
            if (!readUnsignedLongLong(basePath + "/fan" + to_string(index) + "_input", rpm))
                continue;

            unsigned long long maxRpm = 0;
            readUnsignedLongLong(basePath + "/fan" + to_string(index) + "_max", maxRpm);

            fan.available = true;
            fan.label = readSysLabel(basePath, "fan", index);
            fan.rpm = static_cast<int>(rpm);
            fan.maxRpm = static_cast<int>(maxRpm);
            fan.status = rpm > 0 ? "Active" : "Disabled";
            if (maxRpm > 0)
                fan.levelPercent = static_cast<float>((100.0 * rpm) / maxRpm);
            else
                fan.levelPercent = rpm > 0 ? min(100.0f, rpm / 50.0f) : 0.0f;
            closedir(dir);
            return fan;
        }
    }

    closedir(dir);
    return fan;
}

ThermalInfo getThermalInfo()
{
    ThermalInfo thermal = {false, "Temperature", 0.0f};
    DIR *dir = opendir("/sys/class/hwmon");
    if (dir == NULL)
        return thermal;

    dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "hwmon", 5) != 0)
            continue;

        const string basePath = string("/sys/class/hwmon/") + entry->d_name;
        for (int index = 1; index <= 10; ++index)
        {
            unsigned long long tempMilliC = 0;
            if (!readUnsignedLongLong(basePath + "/temp" + to_string(index) + "_input", tempMilliC))
                continue;

            thermal.available = true;
            thermal.label = readSysLabel(basePath, "temp", index);
            thermal.temperatureC = static_cast<float>(tempMilliC / 1000.0);
            closedir(dir);
            return thermal;
        }
    }

    closedir(dir);
    return thermal;
}
