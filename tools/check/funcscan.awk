# funcscan.awk — heuristic structural scan of a style(9)-formatted C/C++ file.
#
# Relies on the project's formatting (Allman function braces, column-0
# definitions), which the `format` gate enforces.  Emits tab-separated records:
#   FUNC  <start-line> <open-brace-line> <name> <has-doxygen:0|1>
#   VAR   <line> <name> <static:0|1> <const:0|1> <after-first-function:0|1>
{ L[NR] = $0 }
END {
	seen_func = 0
	for (i = 1; i <= NR; i++) {
		line = L[i]
		if (line ~ /^\{[ \t]*(\/[*\/].*)?$/) {
			s = i - 1
			while (s >= 1 && L[s] !~ /^[ \t]*$/ && L[s] !~ /(;|\}|\*\/|\{)[ \t]*$/ && L[s] !~ /^#/) s--
			start = s + 1
			if (start >= i) continue
			hdr = ""
			for (k = start; k < i; k++) hdr = hdr " " L[k]
			if (hdr !~ /\(/) continue
			if (L[start] ~ /^(struct|enum|union|typedef|else|do|if|for|while|switch|namespace|extern)[ \t]/) continue
			if (L[start] ~ /^[ \t]/) continue
			pre = hdr; sub(/\(.*/, "", pre)
			if (pre ~ /=/) continue
			n = split(pre, w, /[^A-Za-z0-9_~:]+/)
			name = ""
			for (k = n; k >= 1; k--) if (w[k] != "") { name = w[k]; break }
			doc = 0
			k = start - 1
			while (k >= 1 && L[k] ~ /^[ \t]*$/) k--
			if (k >= 1 && L[k] ~ /\*\/[ \t]*$/) {
				j = k
				while (j >= 1 && L[j] !~ /\/\*/) j--
				if (j >= 1 && L[j] ~ /^\/\*[*!]/) doc = 1
			}
			printf "FUNC\t%d\t%d\t%s\t%d\n", start, i, name, doc
			seen_func = 1
			continue
		}
		if (line ~ /^[A-Za-z_]/ && line !~ /\(/ && line ~ /(;|=|=[ \t]*\{)[ \t]*$/ &&
		    line !~ /^(typedef|extern|return|goto|case|default|enum|union|struct[ \t]+[A-Za-z0-9_]+[ \t]*\{)/ &&
		    line !~ /^#/) {
			v = line; sub(/[ \t]*=.*$/, "", v); sub(/;[ \t]*$/, "", v)
			while (v ~ /\[[^]]*\][ \t]*$/) sub(/\[[^]]*\][ \t]*$/, "", v)
			n = split(v, w, /[^A-Za-z0-9_]+/)
			name = ""
			for (k = n; k >= 1; k--) if (w[k] != "") { name = w[k]; break }
			if (name == "") continue
			printf "VAR\t%d\t%s\t%d\t%d\t%d\n", i, name, (line ~ /^static[ \t]/), (line ~ /(^|[ \t])const[ \t]/), seen_func
		}
	}
}
