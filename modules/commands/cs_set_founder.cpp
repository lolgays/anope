/* ChanServ core functions
 *
 * (C) 2003-2012 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

class CommandCSSetFounder : public Command
{
 public:
	CommandCSSetFounder(Module *creator, const Anope::string &cname = "chanserv/set/founder") : Command(creator, cname, 2, 2)
	{
		this->SetDesc(_("Set the founder of a channel"));
		this->SetSyntax(_("\037channel\037 \037nick\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
		ChannelInfo *ci = cs_findchan(params[0]);
		if (ci == NULL)
		{
			source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
			return;
		}

		EventReturn MOD_RESULT;
		FOREACH_RESULT(I_OnSetChannelOption, OnSetChannelOption(source, this, ci, params[1]));
		if (MOD_RESULT == EVENT_STOP)
			return;

		if (MOD_RESULT != EVENT_ALLOW && source.permission.empty() && !source.AccessFor(ci).HasPriv("SET"))
		{
			source.Reply(ACCESS_DENIED);
			return;
		}

		if (source.permission.empty() && (ci->HasFlag(CI_SECUREFOUNDER) ? !source.IsFounder(ci) : !source.AccessFor(ci).HasPriv("FOUNDER")))
		{
			source.Reply(ACCESS_DENIED);
			return;
		}

		const NickAlias *na = findnick(params[1]);
		if (!na)
		{
			source.Reply(NICK_X_NOT_REGISTERED, params[1].c_str());
			return;
		}

		NickCore *nc = na->nc;
		if (Config->CSMaxReg && nc->channelcount >= Config->CSMaxReg && !source.HasPriv("chanserv/no-register-limit"))
		{
			source.Reply(_("\002%s\002 has too many channels registered."), na->nick.c_str());
			return;
		}

		Log(!source.permission.empty() ? LOG_ADMIN : LOG_COMMAND, source, this, ci) << "to change the founder from " << (ci->GetFounder() ? ci->GetFounder()->display : "(none)") << " to " << nc->display;

		ci->SetFounder(nc);

		source.Reply(_("Founder of \002%s\002 changed to \002%s\002."), ci->name.c_str(), na->nick.c_str());

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &) anope_override
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Changes the founder of a channel. The new nickname must\n"
				"be a registered one."), this->name.c_str());
		return true;
	}
};

class CSSetFounder : public Module
{
	CommandCSSetFounder commandcssetfounder;

 public:
	CSSetFounder(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandcssetfounder(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(CSSetFounder)
