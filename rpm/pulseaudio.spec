Name:       pulseaudio

%define pulseversion 14.2

Summary:    General purpose sound server
Version:    %{pulseversion}
Release:    1
License:    LGPLv2+
URL:        http://pulseaudio.org
Source0:    http://freedesktop.org/software/pulseaudio/releases/pulseaudio-%{version}.tar.xz
Source1:    90-pulse.conf
Source2:    pulseaudio.service
Source3:    50-sfos.daemon.conf
Source4:    50-sfos.client.conf
Source5:    pulseaudio-system.service
Requires:   udev
Requires:   libsbc >= 1.0
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
# system-wide mode %pre
Requires(pre): /usr/bin/getent
Requires(pre): /usr/sbin/groupadd
Requires(pre): /usr/sbin/useradd

BuildRequires:  pkgconfig(alsa) >= 1.0.24
BuildRequires:  pkgconfig(dbus-1) >= 1.4.12
BuildRequires:  pkgconfig(glib-2.0) >= 2.4.0
BuildRequires:  pkgconfig(libasyncns) >= 0.1
BuildRequires:  pkgconfig(libudev) >= 143
BuildRequires:  pkgconfig(orc-0.4) >= 0.4.11
BuildRequires:  pkgconfig(sndfile) >= 1.0.20
BuildRequires:  pkgconfig(speexdsp) >= 1.2
BuildRequires:  pkgconfig(atomic_ops)
BuildRequires:  pkgconfig(sbc) >= 1.0
BuildRequires:  intltool
BuildRequires:  libcap-devel
BuildRequires:  libtool >= 2.4
BuildRequires:  libtool-ltdl-devel
BuildRequires:  fdupes
BuildRequires:  systemd-devel
BuildRequires:  pkgconfig(bluez) >= 4.101

%description
PulseAudio is a layer between audio devices and applications. It removes
the need for applications to care about the details of the hardware.
PulseAudio is responsible for:
 * automatically converting the audio format between applications and sound
   devices
 * mixing audio streams, which allows multiple applications to use the same
   sound device at the same time
 * handling device and application volumes
 * routing audio to the right place without requiring applications to care
   about the routing
 * providing an unified view of all audio devices, regardless of whether
   they are ALSA-supported sound cards, Bluetooth headsets, remote sound
   cards in the local network or anything else
 * and more...

%package devel
Summary:    PulseAudio Development headers and libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.

%package doc
Summary:   Documentation for %{name}
Requires:  %{name} = %{version}-%{release}

%description doc
Man pages for %{name}.

%prep
%setup -q -n %{name}-%{version}

%build
echo "%{version}" > .tarball-version
NOCONFIGURE=1 ./bootstrap.sh

%ifarch %{arm}
export CFLAGS="$CFLAGS -mfpu=neon"
export CXXFLAGS="$CXXFLAGS -mfpu=neon"
%endif

%configure --disable-static \
           --disable-x11 \
%ifarch %{arm} || %{aarch64}
           --enable-neon-opt \
%endif
           --disable-openssl \
           --disable-gconf \
           --disable-esound \
           --with-database=simple

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%make_install

install -d %{buildroot}/etc/security/limits.d
cp -a %{SOURCE1} %{buildroot}/etc/security/limits.d
install -d %{buildroot}%{_userunitdir}
cp -a %{SOURCE2} %{buildroot}%{_userunitdir}
mkdir -p %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../pulseaudio.service %{buildroot}%{_userunitdir}/user-session.target.wants/
install -d %{buildroot}/%{_sysconfdir}/pulse/daemon.conf.d
install -m 644 %{SOURCE3} %{buildroot}/%{_sysconfdir}/pulse/daemon.conf.d
install -d %{buildroot}/%{_sysconfdir}/pulse/client.conf.d
install -m 644 %{SOURCE4} %{buildroot}/%{_sysconfdir}/pulse/client.conf.d

# system-wide mode configuration
install -d %{buildroot}/%{_unitdir}
install -m 644 %{SOURCE5} %{buildroot}/%{_unitdir}/pulseaudio.service

mkdir -p %{buildroot}%{_docdir}/%{name}-%{version}
install -m0644 README %{buildroot}%{_docdir}/%{name}-%{version}

