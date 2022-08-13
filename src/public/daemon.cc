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

#include "daemon.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int32_t daemonize(mybase::BaseLogger* logger)
{
    // fork at first
    pid_t pid = fork();
    if (pid != 0) {
        int32_t status = -1;
        waitpid(pid, &status, 0);
        // _log_warn(logger, "wait child success! child pid[%d] status[%d]", pid, status);
        exit(0);
    } else if(pid < 0) {
        _log_err(logger, "fork failed, errno=%d: %s", errno, strerror(errno));
        return -1;
    }

    // create session
    if (setsid() < 0) {
        _log_err(logger, "setsid failed, errno=%d: %s\n", errno, strerror(errno));
        return -1;
    }

    // chdir
    // chdir("/");

    // umask
    umask(0);

    // fork again
    pid = fork();
    if (pid != 0) {
        // _log_warn(logger, "exit! ppid[%d] child pid[%d]", getppid(), pid);
        exit(0);
    } else if(pid < 0) {
        _log_err(logger, "fork failed, errno=%d: %s\n", errno, strerror(errno));
        return -1;
    }

    _log_info(logger, "daemonize success! pid[%u] ppid[%u]", getpid(), getppid());

    int32_t fd = open("/dev/null", 0);
    if (fd != -1) {
        dup2(fd, 0);
        close(fd);
    }

    // close standard fds
    // close(0);
    // close(1);
    // close(2);

    // // redirect standard fds to null
    // int stdfd = open("/dev/null", O_RDWR);
    // dup2(stdfd, STDIN_FILENO);
    // dup2(stdfd, STDOUT_FILENO);
    // dup2(stdfd, STDERR_FILENO);

    return 0;
}