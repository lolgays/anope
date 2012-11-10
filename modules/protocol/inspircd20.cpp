/* Inspircd 2.0 functions
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

/* inspircd-ts6.h uses these */
static bool has_chghostmod = false;
static bool has_chgidentmod = false;
static bool has_globopsmod = true; // Not a typo
static bool has_rlinemod = false;
static bool has_svstopic_topiclock = false;
static unsigned int spanningtree_proto_ver = 0;
#include "inspircd-ts6.h"

static bool has_servicesmod = false;

class InspIRCd20Proto : public InspIRCdTS6Proto
{
 public:
	InspIRCd20Proto() : InspIRCdTS6Proto("InspIRCd 2.0") { }

	void SendConnect() anope_override
	{
		UplinkSocket::Message() << "CAPAB START 1202";
		UplinkSocket::Message() << "CAPAB CAPABILITIES :PROTOCOL=1202";
		UplinkSocket::Message() << "CAPAB END";
		InspIRCdTS6Proto::SendConnect();
	}
};

class InspIRCdExtBan : public ChannelModeList
{
 public:
	InspIRCdExtBan(ChannelModeName mName, char modeChar) : ChannelModeList(mName, modeChar) { }

	bool Matches(const User *u, const Entry *e) anope_override
	{
		const Anope::string &mask = e->mask;

		if (mask.find("m:") == 0 || mask.find("N:") == 0)
		{
			Anope::string real_mask = mask.substr(3);

			Entry en(this->Name, real_mask);
			if (en.Matches(u))
				return true;
		}
		else if (mask.find("j:") == 0)
		{
			Anope::string channel = mask.substr(3);

			ChannelMode *cm = NULL;
			if (channel[0] != '#')
			{
				char modeChar = ModeManager::GetStatusChar(channel[0]);
				channel.erase(channel.begin());
				cm = ModeManager::FindChannelModeByChar(modeChar);
				if (cm != NULL && cm->Type != MODE_STATUS)
					cm = NULL;
			}

			Channel *c = findchan(channel);
			if (c != NULL)
			{
				UserContainer *uc = c->FindUser(u);
				if (uc != NULL)
					if (cm == NULL || uc->Status->HasFlag(cm->Name))
						return true;
			}
		}
		else if (mask.find("R:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (u->IsIdentified() && real_mask.equals_ci(u->Account()->display))
				return true;
		}
		else if (mask.find("r:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (Anope::Match(u->realname, real_mask))
				return true;
		}
		else if (mask.find("s:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (Anope::Match(u->server->GetName(), real_mask))
				return true;
		}
		else if (mask.find("z:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (Anope::Match(u->fingerprint, real_mask))
				return true;
		}

		return false;
	}
};

struct IRCDMessageCapab : IRCDMessage
{
	IRCDMessageCapab() : IRCDMessage("CAPAB", 1) { SetFlag(IRCDMESSAGE_SOFT_LIMIT); }

	bool Run(MessageSource &source, const std::vector<Anope::string> &params) anope_override
	{
		if (params[0].equals_cs("START"))
		{
			if (params.size() >= 2)
				spanningtree_proto_ver = (Anope::string(params[1]).is_pos_number_only() ? convertTo<unsigned>(params[1]) : 0);

			if (spanningtree_proto_ver < 1202)
			{
				UplinkSocket::Message() << "ERROR :Protocol mismatch, no or invalid protocol version given in CAPAB START";
				quitmsg = "Protocol mismatch, no or invalid protocol version given in CAPAB START";
				quitting = true;
				return false;
			}

			/* reset CAPAB */
			has_servicesmod = false;
			has_chghostmod = false;
			has_chgidentmod = false;
			has_svstopic_topiclock = false;
			ircdproto->CanSVSHold = false;
		}
		else if (params[0].equals_cs("CHANMODES") && params.size() > 1)
		{
			spacesepstream ssep(params[1]);
			Anope::string capab;

			while (ssep.GetToken(capab))
			{
				Anope::string modename = capab.substr(0, capab.find('='));
				Anope::string modechar = capab.substr(capab.find('=') + 1);
				ChannelMode *cm = NULL;

				if (modename.equals_cs("admin"))
					cm = new ChannelModeStatus(CMODE_PROTECT, modechar.length() > 1 ? modechar[1] : modechar[0], modechar.length() > 1 ? modechar[0] : 0);
				else if (modename.equals_cs("allowinvite"))
					cm = new ChannelMode(CMODE_ALLINVITE, modechar[0]);
				else if (modename.equals_cs("auditorium"))
					cm = new ChannelMode(CMODE_AUDITORIUM, modechar[0]);
				else if (modename.equals_cs("ban"))
					cm = new InspIRCdExtBan(CMODE_BAN, modechar[0]);
				else if (modename.equals_cs("banexception"))
					cm = new InspIRCdExtBan(CMODE_EXCEPT, 'e');
				else if (modename.equals_cs("blockcaps"))
					cm = new ChannelMode(CMODE_BLOCKCAPS, modechar[0]);
				else if (modename.equals_cs("blockcolor"))
					cm = new ChannelMode(CMODE_BLOCKCOLOR, modechar[0]);
				else if (modename.equals_cs("c_registered"))
					cm = new ChannelModeRegistered(modechar[0]);
				else if (modename.equals_cs("censor"))
					cm = new ChannelMode(CMODE_FILTER, modechar[0]);
				else if (modename.equals_cs("delayjoin"))
					cm = new ChannelMode(CMODE_DELAYEDJOIN, modechar[0]);
				else if (modename.equals_cs("flood"))
					cm = new ChannelModeFlood(modechar[0], true);
				else if (modename.equals_cs("founder"))
					cm = new ChannelModeStatus(CMODE_OWNER, modechar.length() > 1 ? modechar[1] : modechar[0], modechar.length() > 1 ? modechar[0] : 0);
				else if (modename.equals_cs("halfop"))
					cm = new ChannelModeStatus(CMODE_HALFOP, modechar.length() > 1 ? modechar[1] : modechar[0], modechar.length() > 1 ? modechar[0] : 0);
				else if (modename.equals_cs("invex"))
					cm = new InspIRCdExtBan(CMODE_INVITEOVERRIDE, 'I');
				else if (modename.equals_cs("inviteonly"))
					cm = new ChannelMode(CMODE_INVITE, modechar[0]);
				else if (modename.equals_cs("joinflood"))
					cm = new ChannelModeParam(CMODE_JOINFLOOD, modechar[0], true);
				else if (modename.equals_cs("key"))
					cm = new ChannelModeKey(modechar[0]);
				else if (modename.equals_cs("kicknorejoin"))
					cm = new ChannelModeParam(CMODE_NOREJOIN, modechar[0], true);
				else if (modename.equals_cs("limit"))
					cm = new ChannelModeParam(CMODE_LIMIT, modechar[0], true);
				else if (modename.equals_cs("moderated"))
					cm = new ChannelMode(CMODE_MODERATED, modechar[0]);
				else if (modename.equals_cs("nickflood"))
					cm = new ChannelModeParam(CMODE_NICKFLOOD, modechar[0], true);
				else if (modename.equals_cs("noctcp"))
					cm = new ChannelMode(CMODE_NOCTCP, modechar[0]);
				else if (modename.equals_cs("noextmsg"))
					cm = new ChannelMode(CMODE_NOEXTERNAL, modechar[0]);
				else if (modename.equals_cs("nokick"))
					cm = new ChannelMode(CMODE_NOKICK, modechar[0]);
				else if (modename.equals_cs("noknock"))
					cm = new ChannelMode(CMODE_NOKNOCK, modechar[0]);
				else if (modename.equals_cs("nonick"))
					cm = new ChannelMode(CMODE_NONICK, modechar[0]);
				else if (modename.equals_cs("nonotice"))
					cm = new ChannelMode(CMODE_NONOTICE, modechar[0]);
				else if (modename.equals_cs("op"))
					cm = new ChannelModeStatus(CMODE_OP, modechar.length() > 1 ? modechar[1] : modechar[0], modechar.length() > 1 ? modechar[0] : 0);
				else if (modename.equals_cs("operonly"))
					cm = new ChannelModeOper(modechar[0]);
				else if (modename.equals_cs("permanent"))
					cm = new ChannelMode(CMODE_PERM, modechar[0]);
				else if (modename.equals_cs("private"))
					cm = new ChannelMode(CMODE_PRIVATE, modechar[0]);
				else if (modename.equals_cs("redirect"))
					cm = new ChannelModeParam(CMODE_REDIRECT, modechar[0], true);
				else if (modename.equals_cs("reginvite"))
					cm = new ChannelMode(CMODE_REGISTEREDONLY, modechar[0]);
				else if (modename.equals_cs("regmoderated"))
					cm = new ChannelMode(CMODE_REGMODERATED, modechar[0]);
				else if (modename.equals_cs("secret"))
					cm = new ChannelMode(CMODE_SECRET, modechar[0]);
				else if (modename.equals_cs("sslonly"))
					cm = new ChannelMode(CMODE_SSL, modechar[0]);
				else if (modename.equals_cs("stripcolor"))
					cm = new ChannelMode(CMODE_STRIPCOLOR, modechar[0]);
				else if (modename.equals_cs("topiclock"))
					cm = new ChannelMode(CMODE_TOPIC, modechar[0]);
				else if (modename.equals_cs("voice"))
					cm = new ChannelModeStatus(CMODE_VOICE, modechar.length() > 1 ? modechar[1] : modechar[0], modechar.length() > 1 ? modechar[0] : 0);
				/* Unknown status mode, (customprefix) - add it */
				else if (modechar.length() == 2)
					cm = new ChannelModeStatus(CMODE_END, modechar[1], modechar[0]);
				/* else don't do anything here, we will get it in CAPAB CAPABILITIES */

				if (cm)
					ModeManager::AddChannelMode(cm);
				else
					Log() << "Unrecognized mode string in CAPAB CHANMODES: " << capab;
			}
		}
		if (params[0].equals_cs("USERMODES") && params.size() > 1)
		{
			spacesepstream ssep(params[1]);
			Anope::string capab;

			while (ssep.GetToken(capab))
			{
				Anope::string modename = capab.substr(0, capab.find('='));
				Anope::string modechar = capab.substr(capab.find('=') + 1);
				UserMode *um = NULL;

				if (modename.equals_cs("bot"))
					um = new UserMode(UMODE_BOT, modechar[0]);
				else if (modename.equals_cs("callerid"))
					um = new UserMode(UMODE_CALLERID, modechar[0]);
				else if (modename.equals_cs("cloak"))
					um = new UserMode(UMODE_CLOAK, modechar[0]);
				else if (modename.equals_cs("deaf"))
					um = new UserMode(UMODE_DEAF, modechar[0]);
				else if (modename.equals_cs("deaf_commonchan"))
					um = new UserMode(UMODE_COMMONCHANS, modechar[0]);
				else if (modename.equals_cs("helpop"))
					um = new UserMode(UMODE_HELPOP, modechar[0]);
				else if (modename.equals_cs("hidechans"))
					um = new UserMode(UMODE_PRIV, modechar[0]);
				else if (modename.equals_cs("hideoper"))
					um = new UserMode(UMODE_HIDEOPER, modechar[0]);
				else if (modename.equals_cs("invisible"))
					um = new UserMode(UMODE_INVIS, modechar[0]);
				else if (modename.equals_cs("invis-oper"))
					um = new UserMode(UMODE_INVISIBLE_OPER, modechar[0]);
				else if (modename.equals_cs("oper"))
					um = new UserMode(UMODE_OPER, modechar[0]);
				else if (modename.equals_cs("regdeaf"))
					um = new UserMode(UMODE_REGPRIV, modechar[0]);
				else if (modename.equals_cs("servprotect"))
				{
					um = new UserMode(UMODE_PROTECTED, modechar[0]);
					ircdproto->DefaultPseudoclientModes += "k";
				}
				else if (modename.equals_cs("showwhois"))
					um = new UserMode(UMODE_WHOIS, modechar[0]);
				else if (modename.equals_cs("u_censor"))
					um = new UserMode(UMODE_FILTER, modechar[0]);
				else if (modename.equals_cs("u_registered"))
					um = new UserMode(UMODE_REGISTERED, modechar[0]);
				else if (modename.equals_cs("u_stripcolor"))
					um = new UserMode(UMODE_STRIPCOLOR, modechar[0]);
				else if (modename.equals_cs("wallops"))
					um = new UserMode(UMODE_WALLOPS, modechar[0]);

				if (um)
					ModeManager::AddUserMode(um);
				else
					Log() << "Unrecognized mode string in CAPAB USERMODES: " << capab;
			}
		}
		else if (params[0].equals_cs("MODULES") && params.size() > 1)
		{
			spacesepstream ssep(params[1]);
			Anope::string module;

			while (ssep.GetToken(module))
			{
				if (module.equals_cs("m_svshold.so"))
					ircdproto->CanSVSHold = true;
				else if (module.find("m_rline.so") == 0)
				{
					has_rlinemod = true;
					if (!Config->RegexEngine.empty() && module.length() > 11 && Config->RegexEngine != module.substr(11))
						Log() << "Warning: InspIRCd is using regex engine " << module.substr(11) << ", but we have " << Config->RegexEngine << ". This may cause inconsistencies.";
				}
				else if (module.equals_cs("m_topiclock.so"))
					has_svstopic_topiclock = true;
			}
		}
		else if (params[0].equals_cs("MODSUPPORT") && params.size() > 1)
		{
			spacesepstream ssep(params[1]);
			Anope::string module;

			while (ssep.GetToken(module))
			{
				if (module.equals_cs("m_services_account.so"))
					has_servicesmod = true;
				else if (module.equals_cs("m_chghost.so"))
					has_chghostmod = true;
				else if (module.equals_cs("m_chgident.so"))
					has_chgidentmod = true;
			}
		}
		else if (params[0].equals_cs("CAPABILITIES") && params.size() > 1)
		{
			spacesepstream ssep(params[1]);
			Anope::string capab;
			while (ssep.GetToken(capab))
			{
				if (capab.find("CHANMODES") != Anope::string::npos)
				{
					Anope::string modes(capab.begin() + 10, capab.end());
					commasepstream sep(modes);
					Anope::string modebuf;

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						if (ModeManager::FindChannelModeByChar(modebuf[t]))
							continue;
						ModeManager::AddChannelMode(new ChannelModeList(CMODE_END, modebuf[t]));
					}

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						if (ModeManager::FindChannelModeByChar(modebuf[t]))
							continue;
						ModeManager::AddChannelMode(new ChannelModeParam(CMODE_END, modebuf[t]));
					}

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						if (ModeManager::FindChannelModeByChar(modebuf[t]))
							continue;
						ModeManager::AddChannelMode(new ChannelModeParam(CMODE_END, true));
					}

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						if (ModeManager::FindChannelModeByChar(modebuf[t]))
							continue;
						ModeManager::AddChannelMode(new ChannelMode(CMODE_END, modebuf[t]));
					}
				}
				else if (capab.find("USERMODES") != Anope::string::npos)
				{
					Anope::string modes(capab.begin() + 10, capab.end());
					commasepstream sep(modes);
					Anope::string modebuf;

					sep.GetToken(modebuf);
					sep.GetToken(modebuf);

					if (sep.GetToken(modebuf))
						for (size_t t = 0, end = modebuf.length(); t < end; ++t)
							ModeManager::AddUserMode(new UserModeParam(UMODE_END, modebuf[t]));

					if (sep.GetToken(modebuf))
						for (size_t t = 0, end = modebuf.length(); t < end; ++t)
							ModeManager::AddUserMode(new UserMode(UMODE_END, modebuf[t]));
				}
				else if (capab.find("MAXMODES=") != Anope::string::npos)
				{
					Anope::string maxmodes(capab.begin() + 9, capab.end());
					ircdproto->MaxModes = maxmodes.is_pos_number_only() ? convertTo<unsigned>(maxmodes) : 3;
				}
				else if (capab.find("PREFIX=") != Anope::string::npos)
				{
					Anope::string modes(capab.begin() + 8, capab.begin() + capab.find(')'));
					Anope::string chars(capab.begin() + capab.find(')') + 1, capab.end());
					unsigned short level = modes.length() - 1;

					for (size_t t = 0, end = modes.length(); t < end; ++t)
					{
						ChannelMode *cm = ModeManager::FindChannelModeByChar(modes[t]);
						if (cm == NULL || cm->Type != MODE_STATUS)
						{
							Log() << "CAPAB PREFIX gave unknown channel status mode " << modes[t];
							continue;
						}

						ChannelModeStatus *cms = anope_dynamic_static_cast<ChannelModeStatus *>(cm);
						cms->Level = level--;
					}
				}
			}
		}
		else if (params[0].equals_cs("END"))
		{
			if (!has_servicesmod)
			{
				UplinkSocket::Message() << "ERROR :m_services_account.so is not loaded. This is required by Anope";
				quitmsg = "ERROR: Remote server does not have the m_services_account module loaded, and this is required.";
				quitting = true;
				return false;
			}
			if (!ModeManager::FindUserModeByName(UMODE_PRIV))
			{
				UplinkSocket::Message() << "ERROR :m_hidechans.so is not loaded. This is required by Anope";
				quitmsg = "ERROR: Remote server does not have the m_hidechans module loaded, and this is required.";
				quitting = true;
				return false;
			}
			if (!ircdproto->CanSVSHold)
				Log() << "SVSHOLD missing, Usage disabled until module is loaded.";
			if (!has_chghostmod)
				Log() << "CHGHOST missing, Usage disabled until module is loaded.";
			if (!has_chgidentmod)
				Log() << "CHGIDENT missing, Usage disabled until module is loaded.";
			if ((!has_svstopic_topiclock) && (Config->UseServerSideTopicLock))
				Log() << "m_topiclock missing, server side topic locking disabled until module is loaded.";
		}

		return true;
	}
};

