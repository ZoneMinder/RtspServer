//PHZ
//2019-10-6

#ifndef RTSP_DIGEST_AUTHENTICATOR_H
#define RTSP_DIGEST_AUTHENTICATOR_H

#include "Authenticator.h"

#include <cstdint>
#include <string>

namespace xop
{

class DigestAuthenticator : public Authenticator
{
public:
	DigestAuthenticator(std::string realm, std::string username, std::string password);
	virtual ~DigestAuthenticator();

	std::string GetRealm() const
	{ return realm_; }

	std::string GetUsername() const
	{ return username_; }

	std::string GetPassword() const
	{ return password_; }

	std::string GetResponse(std::string nonce, std::string cmd, std::string url);

	bool Authenticate(std::shared_ptr<RtspRequest> request);
	size_t GetFailedResponse(std::shared_ptr<RtspRequest> request, std::shared_ptr<char> buf, size_t size);

private:
	std::string realm_;
	std::string username_;
	std::string password_;
	std::string nonce_;
};

}

#endif
