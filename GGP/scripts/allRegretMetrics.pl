#!/usr/bin/perl

foreach $line (`cat $ARGV[0]`) {
  unless ($line =~ /target_after_source.log/) {
    next;
  }
  $line =~ /^(.*\/)(\w+)-(\d+)-(\d+)-target_after_source.log/ or die "!$line";
  $path = $1;
  $env = $2;
  $level = $3;
  $scenario = $4;
  $source2 = ();

  if ($env =~ /^e/) {
    if ($level == 6) {
      $source1 = "escape-$level-$scenario-source-1.log";
      $source2 = "escape-$level-$scenario-source-2.log";
    }
    else {
      $source1 = "escape-$level-$scenario-source.log";
    }
    $targetNS = "escape-$level-$scenario-target_no_source.log";
    $targetS = "escape-$level-$scenario-target_after_source.log";
  }
  elsif ($env =~ /^w/) {
    if ($level == 6) {
      $source1 = "wargame-$level-$scenario-source-1.log";
      $source2 = "wargame-$level-$scenario-source-2.log";
    }
    else {
      $source1 = "wargame-$level-$scenario-source.log";
    }
    $targetNS = "wargame-$level-$scenario-target_no_source.log";
    $targetS = "wargame-$level-$scenario-target_after_source.log";
  }
  elsif ($env =~ /^m/) {
    if ($level == 6) {
      $source1 = "mrogue-$level-$scenario-source-1.log";
      $source2 = "mrogue-$level-$scenario-source-2.log";
    }
    else {
      $source1 = "mrogue-$level-$scenario-source.log";
    }
    $targetNS = "mrogue-$level-$scenario-target_no_source.log";
    $targetS = "mrogue-$level-$scenario-target_after_source.log";
  }
  elsif ($env =~ /^b/) {
    $source1 = "build-$level-$scenario-source.log";
    $targetNS = "build-$level-$scenario-target_no_source.log";
    $targetS = "build-$level-$scenario-target_after_source.log";
  }
  elsif ($env =~ /^d/) {
    $source1 = "differing-10-$scenario-source.log";
    $targetNS = "differing-10-$scenario-target_no_source.log";
    $targetS = "differing-10-$scenario-target_after_source.log";
  }

  $source1 = $path . $source1;
  if (defined $source2) {
    $source2 = $path . $source2;
  }
  $targetNS = $path . $targetNS;
  $targetS = $path . $targetS;
  die unless (-e $source1);
  die unless (-e $targetNS);
  die unless (-e $targetS);

  if (defined $source2) {
    $goodthings = $source2;
    die "$source2 not found" unless (-e $source2);
  }
  else {
    $goodthings = $source1;
  }

  $goodthings =~ s/\.log/\.goodthings.soar/;
  die unless (-e $goodthings);
  
  #$sSub = $targetS;
  #$sSub =~ s/log$/submit/;
  #die unless (-e $sSub);
  #@sSubStats = `cat $sSub`;
  
  #$nsSub = $targetNS;
  #$nsSub =~ s/log$/submit/;
  #die unless (-e $nsSub);
  #@nsSubStats = `cat $nsSub`;

  $genUser = 0;
  $genUserSys = 0;
  $genReal = 0;
  foreach $line (`grep 'GEN TIME' $goodthings`) {
    chomp $line;
    $line =~ /(\S+?) real,(\S+?) user,(\S+?) sys/ or die "!$result";
    $genReal = realSeconds($1);
    $genUser += $2;
    $genUserSys += $2 + $3;
  }

  $nsTimeLine = `grep 'UNIX TIME' $targetNS`;
  $sTimeLine = `grep 'UNIX TIME' $targetS`;

  $nsTimeLine =~ /(\S+?) real,(\S+?) user,(\S+?) sys/ or die "!$result";
  $nsReal = realSeconds($1);
  $nsUser = $2;
  $nsUserSys = $2 + $3;

  $nsSubUser = $nsSubStats[1];
  $nsSubUserSys = $nsSubStats[2];
  $nsSubReal = $nsSubStats[3];

  $sTimeLine =~ /(\S+?) real,(\S+?) user,(\S+?) sys/ or die "!$result";
  $sReal = realSeconds($1);
  $sUser = $2;
  $sUserSys = $2 + $3;
  
  $sSubUser = $sSubStats[1];
  $sSubUserSys = $sSubStats[2];
  $sSubReal = $sSubStats[3];
  
  $source1SoarLine = `grep 'decisions (' $source1`;
  $source1SoarLine =~ /^(\d+) decisions \((\S+) msec/ or die "bad: $source1, $source1SoarLine";
  $source1Decisions = $1;

  $source1StatusLine = `grep -B2 "This Agent halted" $source1 | grep succeeded`;  
  $source1Valid = 1;
  unless ($source1StatusLine =~ /succeeded/) {
    $source1Valid = 0;
  }

  if (defined $source2) {
    $source2SoarLine = `grep 'decisions (' $source2`;
    $source2SoarLine =~ /^(\d+) decisions \((\S+) msec/ or die "$source2";
    $source2Decisions = $1;
  
    $source2StatusLine = `grep -B2 "This Agent halted" $source2 | grep succeeded`;  
    $source2Valid = 1;
    unless ($source2StatusLine =~ /succeeded/) {
      $source2Valid = 0;
    }
  }
  else {
    $source2Decisions = "inv";
    $source2Valid = 1;
  }
  
  $nsSoarLine = `grep 'decisions (' $targetNS`;
  $sSoarLine = `grep 'decisions (' $targetS`;

  # grab the Soar time: Soar's decision count * time per decision
  # also convert to seconds
  $nsSoarLine =~ /^(\d+) decisions \((\S+) msec/ or die;
  $nsSoar = $1*$2*0.001;
  $nsDecisions = $1;
  
  $nsStatusLine = `grep -B2 "This Agent halted" $targetNS | grep succeeded`;  
  $nsValid = 1;
  unless ($nsStatusLine =~ /succeeded/) {
    $nsValid = 0;
  }

  $sSoarLine =~ /^(\d+) decisions \((\S+) msec/ or die;
  $sSoar = $1*$2*0.001;
  $sDecisions = $1;

  $sStatusLine = `grep -B2 "This Agent halted" $targetS | grep succeeded`;  
  $sValid = 1;
  unless ($sStatusLine =~ /succeeded/) {
    $sValid = 0;
  }

  @data = ();
  push @data, "$env-$level-$scenario";
  push @data, $source1Decisions;
  push @data, $source2Decisions;
  push @data, $nsDecisions;
  push @data, $sDecisions;
  # four versions of no-source time
  push @data, $nsSoar;
  push @data, $nsUser;
  push @data, $nsUserSys;
  push @data, $nsReal;
  # four versions of source time
  push @data, $sSoar + $genUser;
  push @data, $sUser + $genUser;
  push @data, $sUserSys + $genUserSys;
  push @data, $sReal + $genReal;
  # four versions of no-source time, including submission
  #push @data, $nsSoar + $nsSubUser;
  #push @data, $nsUser + $nsSubUser;
  #push @data, $nsUserSys + $nsSubUserSys;
  #push @data, $nsReal + $nsSubReal;
  # four versions of source time, including submission
  #push @data, $sSoar + $genUser + $sSubUser;
  #push @data, $sUser + $genUser + $sSubUser;
  #push @data, $sUserSys + $genUserSys + $sSubUserSys;
  #push @data, $sReal + $genReal + $sSubReal;
  # four versions of regret
  if ($source1Valid and $source2Valid and $nsValid and $sValid) {
    push @data, regret($nsDecisions, $sDecisions);
    push @data, regret($nsSoar, $sSoar + $genUser);
    push @data, regret($nsUser, $sUser + $genUser);
    push @data, regret($nsUserSys, $sUserSys + $genUserSys);
    push @data, regret($nsReal, $sReal + $genReal);
  }
  else {
    if (not $source1Valid or not $source2Valid) {
      push @data, "source invalid";
      push @data, "source invalid";
      push @data, "source invalid";
      push @data, "source invalid";
      push @data, "source invalid";
    }
    elsif (not $nsValid) {
      push @data, "target no-source invalid";
      push @data, "target no-source invalid";
      push @data, "target no-source invalid";
      push @data, "target no-source invalid";
      push @data, "target no-source invalid";
    }
    else {
      push @data, "target with source invalid";
      push @data, "target with source invalid";
      push @data, "target with source invalid";
      push @data, "target with source invalid";
      push @data, "target with source invalid";
    }
  }
  # four versions of regret, including submission
  #push @data, regret($nsSoar + $nsSubUser, $sSoar + $genUser + $sSubUser);
  #push @data, regret($nsUser + $nsSubUser, $sUser + $genUser + $sSubUser);
  #push @data, regret($nsUserSys + $nsSubUserSys, $sUserSys + $genUserSys + $sSubUserSys);
  #push @data, regret($nsReal + $nsSubReal, $sReal + $genReal + $sSubReal);

  for ($i=0; $i<=$#data; $i++) {
    push @{$line[$i]}, $data[$i];
  }
}

for ($i=0; $i<=$#line; $i++) {
  for ($j=0; $j<=$#{$line[1]}; $j++) {
    print  "$line[$i][$j]\t";
  }
  print "\n";
}


sub regret() {
  $ns = shift;
  $s = shift;

  $denominator = max($ns, $s);
  $numerator = ($ns - $s)*100.0;
  
  return $numerator / $denominator;
}

sub max() {
  $max = -1;
  foreach (@_) {
    if ($_ > $max) {
      $max = $_;
    }
  }
  return $max;
}

sub realSeconds() {
  # the format for all calls to unix time command is '%E real,%U user,%S sys' 
  # real times are reported in (hours:)minutes:seconds format, 
  # need to convert to seconds (I should have used a %e instead of %E)
  # all other times are reported in seconds (see man time)
  $string = shift;
  if ($string =~ /(\d+):(\d+):(\d+)/) {
    return 3600*$1 + 60*$2 + $3;
  }
  elsif ($string =~ /(\d+):(\S+)/) {
    return 60*$1 + $2;
  }
  return "inv";
}