struct IRCDMessageEncap : IRCDMessage
{
	Module *me;

	IRCDMessageEncap(Module *m) : IRCDMessage("ENCAP", 4), me(m) { SetFlag(IRCDMESSAGE_SOFT_LIMIT); }

	bool Run(MessageSource &source, const std::vector<Anope::string> &params) anope_override
	{
		if (Anope::Match(Me->GetSID(), params[0]) == false)
			return true;

		if (params[1] == "CHGIDENT")
		{
			User *u = finduser(params[2]);
			if (!u || u->server != Me)
				return true;

			u->SetIdent(params[3]);
			UplinkSocket::Message(u) << "FIDENT " << params[3];
		}
		else if (params[1] == "CHGHOST")
		{
			User *u = finduser(params[2]);
			if (!u || u->server != Me)
				return true;

			u->SetDisplayedHost(params[3]);
			UplinkSocket::Message(u) << "FHOST " << params[3];
		}
		else if (params[1] == "CHGNAME")
		{
			User *u = finduser(params[2]);
			if (!u || u->server != Me)
				return true;

			u->SetRealname(params[3]);
			UplinkSocket::Message(u) << "FNAME " << params[3];
		}
		else if (Config->NSSASL && params[1] == "SASL" && params.size() == 6)
		{
			class InspIRCDSASLIdentifyRequest : public IdentifyRequest
			{
				Anope::string uid;

			 public:
				InspIRCDSASLIdentifyRequest(Module *m, const Anope::string &id, const Anope::string &acc, const Anope::string &pass) : IdentifyRequest(m, acc, pass), uid(id) { }

				void OnSuccess() anope_override
				{
					UplinkSocket::Message(Me) << "METADATA " << this->uid << " accountname :" << this->GetAccount();
					UplinkSocket::Message(Me) << "ENCAP " << this->uid.substr(0, 3) << " SASL " << Me->GetSID() << " " << this->uid << " D S";
				}

				void OnFail() anope_override
				{
					UplinkSocket::Message(Me) << "ENCAP " << this->uid.substr(0, 3) << " SASL " << Me->GetSID() << " " << this->uid << " " << " D F";

					Log(findbot(Config->NickServ)) << "A user failed to identify for account " << this->GetAccount() << " using SASL";
				}
			};

			/*
			Received: :869 ENCAP * SASL 869AAAAAH * S PLAIN
			Sent: :00B ENCAP 869 SASL 00B 869AAAAAH C +
			Received: :869 ENCAP * SASL 869AAAAAH 00B C QWRhbQBBZGFtAG1vbw==
			                                            base64(account\0account\0pass)
			*/
			if (params[4] == "S")
				UplinkSocket::Message(Me) << "ENCAP " << params[2].substr(0, 3) << " SASL " << Me->GetSID() << " " << params[2] << " C +";
			else if (params[4] == "C")
			{
				Anope::string decoded;
				Anope::B64Decode(params[5], decoded);

				size_t p = decoded.find('\0');
				if (p == Anope::string::npos)
					return true;
				decoded = decoded.substr(p + 1);

				p = decoded.find('\0');
				if (p == Anope::string::npos)
					return true;

				Anope::string acc = decoded.substr(0, p),
					pass = decoded.substr(p + 1);

				if (acc.empty() || pass.empty())
					return true;

				IdentifyRequest *req = new InspIRCDSASLIdentifyRequest(me, params[2], acc, pass);
				FOREACH_MOD(I_OnCheckAuthentication, OnCheckAuthentication(NULL, req));
				req->Dispatch();
			}
		}

		return true;
	}
};

