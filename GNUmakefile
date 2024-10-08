#
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#


OBJDIR := obj



# Run 'make V=1' to turn on verbose commands, or 'make V=0' to turn them off.
ifeq ($(V),1)
override V =
endif
ifeq ($(V),0)
override V = @
endif

-include conf/lab.mk

-include conf/env.mk

LABSETUP ?= ./

TOP = .

# Cross-compiler jos toolchain
#
# This Makefile will automatically use the cross-compiler toolchain
# installed as 'i386-jos-elf-*', if one exists.  If the host tools ('gcc',
# 'objdump', and so forth) compile for a 32-bit x86 ELF target, that will
# be detected as well.  If you have the right compiler toolchain installed
# using a different name, set GCCPREFIX explicitly in conf/env.mk

# try to infer the correct GCCPREFIX
ifndef GCCPREFIX
GCCPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf64-x86-64$$' >/dev/null 2>&1; \
	then echo 'x86-64-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf64-x86-64' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an x86-64-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with x86-64-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your x86-64-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'x86-64-jos-elf-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake GCCPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# try to infer the correct QEMU
ifndef QEMU
QEMU := $(shell if which qemu-system-x86_64 > /dev/null; \
	then echo qemu-system-x86_64; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in conf/env.mk?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

# figure out if we have KVM available
ifndef KVM
KVM := $(shell if kvm-ok > /dev/null; \
	then echo '-enable-kvm'; \
	else echo ''; \
	fi)
endif

# try to generate a unique GDB port
GDBPORT	:= $(shell expr `id -u` % 5000 + 25000)

CC	:= $(GCCPREFIX)gcc -pipe
AS	:= $(GCCPREFIX)as
AR	:= $(GCCPREFIX)ar
LD	:= $(GCCPREFIX)ld
OBJCOPY	:= $(GCCPREFIX)objcopy
OBJDUMP	:= $(GCCPREFIX)objdump
NM	:= $(GCCPREFIX)nm

# Native commands
NCC	:= gcc $(CC_VER) -pipe
NATIVE_CFLAGS := $(CFLAGS) $(DEFS) $(LABDEFS) -I$(TOP) -MD -Wall
TAR	:= gtar
PERL	:= perl

# Compiler flags
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
# Only optimize to -O1 to discourage inlining, which complicates backtraces.
CFLAGS := $(CFLAGS) $(DEFS) $(LABDEFS) -O0 -fno-builtin -I$(TOP) -MD
CFLAGS += -fno-omit-frame-pointer -mno-red-zone
CFLAGS += -Wall -Wno-format -Wno-unused -Werror -gdwarf-2
## dep 1/7/21: Temporarily disable this to get CI working
#-fvar-tracking

CFLAGS += -I$(TOP)/net/lwip/include \
	  -I$(TOP)/net/lwip/include/ipv4 \
	  -I$(TOP)/net/lwip/jos

# Add -fno-stack-protector if the option exists.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Add -fcf-protection=none if the option exists.
CFLAGS += $(shell $(CC) -fcf-protection=none -E -x c /dev/null >/dev/null 2>&1 && echo -fcf-protection=none)

# Common linker flags
LDFLAGS := -m elf_x86_64 -z max-page-size=0x1000 --print-gc-sections
BOOT_LDFLAGS := -m elf_i386

# Linker flags for JOS user programs
ULDFLAGS := -T user/user.ld

GCC_LIB := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kern/%.o \
	   $(OBJDIR)/lib/%.o $(OBJDIR)/fs/%.o $(OBJDIR)/net/%.o \
	   $(OBJDIR)/user/%.o

EXTRA_HOST_KERN_CFLAGS :=
KERN_CFLAGS := $(CFLAGS) -DJOS_KERNEL -DDWARF_SUPPORT -gdwarf-2 -mcmodel=large -m64 -fno-PIC
BOOT_CFLAGS := $(CFLAGS) -DJOS_KERNEL -gdwarf-2 -m32 -fno-PIC
USER_CFLAGS := $(CFLAGS) -DJOS_USER -gdwarf-2 -mcmodel=large -m64


# Update .vars.X if variable X has changed since the last make run.
#
# Rules that use variable X should depend on $(OBJDIR)/.vars.X.  If
# the variable's value has changed, this will update the vars file and
# force a rebuild of the rule that depends on it.
$(OBJDIR)/.vars.%: FORCE
	$(V)echo "$($*)" | cmp -s $@ || echo "$($*)" > $@
.PRECIOUS: $(OBJDIR)/.vars.%
.PHONY: FORCE



# Include Makefrags for subdirectories
include boot/Makefrag
# include boot1/Makefrag
include kern/Makefrag
include lib/Makefrag
include user/Makefrag
include fs/Makefrag
include net/Makefrag



CPUS ?= 1

PORT7	:= $(shell expr $(GDBPORT) + 1)
PORT80	:= $(shell expr $(GDBPORT) + 2)

## QEMU's VMX options have changed over time.  We need to know the
## version of qemu to know the right options.
QEMU_VERSION = $(shell $(QEMU) -version | grep -Po '(?<=QEMU emulator version )\d.\d.\d')
# Not sure which version added all these cpuid flags at the command line, but at least version 4.2
ifeq ($(shell expr $(QEMU_VERSION) \> 4.2), 1)
QEMU_VMX_OPTS = '+vmx-secondary-ctls,+vmx-ept,+vmx-entry-ia32e-mode,+vmx-entry-load-efer,+vmx-ept-advanced-exitinfo,+vmx-exit-load-efer,+vmx-exit-save-efer,+vmx-unrestricted-guest,+vmx-cr3-load-noexit,+vmx-cr3-store-noexit,+vmx-true-ctls,+vmx-hlt-exit,+vmx-page-walk-4,+vmx-invept,+vmx-invept-all-context,+vmx-invept-single-context'
# These qemu options probably should be on, but don't seem needed...  Short list to try for issues on newer host kernels
# +vmx-io-exit,+vmx-io-bitmap'
# +vmx-store-lma,+vmx-posted-intr,,,+vmx-invlpg-exit'
endif

## We need KVM for qemu to export VMX
QEMUOPTS = -no-reboot -no-shutdown -cpu kvm64,+vmx,$(QEMU_VMX_OPTS) $(KVM) -m 256 -drive format=raw,file=$(OBJDIR)/kern/kernel.img -serial mon:stdio -gdb tcp::$(GDBPORT)
QEMUOPTS += $(shell if $(QEMU) -nographic -help | grep -q '^-D '; then echo '-D qemu.log'; fi)
IMAGES = $(OBJDIR)/kern/kernel.img
QEMUOPTS += -smp cpus=$(CPUS),cores=1,threads=1,sockets=$(CPUS)
QEMUOPTS += -hdb $(OBJDIR)/fs/fs.img
IMAGES += $(OBJDIR)/fs/fs.img
QEMUOPTS += -netdev user,id=u0,hostfwd=tcp::$(PORT7)-:7,hostfwd=tcp::$(PORT80)-:80,hostfwd=udp::$(PORT7)-:7, \
	     -device e1000,netdev=u0 -object filter-dump,id=f0,netdev=u0,file=qemu.pcap
QEMUOPTS += $(QEMUEXTRA)


.gdbinit:$(OBJDIR)/kern/kernel.asm .gdbinit.tmpl
	$(eval LONGMODE := $(shell grep -C1 '<jumpto_longmode>:' '$(OBJDIR)/kern/kernel.asm' | sed 's/^\([0-9a-f]*\).*/\1/g'))
	sed -e "s/localhost:1234/localhost:$(GDBPORT)/" -e "s/jumpto_longmode/*0x$(LONGMODE)/" < $^ > $@

pre-qemu: .gdbinit
#	QEMU doesn't truncate the pcap file.  Work around this.
	@rm -f qemu.pcap


qemu: $(IMAGES) pre-qemu
	$(QEMU) $(QEMUOPTS)

qemu-nox: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Use Ctrl-a x to exit qemu"
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS)

