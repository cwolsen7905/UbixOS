/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * vLogin — graphical login screen for UbixOS.
 *
 * Claims a full-screen, decoration-free window from the views compositor,
 * draws a simple login UI using objGFX, authenticates via the authd MPI
 * daemon, then forks taskbar on success.  When the taskbar exits (session
 * ends) vLogin redraws the login screen and loops for the next user.
 *
 * Launch order: init → views → vlogin (via etc/init.d/25-vlogin).
 * vLogin polls for the compositor to become ready before claiming its window.
 */

#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <sched.h>

#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogFont.h>
#include <objgfx/ogPixelFmt.h>
#include <authd.h>

/* ------------------------------------------------------------------ */
/* Colours                                                              */
/* ------------------------------------------------------------------ */

#define BG_COLOR 0x00162033u     /* dark navy background */
#define BOX_COLOR 0x001A2840u    /* slightly lighter panel */
#define ACCENT_COLOR 0x002060A0u /* blue accent line */

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define FONT_PATH "/var/fonts/ROM8X8.DPF"
#define VIEWS_MBOX "views"
#define TASKBAR_PATH "/bin/taskbar"
#define MAX_FIELD 31 /* max username / password length */

extern "C" int pidStatus(int pid);

/* ------------------------------------------------------------------ */
/* LoginUI — draws the login panel onto an ogSurface                   */
/* ------------------------------------------------------------------ */

class LoginUI
{
	ogSurface &surf_;
	ogBitFont font_;
	int sw_, sh_; /* screen width / height */

	/* panel geometry (computed in layout()) */
	int px_, py_, pw_, ph_;       /* panel rect */
	int field_x_;                 /* x start of text fields */
	int user_y_, pass_y_, err_y_; /* y of each field row */

      public:
	bool loaded = false;

	explicit LoginUI(ogSurface &s, int sw, int sh) : surf_(s), sw_(sw), sh_(sh)
	{
		loaded = font_.Load(FONT_PATH, 0);
		layout();
	}

	void layout()
	{
		int fw = (int)font_.GetWidth();
		int fh = (int)font_.GetHeight();

		pw_ = 36 * fw; /* panel width  (~288 px at 8 px/ch) */
		ph_ = 10 * fh; /* panel height (~80 px) */
		px_ = (sw_ - pw_) / 2;
		py_ = (sh_ - ph_) / 2;

		field_x_ = px_ + 11 * fw; /* after "Password: " label */
		user_y_ = py_ + 2 * fh;
		pass_y_ = py_ + 4 * fh;
		err_y_ = py_ + 7 * fh;
	}

	void draw(const std::string &user, const std::string &pass, bool in_pass, const std::string &err)
	{
		int fw = (int)font_.GetWidth();
		int fh = (int)font_.GetHeight();

		/* Background */
		surf_.ogFillRect(0, 0, sw_ - 1, sh_ - 1, BG_COLOR);

		/* Panel */
		surf_.ogFillRect(px_ - 4, py_ - 4, px_ + pw_ + 4, py_ + ph_ + 4, BOX_COLOR);

		/* Accent bar at top of panel */
		surf_.ogFillRect(px_ - 4, py_ - 4, px_ + pw_ + 4, py_ - 4 + fh, ACCENT_COLOR);

		/* Title */
		set_color(0xE0, 0xE0, 0xFF);
		font_.PutString(surf_, px_ + (pw_ - 6 * fw) / 2, py_ - 4, "UbixOS");

		/* Username row */
		set_color(0x80, 0xA0, 0xC0);
		font_.PutString(surf_, px_, user_y_, "Login:    ");
		set_color(in_pass ? 0xA0 : 0xFF, 0xFF, 0xFF);
		font_.PutString(surf_, field_x_, user_y_, user.c_str());
		if (!in_pass)
			surf_.ogFillRect(field_x_ + (int)user.size() * fw,
			                 user_y_,
			                 field_x_ + (int)user.size() * fw + fw - 1,
			                 user_y_ + fh - 1,
			                 0x00C0C0C0u);

		/* Password row */
		set_color(0x80, 0xA0, 0xC0);
		font_.PutString(surf_, px_, pass_y_, "Password: ");
		std::string masked(pass.size(), '*');
		set_color(0xFF, 0xFF, 0xFF);
		font_.PutString(surf_, field_x_, pass_y_, masked.c_str());
		if (in_pass)
			surf_.ogFillRect(field_x_ + (int)pass.size() * fw,
			                 pass_y_,
			                 field_x_ + (int)pass.size() * fw + fw - 1,
			                 pass_y_ + fh - 1,
			                 0x00C0C0C0u);

		/* Error / hint */
		if (!err.empty())
		{
			set_color(0xFF, 0x60, 0x60);
			font_.PutString(surf_, px_, err_y_, err.c_str());
		}
		else
		{
			set_color(0x50, 0x70, 0x90);
			font_.PutString(surf_, px_, err_y_, "Enter to login, Tab to switch field");
		}
	}

