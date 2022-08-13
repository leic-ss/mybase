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

namespace mybase
{

class Pipe
{
public:
    Pipe() { open(); }
    ~Pipe() { close(); }

public:
    /// open a pipe
    bool open()
    {
    	if(0 != ::pipe(&m_handles[0])) {
            m_handles[0] = -1;
            m_handles[1] = -1;
	        return false;
        }
        return true;
    }

    /// get read handle
    int32_t read_handle() const		{ return m_handles[0]; }
    /// get write handle
    int32_t write_handle() const	{ return m_handles[1]; }

    /// read from pipe
    size_t read(void *buf, size_t count) { return ::read(read_handle(), buf, count); }
    /// write to pipe
    size_t write(const void *buf, size_t count) { return ::write(write_handle(), buf, count); }

    bool wait_for_read(timeval * timeout) const
    {
        fd_set rfds;
        FD_SET(read_handle(), &rfds);
        int r = ::select(read_handle() + 1, &rfds, nullptr, nullptr, timeout);
        return r > 0;
    }
    bool wait_for_read() const { return wait_for_read(nullptr); }

    bool wait_for_write(timeval * timeout) const
    {
        fd_set wfds;
        FD_SET(write_handle(), &wfds);
        int r = ::select(write_handle() + 1, nullptr, &wfds, nullptr, timeout);
        return r > 0;
    }
    bool wait_for_write() const { return wait_for_write(nullptr); }

    void close_read()
    {
        if(-1 != read_handle()) {
            ::close(m_handles[0]);
            m_handles[0] = -1;
        }
    }

    void close_write()
    {
        if(-1 != write_handle()) {
            ::close(m_handles[1]);
            m_handles[1] = -1;
        }
    }

    void close()
    {
        close_write();
        close_read();
    }

    /// set nonblocking
    bool set_nonblocking() { return set_nonblocking(read_handle()) && set_nonblocking(write_handle()); }

public:
    /// set nonblocking
    static bool set_nonblocking(int32_t sock)
    {
    	int32_t opts = fcntl(sock, F_GETFL);
        if(opts < 0) {
	        return false;
        }
        opts = opts | O_NONBLOCK;
        return fcntl(sock, F_SETFL, opts) != -1;
    }

private:
    int32_t m_handles[2];

    Pipe(const Pipe&);
    Pipe& operator=(const Pipe&);
};

}