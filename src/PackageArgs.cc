/*---------------------------------------------------------------------------*\
                          ____  _ _ __ _ __  ___ _ _
                         |_ / || | '_ \ '_ \/ -_) '_|
                         /__|\_, | .__/ .__/\___|_|
                             |__/|_|  |_|
\*---------------------------------------------------------------------------*/

/** \file PackageArgs.cc
 * 
 */

#include <iostream>
#include "zypp/base/Logger.h"

#include "PackageArgs.h"
#include "Zypper.h"
#include "repos.h"

using namespace std;
using namespace zypp;


PackageArgs::PackageArgs(const zypp::ResKind & kind)
  : zypper(*Zypper::instance())
{
  preprocess(zypper.arguments());
  argsToCaps(kind);
}

PackageArgs::PackageArgs(
    const vector<string> & args,
    const zypp::ResKind & kind)
  : zypper(*Zypper::instance())
{
  preprocess(args);
  argsToCaps(kind);
}

void PackageArgs::preprocess(const vector<string> & args)
{
  vector<string>::size_type argc = args.size();

  string tmp;
  string arg;
  bool op = false;
  for(unsigned i = 0; i < argc; ++i)
  {
    tmp = args[i];

    if (op)
    {
      arg += tmp;
      op = false;
      tmp.clear();
    }
    // standalone operator
    else if (tmp == "=" || tmp == "==" || tmp == "<"
            || tmp == ">" || tmp == "<=" || tmp == ">=")
    {
      // not at the start or the end
      if (i && i < argc - 1)
        op = true;
    }
    // operator at the end of a random string, e.g. 'zypper='
    else if (tmp.find_last_of("=<>") == tmp.size() - 1 && i < argc - 1)
    {
      if (!arg.empty())
        _args.insert(arg);
      arg = tmp;
      op = true;
      continue;
    }
    // operator at the start of a random string e.g. '>=3.2.1'
    else if (i && tmp.find_first_of("=<>") == 0)
    {
      arg += tmp;
      tmp.clear();
      op = false;
    }

    if (op)
      arg += tmp;
    else
    {
      if (!arg.empty())
        _args.insert(arg);
      arg = tmp;
    }
  }

  if (!arg.empty())
    _args.insert(arg);

  DBG << "args received: ";
  copy(args.begin(), args.end(), ostream_iterator<string>(DBG, " "));
  DBG << endl;

  DBG << "args compiled: ";
  copy(_args.begin(), _args.end(), ostream_iterator<string>(DBG, " "));
  DBG << endl;
}

void PackageArgs::argsToCaps(const zypp::ResKind & kind)
{
  bool dont;
  string arg, repo;
  for_(it, _args.begin(), _args.end())
  {
    arg = *it;
    repo.clear();


    // For given arguments:
    //    +vim
    //    -emacs
    //    libdnet1.i586
    //    perl-devel:perl(Digest::MD5)
    //    ~non-oss:opera-2:10.1-1.2.gcc44.x86_64
    //    zypper>=1.2.15
    //
    // 1) check for and remove the install/remove modifiers
    //    vim                          (install)
    //    emacs                        (remove)
    //    perl-devel:perl(Digest::MD5) (install/remove according to command)
    //
    // 2) check for and remove the repo specifier at the beginning of the arg
    //    vim                           (no repo)
    //    libdnet1.i586                 (no repo)
    //    perl(Digest::MD5)             (perl-devel repo)
    //    opera-2:10.1-1.2.gcc44.x86_64 (non-oss repo)
    //    note: repo can be specified by number/alias/name/URI, use match_repo()
    //
    // 3) parse the rest of the string as standard zypp package specifier into
    //    a Capability using Capability::guessPackageSpec
    //                                  name, arch, op, evr, kind
    //    vim                           'vim', '', '', '', 'package'
    //    libdnet1.i586                 'libdnet', 'i586', '', '', 'package'
    //    perl(Digest::MD5)             'perl(Digest::MD5)', '', '', '', 'package'
    //    opera-2:10.1-1.2.gcc44.x86_64 'opera', 'x86_64', '=', '2:10.1-1.2.gcc44', 'package'
    //    zypper>=1.2.15                'zypper', '', '>=', '1.2.15', 'package'
    //    note: depends on whether the cap in the pool


    // check for and remove the install/remove modifiers
    // sort as do/dont

    if (arg[0] == '+' || arg[0] == '~')
    {
      dont = false;
      arg.erase(0, 1);
    }
    else if (arg[0] == '-' || arg[0] == '!')
    {
      dont = true;
      arg.erase(0, 1);
    }
    else
      dont = false;

    // check for and remove the 'repo:' prefix
    // ignore colons coming after '(' or '=' (bnc #433679)
    // e.g. 'perl(Digest::MD5)', or 'opera=2:10.00-4102.gcc4.shared.qt3'

    string::size_type pos;
    if ((pos = arg.find(':')) != string::npos && arg.find_first_of("(=") > pos)
    {
      repo = arg.substr(0, pos);
      if (match_repo(zypper, repo))
      {
        arg = arg.substr(pos + 1);
        DBG << "got repo '" << repo << "' for '" << arg << "'" << endl;
      }
      // not a repo, continue as usual
      else
        repo.clear();
    }

    // parse the rest of the string as standard zypp package specifier
    Capability parsedcap = Capability::guessPackageSpec(arg);

    // set the right kind (bnc #580571)
    // prefer those specified in args
    // if not in args, use the one from --type
    sat::Solvable::SplitIdent splid(parsedcap.detail().name());
    if (splid.kind() != kind &&
        zypper.cOpts().find("type") != zypper.cOpts().end())
    {
      // kind specified in arg, too - just warn and let it be
      if (parsedcap.detail().name().asString().find(':') != string::npos)
        zypper.out().warning(str::form(
            _("Different package type specified in '%s' option and '%s'"
              " argument. Will use the latter."),
            "--type", arg.c_str()));
      // no kind specified in arg, use --type
      else
        parsedcap = Capability(
            Arch(parsedcap.detail().arch()),
            splid.name().asString(),
            parsedcap.detail().op(),
            parsedcap.detail().ed(),
            kind);
    }

    MIL << "got " << (dont?"un":"") << "wanted '" << parsedcap << "'";
    MIL << "; repo '" << repo << "'" << endl;

    // store
    // TODO remove equal +/- args here
    if (dont)
      _dont_caps.insert(CapRepoPair(parsedcap, repo));
    else
      _do_caps.insert(CapRepoPair(parsedcap, repo));
  }
}