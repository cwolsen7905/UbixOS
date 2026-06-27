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
#include <sys/mman.h>

#include <ubix/mailbox.hh>
#include <ubix/sched.hh>
#include <ubix/process.hh>
#include <views/display.hh>
#include <objgfx/objgfx.h>
#include <objgfx/ogScalableFont.h>
#include <objgfx/ogImage.h>
#include <objgfx/ogPixelFmt.h>
#include <ubistry/ubistry.h>
#include <authd.h>
#include <vector>

/* ------------------------------------------------------------------ */
/* Colours                                                              */
/* ------------------------------------------------------------------ */

/* Calm slate palette — matches the modern (Win11-flat) views chrome instead of
 * the old saturated navy.  Packed 0x00RRGGBB. */
#define BG_COLOR 0x00202530u      /* solid background (wallpaper fallback) */
#define CARD_COLOR 0x00272E3Au    /* login card fill */
#define CARD_BORDER 0x00394456u   /* 1px card outline */
#define FIELD_BG 0x001B2029u      /* inset text field (idle) */
#define FIELD_FOCUS 0x00222B38u   /* inset text field (focused) */
#define ACCENT_COLOR 0x005B8DEFu  /* focus underline / caret (modern blue) */
#define DIVIDER_COLOR 0x00333C4Cu /* header divider line */
#define TEXT_COLOR 0x00F0F2F6u    /* primary text (near white) */
#define LABEL_COLOR 0x008A93A3u   /* field labels (muted slate) */
#define HINT_COLOR 0x00697283u    /* bottom hint */
#define ERROR_COLOR 0x00FF6E6Eu   /* error message (matches close-glyph red) */

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define FONT_PATH "/var/fonts/DejaVuSansMono.ttf"
#define FONT_SIZE 14
#define VIEWS_MBOX "views"
#define TASKBAR_PATH "/usr/bin/taskbar"
#define MAX_FIELD 31 /* max username / password length */

extern "C" int pidStatus(int pid);

/* ------------------------------------------------------------------ */
/* LoginUI — draws the login panel onto an ogSurface                   */
/* ------------------------------------------------------------------ */

class LoginUI
{
	ogSurface &surf_;
	ogScalableFont font_;
	int sw_, sh_; /* screen width / height */

	std::vector<uint32_t> bg_; /* wallpaper stretched to the screen (32bpp) */
	bool have_bg_ = false;

	/* card + field geometry (computed in layout()) */
	int px_, py_, pw_, ph_; /* card rect */
	int fbx_, fbw_;         /* field-box x and width (shared by both fields) */
	int field_h_;           /* field-box height */
	int title_y_, sub_y_, div_y_;
	int user_label_y_, user_field_y_;
	int pass_label_y_, pass_field_y_;
	int msg_y_;

      public:
	bool loaded = false;

	explicit LoginUI(ogSurface &s, int sw, int sh) : surf_(s), sw_(sw), sh_(sh)
	{
		loaded = font_.Load(FONT_PATH, FONT_SIZE);
		layout();
		load_background();
	}

	/* Decode the desktop wallpaper (ubix.bmp) and nearest-neighbour stretch it
	 * into a screen-sized 32bpp cache once, so each redraw is a fast blit.  If
	 * the image is missing, have_bg_ stays false and draw() falls back to the
	 * solid BG_COLOR. */
	void load_background()
	{
		/* Use the *system-default* desktop wallpaper (no user — login does not
		 * know who is logging in yet).  Honour the desktop mode: only show an
		 * image when the system desktop is in image mode; otherwise fall back to
		 * the solid login background. */
		char mode[32] = {0};
		char path[256] = {0};
		if (ubistry_get_str("views/desktop/mode", mode, sizeof(mode)) == 0 && std::strcmp(mode, "image") != 0)
			return;
		if (ubistry_get_str("views/desktop/image", path, sizeof(path)) != 0 || path[0] == '\0')
			std::strncpy(path, "/var/background/ubix.bmp", sizeof(path) - 1);

		ogImage img;
		ogSurface src;
		if (!img.Load(path, src))
			return;
		int iw = (int)src.ogGetMaxX() + 1;
		int ih = (int)src.ogGetMaxY() + 1;
		if (iw <= 0 || ih <= 0 || sw_ <= 0 || sh_ <= 0)
			return;

		bg_.resize((size_t)sw_ * (size_t)sh_);
		for (int dy = 0; dy < sh_; dy++)
		{
			int sy = dy * ih / sh_;
			for (int dx = 0; dx < sw_; dx++)
			{
				int sx = dx * iw / sw_;
				uInt8 r, g, b;
				src.ogUnpack(src.ogGetPixel(sx, sy), r, g, b);
				bg_[(size_t)dy * sw_ + dx] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
			}
		}
		have_bg_ = true;
	}