struct IRCDMessageFIdent : IRCDMessage
{
	IRCDMessageFIdent() : IRCDMessage("FIDENT", 1) { SetFlag(IRCDMESSAGE_REQUIRE_USER); }

	bool Run(MessageSource &source, const std::vector<Anope::string> &params) anope_override
	{
		source.GetUser()->SetIdent(params[0]);
		return true;
	}
};

class ProtoInspIRCd : public Module
{
	InspIRCd20Proto ircd_proto;

	/* Core message handlers */
	CoreIRCDMessageAway core_message_away;
	CoreIRCDMessageCapab core_message_capab;
	CoreIRCDMessageError core_message_error;
	CoreIRCDMessageJoin core_message_join;
	CoreIRCDMessageKick core_message_kick;
	CoreIRCDMessageKill core_message_kill;
	CoreIRCDMessageMOTD core_message_motd;
	CoreIRCDMessagePart core_message_part;
	CoreIRCDMessagePing core_message_ping;
	CoreIRCDMessagePrivmsg core_message_privmsg;
	CoreIRCDMessageQuit core_message_quit;
	CoreIRCDMessageSQuit core_message_squit;
	CoreIRCDMessageStats core_message_stats;
	CoreIRCDMessageTopic core_message_topic;
	CoreIRCDMessageVersion core_message_version;

