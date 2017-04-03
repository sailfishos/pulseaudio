# Conditional building of X11 related things
%bcond_with X11

Name:       pulseaudio

%define pulseversion 8.0

Summary:    General purpose sound server
Version:    %{pulseversion}
Release:    1
Group:      Multimedia/PulseAudio
License:    LGPLv2+
URL:        http://pulseaudio.org
Source0:    http://freedesktop.org/software/pulseaudio/releases/pulseaudio-%{version}.tar.xz
Source1:    90-pulse.conf
Source2:    pulseaudio.service
# Patches backported from upstream
Patch0:     0001-pulsecore-Don-t-allow-unreferencing-linked-object.patch
Patch1:     0002-dbus-fix-crash-on-LoadModule.patch
Patch2:     0003-dbus-Deal-with-double-counting-module-dbus-protocol.patch
# Patches specific to Mer version
Patch3:     1001-core-make-dependencies-compile-for-64bit.patch
Patch4:     1002-build-Install-pulsecore-headers.patch
Patch5:     1003-daemon-Disable-automatic-shutdown-by-default.patch
Patch6:     1004-daemon-Set-default-resampler-to-speex-fixed-2.patch
Patch7:     1005-client-Disable-client-autospawn-by-default.patch
Patch8:     1006-bluez4-device-Allow-leaving-transport-running-while-.patch
Patch9:     1007-bluez4-device-Do-not-lose-transport-pointer-after-ge.patch
Patch10:    1008-bluez4-device-Default-to-using-A2DP-profile-initiall.patch
Patch11:    1009-bluez4-util-Detect-transport-acquire-release-loop.patch
Patch12:    1010-bluez4-device-Fix-assert-when-source-doesn-t-exist-w.patch
Patch13:    1011-bluez4-device-Don-t-acquire-transport-when-connectin.patch
Patch14:    1012-bluez4-device-Don-t-re-add-sink-or-source.patch
Patch15:    1013-loopback-Add-option-to-reset-queues-on-attach.patch
Patch16:    1014-loopback-reset-queues-after-volume-was-muted.patch
Patch17:    1015-bluez5-add-droid-backend.patch
Patch18:    1016-build-include-droid-backend.patch
Patch19:    1017-bluez5-droid-add-ofono-backend-as-base.patch
Patch20:    1018-bluez5-droid-add-backend-that-only-acquires-or-relea.patch
Patch21:    1019-bluetooth-discover-Handle-arguments-for-both-bluez-v.patch
Patch22:    1020-bluez4-Missing-bluez4-is-not-error.patch
Patch23:    1021-bluez5-Missing-bluez5-is-not-error.patch
Patch24:    1022-bluez5-droid-Add-call-volume-control.patch
# Patches pending inclusion to upstream
Patch25:    2001-module-rescue-streams-Add-parameters-to-define-defau.patch
Patch26:    2002-combine-sink-Add-argument-for-ignoring-sinks-in-auto.patch
Patch27:    2003-loopback-Prevent-crash-when-unloading-module-after-s.patch
Patch28:    2004-loopback-Reset-sink-input-rate-when-queue-is-empty.patch
Requires:   udev
Requires:   libsbc >= 1.0
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(alsa) >= 1.0.24
BuildRequires:  pkgconfig(dbus-1) >= 1.4.12
BuildRequires:  pkgconfig(glib-2.0) >= 2.4.0
BuildRequires:  pkgconfig(json-c) >= 0.12
BuildRequires:  pkgconfig(libasyncns) >= 0.1
BuildRequires:  pkgconfig(libudev) >= 143
BuildRequires:  pkgconfig(orc-0.4) >= 0.4.11
BuildRequires:  pkgconfig(sndfile) >= 1.0.20
BuildRequires:  pkgconfig(speexdsp) >= 1.2
BuildRequires:  pkgconfig(atomic_ops)
BuildRequires:  pkgconfig(sbc) >= 1.0
%if %{with X11}
BuildRequires:  pkgconfig(ice)
BuildRequires:  pkgconfig(sm)
BuildRequires:  pkgconfig(x11-xcb)
BuildRequires:  pkgconfig(xcb) >= 1.6
BuildRequires:  pkgconfig(xtst)
%endif
BuildRequires:  intltool
BuildRequires:  libcap-devel
BuildRequires:  libtool >= 2.4
BuildRequires:  libtool-ltdl-devel
BuildRequires:  fdupes
BuildRequires:  systemd-libs
BuildRequires:  systemd-devel

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

%package module-x11
Summary:    PulseAudio components needed for starting x11 User session
Group:      Multimedia/PulseAudio
Requires:   %{name} = %{version}-%{release}
Requires:   /bin/sed

%description module-x11
Description: %{summary}

%package devel
Summary:    PulseAudio Development headers and libraries
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Description: %{summary}

%package esound
Summary:    ESound compatibility
Group:      Multimedia/PulseAudio
Requires:   %{name} = %{version}-%{release}

%description esound
Makes PulseAudio a drop-in replacement for ESound.

%prep
%setup -q -n %{name}-%{version}/pulseaudio

# Patches backported from upstream
%patch0 -p1
%patch1 -p1
%patch2 -p1
# Patches specific to Mer version
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
%patch11 -p1
%patch12 -p1
%patch13 -p1
%patch14 -p1
%patch15 -p1
%patch16 -p1
%patch17 -p1
%patch18 -p1
%patch19 -p1
%patch20 -p1
%patch21 -p1
%patch22 -p1
%patch23 -p1
%patch24 -p1
# Patches pending inclusion to upstream
%patch25 -p1
%patch26 -p1
%patch27 -p1
%patch28 -p1

%build
echo "%{pulseversion}" > .tarball-version
NOCONFIGURE=1 ./bootstrap.sh

%ifarch %{arm}
export CFLAGS="$CFLAGS -mfpu=neon"
export CXXFLAGS="$CXXFLAGS -mfpu=neon"
%endif

%if %{with X11}
%configure --disable-static \
    --disable-openssl \
%ifarch %{arm}
    --enable-neon-opt \
%endif
    --disable-gconf
%else
%configure --disable-static \
    --disable-openssl \
%ifarch %{arm}
    --enable-neon-opt \
%endif
    --disable-gconf \
    --disable-x11
%endif

make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install

install -d %{buildroot}/etc/security/limits.d
cp -a %{SOURCE1} %{buildroot}/etc/security/limits.d
install -d %{buildroot}/usr/lib/systemd/user
cp -a %{SOURCE2} %{buildroot}/usr/lib/systemd/user
mkdir -p %{buildroot}/usr/lib/systemd/user/user-session.target.wants
ln -s ../pulseaudio.service %{buildroot}/usr/lib/systemd/user/user-session.target.wants/

%find_lang pulseaudio

