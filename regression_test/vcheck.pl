#!/usr/bin/perl -wl
###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################

use strict;
my $exename;
my $dirname;

# collect lines that mean errors into this list
my @output;

while (<>)
{
  chomp;

  if (/\.\/(\S+)\s+\S+\.cfg/)
  {
    $exename = $1; # single mode run directly
  }
  elsif (/ttcn3_start\s+(\S+)\s\S+\.cfg/) {
    $exename = $1; # parallel mode via ttcn3_start
  }
  elsif (/Entering directory `([^']+)'/) { # from make
    $dirname = $1;
  }

  if (     s/(?:\S+@\S+:\s+)?(Verdict statistics: \d+ none \(\d+\.\d+ %\), \d+ pass \((\d+)\.\d+ %\).*)/$1/ )
  # filter out "MTC@host: " on the verdict statistics line in parallel mode
  {
    if ($2 ne '100') {
      push @output, $exename.": ".$_;
    }
  }
  elsif (/Looks like/) {
    push @output, "$_ in $dirname";
  }
}

my $errorCount = 0;
foreach my $line (@output) {
  # filter out the Tverdictoper which is never 100% pass
  unless ($line =~ /^TverdictOper(\.exe)?: Verdict statistics: 2 none \(8\.00 %\), 11 pass \(44\.00 %\), 5 inconc \(20\.00 %\), 7 fail \(28\.00 %\), 0 error \(0\.00 %\)\.$/) {
    print $line;
    $errorCount++;
  }
}

exit $errorCount;

__END__

Find verdict statistics which are not 100% pass.
Augment with the executable name collected earlier.

