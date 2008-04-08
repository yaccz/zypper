#ifndef ZYPPER_H
#define ZYPPER_H

#include <string>
#include <vector>

#include "zypp/base/Exception.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/base/PtrTypes.h"

#include "zypp/RepoInfo.h"
#include "zypp/RepoManager.h"
#include "zypp/SrcPackage.h"

#include "output/Out.h"
#include "zypper-command.h"
#include "zypper-getopt.h"

/** directory for storing manually installed (zypper install foo.rpm) RPM files
 */
#define ZYPPER_RPM_CACHE_DIR "/var/cache/zypper/RPMS"

/**
 * Structure for holding global options.
 */
struct GlobalOptions
{
  GlobalOptions()
  :
  verbosity(0),  
  disable_system_sources(false),
  disable_system_resolvables(false),
  is_rug_compatible(false),
  non_interactive(false),
  no_gpg_checks(false),
  machine_readable(false),
  no_refresh(false),
  root_dir("/")
  {}

//  std::list<zypp::Url> additional_sources;

  /**
   * Level of the amount of output.
   *
   * <ul> 
   * <li>-1 quiet</li>
   * <li> 0 normal (default)</li>
   * <li> 1 verbose</li>
   * <li> 2 debug</li>
   * </ul>
   */
  int verbosity;
  bool disable_system_sources;
  bool disable_system_resolvables;
  bool is_rug_compatible;
  bool non_interactive;
  bool no_gpg_checks;
  bool machine_readable;
  /** Whether to disable autorefresh. */
  bool no_refresh;
  std::string root_dir;
  zypp::RepoManagerOptions rm_options;
};


struct CommandOptions
{
  CommandOptions()
    :
  license_auto_agree(false)
  {}

  bool license_auto_agree;
};

struct RuntimeData
{
  RuntimeData()
    : patches_count(0), security_patches_count(0), show_media_progress_hack(false)
  {}

  std::list<zypp::RepoInfo> repos;
  std::list<zypp::RepoInfo> additional_repos;
  int patches_count;
  int security_patches_count;
  zypp::RepoInfo current_repo;

  std::list<zypp::SrcPackage::constPtr> srcpkgs_to_install;

  // hack to enable media progress reporting in the commit phase in normal
  // output level
  bool show_media_progress_hack;

  // Indicates an ongoing raw meta-data refresh.
  // If not empty call zypper.out().progress(
  //  "raw-refresh", raw_refresh_progress_label) in the media download
  // progress callback.
  //! \todo better way to do this would be to propagate the download progress
  // all the way up from the media back-end through fetcher and downloader
  // into the RepoManager::refreshMetadata(), so that we get a combined percentage
  std::string raw_refresh_progress_label;
};

class Zypper : private zypp::base::NonCopyable
{
public:
  typedef zypp::RW_pointer<Zypper,zypp::rw_pointer::Scoped<Zypper> > Ptr;

  static Ptr & instance(); 

  int main(int argc, char ** argv);

  // setters & getters
  Out & out();
  const GlobalOptions & globalOpts() const { return _gopts; }
  const CommandOptions & cmdOpts() const { return _cmdopts; }
  const parsed_opts & cOpts() const { return _copts; }
  const ZypperCommand & command() const { return _command; }
  const std::string & commandHelp() const { return _command_help; }
  const std::vector<std::string> & arguments() const { return _arguments; }
  RuntimeData & runtimeData() { return _rdata; }
  int exitCode() const { return _exit_code; }
  void setExitCode(int exit) { _exit_code = exit; } 
  bool runningShell() const { return _running_shell; }
  bool runningHelp() const { return _running_help; }
  
  int argc() { return _running_shell ? _sh_argc : _argc; } 
  char ** argv() { return _running_shell ? _sh_argv : _argv; }

public:
  ~Zypper();
private:
  Zypper();

  void processGlobalOptions();
  void processCommandOptions();
  void commandShell();
  void shellCleanup();
  void safeDoCommand();
  void doCommand();
  void cleanup();

  void setCommand(const ZypperCommand & command) { _command = command; }
  void setRunningShell(bool value = true) { _running_shell = value; }
  void setRunningHelp(bool value = true) { _running_help = value; }

private:

  int     _argc;
  char ** _argv;

  Out * _out_ptr;
  GlobalOptions _gopts;
  CommandOptions _cmdopts;
  parsed_opts   _copts;
  ZypperCommand _command;
  std::vector<std::string> _arguments;
  std::string _command_help;

  int   _exit_code;
  bool  _running_shell;
  bool  _running_help;

  RuntimeData _rdata;

  int _sh_argc;
  char **_sh_argv;
};

void print_main_help(const Zypper & zypper);
void print_unknown_command_hint(Zypper & zypper);
void print_command_help_hint(Zypper & zypper);

extern RuntimeData gData;

class ExitRequestException : public zypp::Exception
{
public:
  ExitRequestException(const std::string & msg = "") : zypp::Exception(msg) {}
//  ExitRequestException(const std::string & msg = "") : _msg(msg) {}

//  const std::string & msg() const { return _msg; }
private:
//  std::string _msg;
};

#endif /*ZYPPER_H*/

// Local Variables:
// c-basic-offset: 2
// End: