Summary:        usb library
Name:           usb_control
Version:        0.1.10
Release:        1%{?dist}
License:        GPLv2.0
Group:          System Environment/Libraries
URL:            http://www.cypress.com/
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}

BuildRequires:  libusbx-devel

%description

%prep

%setup -q -c -T -a 0

%build
cd ./%{name}/libcyusb/
make

%install
%{__mkdir_p} '%{buildroot}%{_libdir}'
%{__mkdir_p} '%{buildroot}/usr/include/'
%{__mkdir_p} '%{buildroot}/etc/'
%{__mkdir_p} '%{buildroot}/etc/udev/rules.d/'
%{__mkdir_p} '%{buildroot}/usr/local/bin/'

cp ./%{name}/libcyusb/libcyusb.a %{buildroot}%{_libdir}/libcyusb.a
cp ./%{name}/libcyusb/libcyusb.so.1 %{buildroot}%{_libdir}/libcyusb.so.1
cp ./%{name}/libcyusb/libcyusb.so.1 %{buildroot}%{_libdir}/libcyusb.so

cp ./%{name}/include/cyusb.h %{buildroot}/usr/include/cyusb.h

cp ./%{name}/configs/88-cyusb.rules %{buildroot}/etc/udev/rules.d/88-cyusb.rules
cp ./%{name}/configs/cy_renumerate.sh %{buildroot}/usr/local/bin/cy_renumerate.sh

%files
%{buildroot}%{_libdir}/libcyusb.a
%{buildroot}%{_libdir}/libcyusb.so.1
%{buildroot}%{_libdir}/libcyusb.so
%{buildroot}/usr/include/cyusb.h
%{buildroot}/etc/udev/rules.d/88-cyusb.rules
%{buildroot}/usr/local/bin/cy_renumerate.sh

%changelog
