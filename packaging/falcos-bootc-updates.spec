Name:           falcos-bootc-updates
Version:        0.1.0
Release:        1%{?dist}
Summary:        KDE System Settings module for bootc image updates
License:        MIT
URL:            https://github.com/jayelg/falcos-bootc-updates
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-knotifications-devel
BuildRequires:  systemd-rpm-macros

Requires:       bootc
Requires:       kf6-kirigami

%description
System Settings module for bootc based systems: shows the booted image
version, checks for and stages updates through a small polkit gated
D-Bus service, and offers restart now or a scheduled restart.

%prep
%autosetup

%build
%cmake -DKDE_INSTALL_LIBEXECDIR=%{_libexecdir}
%cmake_build

%install
%cmake_install

%post
%systemd_post bootcupdatesd.service
%systemd_user_post bootcupdates-notifier.service

%preun
%systemd_preun bootcupdatesd.service
%systemd_user_preun bootcupdates-notifier.service

%postun
%systemd_postun bootcupdatesd.service

%files
%license LICENSE
%doc README.md
%{_libexecdir}/bootcupdatesd
%{_libexecdir}/bootcupdates-notifier
%{_unitdir}/bootcupdatesd.service
%{_userunitdir}/bootcupdates-notifier.service
%{_userpresetdir}/90-falcos-bootc-updates.preset
%{_datadir}/knotifications6/bootcupdates.notifyrc
%{_datadir}/dbus-1/system.d/io.github.jayelg.BootcUpdates1.conf
%{_datadir}/dbus-1/system-services/io.github.jayelg.BootcUpdates1.service
%{_datadir}/polkit-1/actions/io.github.jayelg.bootcupdates.policy
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_bootcupdates.so
%{_datadir}/applications/kcm_bootcupdates.desktop

%changelog
* Fri Jul 10 2026 John Grant <jlgrant85@gmail.com> - 0.1.0-1
- Initial skeleton