	/* inspircd-ts6.h message handlers */
	IRCDMessageEndburst message_endburst;
	IRCDMessageFHost message_fhost;
	IRCDMessageFJoin message_sjoin;
	IRCDMessageFMode message_fmode;
	IRCDMessageFTopic message_ftopic;
	IRCDMessageIdle message_idle;
	IRCDMessageMetadata message_metadata;
	IRCDMessageMode message_mode;
	IRCDMessageNick message_nick;
	IRCDMessageOperType message_opertype;
	IRCDMessageRSQuit message_rsquit;
	IRCDMessageServer message_server;
	IRCDMessageTime message_time;
	IRCDMessageUID message_uid;

	/* Our message handlers */
	IRCDMessageCapab message_capab;
	IRCDMessageEncap message_encap;
	IRCDMessageFIdent message_fident;

	void SendChannelMetadata(Channel *c, const Anope::string &metadataname, const Anope::string &value)
	{
		UplinkSocket::Message(Me) << "METADATA " << c->name << " " << metadataname << " :" << value;
	}

 public:
	ProtoInspIRCd(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, PROTOCOL),
		message_fhost("FHOST"), message_encap(this)
	{
		this->SetAuthor("Anope");

		Capab.insert("NOQUIT");

		Implementation i[] = { I_OnUserNickChange, I_OnServerSync, I_OnChannelCreate, I_OnChanRegistered, I_OnDelChan, I_OnMLock, I_OnUnMLock, I_OnSetChannelOption };
		ModuleManager::Attach(i, this, sizeof(i) / sizeof(Implementation));

		if (Config->Numeric.empty())
		{
			Anope::string numeric = ts6_sid_retrieve();
			Me->SetSID(numeric);
			Config->Numeric = numeric;
		}

		for (botinfo_map::iterator it = BotListByNick->begin(), it_end = BotListByNick->end(); it != it_end; ++it)
			it->second->GenerateUID();
	}