	/* Paint the cached wallpaper (or the solid fallback) as the full background. */
	void draw_background()
	{
		if (!have_bg_)
		{
			surf_.ogFillRect(0, 0, sw_ - 1, sh_ - 1, BG_COLOR);
			return;
		}
		for (int y = 0; y < sh_; y++)
		{
			uint32_t *row = (uint32_t *)surf_.ogGetPtr(0, (uInt32)y);
			if (row)
				std::memcpy(row, &bg_[(size_t)y * sw_], (size_t)sw_ * sizeof(uint32_t));
		}
	}

	void layout()
	{
		int fw = (int)font_.GetWidth();
		int fh = (int)font_.GetHeight();

		const int pad = fh;               /* card inner padding */
		const int label_gap = 4;          /* label baseline → its field box */
		const int block_gap = fh / 2 + 4; /* one field block → the next */
		field_h_ = fh + 12;               /* inset field box height */

		pw_ = 44 * fw;

		/* Stack the contents top-to-bottom, accumulating the card height; then
		 * centre the card and convert the running offsets to absolute y. */
		int y = pad;
		int title_o = y;
		y += fh + 8;
		int sub_o = y;
		y += fh + 6;
		int div_o = y;
		y += 10;
		int ul_o = y;
		y += fh + label_gap;
		int uf_o = y;
		y += field_h_ + block_gap;
		int pl_o = y;
		y += fh + label_gap;
		int pf_o = y;
		y += field_h_ + block_gap;
		int msg_o = y;
		y += fh;
		y += pad;
		ph_ = y;

		/* Horizontally centred; nudged below dead-centre so the card sits a touch
		 * lower on the screen (feels more grounded than perfectly centred). */
		const int vertical_offset = 45;
		px_ = (sw_ - pw_) / 2;
		py_ = (sh_ - ph_) / 2 + vertical_offset;

		fbx_ = px_ + pad;
		fbw_ = pw_ - 2 * pad;

		title_y_ = py_ + title_o;
		sub_y_ = py_ + sub_o;
		div_y_ = py_ + div_o;
		user_label_y_ = py_ + ul_o;
		user_field_y_ = py_ + uf_o;
		pass_label_y_ = py_ + pl_o;
		pass_field_y_ = py_ + pf_o;
		msg_y_ = py_ + msg_o;
	}

	void draw(const std::string &user, const std::string &pass, bool in_pass, const std::string &err)
	{
		int fw = (int)font_.GetWidth();

		/* Background (desktop wallpaper, or solid fallback) */
		draw_background();

		/* Soft drop shadow, then the rounded card and its hairline border —
		 * all shared objGFX primitives (ogDropShadow / ogFillRoundRect /
		 * ogRoundRect) rather than bespoke per-app drawing. */
		const int radius = 12;
		surf_.ogDropShadow(px_, py_, px_ + pw_, py_ + ph_, 12, 6, 110);
		surf_.ogFillRoundRect(px_, py_, px_ + pw_, py_ + ph_, radius, CARD_COLOR);
		surf_.ogRoundRect(px_, py_, px_ + pw_, py_ + ph_, radius, CARD_BORDER);

		/* Header: stylised product name + sign-in subtitle, both centred. */
		put_centered(title_y_, "uBixOS", TEXT_COLOR, CARD_COLOR);
		put_centered(sub_y_, "Sign in", LABEL_COLOR, CARD_COLOR);
		surf_.ogHLine(fbx_, fbx_ + fbw_, div_y_, DIVIDER_COLOR);

		/* Fields */
		draw_field("Username", user, !in_pass, user_label_y_, user_field_y_, fw);
		std::string masked(pass.size(), '*');
		draw_field("Password", masked, in_pass, pass_label_y_, pass_field_y_, fw);

		/* Error / hint */
		if (!err.empty())
		{
			set_text(ERROR_COLOR, CARD_COLOR);
			font_.PutString(surf_, fbx_, msg_y_, err.c_str());
		}
		else
		{
			set_text(HINT_COLOR, CARD_COLOR);
			font_.PutString(surf_, fbx_, msg_y_, "Enter to sign in   |   Tab to switch field");
		}
	}

