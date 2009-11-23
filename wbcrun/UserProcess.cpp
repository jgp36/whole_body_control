/*
 * Copyright (c) 2008 Roland Philippsen <roland DOT philippsen AT gmx DOT net>
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

#include "UserProcess.hpp"
#include "message_id.hpp"
#include <wbcnet/NetConfig.hpp>
#include <wbcnet/log.hpp>
#include <iostream>
#include <sstream>
#include <limits>
#include <cmath>

#ifdef WIN32
#define M_PI 3.141592653
#endif

static wbcnet::logger_t logger(wbcnet::get_logger("wbcrun"));

using namespace std; 

#ifdef WBCRUN_HAVE_CURSES
# include <curses.h>
#endif // WBCRUN_HAVE_CURSES


#ifdef WBCRUN_HAVE_XMLRPC

#include "xmlrpc/directory.hpp"
#include "XmlRpcServer.h"
#include <cstring>
#include <signal.h>
#include <err.h>

static wbcrun::XMLRPCDirectoryServer * xmlrpc_directory(0);

static void handle_SIGTSTP(int signum)
{
  // could check if we really got SIGTSTP...
  if (xmlrpc_directory) {
    warnx("handle_SIGTSTP(): exiting xmlrpc_directory");
    xmlrpc_directory->GetServer()->exit();
  }
  else
    warnx("handle_SIGTSTP(): no xmlrpc_directory found");
}

#endif // WBCRUN_HAVE_XMLRPC


static bool ncurses_active(false);
static bool keyboard_querry(false);


namespace {
  
  class MyServiceTransaction: public wbcrun::ServiceTransaction {
  public:
    wbcrun::UserProcess * m_user;
    
    explicit MyServiceTransaction(wbcrun::UserProcess * user): m_user(user) {}
    
    virtual wbcrun::ServiceMessage * GetRequest()
    { return &m_user->m_user_request; }
    
    virtual wbcrun::ServiceMessage * GetReply()
    { return &m_user->m_user_reply; }
    
    virtual void SendWaitReceive() {
      m_user->EnqueueMessage(m_user->m_channel, GetRequest(), false, false);
      m_user->SendWait(10000);
      GetReply()->code.SetNElements(0);
      GetReply()->matrix.SetSize(0, 0);
      m_user->ReceiveWait(10000, 1);
    }
  };
  
}


namespace wbcrun {
  
  
  bool UserProcess::
  Step() throw(std::exception)
  {
    keyboard_querry=false;
    static const int buflen(1024);
    static char buffer[buflen];
    
    cout << "user> " << flush;
    if ( ! cin.getline(buffer, buflen)) {
      cout << "FATAL ERROR reading standard input\n";
      return false;
    }
    keyboard_querry=true;
    istringstream line(buffer);
    string token;
    line >> token;
    if ( ! line) {
      cout << "SYNTAX ERROR reading first token\n";
      return true;
    }
    try {

      if ("pos" == token) {
        wbcrun::srv::get_pos(m_user_request);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("endpos" == token) {
        wbcrun::srv::get_end_pos(m_user_request);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("vel" == token) {
        wbcrun::srv::get_vel(m_user_request);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("tau" == token) {
        wbcrun::srv::get_torques(m_user_request);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("go" == token) {
        InteractiveGoalRequest();
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("float" == token) {
        wbcrun::srv::float_command(m_user_request);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("activate" == token) {
        wbcrun::srv::activate_command(m_user_request);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("b?" == token) {
        cout << "available behaviors:\n";
        size_t ii(0);
        for (listing_t::const_iterator bb(GetBehaviorList().begin()), bend(GetBehaviorList().end());
            bb != bend; ++bb, ++ii)
          cout << "  [" << ii << "] " << *bb << "\n";
        return true;
      }

      if (("b" == token) || ("B" == token)) {
        int bnum;
        line >> bnum;
        if ( ! line) {
          cout << "SYNTAX ERROR reading behavior number\n";
          return true;
        }
        if (0 > bnum) {
          cout << "ERROR behavior number " << bnum << " is negative\n";
          return true;
        }
        int const nbehaviors(static_cast<int>(GetBehaviorList().size()));
        if (nbehaviors <= bnum) {
          cout << "ERROR behavior number " << bnum << " is too large (max "
            << nbehaviors - 1 << ")\n";
          return true;
        }
        if (("b" == token) && (bnum == m_task_spec.behaviorID)) {
          cout << "already running behavior " << bnum << ", skipping (use capital B to override)\n";
          return true;
        }
        ++m_task_spec.requestID;
        m_task_spec.behaviorID = bnum;
        EnqueueMessage(m_channel, &m_task_spec, false, false);
        SendWait(10000);
        // do NOT ReceiveWait(), because task specs are fire-and-forget
        return true;
      }

      if ("setgoal" == token) {
        double goal[7];
        line >> goal[0] >>goal[1] >>goal[2] >>goal[3] >>goal[4] >>goal[5] >>goal[6] ;
        if ( ! line) {
          cout << "SYNTAX ERROR reading goal pos and orientation\n";
          return true;
        }
        wbcrun::srv::set_goal(m_user_request, goal, 7);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }	

      if ("r" == token) {
        wbcrun::srv::toggle_recorder(m_user_request);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }

      if ("k" == token) {
        InteractiveKeyPressLoop();
        return true;
      }

      if ("xmlrpc" == token) {
        XmlRpcLoop();
        return true;
      }

      if ("setgains" == token) {
        double gains[4];
        line >> gains[0] >>gains[1] >>gains[2] >>gains[3];
        if ( ! line) {
          cout << "SYNTAX ERROR reading gains\n";
          return true;
        }
        wbcrun::srv::set_gains(m_user_request, gains, 4);
        EnqueueMessage(m_channel, &m_user_request, false, false);
        SendWait(10000);
        ReceiveWait(10000, 1);
        return true;
      }	

      
      cout << "SYNTAX ERROR: unknown command \"" << token << "\"\n"
	" known commands:\n"
	"  pos      -  show position data\n"
	"  endpos   -  show end effector position end orientation\n"	
	"  vel      -  show velocity data\n"
	"  tau      -  show torque command\n"
	"  go       -  enter and send goal\n"
	"  float    -  send a FLOAT request (not understood by all behaviors)\n"
	"  activate -  send an ACTIVATE request (not understood by all behaviors)\n"
	"  setgoal  -  enter goal position (hardcoded 7-D vector -- x y z axis angle)\n"
	"  b?       -  list available behaviors\n"
	"  b   <N>  -  switch to behavior number <N>\n"
	"  r        -  toggle recorder state (writes them to file after the 2nd time)\n"
	"  k        -  enter interactive_key_press mode (use 'q' to leave it again)\n"
	"  xmlrpc   -  spawn XMLRPC loop, runs until SIGTSTP (i.e. Ctrl-Z)\n"
	"  setgains  -  enter control gains (type (1(x) or 2(y) or 3(yaw)) kp kd ki)\n";
    }
    catch (exception const & ee) {
      cout << "EXCEPTION " << ee.what() << "\n";
      return false;
    }
    return true;
  }
  
  
  void UserProcess::
  InteractiveKeyPressLoop()
  {
#ifndef WBCRUN_HAVE_CURSES

    cout <<
      "Sorry, but curses headers were not found on your system,"
      "so the UserProcess::InteractiveKeyPressLoop() is not available.\n"
      "If you want it, install curses (e.g. libncurses5-dev or so), wipe\n"
      "away your build dir, and rebuild.\n";

#else // WBCRUN_HAVE_CURSES (rest of method)

    if (ncurses_active) 		// "never" happens though
      endwin();
    
    // race condition with ncurses_active flag, which is not a real mutex...
    initscr();
    ncurses_active = true;
    
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    
    ostringstream tmp_error_os;
    
    bool ok(true);
    if (ERR == mvaddstr(0, 0,
			"---> interactive_key_press mode <---\n"
			"each key code is sent to the servo process and echoed here\n"
			"press 'q' or 'Q' to quit this mode\n")) {
      ok = false;
      tmp_error_os << "mvaddstr() failed for title\n";
    }
    
    while (ok) {
      
      int const ch(getch());
      if (ERR == ch) {
	usleep(10000);
	continue;
      }
      if (('q' == ch) || ('Q' == ch))
	break;
      
      wbcrun::srv::key_press(m_user_request, ch);
      EnqueueMessage(m_channel, &m_user_request, false, false);
      ostringstream os_status;
      try {
	SendWait(10000);
	ReceiveWait(10000, 1);
      }
      catch (exception const & ee) {
	ok = false;
	tmp_error_os << "EXCEPTION during send or receive\n  " << ee.what() << "\n";
	os_status << "EXCEPTION during send or receive\n  " << ee.what();
	break;
      }
      if (ok)
	os_status << "sent: " << ch;
      
      if (ERR == mvaddstr(5, 5, os_status.str().c_str())) {
	ok = false;
	tmp_error_os << "mvaddstr() failed\n";
	break;
      }
      if (ERR == refresh()) {
	ok = false;
	tmp_error_os << "refresh() failed\n";
	break;
      }
      
    }
    
    // another race condition...
    endwin();
    ncurses_active = false;
    
    if (logger->isErrorEnabled() && ( ! tmp_error_os.str().empty()))
      LOG_ERROR (logger, "errors during ncurses mode:\n" << tmp_error_os.str());
    
#endif // WBCRUN_HAVE_CURSES
  }
  
  
  UserProcess::
  UserProcess()
    : Process("user", 0, -1, wbcnet::ENDIAN_DETECT),
      m_channel(0),
      m_user_request(msg::USER_REQUEST),
      m_user_reply(msg::USER_REPLY),
      m_task_spec(msg::TASK_SPEC),
      m_directory_client(0)
  {
  }
  
  
  UserProcess::
  ~UserProcess()
  {
    delete m_channel;
    delete m_directory_client;
  }
  
  
  void UserProcess::
  Init(wbcnet::NetConfig const & netconf) throw(std::exception)
  {
    if (m_channel) {
      // We could also just return, but maybe people think they can
      // re-configure us by calling Init() with a different NetConfig,
      // but that's trickier than it might seem because of the
      // incoming and outgoing message queues.
      throw runtime_error("wbcrun::UserProcess::DoInit(): already initialized");
    }
    
    m_directory_client = new DirectoryCmdClient(new MyServiceTransaction(this), true);
    
    m_channel = netconf.CreateChannel(wbcnet::NetConfig::USER, wbcnet::NetConfig::SERVO);
    if ( ! m_channel)
      throw runtime_error("wbcrun::UserProcess::DoInit(): could not create channel");
    AddSink(m_channel, 100);
    AddSource(m_channel, 100);
    
    CreateHandler(msg::USER_REPLY, "UserReply", &m_user_reply);
    
    m_task_spec.requestID = 0;
    m_task_spec.behaviorID = numeric_limits<uint8_t>::max();
  }
  
  
  int UserProcess::
  HandleMessagePayload(wbcnet::unique_id_t msg_id)
  {
    if (wbcrun::msg::USER_REPLY != msg_id) {
      LOG_TRACE (logger,
		     "wbcrun::UserProcess::HandleMessagePayload()\n"
		     << "  unknown message ID " << (int) msg_id);
      return 0;
    }
    
    if (logger->isTraceEnabled()) {
      if (m_user_request.requestID != m_user_reply.requestID) {
	LOG_TRACE (logger,
		       "wbcrun::UserProcess::HandleMessagePayload()\n"
		       << "  requestID mismatch: expected " << (int) m_user_request.requestID
		       << " but got " << (int) m_user_reply.requestID);
      }
      if (1 < m_user_reply.nCodes) {
	LOG_TRACE (logger,
		       "wbcrun::UserProcess::HandleMessagePayload()\n"
		       << "  no status info in user_reply");
      }
      else {
	LOG_TRACE (logger,
		       "wbcrun::UserProcess::HandleMessagePayload()\n"
		       << "  user_reply status " << m_user_reply.code[0] << ": "
		       << wbcrun::srv::result_to_string(m_user_reply.code[0]));
      }
    }
    
    if ( ! ncurses_active && keyboard_querry)
      m_user_reply.Dump(cout, "  ");
    
    return 0;
  }
  
  
  void UserProcess::
  InteractiveGoalRequest()
  {
    static const int buflen(1024);
    char buffer[buflen];
    static char const * field[] =
      { "x [m]", "y [m]", "z [m]", "psi [deg]", "theta [deg]", "phi [deg]" };
    double goal[6];
    
    cout << "interactive_goal_request()\n";
    for (size_t ii(0); ii < 6; ++ii) {
      cout << "  enter " << field[ii] << ": ";
      if ( ! cin.getline(buffer, buflen)) {
	cout << "ERROR reading standard input\n";
	return;
      }
      istringstream line(buffer);
      line >> goal[ii];
      if ( ! line) {
	cout << "ERROR reading field\n";
	--ii;
	continue;
      }
    }
    // deg -> rad conversion
    for (size_t ii(3); ii < 6; ++ii)
      goal[ii] *= M_PI / 180;
    
    wbcrun::srv::set_goal(m_user_request, goal, 6);
  }
  
  
  void UserProcess::
  Cleanup()
  {
#ifdef WBCRUN_HAVE_CURSES
    if (ncurses_active) {
      endwin();
      cout << "cleanup up curses\n";
    }
#endif // WBCRUN_HAVE_CURSES
    
#ifdef WBCRUN_HAVE_XMLRPC
    if (xmlrpc_directory) {
      cout << "cleanup up XmlRpc\n";
      handle_SIGTSTP(SIGTSTP);
      usleep(500000);
      delete xmlrpc_directory;
      xmlrpc_directory = 0;
    }
#endif // WBCRUN_HAVE_XMLRPC
    
    cout << "see you later\n";
  }
  
  
  void UserProcess::
  XmlRpcLoop()
  {
#ifndef WBCRUN_HAVE_XMLRPC
    cout << "Sorry, XMLRPC not available in this build\n"
	 << "  You need xmlrpc++, which you can get by going into the wbc/xmlrpc++ directory\n"
	 << "  and typing './buildme.sh' at the prompt.  At the next configure / build it\n"
	 << "  should get picked up\n";
#else // WBCRUN_HAVE_XMLRPC
    
    cout << "Installing new signal handler for SIGTSTP\n";
    struct sigaction sig;
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = handle_SIGTSTP;
    struct sigaction save_sig;
    memset(&save_sig, 0, sizeof(save_sig));
    if (0 != sigaction(SIGTSTP, &sig, &save_sig)) {
      warn("wbcrun::UserProcess::XmlRpcLoop(): sigaction()");
      return;
    }
    
    cout << "Spawning XmlRpc server on port 8080.\n"
	 << "  Press Ctrl-Z to quit.\n";
    xmlrpc_directory = new XMLRPCDirectoryServer(m_directory_client);
    xmlrpc_directory->RunForever(8080);

    cout << "XmlRpc server has exited.\n";
    usleep(200000);		// needed?
    delete xmlrpc_directory;
    xmlrpc_directory = 0;
    
    cout << "Restoring old signal handler for SIGTSTP\n";
    if (0 != sigaction(SIGTSTP, &save_sig, 0))
      warn("wbcrun::UserProcess::XmlRpcLoop(): sigaction()");
    
#endif // WBCRUN_HAVE_XMLRPC
  }
  
  
  listing_t const & UserProcess::
  GetBehaviorList() const throw(std::exception)
  {
    if (m_lazy_behavior_list.empty()) {
      srv::result_t const stat(m_directory_client->ListBehaviors(m_lazy_behavior_list));
      if (srv::SUCCESS != stat)
	throw runtime_error("wbcrun::UserProcess::GetBehaviorList():\n"
			    "  m_directory_client->ListBehaviors() failed: "
			    + string(srv::result_to_string(stat)));
    }
    return m_lazy_behavior_list;
  }

}