qemu-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'gdb'." 1>&2
	@echo "***"
	$(QEMU) $(QEMUOPTS) -S

qemu-nox-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'gdb'." 1>&2
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS) -S

print-qemu:
	@echo $(QEMU)

print-gdbport:
	@echo $(GDBPORT)

# For deleting the build
clean:
	rm -rf $(OBJDIR) .gdbinit jos.in qemu.log

realclean: clean
	rm -rf lab$(LAB).tar.gz \
		jos.out $(wildcard jos.out.*) \
		qemu.pcap $(wildcard qemu.pcap.*)

distclean: realclean
	rm -rf conf/gcc.mk

ifneq ($(V),@)
GRADEFLAGS += -v
endif

grade:
	@echo $(MAKE) clean
	@$(MAKE) clean || \
	  (echo "'make clean' failed.  HINT: Do you have another running instance of JOS?" && exit 1)
	./grade-lab$(LAB) $(GRADEFLAGS)


update:
ifeq ($(LAB),1)
	git pull https://github.com/comp630-s24/jos.git main
else ifeq ($(LAB),6)
	git pull https://github.com/comp630-s24/jos.git lab6a
else ifeq ($(LAB),8)
	git pull https://github.com/comp630-s24/jos.git lab6b
else
	git pull https://github.com/comp630-s24/jos.git lab$(LAB)