%find_lang pulseaudio

%fdupes  %{buildroot}/%{_datadir}
%fdupes  %{buildroot}/%{_includedir}

# Stray X11 manpage
rm %{buildroot}%{_mandir}/man1/start-pulseaudio-x11.1

%pre
getent group pulse-access >/dev/null || groupadd -r pulse-access
getent group pulse >/dev/null || groupadd -f -g 171 -r pulse
if ! getent passwd pulse >/dev/null ; then
    if ! getent passwd 171 >/dev/null ; then
        useradd -r -u 171 -g pulse -G audio -d %{_localstatedir}/run/pulse -s /sbin/nologin -c "PulseAudio System Daemon" pulse
    else
        useradd -r -g pulse -G audio -d %{_localstatedir}/run/pulse -s /sbin/nologin -c "PulseAudio System Daemon" pulse
    fi
fi
usermod -G pulse-access -a root || :

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f pulseaudio.lang
%defattr(-,root,root,-)
%license GPL LGPL LICENSE
%config %{_sysconfdir}/pulse/*.conf
%config %{_sysconfdir}/pulse/*.pa
%config %{_sysconfdir}/security/limits.d/90-pulse.conf
%dir %{_sysconfdir}/pulse/daemon.conf.d
%config %{_sysconfdir}/pulse/daemon.conf.d/50-sfos.daemon.conf
%dir %{_sysconfdir}/pulse/client.conf.d
%config %{_sysconfdir}/pulse/client.conf.d/50-sfos.client.conf
%{_datadir}/bash-completion/completions/*
%{_datadir}/zsh/site-functions/_pulseaudio
%{_userunitdir}/pulseaudio.socket
%dir %{_sysconfdir}/pulse
%{_userunitdir}/pulseaudio.service
%{_userunitdir}/user-session.target.wants/pulseaudio.service
%{_udevrulesdir}/90-pulseaudio.rules
%{_bindir}/pacat
%{_bindir}/pacmd
%{_bindir}/pactl
%{_bindir}/padsp
%{_bindir}/pamon
%{_bindir}/paplay
%{_bindir}/parec
%{_bindir}/parecord
%{_bindir}/pasuspender
%{_bindir}/pulseaudio
%{_bindir}/pa-info
%{_libdir}/*.so.*
%dir %{_libdir}/pulseaudio
%dir %{_libdir}/pulse-%{pulseversion}/
%dir %{_libdir}/pulse-%{pulseversion}/modules/
%{_libdir}/pulse-%{pulseversion}/modules/libalsa-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libbluez5-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libcli.so
%{_libdir}/pulse-%{pulseversion}/modules/liboss-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-cli.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-http.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-native.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-simple.so
%{_libdir}/pulse-%{pulseversion}/modules/librtp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-allow-passthrough.so
%{_libdir}/pulse-%{pulseversion}/modules/module-alsa-card.so
%{_libdir}/pulse-%{pulseversion}/modules/module-alsa-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-alsa-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-always-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-always-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-augment-properties.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluetooth-discover.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluetooth-policy.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluez5-device.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluez5-discover.so
%{_libdir}/pulse-%{pulseversion}/modules/module-card-restore.so
%{_libdir}/pulse-%{pulseversion}/modules/module-cli-protocol-tcp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-cli-protocol-unix.so
%{_libdir}/pulse-%{pulseversion}/modules/module-cli.so
%{_libdir}/pulse-%{pulseversion}/modules/module-combine-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-combine.so
%{_libdir}/pulse-%{pulseversion}/modules/module-console-kit.so
%{_libdir}/pulse-%{pulseversion}/modules/module-dbus-protocol.so
%{_libdir}/pulse-%{pulseversion}/modules/module-default-device-restore.so
%{_libdir}/pulse-%{pulseversion}/modules/module-detect.so
%{_libdir}/pulse-%{pulseversion}/modules/module-device-manager.so
%{_libdir}/pulse-%{pulseversion}/modules/module-device-restore.so
%{_libdir}/pulse-%{pulseversion}/modules/module-echo-cancel.so
%{_libdir}/pulse-%{pulseversion}/modules/module-filter-apply.so
%{_libdir}/pulse-%{pulseversion}/modules/module-filter-heuristics.so
%{_libdir}/pulse-%{pulseversion}/modules/module-gsettings.so
%{_libdir}/pulse-%{pulseversion}/modules/module-hal-detect.so
%{_libdir}/pulse-%{pulseversion}/modules/module-http-protocol-tcp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-http-protocol-unix.so
%{_libdir}/pulse-%{pulseversion}/modules/module-intended-roles.so
%{_libdir}/pulse-%{pulseversion}/modules/module-ladspa-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-loopback.so
%{_libdir}/pulse-%{pulseversion}/modules/module-match.so
%{_libdir}/pulse-%{pulseversion}/modules/module-mmkbd-evdev.so
%{_libdir}/pulse-%{pulseversion}/modules/module-native-protocol-fd.so
%{_libdir}/pulse-%{pulseversion}/modules/module-native-protocol-tcp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-native-protocol-unix.so
%{_libdir}/pulse-%{pulseversion}/modules/module-null-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-null-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-oss.so
%{_libdir}/pulse-%{pulseversion}/modules/module-pipe-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-pipe-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-position-event-sounds.so
%{_libdir}/pulse-%{pulseversion}/modules/module-remap-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-remap-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rescue-streams.so
%{_libdir}/pulse-%{pulseversion}/modules/module-role-cork.so
%{_libdir}/pulse-%{pulseversion}/modules/module-role-ducking.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rtp-recv.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rtp-send.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rygel-media-server.so
%{_libdir}/pulse-%{pulseversion}/modules/module-simple-protocol-tcp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-simple-protocol-unix.so
%{_libdir}/pulse-%{pulseversion}/modules/module-sine-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-sine.so
%{_libdir}/pulse-%{pulseversion}/modules/module-stream-restore.so
%{_libdir}/pulse-%{pulseversion}/modules/module-suspend-on-idle.so
%{_libdir}/pulse-%{pulseversion}/modules/module-switch-on-connect.so
%{_libdir}/pulse-%{pulseversion}/modules/module-switch-on-port-available.so
%{_libdir}/pulse-%{pulseversion}/modules/module-systemd-login.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-sink-new.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-source-new.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-udev-detect.so
%{_libdir}/pulse-%{pulseversion}/modules/module-virtual-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-virtual-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-virtual-surround-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-volume-restore.so
%{_libdir}/pulseaudio/*.so
%dir %{_datadir}/pulseaudio
%dir %{_datadir}/pulseaudio/alsa-mixer
%dir %{_datadir}/pulseaudio/alsa-mixer/paths
%dir %{_datadir}/pulseaudio/alsa-mixer/profile-sets
%{_datadir}/pulseaudio/alsa-mixer/paths/*.conf
%{_datadir}/pulseaudio/alsa-mixer/paths/*.common
%{_datadir}/pulseaudio/alsa-mixer/profile-sets/*.conf
%dir %{_libexecdir}/pulse
%{_libexecdir}/pulse/gsettings-helper
%{_datadir}/GConf/gsettings/pulseaudio.convert
%{_datadir}/glib-2.0/schemas/org.freedesktop.pulseaudio.gschema.xml
# system-wide mode
%{_unitdir}/pulseaudio.service
%config %{_sysconfdir}/dbus-1/system.d/pulseaudio-system.conf

%files devel
%defattr(-,root,root,-)
%dir %{_includedir}/pulse
%dir %{_includedir}/pulsecore
%dir %{_includedir}/pulsecore/filter
%dir %{_includedir}/pulsecore/ffmpeg
%{_includedir}/pulse/*.h
%{_includedir}/pulsecore/filter/*.h
%{_includedir}/pulsecore/ffmpeg/*.h
%{_includedir}/pulsecore/*.h
%dir %{_libdir}/cmake/PulseAudio
%{_libdir}/cmake/PulseAudio/*.cmake
%{_libdir}/libpulse-mainloop-glib.so
%{_libdir}/libpulse-simple.so
%{_libdir}/libpulse.so
%{_libdir}/pkgconfig/*.pc
%{_datadir}/vala/vapi/*.deps
%{_datadir}/vala/vapi/*.vapi

%files doc
%defattr(-,root,root,-)
%{_mandir}/man1/p*
%{_mandir}/man5/*.*
%{_docdir}/%{name}-%{version}
