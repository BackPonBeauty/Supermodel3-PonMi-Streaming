#ifdef SUPERMODEL_WIN32
#define MINIUPNP_STATICLIB
#include "UPnPHelper.h"
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <cstdio>
#include <string>

bool UPnPHelper::OpenPort(int port, const std::string& description)
{
    int error = 0;
    UPNPDev* devList = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);
    if (!devList)
    {
        printf("[UPnP] No devices found (error=%d)\n", error);
        return false;
    }

    UPNPUrls urls = {};
    IGDdatas data = {};
    char localIP[64] = {};
    char wanIP[64] = {};

    int ret = UPNP_GetValidIGD(devList, &urls, &data, localIP, sizeof(localIP), wanIP, sizeof(wanIP));
    freeUPNPDevlist(devList);

    if (ret <= 0)
    {
        printf("[UPnP] No valid IGD found\n");
        return false;
    }

    std::string portStr = std::to_string(port);
    int r = UPNP_AddPortMapping(
        urls.controlURL,
        data.first.servicetype,
        portStr.c_str(),
        portStr.c_str(),
        localIP,
        description.c_str(),
        "UDP",
        nullptr,
        "0");

    FreeUPNPUrls(&urls);

    if (r == UPNPCOMMAND_SUCCESS)
    {
        printf("[UPnP] Port %d opened (%s)\n", port, description.c_str());
        return true;
    }
    else
    {
        printf("[UPnP] Port %d failed (err=%d)\n", port, r);
        return false;
    }
}

void UPnPHelper::ClosePort(int port)
{
    int error = 0;
    UPNPDev* devList = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);
    if (!devList) return;

    UPNPUrls urls = {};
    IGDdatas data = {};
    char localIP[64] = {};
    char wanIP[64] = {};

    int ret = UPNP_GetValidIGD(devList, &urls, &data, localIP, sizeof(localIP), wanIP, sizeof(wanIP));
    freeUPNPDevlist(devList);
    if (ret <= 0) return;

    std::string portStr = std::to_string(port);
    UPNP_DeletePortMapping(
        urls.controlURL,
        data.first.servicetype,
        portStr.c_str(),
        "UDP",
        nullptr);

    FreeUPNPUrls(&urls);
    printf("[UPnP] Port %d closed\n", port);
}

void UPnPHelper::OpenStreamingPorts(int linkplay)
{
    int base = 5000 + (linkplay - 1) * 4;
    OpenPort(base,     "Supermodel XInput");
    OpenPort(base + 1, "Supermodel Handshake");
    OpenPort(base + 2, "Supermodel Video");
    OpenPort(base + 3, "Supermodel Audio");
}

void UPnPHelper::CloseStreamingPorts(int linkplay)
{
    int base = 5000 + (linkplay - 1) * 4;
    ClosePort(base);
    ClosePort(base + 1);
    ClosePort(base + 2);
    ClosePort(base + 3);
}

#endif // SUPERMODEL_WIN32