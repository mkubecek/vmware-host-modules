MODULES = vmmon vmnet
SUBDIRS = $(MODULES:%=%-only)
TARBALLS = $(MODULES:%=%.tar)
MODFILES = $(foreach mod,$(MODULES),$(mod)-only/$(mod).ko)
VM_UNAME = $(shell uname -r)
MODDIR = /lib/modules/$(VM_UNAME)/misc

MODINFO = /sbin/modinfo
DEPMOD = /sbin/depmod

%.tar: FORCE gitcleancheck
	git archive -o $@ --format=tar HEAD $(@:.tar=-only)

.PHONY: FORCE subdirs $(SUBDIRS) clean tarballs

subdirs: retiredcheck $(SUBDIRS)

FORCE:

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

gitcheck:
	@git status >/dev/null 2>&1 \
	     || ( echo "This only works in a git repository."; exit 1 )

gitcleancheck: gitcheck
	@git diff --exit-code HEAD >/dev/null 2>&1 \
	     || echo "Warning: tarballs will reflect current HEAD (no uncommited changes)"

retiredcheck:
	@test -f RETIRED && cat RETIRED || true

install: retiredcheck $(MODFILES)
	@for f in $(MODFILES); do \
	    mver=$$($(MODINFO) -F vermagic $$f);\
	    mver=$${mver%% *};\
	    test "$${mver}" = "$(VM_UNAME)" \
	        || ( echo "Version mismatch: module $$f $${mver}, kernel $(VM_UNAME)" ; exit 1 );\
	done
	install -D -t $(DESTDIR)$(MODDIR) $(MODFILES)
	strip --strip-debug $(MODULES:%=$(DESTDIR)$(MODDIR)/%.ko)
	if test -z "$(DESTDIR)"; then $(DEPMOD) -a $(VM_UNAME); fi

clean: $(SUBDIRS)
	rm -f *.o

tarballs: $(TARBALLS)

