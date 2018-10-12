Summary:    ocpjbod for Knox, HoneyBadger, etc.
Name:       ocpjbod
Version:    1.09
Release:    1
Group:      Facebook/System
License:    Copyright (c) 2013-present, Facebook, Inc. All rights reserved.
Vendor:     Facebook, Inc
Source:     %{name}-%{version}.tar.gz
Packager:   songliubraving
Requires:   sg3_utils-libs >= 1.28
BuildRoot:   %{_tmppath}/%{name}-root

%description
ocpjbod is the enclosure management tool for Facebook storage enclosures (Knox, HoneyBadger, etc.)

%prep
%setup -q

%build
make UTIL_VERSION=%{version}

%install
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/ocpjbod