      private:
	void set_color(uint8_t r, uint8_t g, uint8_t b)
	{
		font_.SetFGColor(r, g, b, 255);
		font_.SetBGColor((BG_COLOR >> 16) & 0xFF, (BG_COLOR >> 8) & 0xFF, BG_COLOR & 0xFF, 255);
	}
};

/* ------------------------------------------------------------------ */
/* Auth via authd MPI (mirrors the pattern in bin/login/main.c)        */
/* ------------------------------------------------------------------ */

static struct auth_response do_auth(const std::string &reply_mbox,
                                    const std::string &username,
                                    const std::string &password)
{
	struct auth_response resp;
	::memset(&resp, 0, sizeof(resp));

	struct auth_request req;
	::memset(&req, 0, sizeof(req));
	::strncpy(req.reply_mbox, reply_mbox.c_str(), AUTH_MBOX_MAX - 1);
	::strncpy(req.username, username.c_str(), AUTH_USER_MAX - 1);
	::strncpy(req.password, password.c_str(), AUTH_PASS_MAX - 1);

	mpi_message_t msg = {};
	msg.header = AUTHD_MSG_REQUEST;
	::memcpy(msg.data, &req, sizeof(req));

	if (mpi_postMessage(AUTHD_MBOX, AUTHD_MSG_REQUEST, &msg) != 0)
		return resp; /* authd not reachable */

	mpi_message_t rmsg;
	for (int i = 0; i < 2000; i++)
	{
		if (mpi_fetchMessage(reply_mbox.c_str(), &rmsg) == 0 && rmsg.header == AUTHD_MSG_RESPONSE)
		{
			::memcpy(&resp, rmsg.data, sizeof(resp));
			return resp;
		}
		ubix::yield();
	}

	return resp; /* timeout */
}

/* ------------------------------------------------------------------ */
/* Session: fork taskbar, wait for it to exit                          */
/* ------------------------------------------------------------------ */

/**
 * Tell the views compositor which user owns the session so it renders that
 * user's desktop (per-user settings).  An empty name reverts to the system
 * default at logout.  Best-effort: ignored if views is unreachable.
 */
static void set_display_user(const std::string &username)
{
	mpi_message_t msg = {};
	struct display_set_user *su = (struct display_set_user *)msg.data;

	::snprintf(su->user, sizeof(su->user), "%s", username.c_str());
	msg.header = DISPLAY_SET_USER;
	mpi_postMessage(VIEWS_MBOX, DISPLAY_SET_USER, &msg);
}

