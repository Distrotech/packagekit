#!/usr/bin/perl
#
# Copyright (C) 2008 Aurelien Lefebvre <alkh@mandriva.org>
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

use strict;
local $| = 1; # stdout autoflush

use lib;
use File::Basename;
use File::Temp;

BEGIN {
  push @INC, dirname($0);
}

use URPM;
use urpm;
use urpm::media;
use urpm::args;
use urpm::select;

use urpmi_backend::actions;
use urpmi_backend::open_db;
use urpmi_backend::tools;
use urpmi_backend::filters;
use urpmi_backend::groups;

use perl_packagekit::enums;
use perl_packagekit::prints;

use MDK::Common;

my $urpm = urpm->new_parse_cmdline;
urpm::media::configure($urpm);
dispatch_command($urpm, \@ARGV);
print "finished\n";

while(<STDIN>) {
  chomp($_);
  my @args = split (/\t/, $_);
  dispatch_command($urpm, \@args);
  print "finished\n";
}

sub dispatch_command {

  my ($urpm, $args) = @_;

  my $command = shift(@{$args});
  if($command eq "get-depends") {
    get_depends($urpm, $args);
  }
  elsif($command eq "get-details") {
    get_details($urpm, $args);
  }
  elsif($command eq "get-distro-upgrades") {
    get_distro_upgrades();
  }
  elsif($command eq "get-files") {
    get_files($urpm, $args);
  }
  elsif($command eq "get-packages") {
    get_packages($urpm, $args);
  }
  elsif($command eq "get-repo-list") {
    get_repo_list($urpm, $args);
  }
  elsif($command eq "get-requires") {
    get_requires($urpm, $args);
  }
  elsif($command eq "get-update-detail") {
    get_update_detail($urpm, $args);
  }
  elsif($command eq "get-updates") {
    get_updates($urpm, $args);
  }
  elsif($command eq "install-packages") {
    install_packages($urpm, $args);
    urpm::media::configure($urpm);
  }
  elsif($command eq "search-name") {
    search_name($urpm, $args);
  }
  elsif($command eq "refresh-cache") {
    refresh_cache($urpm);
    urpm::media::configure($urpm);
  }
  elsif($command eq "remove-packages") {
    remove_packages($urpm, $args);
    urpm::media::configure($urpm);
  }
  elsif($command eq "repo-enable") {
    repo_enable($urpm, $args);
  }
  elsif($command eq "resolve") {
    resolve($urpm, $args);
  }
  elsif($command eq "search-details") {
    search_details($urpm, $args);
  }
  elsif($command eq "search-file") {
    search_file($urpm, $args);
  }
  elsif($command eq "search-group") {
    search_group($urpm, $args);
  }
  elsif($command eq "update-packages") {
    update_packages($urpm, $args);
    urpm::media::configure($urpm);
  }
  elsif($command eq "update-system") {
    update_system($urpm, $args);
    urpm::media::configure($urpm);
  }
  elsif($command eq "exit") {
    exit 0;
  }
  else {}
}
sub get_depends {

  my ($urpm, $args) = @_;
  
  my @filterstab = split(/;/, @{$args}[0]);
  my @packageidstab = split(/&/, @{$args}[1]);
  my $recursive_option = @{$args}[2] eq "yes" ? 1 : 0;
  
  pk_print_status(PK_STATUS_ENUM_DEP_RESOLVE);
  
  my @pkgnames;
  foreach (@packageidstab) {
    my @pkgid = split(/;/, $_);
    push(@pkgnames, $pkgid[0]);
  }
  
  my %requested;
  my $results = urpm::select::search_packages($urpm, \%requested, \@pkgnames,
    fuzzy => 0,
    caseinsensitive => 0,
    all => 0
  );
  
  $results 
      or (_finished() and return);
  
  my $empty_db = new URPM;
  my $state = {};
  $urpm->resolve_requested($empty_db,
    $state,
    \%requested,
  );
  
  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);
  
  my %selected = %{$state->{selected}};
  my @selected_keys = keys %selected;
  my @depslist = @{$urpm->{depslist}};
  
  foreach(sort {@depslist[$b]->flag_installed <=> @depslist[$a]->flag_installed} @selected_keys) {
    my $pkg = @depslist[$_];
    if($pkg->flag_installed) {
      grep(/^${\FILTER_NOT_INSTALLED}$/, @filterstab) and next;
      pk_print_package(INFO_INSTALLED, get_package_id($pkg), $pkg->summary);
    }
    else {
      grep(/^${\FILTER_INSTALLED}$/, @filterstab) and next;
      pk_print_package(INFO_AVAILABLE, get_package_id($pkg), $pkg->summary);
    }
  }
  _finished();
}

