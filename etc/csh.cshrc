# /etc/csh.cshrc - system-wide tcsh settings (sourced for all shells)

if ($?TERM == 0 || $TERM == '') setenv TERM vt100

setenv PATH /bin:/sbin:/usr/bin:/usr/sbin
setenv LD_LIBRARY_PATH /lib:/usr/lib

alias ls    'ls -F'
alias ll    'ls -la'
alias ..    'cd ..'
alias h     'history'