%fdupes  %{buildroot}/%{_datadir}
%fdupes  %{buildroot}/%{_includedir}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f pulseaudio.lang
%defattr(-,root,root,-)
%doc GPL LGPL LICENSE README
%doc %{_mandir}/man1/pacat.1.gz
%doc %{_mandir}/man1/pacmd.1.gz
%doc %{_mandir}/man1/pactl.1.gz
%doc %{_mandir}/man1/padsp.1.gz
%doc %{_mandir}/man1/paplay.1.gz
%doc %{_mandir}/man1/pamon.1.gz
%doc %{_mandir}/man1/parec.1.gz
%doc %{_mandir}/man1/parecord.1.gz
%doc %{_mandir}/man1/pasuspender.1.gz
%doc %{_mandir}/man1/pulseaudio.1.gz
%doc %{_mandir}/man5/*.5.gz
%if ! %{with X11}
%exclude %{_mandir}/man1/start-pulseaudio-x11.1.gz
%exclude %{_mandir}/man1/pax11publish.1.gz
%endif
%exclude %{_sysconfdir}/dbus-1/system.d/pulseaudio-system.conf
%config(noreplace) %{_sysconfdir}/pulse/*.conf
%config(noreplace) %{_sysconfdir}/pulse/*.pa
%config(noreplace) %{_sysconfdir}/security/limits.d/90-pulse.conf
%{_datadir}/bash-completion/completions/*
%{_datadir}/zsh/site-functions/_pulseaudio
%{_libdir}/systemd/user/pulseaudio.socket
%dir %{_sysconfdir}/pulse
%{_libdir}/systemd/user/pulseaudio.service
%{_libdir}/systemd/user/user-session.target.wants/pulseaudio.service
/lib/udev/rules.d/90-pulseaudio.rules
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
%{_libdir}/*.so.*
%dir %{_libdir}/pulseaudio
%dir %{_libdir}/pulse-%{pulseversion}/
%dir %{_libdir}/pulse-%{pulseversion}/modules/
%{_libdir}/pulse-%{pulseversion}/modules/libalsa-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libbluez4-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libbluez5-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libcli.so
%{_libdir}/pulse-%{pulseversion}/modules/liboss-util.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-cli.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-http.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-native.so
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-simple.so
%{_libdir}/pulse-%{pulseversion}/modules/librtp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-alsa-card.so
%{_libdir}/pulse-%{pulseversion}/modules/module-alsa-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-alsa-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-always-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-augment-properties.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluez4-device.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluez4-discover.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluez5-device.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluez5-discover.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluetooth-discover.so
%{_libdir}/pulse-%{pulseversion}/modules/module-bluetooth-policy.so
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
%{_libdir}/pulse-%{pulseversion}/modules/module-esound-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-filter-apply.so
%{_libdir}/pulse-%{pulseversion}/modules/module-filter-heuristics.so
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
%{_libdir}/pulse-%{pulseversion}/modules/module-rescue-streams.so
%{_libdir}/pulse-%{pulseversion}/modules/module-role-cork.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rtp-recv.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rtp-send.so
%{_libdir}/pulse-%{pulseversion}/modules/module-rygel-media-server.so
%{_libdir}/pulse-%{pulseversion}/modules/module-simple-protocol-tcp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-simple-protocol-unix.so
%{_libdir}/pulse-%{pulseversion}/modules/module-sine-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-sine.so
%{_libdir}/pulse-%{pulseversion}/modules/module-stream-restore.so
%{_libdir}/pulse-%{pulseversion}/modules/module-suspend-on-idle.so
%{_libdir}/pulse-%{pulseversion}/modules/module-systemd-login.so
%{_libdir}/pulse-%{pulseversion}/modules/module-switch-on-port-available.so
%{_libdir}/pulse-%{pulseversion}/modules/module-switch-on-connect.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-sink-new.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-tunnel-source-new.so
%{_libdir}/pulse-%{pulseversion}/modules/module-udev-detect.so
%{_libdir}/pulse-%{pulseversion}/modules/module-virtual-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-virtual-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-virtual-surround-sink.so
%{_libdir}/pulse-%{pulseversion}/modules/module-volume-restore.so
%{_libdir}/pulse-%{pulseversion}/modules/module-remap-source.so
%{_libdir}/pulse-%{pulseversion}/modules/module-role-ducking.so
%{_libdir}/pulseaudio/*.so
%dir %{_datadir}/pulseaudio
%dir %{_datadir}/pulseaudio/alsa-mixer
%dir %{_datadir}/pulseaudio/alsa-mixer/paths
%dir %{_datadir}/pulseaudio/alsa-mixer/profile-sets
%{_datadir}/pulseaudio/alsa-mixer/paths/*.conf
%{_datadir}/pulseaudio/alsa-mixer/paths/*.common
%{_datadir}/pulseaudio/alsa-mixer/profile-sets/*.conf

%if %{with X11}
%files module-x11
%defattr(-,root,root,-)
%doc %{_mandir}/man1/pax11publish.1.gz
%doc %{_mandir}/man1/start-pulseaudio-x11.1.gz
%config %{_sysconfdir}/xdg/autostart/pulseaudio.desktop
%{_bindir}/pax11publish
%{_bindir}/start-pulseaudio-x11
%{_libdir}/pulse-%{pulseversion}/modules/module-x11-bell.so
%{_libdir}/pulse-%{pulseversion}/modules/module-x11-cork-request.so
%{_libdir}/pulse-%{pulseversion}/modules/module-x11-publish.so
%{_libdir}/pulse-%{pulseversion}/modules/module-x11-xsmp.so
%endif

%files devel
%defattr(-,root,root,-)
%dir %{_includedir}/pulse
%dir %{_includedir}/pulsecore
%dir %{_includedir}/pulsecore/filter
%{_includedir}/pulse/*.h
%{_includedir}/pulsecore/filter/*.h
%{_includedir}/pulsecore/*.h
%dir %{_libdir}/cmake/PulseAudio
%{_libdir}/cmake/PulseAudio/*.cmake
%{_libdir}/libpulse-mainloop-glib.so
%{_libdir}/libpulse-simple.so
%{_libdir}/libpulse.so
%{_libdir}/pkgconfig/*.pc
%{_datadir}/vala/vapi/*.deps
%{_datadir}/vala/vapi/*.vapi

%files esound
%defattr(-,root,root,-)
%doc %{_mandir}/man1/esdcompat.1.gz
%{_bindir}/esdcompat
%{_libdir}/pulse-%{pulseversion}/modules/libprotocol-esound.so
%{_libdir}/pulse-%{pulseversion}/modules/module-esound-compat-spawnfd.so
%{_libdir}/pulse-%{pulseversion}/modules/module-esound-compat-spawnpid.so
%{_libdir}/pulse-%{pulseversion}/modules/module-esound-protocol-tcp.so
%{_libdir}/pulse-%{pulseversion}/modules/module-esound-protocol-unix.so