static void run_session(const struct auth_response &resp, const std::string &username)
{
	static const char *taskbar_argv[] = {"taskbar", nullptr};
	char home_env[140], user_env[48], logname_env[48], shell_env[140], path_env[64];

	::snprintf(home_env, sizeof(home_env), "HOME=%s", resp.home[0] ? resp.home : "/");
	::snprintf(user_env, sizeof(user_env), "USER=%s", username.c_str());
	::snprintf(logname_env, sizeof(logname_env), "LOGNAME=%s", username.c_str());
	::snprintf(shell_env, sizeof(shell_env), "SHELL=%s", resp.shell[0] ? resp.shell : "/bin/shell");
	::snprintf(path_env, sizeof(path_env), "PATH=/bin:/sbin:/usr/bin:/usr/sbin");

	static const char *taskbar_envp[7];
	taskbar_envp[0] = home_env;
	taskbar_envp[1] = user_env;
	taskbar_envp[2] = logname_env;
	taskbar_envp[3] = shell_env;
	taskbar_envp[4] = path_env;
	taskbar_envp[5] = "LD_LIBRARY_PATH=/lib:/usr/lib";
	taskbar_envp[6] = nullptr;

	int pid = ::fork();
	if (pid == 0)
	{
		::setuid(resp.uid);
		::setgid(resp.gid);
		::execve(TASKBAR_PATH, (char *const *)taskbar_argv, (char *const *)taskbar_envp);
		::_exit(1);
	}
	if (pid < 0)
		return;

	/* Apply this user's desktop for the lifetime of the session. */
	set_display_user(username);

	/* Wait for the session to end */
	while (pidStatus(pid) == pid)
		ubix::yield();

	/* Revert to the system-default desktop at logout. */
	set_display_user("");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* Unique mailboxes for this process */
	std::string my_mbox = "vlogin." + std::to_string(ubix::pid());
	std::string auth_mbox = "vlogin.auth." + std::to_string(ubix::pid());

	ubix::Mailbox mbox;
	mbox.assign(my_mbox);
	if (!mbox.create())
	{
		std::printf("vlogin: mbox create failed\n");
		return 1;
	}

	mpi_createMbox(auth_mbox.c_str());

	std::printf("vlogin: waiting for views compositor\n");

	/* Poll until the compositor is ready (its mailbox exists) */
	{
		mpi_message_t qmsg = {};
		struct display_query *dq = (struct display_query *)qmsg.data;
		qmsg.header = DISPLAY_QUERY;
		::strncpy(dq->reply, my_mbox.c_str(), sizeof(dq->reply) - 1);
		while (mpi_postMessage(VIEWS_MBOX, DISPLAY_QUERY, &qmsg) != 0)
			ubix::yield();
	}

	/* Wait for the DISPLAY_INFO response to get screen dimensions */
	int scr_w = 800, scr_h = 600;
	{
		mpi_message_t reply;
		int got = 0;
		for (int i = 0; i < 5000; i++)
		{
			if (mbox.try_fetch(reply) && reply.header == DISPLAY_INFO)
			{
				struct display_info *di = (struct display_info *)reply.data;
				if (di->screen_w > 0)
					scr_w = (int)di->screen_w;
				if (di->screen_h > 0)
					scr_h = (int)di->screen_h;
				got = 1;
				break;
			}
			ubix::yield();
		}
		std::printf("vlogin: screen %dx%d%s\n", scr_w, scr_h, got ? "" : " (timeout, using defaults)");
	}

	/* ---- Window claim/release helpers -------------------------------- */
	uint32_t win_id = 0;
	void *shm = nullptr;
	int act_w = 0, act_h = 0;

	auto claim_window = [&]() -> bool
	{
		mpi_message_t msg = {};
		struct display_claim_req *cr = (struct display_claim_req *)msg.data;
		msg.header = DISPLAY_CLAIM;
		cr->x = 0;
		cr->y = 0;
		cr->w = scr_w;
		cr->h = scr_h;
		cr->sender_pid = ubix::pid();
		cr->no_decor = 1;
		::strncpy(cr->title, "vLogin", sizeof(cr->title) - 1);
		::strncpy(cr->reply, my_mbox.c_str(), sizeof(cr->reply) - 1);
		ubix::post_message(VIEWS_MBOX, DISPLAY_CLAIM, msg);

		mpi_message_t reply;
		while (!mbox.try_fetch(reply))
			ubix::yield();

		if (reply.header != DISPLAY_ACK)
		{
			std::printf("vlogin: DISPLAY_CLAIM denied (hdr=%u)\n", reply.header);
			return false;
		}

		struct display_ack *da = (struct display_ack *)reply.data;
		win_id = da->window_id;
		shm = da->shm_base;
		act_w = da->w;
		act_h = da->h;
		std::printf("vlogin: window %u shm=%p %dx%d\n", win_id, shm, act_w, act_h);
		return (shm != nullptr && act_w > 0 && act_h > 0);
	};

	auto release_window = [&]()
	{
		mpi_message_t m = {};
		struct display_release *dr = (struct display_release *)m.data;
		m.header = DISPLAY_RELEASE;
		dr->window_id = win_id;
		ubix::post_message(VIEWS_MBOX, DISPLAY_RELEASE, m);
		win_id = 0;
		shm = nullptr;
	};

	/* ---- Outer session loop: claim → login → session → release → repeat */
	for (;;)
	{
		if (!claim_window())
		{
			std::printf("vlogin: failed to claim window\n");
			return 1;
		}

		ogSurface surf;
		if (!surf.ogAttach(shm, (uint32_t)act_w, (uint32_t)act_h, OG_PIXFMT_32BPP))
		{
			std::printf("vlogin: ogAttach failed\n");
			return 1;
		}

		LoginUI ui(surf, act_w, act_h);
		if (!ui.loaded)
		{
			std::printf("vlogin: font load failed (%s)\n", FONT_PATH);
			return 1;
		}

		std::printf("vlogin: UI ready, entering event loop\n");

		auto flip = [&]()
		{
			mpi_message_t m = {};
			struct display_flip *fl = (struct display_flip *)m.data;
			m.header = DISPLAY_FLIP;
			fl->window_id = win_id;
			fl->dirty_x = 0;
			fl->dirty_y = 0;
			fl->dirty_w = act_w;
			fl->dirty_h = act_h;
			ubix::post_message(VIEWS_MBOX, DISPLAY_FLIP, m);
		};

		std::string username, password, errmsg;
		bool in_pass = false;
		bool do_session = false;
		struct auth_response session_resp = {};

		auto reset = [&]()
		{
			username.clear();
			password.clear();
			errmsg.clear();
			in_pass = false;
		};

		reset();
		ui.draw(username, password, in_pass, errmsg);
		flip();

		/* ---- Inner login loop ------------------------------------- */
		for (;;)
		{
			mpi_message_t ev;
			if (!mbox.try_fetch(ev))
			{
				ubix::yield();
				continue;
			}

			if (ev.header == DISPLAY_CLOSE)
				goto done;

			if (ev.header != DISPLAY_KEY)
				continue;

			struct display_key *dk = (struct display_key *)ev.data;
			if (!dk->pressed)
				continue;

			uint32_t kc = dk->keycode;

			if (kc == '\t')
			{
				in_pass = !in_pass;
				errmsg.clear();
			}
			else if (kc == '\n' || kc == '\r')
			{
				if (!in_pass)
				{
					if (!username.empty())
						in_pass = true;
				}
				else
				{
					ui.draw(username, password, in_pass, "Authenticating...");
					flip();

					session_resp = do_auth(auth_mbox, username, password);

					if (session_resp.ok)
					{
						do_session = true;
						break; /* exit inner loop → release → session */
					}
					else
					{
						errmsg = "Login incorrect.";
						password.clear();
						in_pass = false;
					}
				}
			}
			else if (kc == '\b')
			{
				std::string &field = in_pass ? password : username;
				if (!field.empty())
					field.pop_back();
			}
			else if (kc >= 0x20 && kc < 0x7F)
			{
				std::string &field = in_pass ? password : username;
				if ((int)field.size() < MAX_FIELD)
					field += (char)kc;
			}

			ui.draw(username, password, in_pass, errmsg);
			flip();
		}

		/* Release window so the session (taskbar + apps) owns the screen */
		release_window();

		if (do_session)
			run_session(session_resp, username);
		/* Loop back to claim_window for the next user */
	}

done:
	release_window();
	mbox.destroy();
	return 0;
}