	void OnUserNickChange(User *u, const Anope::string &) anope_override
	{
		u->RemoveModeInternal(ModeManager::FindUserModeByName(UMODE_REGISTERED));
	}

	void OnServerSync(Server *s) anope_override
	{
		if (nickserv)
			for (user_map::const_iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it)
			{
				User *u = it->second;
				if (u->server == s && !u->IsIdentified())
					nickserv->Validate(u);
			}
	}

	void OnChannelCreate(Channel *c) anope_override
	{
		if (c->ci && (Config->UseServerSideMLock || Config->UseServerSideTopicLock))
			this->OnChanRegistered(c->ci);
	}

	void OnChanRegistered(ChannelInfo *ci) anope_override
	{
		if (Config->UseServerSideMLock && ci->c)
		{
			Anope::string modes = ci->GetMLockAsString(false).replace_all_cs("+", "").replace_all_cs("-", "");
			SendChannelMetadata(ci->c, "mlock", modes);
		}

		if (Config->UseServerSideTopicLock && has_svstopic_topiclock && ci->c)
		{
			Anope::string on = ci->HasFlag(CI_TOPICLOCK) ? "1" : "";
			SendChannelMetadata(ci->c, "topiclock", on);
		}
	}

	void OnDelChan(ChannelInfo *ci) anope_override
	{
		if (Config->UseServerSideMLock && ci->c)
			SendChannelMetadata(ci->c, "mlock", "");

		if (Config->UseServerSideTopicLock && has_svstopic_topiclock && ci->c)
			SendChannelMetadata(ci->c, "topiclock", "");
	}

