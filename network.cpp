#include "header.h"

namespace
{
TX emptyTX()
{
    TX tx = {0, 0, 0, 0, 0, 0, 0, 0};
    return tx;
}

RX emptyRX()
{
    RX rx = {0, 0, 0, 0, 0, 0, 0, 0};
    return rx;
}
}

Networks getIPv4Addresses()
{
    Networks networks;
    ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1)
        return networks;

    set<string> seen;
    for (ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        char host[INET_ADDRSTRLEN];
        memset(host, 0, sizeof(host));
        const void *address = &reinterpret_cast<sockaddr_in *>(ifa->ifa_addr)->sin_addr;
        if (inet_ntop(AF_INET, address, host, sizeof(host)) == NULL)
            continue;

        const string name = ifa->ifa_name;
        const string ip = host;
        if (seen.insert(name + ip).second)
            networks.ip4s.push_back(IP4{name, ip});
    }

    freeifaddrs(ifaddr);
    sort(networks.ip4s.begin(), networks.ip4s.end(), [](const IP4 &left, const IP4 &right) {
        return left.name < right.name;
    });
    return networks;
}

vector<NetworkEntry> getNetworkEntries()
{
    map<string, string> ipv4ByInterface;
    Networks networks = getIPv4Addresses();
    for (size_t i = 0; i < networks.ip4s.size(); ++i)
        ipv4ByInterface[networks.ip4s[i].name] = networks.ip4s[i].address;

    vector<NetworkEntry> entries;
    ifstream file("/proc/net/dev");
    string line;
    int lineNumber = 0;
    while (getline(file, line))
    {
        lineNumber++;
        if (lineNumber <= 2)
            continue;

        const size_t colon = line.find(':');
        if (colon == string::npos)
            continue;

        NetworkEntry entry;
        entry.name = line.substr(0, colon);
        entry.name.erase(0, entry.name.find_first_not_of(" \t"));
        entry.name.erase(entry.name.find_last_not_of(" \t") + 1);
        entry.ipv4 = ipv4ByInterface.count(entry.name) ? ipv4ByInterface[entry.name] : "N/A";
        entry.rx = emptyRX();
        entry.tx = emptyTX();

        istringstream values(line.substr(colon + 1));
        values >> entry.rx.bytes >> entry.rx.packets >> entry.rx.errs >> entry.rx.drop >> entry.rx.fifo >> entry.rx.frame >> entry.rx.compressed >> entry.rx.multicast;
        values >> entry.tx.bytes >> entry.tx.packets >> entry.tx.errs >> entry.tx.drop >> entry.tx.fifo >> entry.tx.colls >> entry.tx.carrier >> entry.tx.compressed;
        entries.push_back(entry);
    }

    sort(entries.begin(), entries.end(), [](const NetworkEntry &left, const NetworkEntry &right) {
        return left.name < right.name;
    });
    return entries;
}

string formatBytes(unsigned long long bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4)
    {
        value /= 1024.0;
        unitIndex++;
    }

    ostringstream output;
    output << fixed << setprecision(unitIndex == 0 ? 0 : 2) << value << ' ' << units[unitIndex];
    return output.str();
}

float bytesToDisplayScale(unsigned long long bytes, float maxDisplayGB)
{
    const double maxBytes = static_cast<double>(maxDisplayGB) * 1024.0 * 1024.0 * 1024.0;
    if (maxBytes <= 0.0)
        return 0.0f;
    const double ratio = static_cast<double>(bytes) / maxBytes;
    return static_cast<float>(ratio > 1.0 ? 1.0 : ratio);
}
