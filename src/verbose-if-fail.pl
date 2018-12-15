#
# ============================================================================
# Copyright (c) 2014 Hardy-Francis Enterprises Inc.
# This file is part of SharedHashFile.
#
# SharedHashFile is free software: you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# SharedHashFile is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see www.gnu.org/licenses/.
# ----------------------------------------------------------------------------
# To use SharedHashFile in a closed-source product, commercial licenses are
# available; email office [@] sharedhashfile [.] com for more information.
# ============================================================================
#

use strict;

$| ++;

my $all;
my $squirrel;
my $ok = 0;
my $not_ok = 0;
my $still_alive = 0;
while (my $line = <STDIN>) {
    $line =~ s~[\n\r]+$~\n~;
    #debug printf qq[debug: %s], $line;
    $all .= $line;

    if ($line =~ m~^ok \d+ - .*test still alive~) { # e.g. "ok 12 - test still alive" <-- must have "test still alive" message to be a success!
        $still_alive = 1;
        printf qq[%s], $line;
        undef $squirrel;
    }
    elsif ($line =~ m~^ok ~) { # e.g. "ok 12 ..."
        $ok ++;
        printf qq[%s], $line;
        undef $squirrel;
    }
    elsif ($line =~ m~^not ok ~) { # e.g. "not ok 12 ..."
        $not_ok ++;
        printf qq[%s], $squirrel;
        printf qq[%s], $line;
        undef $squirrel;
    }
    elsif ($line =~ m~^# Expected ~) { # e.g. "# Expected ..." tap test
        $not_ok ++;
        printf qq[%s], $squirrel;
        printf qq[%s], $line;
        $squirrel .= $line;
    }
    elsif ($line =~ m~^# Looks like you planned \d+ tests but~) { # e.g. "# Looks like ..."
        #debug printf qq[debug: found: Looks like you planned x tests but ...\n], $not_ok, $ok, $still_alive;
        $not_ok ++;
        printf qq[%s], $squirrel;
        printf qq[%s], $line;
        $squirrel .= $line;
    }
    else { # e.g. "----:TestIpcSocketjs: debug: client disconnected"
        $squirrel .= $line;
    }
}

if (0 == $still_alive) {
    $not_ok ++;
    my $line = sprintf qq[ERROR: did not find final 'test still alive' ok message; check if test crashed!\n];
    $all .= $line;
    printf qq[%s], $squirrel;
    printf qq[%s], $line;
    $squirrel .= $line;
}

if ($ARGV[0] =~ m~\.tout$~) {
    open (OUT, '>', $ARGV[0]) || die qq[ERROR: cannot open file '$ARGV[0]': $!];
    syswrite (OUT, $all, length $all);
    close OUT;
    printf qq[NOTE: see full log file: %s\n], $ARGV[0];
}
else {
    printf qq[%s], $squirrel;
}

#debug printf qq[debug: %u=not_ok %u=ok %u=still_alive\n], $not_ok, $ok, $still_alive;
if ($not_ok) { exit(1); }
else         { exit(0); }