sub get_details {

  my ($urpm, $args) = @_;
  
  my @packageidstab = split(/&/, @{$args}[0]);
  pk_print_status(PK_STATUS_ENUM_QUERY);

  foreach (@packageidstab) {
    _print_package_details($urpm, $_);
  }
  _finished();
}

sub get_distro_upgrades {

  pk_print_status(PK_STATUS_ENUM_QUERY);

  open(PRODUCT_FILE, "/etc/product.id");

  my %product_id;
  %product_id = _parse_line(<PRODUCT_FILE>);
  close(PRODUCT_FILE);

  my (undef, $distribfile_path) = tempfile("packagekit_urpmi_XXXXXX", UNLINK => 1);
  _download_distrib_file($distribfile_path, \%product_id);

  -f $distribfile_path or goto finished;

  my @distribs;
  open(DISTRIB_FILE, $distribfile_path);
  while(<DISTRIB_FILE>) {
    my %distrib = _parse_line($_);
    push(@distribs, \%distrib);
  }
  close(DISTRIB_FILE);

  my $distrib;
  foreach (@distribs) {
    if($_->{version} == $product_id{version}) {
      $distrib = $_;
    }
  }

  $distrib or goto finished;
  @distribs = sort { $b->{release_date} <=> $a->{release_date} } @distribs;

  my $newer_version = _get_newer_distrib($distrib->{version}, \@distribs);
  $newer_version or goto finished;
  pk_print_distro_upgrade(PK_DISTRO_UPGRADE_ENUM_STABLE, join(" ", "Mandriva", $product_id{product}, $newer_version->{version}), "");

  unlink($distribfile_path);
  finished:
  _finished();
}

sub get_files {
  
  my ($urpm, $args) = @_;
  
  my @packageidstab = split(/&/, @{$args}[0]);
  pk_print_status(PK_STATUS_ENUM_QUERY);
  
  foreach (@packageidstab) {
    _print_package_files($urpm, $_);
  }
  _finished();
}