endif

handin: realclean
	@if [ `git status --porcelain| wc -l` != 0 ] ; then echo "\n\n\n\n\t\tWARNING: YOU HAVE UNCOMMITTED CHANGES\n\n    Consider committing any pending changes and rerunning make handin.\n\n\n\n"; fi
	git tag -f -a lab$(LAB)-handin -m "Lab$(LAB) Handin"
	git push --tags origin

handin-check:
	@if test "$$(git symbolic-ref HEAD)" != refs/heads/lab$(LAB); then \
		git branch; \
		read -p "You are not on the lab$(LAB) branch.  Hand-in the current branch? [y/N] " r; \
		test "$$r" = y; \
	fi
	@if ! git diff-files --quiet || ! git diff-index --quiet --cached HEAD; then \
		git status; \
		echo; \
		echo "You have uncomitted changes.  Please commit or stash them."; \
		false; \
	fi
	@if test -n "`git ls-files -o --exclude-standard`"; then \
		git status; \
		read -p "Untracked files will not be handed in.  Continue? [y/N] " r; \
		test "$$r" = y; \
	fi

tarball: handin-check
	git archive --format=tar HEAD | gzip > lab$(LAB)-handin.tar.gz

handin-prep:
	@./handin-prep

# For test runs
prep-net_%: override INIT_CFLAGS+=-DTEST_NO_NS

prep-%:
	$(V)$(MAKE) "INIT_CFLAGS=${INIT_CFLAGS} -DTEST=`case $* in *_*) echo $*;; *) echo user_$*;; esac`" $(IMAGES)

run-%-nox-gdb: prep-% pre-qemu
	$(QEMU) -nographic $(QEMUOPTS) -S

run-%-gdb: prep-% pre-qemu
	$(QEMU) $(QEMUOPTS) -S

run-%-nox: prep-% pre-qemu
	$(QEMU) -nographic $(QEMUOPTS)

run-%: prep-% pre-qemu
	$(QEMU) $(QEMUOPTS)

# For network connections
which-ports:
	@echo "Local port $(PORT7) forwards to JOS port 7 (echo server)"
	@echo "Local port $(PORT80) forwards to JOS port 80 (web server)"

nc-80:
	nc localhost $(PORT80)

nc-7:
	nc localhost $(PORT7)

telnet-80:
	telnet localhost $(PORT80)

telnet-7:
	telnet localhost $(PORT7)

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	@$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

always:
	@:

.PHONY: all always \
	handin tarball clean realclean distclean grade handin-prep handin-check update