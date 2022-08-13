#include "gtest/gtest.h"

#include "sessionmgn.h"
#include "common.h"
#include "cast_helper.h"

class ProxySession : public nubase::BaseSession
{
public:
    ProxySession(uint64_t timeout, uint64_t seq) : nubase::BaseSession(timeout, seq) {}

public:
    int32_t number;
};

TEST(sessionmgn, test_sessionmgn)
{
	nubase::CSessionMgn sessionMgn(1000, 1);

	for (uint32_t i = 0; i < 1000; i++) {
		std::shared_ptr<nubase::BaseSession> session =
                std::make_shared<ProxySession>(TimeHelper::currentMs() + 4, 100);
	    _SC(ProxySession*, session.get())->number = 100;
	    sessionMgn.saveSession(session);

	    if (i % 2 == 0) {
	    	session = sessionMgn.eraseAndGetSession(i);
	    	session = sessionMgn.eraseAndGetSession(100000000);
	    }
	}
}