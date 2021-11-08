#include "DigestAuthenticator.h"
#include "md5/md5.hpp"

#include <iostream>

using namespace xop;

DigestAuthenticator::DigestAuthenticator(std::string realm, std::string username, std::string password)
	: realm_(realm)
	, username_(username)
	, password_(password)
{
#ifdef DEBUG
	#if defined(ANDROID)
			__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "DigestAuthenticator Constructor");
	#else
			std::cout << "DigestAuthenticator Constructor" <<  std::endl;
	#endif
#endif
}

DigestAuthenticator::~DigestAuthenticator()
{

}

std::string DigestAuthenticator::GetNonce()
{
#ifdef DEBUG
	#if defined(ANDROID)
			__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "DigestAuthenticator GetNonce");
	#else
			std::cout << "DigestAuthenticator GetNonce" <<  std::endl;
	#endif
#endif
	return md5::generate_nonce();
}

std::string DigestAuthenticator::GetResponse(std::string nonce, std::string cmd, std::string url)
{
	//md5(md5(<username>:<realm> : <password>) :<nonce> : md5(<cmd>:<url>))

	auto hex1 = md5::md5_hash_hex(username_ + ":" + realm_ + ":" + password_);
	auto hex2 = md5::md5_hash_hex(cmd + ":" + url);
	auto response = md5::md5_hash_hex(hex1 + ":" + nonce + ":" + hex2);
#ifdef DEBUG
	#if defined(ANDROID)
			__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "DigestAuthenticator GetResponse");
	#else
			std::cout << "DigestAuthenticator GetResponse" <<  std::endl;
	#endif
#endif

	return response;
}

bool parseParam(std::string& line, std::string param, std::string& value)
{
    size_t idx = line.find(param);
    if(idx != line.npos)
    {
        int cnt1 = idx + param.size();
        while(line[cnt1++] != '\"');
        int cnt2 = cnt1+1;
        while(line[cnt2++] != '\"');
        value = line.substr(cnt1, cnt2 - cnt1 - 1);
        std::cout << "Value = " << value << std::endl;
        return true;
    }
    return false;
}

bool DigestAuthenticator::HandleUnauthorized(BufferReader* buffer, std::string& nonce, bool& unauthorized)
{
    bool complete = false;
    unauthorized = false;
    // TRY PARSE STUFF OF THIS FORM:
    // "RTSP/1.0 401 Unauthorized"
    // "Content-Length: 91"
    // "Content-Type: text/html"
    // "WWW-Authenticate: Digest realm=\"visocon.rtsp\" charset=\"UTF-8\" nonce=\"761ca587865b0c3ba7af2a0e95876833\""
    while(!complete)
    {
        const char* firstCrlf = buffer->FindFirstCrlf();
        const char* start = buffer->Peek();
        if(firstCrlf != nullptr)
        {
            std::string line(start, firstCrlf);
            if(line.find("Unauthorized") != line.npos)
                unauthorized = true;

            if(line.find("WWW-Authenticate") != line.npos && line.find("Digest") != line.npos)
            {
                if(parseParam(line, "realm", realm_) &&
                   parseParam(line, "nonce", nonce))
                    complete = true;
            }
            buffer->RetrieveUntil(firstCrlf + 2);
        }
        else
            break;
    }
    return complete;
}

bool DigestAuthenticator::Authenticate(
    std::shared_ptr<RtspRequest> rtsp_request,
    std::string &nonce)
{
  std::string cmd = rtsp_request->MethodToString[rtsp_request->GetMethod()];
  std::string url = rtsp_request->GetRtspUrl();

  if (nonce.size() > 0 && (GetResponse(nonce, cmd, url) == rtsp_request->GetAuthResponse()))
	{
#ifdef DEBUG
		#if defined(ANDROID)
				__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "DigestAuthenticator Authenticate success %s - %s - %s", cmd.c_str(), url.c_str(), nonce.c_str());
		#else
				std::cout << "DigestAuthenticator Authenticate success" <<  cmd << " - "  << url.c_str() << " - " << nonce.c_str() << std::endl;
		#endif
#endif
    return true;
  } else {
#ifdef DEBUG
		#if defined(ANDROID)
				__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "DigestAuthenticator Authenticate failed %s - %s - %s", cmd.c_str(), url.c_str(), nonce.c_str());
		#else
				std::cout << "DigestAuthenticator Authenticate failed" <<  cmd << " - "  << url.c_str() << " - " << nonce.c_str() << std::endl;
		#endif
#endif
    return false;
  }
}

size_t DigestAuthenticator::GetFailedResponse(
    std::shared_ptr<RtspRequest> rtsp_request,
    std::shared_ptr<char> buf,
    size_t size)
{
  std::string nonce = md5::generate_nonce();
#ifdef DEBUG
#if defined(ANDROID)
		__android_log_print(ANDROID_LOG_ERROR,  MODULE_NAME, "DigestAuthenticator GetFailedResponse %s - %s - %s", buf.get(), realm_.c_str(), nonce.c_str());
#else
		std::cout << "DigestAuthenticator GetFailedResponse" <<  buf.get() << " - "  << realm_.c_str() << " - " << nonce.c_str() << std::endl;
#endif
#endif
  return rtsp_request->BuildUnauthorizedRes(buf.get(), size, realm_.c_str(), nonce.c_str());
}
