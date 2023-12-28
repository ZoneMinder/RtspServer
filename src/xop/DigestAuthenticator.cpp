#include "DigestAuthenticator.h"
#include "md5/md5.hpp" 

using namespace xop;

DigestAuthenticator::DigestAuthenticator(std::string realm, std::string username, std::string password)
	: realm_(realm)
	, username_(username)
	, password_(password)
{

}

DigestAuthenticator::~DigestAuthenticator()
{

}

std::string DigestAuthenticator::GetResponse(std::string nonce, std::string cmd, std::string url)
{
	//md5(md5(<username>:<realm> : <password>) :<nonce> : md5(<cmd>:<url>))

	auto hex1 = md5::md5_hash_hex(username_ + ":" + realm_ + ":" + password_);
	auto hex2 = md5::md5_hash_hex(cmd + ":" + url);
	auto response = md5::md5_hash_hex(hex1 + ":" + nonce + ":" + hex2);
	return response;
}

bool DigestAuthenticator::Authenticate(
    std::shared_ptr<RtspRequest> rtsp_request)
{
  std::string cmd = rtsp_request->MethodToString[rtsp_request->GetMethod()];
  std::string url = rtsp_request->GetRtspUrl();

  if (nonce_.size() > 0 && (GetResponse(nonce_, cmd, url) == rtsp_request->GetAuthResponse())) {
    return true;
  } else {
#if 0
#endif
    return false;
  }
}

size_t DigestAuthenticator::GetFailedResponse(
    std::shared_ptr<RtspRequest> rtsp_request,
    std::shared_ptr<char> buf,
    size_t size)
{
  nonce_ = md5::generate_nonce();
  return rtsp_request->BuildUnauthorizedRes(buf.get(), size, realm_.c_str(), nonce_.c_str());
}