      private:
	/* Draw one labelled text field: muted label, an inset box (lit when focused
	 * with an accent underline + caret), and the value text centred in the box. */
	void draw_field(const char *label, const std::string &text, bool focused, int label_y, int field_y, int fw)
	{
		int fh = (int)font_.GetHeight();

		set_text(LABEL_COLOR, CARD_COLOR);
		font_.PutString(surf_, fbx_, label_y, label);

		uint32_t fill = focused ? FIELD_FOCUS : FIELD_BG;
		surf_.ogFillRect(fbx_, field_y, fbx_ + fbw_, field_y + field_h_, fill);
		if (focused)
			surf_.ogFillRect(fbx_, field_y + field_h_ - 2, fbx_ + fbw_, field_y + field_h_, ACCENT_COLOR);
		else
			surf_.ogHLine(fbx_, fbx_ + fbw_, field_y + field_h_, CARD_BORDER);

		int tx = fbx_ + 10;
		int ty = field_y + (field_h_ - fh) / 2;
		set_text(TEXT_COLOR, fill);
		font_.PutString(surf_, tx, ty, text.c_str());

		if (focused)
		{
			int cx = tx + (int)text.size() * fw;
			surf_.ogFillRect(cx + 1, ty + 2, cx + 2, ty + fh - 2, ACCENT_COLOR);
		}
	}

	void put_centered(int y, const char *s, uint32_t fg, uint32_t bg)
	{
		int tw = (int)std::strlen(s) * (int)font_.GetWidth();
		set_text(fg, bg);
		font_.PutString(surf_, px_ + (pw_ - tw) / 2, y, s);
	}

	void set_text(uint32_t fg, uint32_t bg)
	{
		font_.SetFGColor((fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, 255);
		font_.SetBGColor((bg >> 16) & 0xFF, (bg >> 8) & 0xFF, bg & 0xFF, 255);
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

	/* Drain any stale reply left in our mailbox from an earlier attempt, so we
	 * accept only THIS request's response (otherwise a slow first attempt's late
	 * reply would be consumed by the next attempt). */
	mpi_message_t rmsg;
	while (mpi_fetchMessage(reply_mbox.c_str(), &rmsg) == 0)
		;

	if (mpi_postMessage(AUTHD_MBOX, AUTHD_MSG_REQUEST, &msg) != 0)
		return resp; /* authd not reachable */

	/* Block for the reply rather than spin a fixed number of yields.  authd verifies
	 * with PBKDF2 (50k iterations) which takes tens of ms; under SMP it runs in
	 * parallel on another core, so a brief yield-poll elapses long before the hash
	 * completes and mis-reports a correct-but-slow login as rejected ("incorrect on
	 * first attempt").  mpi_waitMessage sleeps until a message arrives; the loop
	 * skips any non-response message and bounds the total wait (~5 s) against a dead
	 * authd. */
	for (int i = 0; i < 50; i++)
	{
		if (mpi_waitMessage(reply_mbox.c_str(), &rmsg, 10 /* ticks ≈ 100 ms */) == 0 &&
		    rmsg.header == AUTHD_MSG_RESPONSE)
		{
			::memcpy(&resp, rmsg.data, sizeof(resp));
			return resp;
		}
	}

	return resp; /* timeout — authd unresponsive */
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
		/* Put the session in its own process group (== the taskbar's pid) so
		 * everything it launches (apps, their children) inherits the group and
		 * vlogin can tear the whole session down on logout.  Set it in the child
		 * too, race-free against the parent's setpgid. */
		::setpgid(0, 0);
		::setuid(resp.uid);
		::setgid(resp.gid);
		/* Start the GUI session in the user's home directory, mirroring
		 * /bin/login's text-console behaviour.  Everything the taskbar
		 * launches (terminal → tcsh, apps) inherits this cwd, so the shell
		 * opens in $HOME instead of "/". */
		::chdir(resp.home[0] ? resp.home : "/");
		::execve(TASKBAR_PATH, (char *const *)taskbar_argv, (char *const *)taskbar_envp);
		::_exit(1);
	}
	if (pid < 0)
		return;

	/* Session group = taskbar pid (also set here to win the fork/exec race). */
	::setpgid(pid, pid);

	/* Apply this user's desktop for the lifetime of the session. */
	set_display_user(username);

	/* Wait for the session leader (taskbar) to exit (logout). */
	while (pidStatus(pid) == pid)
		ubix::yield();

	/* Tear down the rest of the session: kill every process still in the
	 * group — the apps the user launched, which would otherwise keep running
	 * (and holding memory + windows) across the next login.  vlogin itself is in
	 * a different group, so it survives to show the login screen again. */
	::kill(-pid, SIGKILL);

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

		/* Unmap our view of the shared window buffer.  views frees its own copy
		 * on DISPLAY_RELEASE, but the shared frames are only reclaimed once every
		 * mapper unmaps — and vlogin loops (it does not exit between logins), so
		 * without this each login would strand a full-screen (~scr_w*scr_h*4)
		 * mapping.  munmap drops vlogin's reference; the frame frees when views
		 * has also released it. */
		if (shm != nullptr && act_w > 0 && act_h > 0)
			munmap(shm, (size_t)act_w * (size_t)act_h * 4u);
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
			else if (kc == '\b' || kc == 0x7F) /* BS or DEL — both keyboards send 0x7F */
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
