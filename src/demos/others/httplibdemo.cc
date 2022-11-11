/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#include "public/dlog.h"
#include "public/httplib/httplib.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <signal.h>

int32_t main(int32_t argc, char* argv[])
{
	// HTTP
    httplib::Client cli("192.168.56.103:7080");

    // HTTPS
    // httplib::Client cli("https://github.com/yhirose/cpp-httplib");
    cli.set_connection_timeout(0, 200000);

    auto res = cli.Post("/api/v1/storage/rebalance");
    if (res) {
        log_info("status: %d body: %s", res->status, res->body.c_str());
    } else {
        log_error("error");
    }

	return 0;
}