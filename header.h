// To make sure you don't declare the function more than once by including the header multiple times.
#ifndef header_H
#define header_H

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>
// lib to read from file
#include <fstream>
// for the name of the computer and the logged in user
#include <unistd.h>
#include <limits.h>
// this is for us to get the cpu information
// mostly in unix system
// not sure if it will work in windows
#include <cpuid.h>
// this is for the memory usage and other memory visualization
// for linux gotta find a way for windows
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
// for time and date
#include <ctime>
// ifconfig ip addresses
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>

using namespace std;

struct CPUStats
{
    long long int user;
    long long int nice;
    long long int system;
    long long int idle;
    long long int iowait;
    long long int irq;
    long long int softirq;
    long long int steal;
    long long int guest;
    long long int guestNice;
};

// processes `stat`
struct Proc
{
    int pid;
    string name;
    char state;
    long long int vsize;
    long long int rss;
    long long int utime;
    long long int stime;
};

struct IP4
{
    string name;
    string address;
};

struct Networks
{
    vector<IP4> ip4s;
};

struct TX
{
    unsigned long long bytes;
    unsigned long long packets;
    unsigned long long errs;
    unsigned long long drop;
    unsigned long long fifo;
    unsigned long long colls;
    unsigned long long carrier;
    unsigned long long compressed;
};

struct RX
{
    unsigned long long bytes;
    unsigned long long packets;
    unsigned long long errs;
    unsigned long long drop;
    unsigned long long fifo;
    unsigned long long frame;
    unsigned long long compressed;
    unsigned long long multicast;
};

struct SystemIdentity
{
    string osName;
    string userName;
    string hostName;
    string cpuName;
};

struct MemoryStats
{
    unsigned long long memTotalKB;
    unsigned long long memUsedKB;
    unsigned long long swapTotalKB;
    unsigned long long swapUsedKB;
};

struct DiskStats
{
    unsigned long long totalBytes;
    unsigned long long usedBytes;
    unsigned long long freeBytes;
};

struct ProcessInfo
{
    int pid;
    string name;
    char state;
    float cpuPercent;
    float memoryPercent;
    unsigned long long totalTimeTicks;
};

struct ProcessCounts
{
    int running;
    int sleeping;
    int uninterruptible;
    int zombie;
    int stopped;
    int other;
    int total;
};

struct FanInfo
{
    bool available;
    string label;
    string status;
    int rpm;
    int maxRpm;
    float levelPercent;
};

struct ThermalInfo
{
    bool available;
    string label;
    float temperatureC;
};

struct NetworkEntry
{
    string name;
    string ipv4;
    RX rx;
    TX tx;
};

string CPUinfo();
string getOsName();
SystemIdentity getSystemIdentity();
CPUStats readCPUStats();
float calculateCPUUsage(const CPUStats &previous, const CPUStats &current);
ProcessCounts getProcessCounts();
FanInfo getFanInfo();
ThermalInfo getThermalInfo();

MemoryStats getMemoryStats();
DiskStats getDiskStats(const char *path = "/");
vector<ProcessInfo> getProcesses(unsigned long long totalCpuTicksDelta, const map<int, unsigned long long> &previousTicks, unsigned long long memTotalKB);

Networks getIPv4Addresses();
vector<NetworkEntry> getNetworkEntries();
string formatBytes(unsigned long long bytes);
float bytesToDisplayScale(unsigned long long bytes, float maxDisplayGB = 2.0f);

#endif
