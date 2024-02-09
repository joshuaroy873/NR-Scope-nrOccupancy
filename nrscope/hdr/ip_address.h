#pragma once
#ifndef __Ip_Address_H__
#define __Ip_Address_H__


#include <string>

class IpAddress
{
public:
    enum Type
    {
        Unknown,
        IpV4,
        IpV6
    };
    ~IpAddress(void);

    /**
     * \brief   Gets the host address part of the IP address.
     * \return  The host address part of the IP address.
    **/
    const std::string& getHostAddress() const;

    /**
     * \brief   Gets the port number part of the address if any.
     * \return  The port number.
    **/
    unsigned int getPortNumber() const;

    /**
     * \brief   Gets the type of the IP address.
     * \return  The type.
    **/
    IpAddress::Type getType() const;

    /**
     * \fn  static IpAddress Parse(const std::string& ip_address_str)
     *
     * \brief   Parses a given string to an IP address.
     * \param   ip_address_str  The ip address string to be parsed.
     * \return  Returns the parsed IP address. If the IP address is
     *          invalid then the IpAddress instance returned will have its
     *          type set to IpAddress::Unknown
    **/
    static IpAddress Parse(const std::string& ip_address_str);

    /**
     * \brief   Converts the given type to string.
     * \param   address_type    Type of the address to be converted to string.
     * \return  String form of the given address type.
    **/
    static std::string TypeToString(IpAddress::Type address_type);
private:
    IpAddress(void);

    Type m_type;
    std::string m_hostAddress;
    int m_portNumber;
};

#endif // __IpAddress_H__