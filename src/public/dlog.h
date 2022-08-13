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

#pragma once

#include "cclogger.h"
#include "cast_helper.h"

static mybase::BaseLogger* sDefLogger = _SC(mybase::BaseLogger*, mybase::CCLogger::instance());

#define log_error(...) _log_err(sDefLogger, __VA_ARGS__)
#define log_warn(...) _log_warn(sDefLogger, __VA_ARGS__)
#define log_info(...) _log_info(sDefLogger, __VA_ARGS__)
#define log_debug(...) _log_debug(sDefLogger, __VA_ARGS__)
#define log_trace(...) _log_trace(sDefLogger, __VA_ARGS__)