	EventReturn OnMLock(ChannelInfo *ci, ModeLock *lock) anope_override
	{
		ChannelMode *cm = ModeManager::FindChannelModeByName(lock->name);
		if (cm && ci->c && (cm->Type == MODE_REGULAR || cm->Type == MODE_PARAM) && Config->UseServerSideMLock)
		{
			Anope::string modes = ci->GetMLockAsString(false).replace_all_cs("+", "").replace_all_cs("-", "") + cm->ModeChar;
			SendChannelMetadata(ci->c, "mlock", modes);
		}

		return EVENT_CONTINUE;
	}

	EventReturn OnUnMLock(ChannelInfo *ci, ModeLock *lock) anope_override
	{
		ChannelMode *cm = ModeManager::FindChannelModeByName(lock->name);
		if (cm && ci->c && (cm->Type == MODE_REGULAR || cm->Type == MODE_PARAM) && Config->UseServerSideMLock)
		{
			Anope::string modes = ci->GetMLockAsString(false).replace_all_cs("+", "").replace_all_cs("-", "").replace_all_cs(cm->ModeChar, "");
			SendChannelMetadata(ci->c, "mlock", modes);
		}

		return EVENT_CONTINUE;
	}

	EventReturn OnSetChannelOption(CommandSource &source, Command *cmd, ChannelInfo *ci, const Anope::string &setting) anope_override
	{
		if (cmd->name == "chanserv/set/topiclock" && ci->c)
			SendChannelMetadata(ci->c, "topiclock", setting.equals_ci("ON") ? "1" : "");

		return EVENT_CONTINUE;
	}
};

MODULE_INIT(ProtoInspIRCd)
