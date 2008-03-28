/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZMART_RPM_CALLBACKS_H
#define ZMART_RPM_CALLBACKS_H

#include <iostream>
#include <string>

#include <zypp/base/Logger.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/Package.h>
//#include <zypp/target/rpm/RpmCallbacks.h>

#include "zypper-callbacks.h"
#include "AliveCursor.h"

using namespace std;

///////////////////////////////////////////////////////////////////
namespace ZmartRecipients
{


// resolvable Message
struct MessageResolvableReportReceiver : public zypp::callback::ReceiveReport<zypp::target::MessageResolvableReport>
{
  virtual void show( zypp::Message::constPtr message )
  {
    cerr_v << message << endl; // [message]important-msg-1.0-1
    std::cerr << message->text() << endl;
    // TODO in interactive mode, wait for ENTER?
  }
};

#ifndef LIBZYPP_1xx
ostream& operator<< (ostream& stm, zypp::target::ScriptResolvableReport::Task task) {
  return stm << (task==zypp::target::ScriptResolvableReport::DO? "DO": "UNDO");
}

struct ScriptResolvableReportReceiver : public zypp::callback::ReceiveReport<zypp::target::ScriptResolvableReport>
{

  /** task: Whether executing do_script on install or undo_script on delete. */
  virtual void start( const zypp::Resolvable::constPtr & script_r,
		      const zypp::Pathname & path_r,
		      Task task) {
    cerr << "Running: " << script_r
	 << " (" << task << ", " << path_r << ")" << endl;
  }
  /** Progress provides the script output. If the script is quiet ,
   * from time to time still-alive pings are sent to the ui. (Notify=PING)
   * Returning \c FALSE
   * aborts script execution.
   */
  virtual bool progress( Notify kind, const std::string &output ) {
    if (kind == PING) {
      static AliveCursor cursor;
      cerr_v << '\r' << cursor++ << flush;
    }
    else {
      cerr << output << flush;
    }
    // hmm, how to signal abort in zypper? catch sigint? (document it)
    return true;
  }
  /** Report error. */
  virtual void problem( const std::string & description ) {
    display_done ();
    cerr << description << endl;
  }

  /** Report success. */
  virtual void finish() {
    display_done ();
  }

};
#endif

///////////////////////////////////////////////////////////////////
struct ScanRpmDbReceive : public zypp::callback::ReceiveReport<zypp::target::rpm::ScanDBReport>
{
  int & _step;				// step counter for install & receive steps
  int last_reported;
  
  ScanRpmDbReceive( int & step )
  : _step( step )
  {
  }

  virtual void start()
  {
    last_reported = 0;
    progress (0);
  }

  virtual bool progress(int value)
  {
    // this is called too often. relax a bit.
    static int last = -1;
    if (last != value)
      display_progress ("RPM database", value);
    last = value;
    return true;
  }

  virtual Action problem( zypp::target::rpm::ScanDBReport::Error error, cbstring description )
  {
    return zypp::target::rpm::ScanDBReport::problem( error, description );
  }

  virtual void finish( Error error, cbstring reason )
  {
    display_done ();
    display_error (error, reason);
  }
};

///////////////////////////////////////////////////////////////////
 // progress for removing a resolvable
struct RemoveResolvableReportReceiver : public zypp::callback::ReceiveReport<zypp::target::rpm::RemoveResolvableReport>
{
  virtual void start( zypp::Resolvable::constPtr resolvable )
  {
    std::cerr << "Removing: " << *resolvable << std::endl;
  }

  virtual bool progress(int value, zypp::Resolvable::constPtr resolvable)
  {
    display_progress ("Removing " + to_string (resolvable), value);
    return true;
  }

  virtual Action problem( zypp::Resolvable::constPtr resolvable, Error error, cbstring description )
  {
    cerr << resolvable << endl;
    display_error (error, description);
    return (Action) read_action_ari (ABORT);
  }

  virtual void finish( zypp::Resolvable::constPtr /*resolvable*/, Error error, cbstring reason )
  {
    display_done ();
    display_error (error, reason);
  }
};

ostream& operator << (ostream& stm, zypp::target::rpm::InstallResolvableReport::RpmLevel level) {
  static const char * level_s[] = {
    "", "(with nodeps)", "(with nodeps+force)"
  };
  return stm << level_s[level];
}

///////////////////////////////////////////////////////////////////
// progress for installing a resolvable
struct InstallResolvableReportReceiver : public zypp::callback::ReceiveReport<zypp::target::rpm::InstallResolvableReport>
{
  zypp::Resolvable::constPtr _resolvable;
  
  void display_step( zypp::Resolvable::constPtr /*resolvable*/, int value )
  {
    display_progress ("Installing " /* + to_string (resolvable) */,  value);
  }
  
  virtual void start( zypp::Resolvable::constPtr resolvable )
  {
    _resolvable = resolvable;
    cerr << "Installing: " + to_string (resolvable) << endl;
  }

  virtual bool progress(int value, zypp::Resolvable::constPtr resolvable)
  {
    display_step( resolvable, value );
    return true;
  }

  virtual Action problem( zypp::Resolvable::constPtr resolvable, Error error, cbstring description, RpmLevel level )
  {
    cerr << resolvable << " " << description << std::endl;
    cerr << level;
    display_error (error, "");
    if (level < RPM_NODEPS_FORCE) {
      cerr_v << "Will retry more aggressively" << endl;
      return ABORT;
    }
    return (Action) read_action_ari (ABORT);
  }

  virtual void finish( zypp::Resolvable::constPtr /*resolvable*/, Error error, cbstring reason, RpmLevel level )
  {
    display_done ();
    if (error != NO_ERROR) {
      cerr << level;
    }
    display_error (error, reason);
  }
};


///////////////////////////////////////////////////////////////////
}; // namespace ZyppRecipients
///////////////////////////////////////////////////////////////////

class RpmCallbacks {

  private:
    ZmartRecipients::MessageResolvableReportReceiver _messageReceiver;
#ifndef LIBZYPP_1xx
    ZmartRecipients::ScriptResolvableReportReceiver _scriptReceiver;
#endif
    ZmartRecipients::ScanRpmDbReceive _readReceiver;
    ZmartRecipients::RemoveResolvableReportReceiver _installReceiver;
    ZmartRecipients::InstallResolvableReportReceiver _removeReceiver;
    int _step_counter;

  public:
    RpmCallbacks()
	: _readReceiver( _step_counter )
	//, _removeReceiver( _step_counter )
	, _step_counter( 0 )
    {
      _messageReceiver.connect();
#ifndef LIBZYPP_1xx
      _scriptReceiver.connect();
#endif
      _installReceiver.connect();
      _removeReceiver.connect();
      _readReceiver.connect();
    }

    ~RpmCallbacks()
    {
      _messageReceiver.disconnect();
#ifndef LIBZYPP_1xx
      _scriptReceiver.disconnect();
#endif
      _installReceiver.disconnect();
      _removeReceiver.disconnect();
      _readReceiver.connect();
    }
};

#endif // ZMD_BACKEND_RPMCALLBACKS_H
// Local Variables:
// mode: c++
// c-basic-offset: 2
// End: