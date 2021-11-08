#ifndef RTSP_AUTHENTICATOR_H
#define RTSP_AUTHENTICATOR_H

#include <string>
#include "RtspMessage.h"
#include "net/BufferReader.h"

namespace xop
{

class Authenticator
{
public:
	Authenticator() {};
	virtual ~Authenticator() {};

    virtual std::string GetUsername() const = 0;
    virtual std::string GetRealm() const = 0;
    
    virtual bool Authenticate(std::shared_ptr<RtspRequest> request, std::string &nonce) = 0;
    virtual bool HandleUnauthorized(BufferReader* buffer, std::string& nonce, bool& unauthorized) = 0;
    virtual std::string GetResponse(std::string nonce, std::string cmd, std::string url)  = 0;
    virtual size_t GetFailedResponse(std::shared_ptr<RtspRequest> request, std::shared_ptr<char> buf, size_t size) = 0;

private:

};

}

#endif
