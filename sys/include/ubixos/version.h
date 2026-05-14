/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UBIXOS_VERSION_H_
#define _UBIXOS_VERSION_H_

/*
 * To bump the version, edit only the four macros below.
 * All version strings in the kernel (sysctl, boot banner) derive from these.
 *
 * Version bump checklist:
 *   1. Edit UBIXOS_VERSION_MAJOR / MINOR / PATCH / TAG below.
 *   2. Add a dated release section to CHANGELOG.md.
 *   3. Rebuild: bmake kernel world image
 *   4. Commit and tag: git tag -a vMAJOR.MINOR.PATCH -m "Release MAJOR.MINOR.PATCH-TAG"
 */

#define UBIXOS_VERSION_MAJOR  2
#define UBIXOS_VERSION_MINOR  1
#define UBIXOS_VERSION_PATCH  0
#define UBIXOS_VERSION_TAG    "BETA"

/* Internal stringify helpers — do not use directly. */
#define _UBIXOS_STR(x)   #x
#define _UBIXOS_XSTR(x)  _UBIXOS_STR(x)

/* "2.0.0-BETA" */
#define UBIXOS_VERSION_RELEASE \
  _UBIXOS_XSTR(UBIXOS_VERSION_MAJOR) "." \
  _UBIXOS_XSTR(UBIXOS_VERSION_MINOR) "." \
  _UBIXOS_XSTR(UBIXOS_VERSION_PATCH) "-" \
  UBIXOS_VERSION_TAG

/* "UbixOS 2.0.0-BETA" */
#define UBIXOS_VERSION_STRING  "UbixOS " UBIXOS_VERSION_RELEASE

#endif /* _UBIXOS_VERSION_H_ */
