# /etc/csh.cshrc - system-wide tcsh settings, sourced for every shell.
# uBixOS global default: applies to all users (no per-user ~/.tcshrc required).

# --- Environment (all shells) ---
if ($?TERM == 0 || "$TERM" == "") setenv TERM vt100
setenv PATH		/bin:/sbin:/usr/bin:/usr/sbin:/usr/tests
setenv LD_LIBRARY_PATH	/lib:/usr/lib
setenv EDITOR		vi
setenv PAGER		less
setenv LESS		-R

# Prepend personal bin dirs when present.
foreach d (~/bin ~/.local/bin)
	if (-d "$d") set path = ($d $path)
end

# --- Interactive shells only ---
if ($?prompt) then
	# History
	set history = 1000
	set savehist = (1000 merge)
	set histdup = erase

	# Completion & editing niceties
	set autolist		# list choices on an ambiguous tab
	set filec		# filename completion
	set complete = enhance	# case-insensitive, smarter completion
	set color		# colourise ls-F + completion listings
	set nobeep
	set rmstar		# confirm `rm *`
	set notify		# report background job state changes immediately
	set correct = cmd	# offer a spelling correction for mistyped commands

	# Arrow-key prefix history search; ^W deletes one path component.
	bindkey "\e[A" history-search-backward
	bindkey "\e[B" history-search-forward
	bindkey "^W"   backward-delete-word

	# Prompt: user@host:cwd  (green id, blue path).  This static form is the
	# fallback; the precmd below upgrades the trailing %# to green-after-success /
	# red-after-failure.  If precmd ever misbehaves, delete that one line and this
	# base prompt remains.
	set prompt = "%{\033[1;32m%}%n@%m%{\033[0m%}:%{\033[1;34m%}%~%{\033[0m%}%# "
	alias precmd 'set _st=$status; if ($_st == 0) set _c=32; if ($_st != 0) set _c=31; set prompt="%{\033[1;32m%}%n@%m%{\033[0m%}:%{\033[1;34m%}%~%{\033[0m%} %{\033[1;${_c}m%}%#%{\033[0m%} "'

	# Aliases - standard flags only (uBixOS tools are not GNU coreutils).
	alias ls   'ls -F'
	alias ll   'ls -l'
	alias la   'ls -a'
	alias l    'ls -CF'
	alias ..   'cd ..'
	alias ...  'cd ../..'
	alias h    'history 25'
	alias j    'jobs -l'
	alias rm   'rm -i'
	alias cp   'cp -i'
	alias mv   'mv -i'
endif
