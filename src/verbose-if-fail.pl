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
my $success = 1;
while (my $line = <STDIN>) {
    $line =~ s~[\n\r]+$~\n~;
    #debug printf qq[debug: %s], $line;
    $all .= $line;

    if ($line =~ m~^ok ~) { # e.g. "ok 12 ..."
        printf qq[%s], $line;
        undef $squirrel;
    }
    elsif ($line =~ m~^not ok ~) { # e.g. "not ok 12 ..."
        $success = 0;
        printf qq[%s], $squirrel;
        printf qq[%s], $line;
        undef $squirrel;
    }
    else { # e.g. "----:TestIpcSocketjs: debug: client disconnected"
        $squirrel .= $line;
    }
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

if ($success) { exit(0); }
else          { exit(1); }
