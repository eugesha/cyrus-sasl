# Top-level Makefile.am for SASL
# Rob Earhart
# $Id: Makefile.am,v 1.5 1998/11/25 06:47:54 rob Exp $
#
################################################################
#        Copyright 1998 by Carnegie Mellon University
#
#                      All Rights Reserved
#
#Permission to use, copy, modify, and distribute this software and its
#documentation for any purpose and without fee is hereby granted,
#provided that the above copyright notice appear in all copies and that
#both that copyright notice and this permission notice appear in
#supporting documentation, and that the name of Carnegie Mellon University
#not be used in advertising or publicity pertaining to distribution of the
#software without specific, written prior permission.
#
#CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
#SOFTWARE, INCLUDING #ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, 
#IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR ANY SPECIAL, 
#INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
#LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
#OR OTHER TORTIOUS ACTION, #ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#PERFORMANCE OF THIS SOFTWARE.
################################################################

SUBDIRS=include lib plugins @DIRS@

dist-hook:
	cp -r -p $(srcdir)/config $(srcdir)/win32 $(distdir)
	find $(distdir) -name CVS -print | xargs rm -rf
