#include <netdb.h>
#include <strings.h> // bzero
#include <netinet/in.h>
#include <assert.h>
#include <stddef.h>

#include "InetAddress.hpp"
#include "SocketHelp.hpp"
#include "Endian.hpp"

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6)
{
    assert(offsetof(InetAddress, m_addr6) == 0);
    assert(offsetof(InetAddress, m_addr) == 0);
    if (ipv6)
    {
        bzero(&m_addr6, sizeof m_addr6);
        m_addr6.sin6_family = AF_INET6;
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        m_addr6.sin6_addr = ip;
        m_addr6.sin6_port = sockets::hostToNetwork16(port);
    }
    else
    {
        bzero(&m_addr, sizeof m_addr);
        m_addr.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
        m_addr.sin_addr.s_addr = sockets::hostToNetwork32(ip);
        m_addr.sin_port = sockets::hostToNetwork16(port);
    }
}

InetAddress::InetAddress(std::string ip, uint16_t port, bool ipv6)
{
    if (ipv6)
    { //now no unsed
    }
    else
    {
        ::bzero(&m_addr, sizeof m_addr);
        sockets::fromIpPort(ip.c_str(), port, &m_addr);
    }
}

uint32_t InetAddress::ipNetEndian() const
{
    assert(family() == AF_INET);
    return m_addr.sin_addr.s_addr;
}

std::string InetAddress::toIpPort() const
{
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof buf, getSockAddr());
    return buf;
}

std::string InetAddress::toIp() const
{
    char buf[64] = "";
    sockets::toIp(buf, sizeof buf, getSockAddr());
    return buf;
}

uint16_t InetAddress::toPort() const
{
    //port net endian
    return sockets::networkToHost16(m_addr.sin_port);
}