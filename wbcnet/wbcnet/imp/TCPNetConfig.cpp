/* 
 * Copyright (C) 2008 Roland Philippsen <roland dot philippsen at gmx dot net>
 *
 * BSD license:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of
 *    contributors to this software may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR THE CONTRIBUTORS TO THIS SOFTWARE BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TCPNetConfig.hpp"
#include <wbcnet/SockWrap.hpp>
#include <wbcnet/strutil.hpp>
#include <wbcnet/log.hpp>
#include <iostream>

using namespace std;

static wbcnet::logger_t logger(wbcnet::get_logger("wbcnet"));


namespace wbcnet {
  
  
  TCPServerNetConfig::
  TCPServerNetConfig(std::string const & _bind_ip)
    : NetConfig(),
      bind_ip(_bind_ip)
  {
  }
  
  
  TCPClientNetConfig::
  TCPClientNetConfig(std::string const & _server_ip)
    : NetConfig(),
      server_ip(_server_ip)
  {
  }
  

#ifdef DISABLE_NETWORKING
  
  
  wbcnet::Channel * TCPServerNetConfig::
  CreateChannel(process_t from_process,
		process_t to_process) const
    throw(runtime_error)
  {
    throw runtime_error("wbcnet::TCPServerNetConfig::CreateChannel(): no networking support");
  }
  
  
  wbcnet::Channel * TCPClientNetConfig::
  CreateChannel(process_t from_process,
		process_t to_process) const
    throw(runtime_error)
  {
    throw runtime_error("wbcnet::TCPClientNetConfig::CreateChannel(): no networking support");
  }
  
  
#else // DISABLE_NETWORKING
  
  
  static in_port_t get_tcp_port(NetConfig::process_t from_process,
				NetConfig::process_t to_process)
    throw(runtime_error)
  {
    switch (from_process) {
      
    case NetConfig::SERVO:
      if (NetConfig::MODEL == to_process)
	return 9999;
      if (NetConfig::USER == to_process)
	return 8888;
      throw runtime_error("get_tcp_port(SERVO, " + sfl::to_string(to_process)
			  + "): invalid to_process");
      
    case NetConfig::MODEL:
      if (NetConfig::SERVO == to_process)
	return 9999;
      throw runtime_error("get_tcp_port(MODEL, " + sfl::to_string(to_process)
			  + "): invalid to_process");
      
    case NetConfig::USER:
      if (NetConfig::SERVO == to_process)
	return 8888;
      throw runtime_error("get_tcp_port(USER, " + sfl::to_string(to_process)
			  + "): invalid to_process");
      
      ////case NetConfig::DISTANCE:
      ////case NetConfig::MOTOR:
    }
    
    throw runtime_error("get_tcp_port(" + sfl::to_string(from_process)
			+ ", " + sfl::to_string(to_process)
			+ "): invalid from_process / to_process combination");
    return 0;
  }
  
  
  wbcnet::Channel * TCPServerNetConfig::
  CreateChannel(process_t from_process,
		process_t to_process) const
    throw(runtime_error)
  {
    in_port_t const port(get_tcp_port(from_process, to_process));
    static bool const is_nonblocking(true);
    LOG_TRACE (logger,
	       "TCPServerNetConfig::CreateChannel(" << from_process << ","
	       << to_process << "): calling sos->Open(" << port << ", "
	       << sfl::to_string(is_nonblocking) << ", " << bind_ip << ")");
    wbcnet::SoServer * sos(new wbcnet::SoServer(0, -1));
    if ( ! sos->Open(port, is_nonblocking, bind_ip)) {
      delete sos;
      throw runtime_error("TCPServerNetConfig::CreateChannel(" + sfl::to_string(from_process) + ", "
			  + sfl::to_string(to_process) + "): sos->Open(" + sfl::to_string(port)
			  + ", " + sfl::to_string(is_nonblocking) + ", " + bind_ip
			  + ") failed");
    }
    
    LOG_TRACE (logger,
	       "TCPServerNetConfig::CreateChannel(" << from_process << ","
	       << to_process << "): calling sos->BindListen(1)");
    if ( ! sos->BindListen(1)) {
      delete sos; 
      throw runtime_error("TCPServerNetConfig::CreateChannel(" + sfl::to_string(from_process) + ", "
			  + sfl::to_string(to_process) + "): sos->BindListen(1) failed");
    }
    
    cout << "TCPServerNetConfig::CreateChannel(" << from_process << ","
	 << to_process << "): accepting " << flush;
    wbcnet::com_status cs(sos->Accept());
    while (wbcnet::COM_TRY_AGAIN == cs) {
      cout << "." << flush;
      usleep(250000);
      cs = sos->Accept();
    }
    if (wbcnet::COM_OK != cs) {
      LOG_TRACE (logger,
		 "TCPServerNetConfig::CreateChannel(" << from_process << ","
		 << to_process << "): sos->Accept() POOPOO");
      delete sos;
      throw runtime_error("TCPServerNetConfig::CreateChannel(" + sfl::to_string(from_process) + ", "
			  + sfl::to_string(to_process) + "): Accept() failed with "
			  + wbcnet::com_status_str(cs));
    }
    cout << "OK\n";
    
    return sos;
  }


  wbcnet::Channel * TCPClientNetConfig::
  CreateChannel(process_t from_process,
		process_t to_process) const
    throw(runtime_error)
  {
    in_port_t const port(get_tcp_port(from_process, to_process));
    static bool const is_nonblocking(true);
    wbcnet::SoClient * soc(new wbcnet::SoClient(0, -1));
    if ( ! soc->Open(port, is_nonblocking, server_ip)) {
      delete soc;
      throw runtime_error("TCPClientNetConfig::CreateChannel(" + sfl::to_string(from_process) + ", "
			  + sfl::to_string(to_process) + "): soc->Open(" + sfl::to_string(port)
			  + ", " + sfl::to_string(is_nonblocking) + ", " + server_ip
			  + ") failed");
    }
    
    cout << "TCPClientNetConfig::CreateChannel(" << from_process << ","
	 << to_process << "): connecting " << flush;
    wbcnet::com_status cs(soc->Connect());
    while ((wbcnet::COM_TRY_AGAIN == cs) || (wbcnet::COM_OTHER_ERROR == cs)) {
      if (wbcnet::COM_TRY_AGAIN == cs)
	cout << "." << flush;
      else
	cout << "x" << flush;      
      usleep(250000);
      cs = soc->Connect();
    }
    if (wbcnet::COM_OK != cs) {
      delete soc;
      throw runtime_error("TCPClientNetConfig::CreateChannel(" + sfl::to_string(from_process) + ", "
			  + sfl::to_string(to_process) + "): soc->Connect() failed with "
			  + wbcnet::com_status_str(cs));
    }
    cout << "OK\n";
    
    return soc;
  }

#endif // DISABLE_NETWORKING
  
}
