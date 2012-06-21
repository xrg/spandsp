%define git_repo spandsp
%define major 2

%define libnamedevold %{mklibname spandsp 0}-devel
%define libname %mklibname spandsp %{major}
%define libnamedev %mklibname spandsp -d
%define libnamestaticdev %mklibname spandsp -d -s

Summary:        Steve's SpanDSP library for telephony spans
Name:           spandsp
Version:	%git_get_ver
Release:	%mkrel %{git_get_rel}
License:        LGPL
Group:          System/Libraries
URL:            http://www.soft-switch.org/
Source0:        %git_bs_source %{name}-%{version}.tar.gz
Source1:	%{name}-gitrpm.version
Source2:	%{name}-changelog.gitrpm.txt
BuildRequires:  audiofile-devel
BuildRequires:  fftw-devel
BuildRequires:  file
BuildRequires:  fltk-devel
BuildRequires:  jpeg-devel
BuildRequires:  libtool
BuildRequires:  libxml2-devel
BuildRequires:  tiff-devel

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
%git_get_source
%setup -q

%build
%configure2_5x
%make

%install
rm -rf %{buildroot}

%makeinstall_std

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

%changelog -f  %{_sourcedir}/%{name}-changelog.gitrpm.txt

