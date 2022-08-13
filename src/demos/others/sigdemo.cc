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

#include "dlog.h"

#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <signal.h>

void sign_handler(int sig)
{
    log_warn("recv sig: %d", sig);
}

int32_t main(int32_t argc, char* argv[])
{
	log_info("start");
	for (uint32_t i=0; i<64; i++) {
        if ( (i == 9) || (i == SIGINT) || (i == SIGTERM) || (i == 40) ) {
        	continue;
        }
        signal(i, SIG_IGN);
    }
    // signal(SIGINT, sign_handler);
    signal(SIGTERM, sign_handler);
    signal(40, sign_handler);
    signal(41, sign_handler);
    signal(42, sign_handler);
    signal(43, sign_handler);
    signal(44, sign_handler);
    signal(45, sign_handler);
    signal(46, sign_handler);
    signal(47, sign_handler); 
    signal(48, sign_handler); 
    signal(50, sign_handler);
    signal(51, sign_handler);
    signal(52, sign_handler);
    signal(53, sign_handler);

	uint32_t* arr = nullptr;
	*arr = 1;

	sleep(10);

	return 0;
}