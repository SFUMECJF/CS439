# Greetings traveler! I see you've opened our Makefile. If you're just browsing,
# feel free to pass on through. If you intend to make edits, heed this warning:
# some of these settings are intended to familiarize you with PintOS, where
# making these changes may not be so simple. You may edit this file however you
# please--it belongs to you, after all, but be wary of changing certain flags,
# lest you find yourself forced to work with them later on.

SHELLNAME = utcsh
CC = gcc
CFLAGS = -Wall -Wextra -ansi
CFLAGS_REL = -O3 -g
CFLAGS_DEB = -Og -g3 -fno-omit-frame-pointer
CFLAGS_SAN = -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -g

TESTDIR=tests
TESTSCRIPT=$(TESTDIR)/run-tests.sh

SRCS = utcsh.c util.c
HEADERS = util.h
FILES = $(SRCS) $(HEADERS)

$(SHELLNAME): $(FILES)
	$(CC) $(CFLAGS) $(CFLAGS_REL) $(SRCS) -o $(SHELLNAME)

debug: $(FILES)
	$(CC) $(CFLAGS) $(CFLAGS_DEB) $(SRCS) -o $(SHELLNAME)

asan: $(FILES)
	$(CC) $(CFLAGS) $(CFLAGS_SAN) $(SRCS) -o $(SHELLNAME)

################################
# Prepare your work for upload #
################################

FILENAME = shell_project.tar

turnin.tar: clean
	tar cvf $(FILENAME) `find . -type f | grep -v \.git | grep -v \.tar$$ | grep -v \.tar\.gz$$ | grep -v \.swp$$ | grep -v ~$$`
	gzip $(FILENAME)

turnin: turnin.tar
	@echo "================="
	@echo "Created $(FILENAME).gz for submission.  Please upload to Canvas."
	@echo "Before uploading, please verify:"
	@echo "     - Your README is correctly filled out."
	@echo "     - Your pair programming log is in the project directory."
	@echo "If either of those items are not done, please update your submission and run the make turnin command again."
	@ls -al $(FILENAME).gz

#########################
# Various utility rules #
#########################

clean:
	rm -f $(SHELLNAME) *.o *~
	rm -rf tests-out

.PHONY: clean

##############
# Test Cases #
##############

test: $(SHELLNAME)
	@echo "Running all tests..."
	$(TESTSCRIPT) -cv

testcase: $(SHELLNAME)
	@echo -n "Running Test $(id): "
	@cat $(TESTDIR)/$(id)/desc
	$(TESTSCRIPT) -t $(id)