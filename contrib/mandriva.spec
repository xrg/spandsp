%define major 2

%define libnamedevold %{mklibname spandsp 0}-devel
%define libname %mklibname spandsp %{major}
%define libnamedev %mklibname spandsp -d
%define libnamestaticdev %mklibname spandsp -d -s

Summary:        Steve's SpanDSP library for telephony spans
Name:           spandsp
Version:        0.0.6
Release:        %mkrel 0.pre18
License:        GPL
Group:          System/Libraries
URL:            http://www.soft-switch.org/
Source0:        http://www.soft-switch.org/downloads/spandsp/spandsp-%{version}pre18.tgz
BuildRequires:  audiofile-devel
BuildRequires:  fftw2-devel
BuildRequires:  file
BuildRequires:  fltk-devel
BuildRequires:  jpeg-devel
BuildRequires:  libtool
BuildRequires:  libxml2-devel
BuildRequires:  tiff-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
spandsp is a library for DSP in telephony spans. It can perform many of the
common DSP functions, such as the generation and detection of DTMF and
supervisory tones.

%package -n %{libname}
Summary:        Steve's SpanDSP library for telephony spans
Group:          System/Libraries

%description -n %{libname}
spandsp is a library for DSP in telephony spans. It can perform many of the
common DSP functions, such as the generation and detection of DTMF and
supervisory tones.

%package -n %{libnamedev}
Summary:        Header files and libraries needed for development with SpanDSP
Group:          Development/C
Obsoletes:      %{libnamedevold} < %{version}-%{release}
Provides:       %{name}-devel = %{version}-%{release}
Requires:       %{libname} = %{version}-%{release}

%description -n %{libnamedev}
This package includes the header files and libraries needed for developing
programs using SpanDSP.

%package -n %{libnamestaticdev}
Summary:        Static libraries needed for development with SpanDSP
Group:          Development/C
Provides:       %{name}-static-devel = %{version}-%{release}
Requires:       %{libnamedev} = %{version}-%{release}

%description -n %{libnamestaticdev}
This package includes the static libraries needed for developing programs
using SpanDSP.

%prep

%setup -q

%build
%configure2_5x
%make

%install
rm -rf %{buildroot}

%makeinstall_std

%if %mdkversion < 200900
%post -n %{libname} -p /sbin/ldconfig
%endif

%if %mdkversion < 200900
%postun -n %{libname} -p /sbin/ldconfig
%endif

%clean
rm -rf %{buildroot}

%files -n %{libname}
%defattr(-,root,root)
%doc AUTHORS ChangeLog COPYING DueDiligence INSTALL NEWS README
%{_libdir}/lib*.so.%{major}*

%files -n %{libnamedev}
%defattr(-,root,root)
%{_includedir}/spandsp
%{_includedir}/*.h
%{_libdir}/*.so
%{_libdir}/*.la
%{_libdir}/pkgconfig/*.pc

%files -n %{libnamestaticdev}
%defattr(-,root,root)
%{_libdir}/*.a