sub get_packages {

  my ($urpm, $args) = @_;
  my @filterstab = split(/;/, @{$args}[0]);
  
  pk_print_status(PK_STATUS_ENUM_QUERY);
  
  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);
  
  # Here we display installed packages
  if(not grep(/^${\FILTER_NOT_INSTALLED}$/, @filterstab)) {
    $db->traverse(sub {
        my ($pkg) = @_;
        if(filter($pkg, \@filterstab, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          pk_print_package(INFO_INSTALLED, get_package_id($pkg), ensure_utf8($pkg->summary));
        }
      });
  }
  
  # Here are package which can be installed
  if(not grep(/^${\FILTER_INSTALLED}$/, @filterstab)) {
    foreach my $pkg(@{$urpm->{depslist}}) {
      if($pkg->flag_upgrade) {
        if(filter($pkg, \@filterstab, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          pk_print_package(INFO_AVAILABLE, get_package_id($pkg), ensure_utf8($pkg->summary));
        }
      }  
    }
  }
  _finished();
}

sub get_repo_list {

  my ($urpm, $args) = @_;
  foreach my $media (@{$urpm->{media}}) {
    pk_print_repo_details($media->{name}, $media->{name}, $media->{ignore});
  }
  _finished();
}


sub get_requires {
  
  my ($urpm, $args) = @_;
  
  my @filterstab = split(/;/, @{$args}[0]);
  my @packageidstab = split(/&/, @{$args}[1]);
  my $recursive_option = @{$args}[2] eq "yes" ? 1 : 0;
  
  my @pkgnames;
  foreach (@packageidstab) {
    my $pkg = get_package_by_package_id($urpm, $_);
    $pkg and push(@pkgnames, $pkg->name);
  }
  
  pk_print_status(PK_STATUS_ENUM_DEP_RESOLVE);
  my @requires = perform_requires_search($urpm, \@pkgnames, $recursive_option);
  
  foreach(@requires) {
    if(filter($_, \@filterstab, { FILTER_GUI => 1, FILTER_DEVELOPMENT => 1 })) {
      if(package_version_is_installed($_)) {
        grep(/^${\FILTER_NOT_INSTALLED}$/, @filterstab) or pk_print_package(INFO_INSTALLED, get_package_id($_), $_->summary);
      }
      else {
        grep(/^${\FILTER_INSTALLED}$/, @filterstab) or pk_print_package(INFO_AVAILABLE, get_package_id($_), $_->summary);
      }
    }
  }
  _finished();
}

sub get_update_detail {

  my ($urpm, $args) = @_;
  
  pk_print_status(PK_STATUS_ENUM_QUERY);
  my @packageidstab = split(/&/, @{$args}[0]);
  
  foreach (@packageidstab) {
    _print_package_update_details($urpm, $_);
  }
  _finished();
}

sub get_updates {

  my ($urpm, $args) = @_;
  # Fix me
  # Filter are to be implemented.
  my $filters = @{$args}[0];
  
  pk_print_status(PK_STATUS_ENUM_DEP_RESOLVE);

  my $state = {};
  my %requested;
  my $restart = urpm::select::resolve_dependencies($urpm, $state, \%requested,
    auto_select => 1);
  
  my %selected = %{$state->{selected} || {}};
  my @ask_unselect = urpm::select::unselected_packages($urpm, $state);
  my @to_remove = urpm::select::removed_packages($urpm, $state);
  my @to_install = @{$urpm->{depslist}}[sort { $a <=> $b } keys %{$state->{selected}}]; 
  @to_install = grep { $_->arch ne 'src' } @to_install;
  my $updates_descr = urpm::get_updates_description($urpm);
  
  foreach(@to_install) {
    my $updesc = $updates_descr->{URPM::pkg2media($urpm->{media}, $_)->{name}}{$_->name};
    pk_print_package($updesc->{importance} eq "bugfix" ? INFO_BUGFIX :
                        $updesc->{importance} eq "security" ? INFO_SECURITY :
                        INFO_NORMAL, get_package_id($_), $_->summary);
  }
  _finished();
}

sub install_packages {

  my ($urpm, $args) = @_;

  my $only_trusted = @{$args}[0];
  my @packageidstab = split(/&/, @{$args}[1]);
  
  my @names;
  foreach(@packageidstab) {
    my @pkg_id = (split(/;/, $_));
    push @names, $pkg_id[0];
  }
  
  my %requested;
  
  urpm::select::search_packages($urpm, \%requested, \@names, 
    fuzzy => 0, 
    caseinsensitive => 0,
    all => 0);
  eval {
    perform_installation($urpm, \%requested, only_trusted => $only_trusted);
  };
  _finished();
}

sub search_name {

  my ($urpm, $args) = @_;
  
  pk_print_status(PK_STATUS_ENUM_QUERY);

  my @filterstab = split(/;/, @{$args}[0]);
  my $search_term = @{$args}[1];
  
  my $basename_option = FILTER_BASENAME;
  $basename_option = grep(/$basename_option/, @filterstab);

  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);
  
  # Here we display installed packages
  if(not grep(/^${\FILTER_NOT_INSTALLED}$/, @filterstab)) {
    $db->traverse(sub {
        my ($pkg) = @_;
        if(filter($pkg, \@filterstab, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          if( (!$basename_option && $pkg->name =~ /$search_term/)
            || $pkg->name =~ /^$search_term$/ ) {
            pk_print_package(INFO_INSTALLED, get_package_id($pkg), ensure_utf8($pkg->summary));
          }
        }
      });
  }
  
  # Here are packages which can be installed
  grep(/^${\FILTER_INSTALLED}$/, @filterstab) 
    and _finished()
    and return;
  
  foreach my $pkg(@{$urpm->{depslist}}) {
    if($pkg->flag_upgrade && filter($pkg, \@filterstab, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
      if( (!$basename_option && $pkg->name =~ /$search_term/)
        || $pkg->name =~ /^$search_term$/ ) {
        pk_print_package(INFO_AVAILABLE, get_package_id($pkg), ensure_utf8($pkg->summary));
      }
    }
  }

  _finished();
}

sub refresh_cache {

  my ($urpm) = @_;

  $urpm->{fatal} = sub { 
    pk_print_error(PK_ERROR_ENUM_TRANSACTION_ERROR, $_[1]."\n"); 
    die;
  };
  my $urpmi_lock = urpm::lock::urpmi_db($urpm, 'exclusive', wait => 0);
  urpm::media::read_config($urpm);

  my @entries = map { $_->{name} } @{$urpm->{media}};
  @entries == 0 and pk_print_error(PK_ERROR_ENUM_TRANSACTION_ERROR, "nothing to update (use urpmi.addmedia to add a media)\n");

  my %options = ( all => 1 );
  
  eval {
    my $ok = urpm::media::update_media($urpm, %options, quiet => 0);
  };
  _finished();

}

sub remove_packages {

  my ($urpm, $args) = @_;

  my $notfound = 0;
  my $notfound_callback = sub {
    $notfound = 1;
  };

  my $urpmi_lock = urpm::lock::urpmi_db($urpm, 'exclusive', wait => 1);

  my $allowdeps_option = @{$args}[0] eq "yes" ? 1 : 0;
  my @packageidstab = split(/&/, @{$args}[1]);

  my @names;
  foreach(@packageidstab) {
    my @pkg_id = (split(/;/, $_));
    push @names, $pkg_id[0];
  }

  pk_print_status(PK_STATUS_ENUM_DEP_RESOLVE);

  my $state = {};
  my @breaking_pkgs = ();
  my @to_remove = urpm::select::find_packages_to_remove($urpm,
    $state,
    \@names,
    callback_notfound => $notfound_callback,
    callback_fuzzy => $notfound_callback,
    callback_base => sub {
      my $urpm = shift @_;
      push @breaking_pkgs, @_;
    }
  );

  if($notfound) {
    pk_print_error(PK_ERROR_ENUM_PACKAGE_NOT_INSTALLED, "Some selected packages are not installed on your system");
  }
  elsif(@breaking_pkgs) {
    pk_print_error(PK_ERROR_ENUM_CANNOT_REMOVE_SYSTEM_PACKAGE, "Removing selected packages will break your system");
  }
  elsif(!$allowdeps_option && scalar(@to_remove) != scalar(@names)) {
    pk_print_error(PK_ERROR_ENUM_TRANSACTION_ERROR, "Packages can't be removed because dependencies remove is forbidden");
  }
  else {
    pk_print_status(PK_STATUS_ENUM_REMOVE);
    urpm::install::install($urpm,
      \@to_remove, {}, {},
      callback_report_uninst => sub {
        my @return = split(/ /, $_[0]);
        pk_print_package(INFO_REMOVING, fullname_to_package_id($return[$#return]), "");
      }
    );
  }

  $urpmi_lock->unlock;
  _finished();
}

sub repo_enable {

  my ($urpm, $args) = @_;

  my $name = @{$args}[0];
  my $enable = @{$args}[1] eq "yes" ? 1 : 0;

  my @media = grep { $_->{name} eq $name } @{$urpm->{media}};
  if ($#media == 0) {
    if ($enable) {
      delete @media[0]->{ignore};
    } else {
      $media[0]->{ignore} = 1;
    }
    urpm::media::write_config($urpm);
  } else {
    pk_print_error(PK_ERROR_ENUM_REPO_NOT_FOUND, qq(Repository named "$name" not found.\n));
  }

  _finished();
}

sub resolve {

  my ($urpm, $args) = @_;

  my @filters = split(/;/, @{$args}[0]);
  my @patterns = split(/&/, @{$args}[1]);

  pk_print_status(PK_STATUS_ENUM_QUERY);

  my %requested;
  urpm::select::search_packages($urpm, \%requested, \@patterns, 
    fuzzy => 0, 
    caseinsensitive => 0,
    all => 0
  );

  my @requested_keys = keys %requested;
  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);

  foreach (@requested_keys) {
    my $pkg = @{$urpm->{depslist}}[$_];
    ($_ && $pkg) or next;

    # We exit the script if found package does not match with specified filters
    filter($pkg, \@filters, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1}) or next;

    if($pkg->version."-".$pkg->release eq find_installed_version($pkg)) {
      grep(/^${\FILTER_NOT_INSTALLED}$/, @filters) and next;
      pk_print_package(INFO_INSTALLED, get_package_id($pkg), $pkg->summary);
    }
    else {
      grep(/^${\FILTER_INSTALLED}$/, @filters) and next;
      pk_print_package(INFO_AVAILABLE, get_package_id($pkg), $pkg->summary);
    }
  }
  _finished();
}

sub search_details {

  my ($urpm, $args) = @_;
  my @filters = split(/;/, @{$args}[0]);
  my $search_term = @{$args}[1];

  pk_print_status(PK_STATUS_ENUM_QUERY);

  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);

  if(not grep(/^${\FILTER_NOT_INSTALLED}$/, @filters)) {
    $db->traverse(sub {
        my ($pkg) = @_;
        if(filter($pkg, \@filters, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          if($pkg->name =~ /$search_term/ || $pkg->summary =~ /$search_term/ || $pkg->url =~ /$search_term/) {
            pk_print_package(INFO_INSTALLED, get_package_id($pkg), ensure_utf8($pkg->summary));
          }
        }
      });
  }

  if(not grep(/^${\FILTER_INSTALLED}$/, @filters)) {
    foreach my $pkg(@{$urpm->{depslist}}) {
      if($pkg->flag_upgrade) {
        if(filter($pkg, \@filters, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          if($pkg->name =~ /$search_term/ || $pkg->summary =~ /$search_term/ || $pkg->url =~ /$search_term/) {
            pk_print_package(INFO_AVAILABLE, get_package_id($pkg), ensure_utf8($pkg->summary));
          }
        }
      }  
    }
  }
  _finished();
}

sub search_file {

  my ($urpm, $args) = @_;
  my @filters = split(/;/, @{$args}[0]);
  my $search_term = @{$args}[1];

  my %requested;

  pk_print_status(PK_STATUS_ENUM_QUERY);

  perform_file_search($urpm, \%requested, $search_term, fuzzy => 1);

  foreach(keys %requested) {
    my $p = @{$urpm->{depslist}}[$_];
    if(filter($p, \@filters, { FILTER_INSTALLED => 1, FILTER_DEVELOPMENT=> 1, FILTER_GUI => 1})) {
      my $version = find_installed_version($p);
      if($version eq $p->version."-".$p->release) {
        pk_print_package(INFO_INSTALLED, get_package_id($p), ensure_utf8($p->summary));
      }
      else {
        pk_print_package(INFO_AVAILABLE, get_package_id($p), ensure_utf8($p->summary));
      }
    }
  }
  _finished();
}

sub search_group {

  my ($urpm, $args) = @_;
  my @filters = split(/;/, @{$args}[0]);
  my $pk_group = @{$args}[1];
  
  pk_print_status(PK_STATUS_ENUM_QUERY);

  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);

  if(not grep(/^${\FILTER_NOT_INSTALLED}$/, @filters)) {
    $db->traverse(sub {
        my ($pkg) = @_;
        if(filter($pkg, \@filters, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          if(package_belongs_to_pk_group($pkg, $pk_group)) {
            pk_print_package(INFO_INSTALLED, get_package_id($pkg), ensure_utf8($pkg->summary));
          }
        }
      });
  }

  if(not grep(/^${\FILTER_INSTALLED}$/, @filters)) {
    foreach my $pkg(@{$urpm->{depslist}}) {
      if($pkg->flag_upgrade) {
        if(filter($pkg, \@filters, {FILTER_DEVELOPMENT => 1, FILTER_GUI => 1})) {
          if(package_belongs_to_pk_group($pkg, $pk_group)) {
            pk_print_package(INFO_AVAILABLE, get_package_id($pkg), ensure_utf8($pkg->summary));
          }
        }
      }  
    }
  }
  _finished();
}

sub update_packages {

  my ($urpm, $args) = @_;

  my $only_trusted = @{$args}[0];
  my @packageidstab = split(/&/, @{$args}[1]);

  my @names;
  foreach(@packageidstab) {
    my @pkgid = split(/;/, $_);
    push @names, $pkgid[0];
  }

  my $db = open_rpm_db();
  $urpm->compute_installed_flags($db);

  my %requested;

  my @depslist = @{$urpm->{depslist}};
  my $pkg = undef;
  foreach my $depslistpkg (@depslist) {
    foreach my $name (@names) {
      if($depslistpkg->name =~ /^$name$/ && $depslistpkg->flag_upgrade) {
        $requested{$depslistpkg->id} = 1;
        goto tonext;
      }
    }
    tonext:
  }
  eval {
    perform_installation($urpm, \%requested, only_trusted => $only_trusted);
  };
  _finished();
}

sub update_system {
  
  my ($urpm, $args) = @_;

  my $only_trusted = @{$args}[0];
  eval {
    perform_installation($urpm, {}, auto_select => 1, only_trusted => $only_trusted);
  };
  _finished();
}

sub _finished {
  pk_print_status(PK_STATUS_ENUM_FINISHED);
}

sub _print_package_details {

  my ($urpm, $pkgid) = @_;
  
  my $pkg = get_package_by_package_id($urpm, $pkgid);
  $pkg or return;

  my $medium = pkg2medium($pkg, $urpm);
  my $xml_info = 'info';
  my $xml_info_file = urpm::media::any_xml_info($urpm, $medium, $xml_info, undef, undef);
  
  if(!$xml_info_file) {
    pk_print_details(get_package_id($pkg), "N/A", $pkg->group, "N/A", "N/A", 0);
    return;
  }
  
  require urpm::xml_info;
  require urpm::xml_info_pkg;
  my $name = urpm_name($pkg);
  my %nodes = eval { urpm::xml_info::get_nodes($xml_info, $xml_info_file, [ $name ]) };
  my %xml_info_pkgs;
  put_in_hash($xml_info_pkgs{$name} ||= {}, $nodes{$name});
  my $description = $xml_info_pkgs{$name}{description};
  $description =~ s/\n/;/g;
  $description =~ s/\t/ /g;
  
  pk_print_details(get_package_id($pkg), "N/A", get_pk_group($pkg->group), ensure_utf8($description), $xml_info_pkgs{$name}{url}, $pkg->size);
}

sub _print_package_files {

  my ($urpm, $pkgid) = @_;

  my $pkg = get_package_by_package_id($urpm, $pkgid);
  $pkg or return;
  
  my $medium = pkg2medium($pkg, $urpm);
  my $xml_info = 'files';
  my $xml_info_file = urpm::media::any_xml_info($urpm, $medium, $xml_info, undef, undef);
  require urpm::xml_info;
  require urpm::xml_info_pkg;
  my $name = urpm_name($pkg);
  my %nodes = eval { urpm::xml_info::get_nodes($xml_info, $xml_info_file, [ $name ]) };
  my %xml_info_pkgs;
  put_in_hash($xml_info_pkgs{$name} ||= {}, $nodes{$name});
  my @files = map { chomp_($_) } split("\n", $xml_info_pkgs{$name}{files});
  
  pk_print_files(get_package_id($pkg), join(';', @files));
}

sub _print_package_update_details {

  my ($urpm, $pkgid) = @_;
  my $pkg = get_package_by_package_id($urpm, $pkgid);
  $pkg or return;

  my %requested;
  $requested{$pkg->id} = 1;
  my $state = {};
  my $restart = urpm::select::resolve_dependencies($urpm, $state, \%requested);
  my @ask_unselect = urpm::select::unselected_packages($urpm, $state);
  my @to_remove = urpm::select::removed_packages($urpm, $state);
  my @to_install = @{$urpm->{depslist}}[sort { $a <=> $b } keys %{$state->{selected}}]; 
  my ($src, $binary) = partition { $_->arch eq 'src' } @to_install;
  @to_install = @$binary;
  my $updates_descr = urpm::get_updates_description($urpm);
  my $updesc = $updates_descr->{URPM::pkg2media($urpm->{media}, $pkg)->{name}}{$pkg->name};
  my $desc;
  if($updesc) {
    $desc = $updesc->{pre};
    $desc =~ s/\n/;/g;
  }
  
  my @to_upgrade_pkids;
  foreach(@to_install) {
    my $pkid = get_installed_version_pkid($_);
    push @to_upgrade_pkids, $pkid if $pkid;
  }
  
  pk_print_update_detail(get_package_id($pkg),
    join("&", @to_upgrade_pkids),
    join("&", map(fullname_to_package_id($_), @to_remove)),
    "http://qa.mandriva.com",
    "http://qa.mandriva.com",
    "http://qa.mandriva.com",
    $restart ? PK_RESTART_ENUM_SYSTEM : PK_RESTART_ENUM_NONE,
    $desc);
}

sub _parse_line {
  my ($line) = @_;
  my %hash;
  my @affects = split(/,/, $line);
  foreach my $affect (@affects) {
    my ($variable, $value) = split(/=/, $affect);
    chomp($variable);
    chomp($value);
    $hash{$variable} = $value;
  }
  return %hash;
}

sub _download_distrib_file {

  my ($outfile, $product_id) = @_;
  
  -x "/usr/bin/wget" or die "wget is missing\n";
  
  my $api_url = sprintf("http://api.mandriva.com/distributions/%s.%s.list?product=%s",
                  lc($product_id->{type}),
                  lc($product_id->{arch}),
                  lc($product_id->{product}));
  
  my $wget_command = join(" ", 
                          "/usr/bin/wget",
                          "--quiet",
                          "--output-document", $outfile,
                          $api_url);
  
  my $wget_pid = open(my $wget, "$wget_command |");
  close($wget);
}

sub _get_newer_distrib {

  my ($installed_version, $distrib_list) = @_;
  my $installed_distrib;
  foreach (@$distrib_list) {
    if($_->{version} == $installed_version) {
      $installed_distrib = $_;
    }
  }
  $installed_distrib or return;
  foreach (@$distrib_list) {
    if($installed_distrib->{release_date} < $_->{release_date}) {
      return $_;
    }
  }
}
