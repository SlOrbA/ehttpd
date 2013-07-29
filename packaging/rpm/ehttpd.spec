Summary: a http text file server for small and embedded systems
Name: ehttpd
Version: 0.03
Release: 1
Copyright: GPL
Group: System Environment/Daemons
Source: http://ehttpd.hlan.net/rel/ehttpd-0.03.tar.gz
URL: http://ehttpd.hlan.net/
Packager: Lari Korpi <lari.korpi@iki.fi>
Buildroot: /var/tmp/rpm/ehttpd

%description
This is a reference package for ehttpd http daemon.

ehttpd is a small http server that is mainly developed 
to be a easy http implementation to small and embedded 
systems to provide access to text files.

%prep
%setup -n %{name}-%{version}

%build

mkdir build
cd build
../configure --prefix $RPM_BUILD_ROOT/usr/local
make

%install

cd build
make install

%files
/usr/local/bin/ehttpd
