# falcos-bootc-updates

KDE System Settings module for bootc based systems, built for
[falcos](https://github.com/jayelg/falcos). Shows the booted image version,
whether an update is staged, and offers check, stage, restart now, and
scheduled restart. Updates are only ever staged; applying is always an
explicit reboot.

## Components

- `src/kcm`: QML KCM (kcmutils) that appears in System Settings as
  "System Updates". Talks to the daemon over the system bus, to logind
  for reboot and scheduled reboot, and reads
  `/run/ostree/staged-deployment` for a cheap staged check.
- `src/daemon`: `bootcupdatesd`, a D-Bus activated root service exposing
  `io.github.jayelg.BootcUpdates1` with three methods, `Status`, `Check`,
  and `Stage`, each gated by its own polkit action. It only shells out to
  the bootc CLI and exits when idle.
- `src/notifier`: `bootcupdates-notifier`, a per-session systemd user
  service that watches `/run/ostree/staged-deployment` and raises a
  persistent notification with Restart Now and scheduled restart actions.
  Closing the notification dismisses it; no snooze state. Replaces the
  falcos `update-notify` script.

## Why a custom daemon

bootc has no daemon or D-Bus API (bootc-dev/bootc#558 tracks an
rpm-ostree compatible shim, unresolved). Plasma Discover's rpm-ostree
backend drives rpm-ostreed, and mixing rpm-ostree mutations into a bootc
managed host is discouraged upstream. Neither covers the falcos changelog
flow (image label revisions resolved against GitHub history), so the
privileged surface is kept to a ~200 line wrapper instead.

## Building

The falcos host has no toolchain; builds run in a Fedora 44 container:

```sh
scripts/build.sh        # compile into ./build
scripts/build-rpm.sh    # produce RPMs in ./dist
```

To test on the running system without a rebuild, use a transient overlay
(reverts on reboot):

```sh
sudo bootc usroverlay
sudo rpm -Uvh --force dist/falcos-bootc-updates-*.x86_64.rpm
sudo systemctl daemon-reload && sudo systemctl reload dbus-broker
sudo systemctl restart polkit   # overlay writes do not trigger polkitd's reload
sudo systemctl try-restart bootcupdatesd   # replace a running daemon
```

System Settings compiles the KCM's QML into a per-app cache and does not
notice when a reinstalled plugin ships new QML, so after reinstalling:

```sh
rm -rf ~/.cache/systemsettings/qmlcache
kquitapp6 systemsettings 2>/dev/null || pkill -x systemsettings
systemsettings kcm_bootcupdates
```

## Releasing

Push a `v*` tag. The Release workflow builds the RPM with the version
taken from the tag and attaches it to a GitHub release. Every push to
`main` also builds the RPM as a CI check (workflow artifact only, no
release). falcos installs the release asset by URL pin with a Renovate
annotation (see falcos `build_files/common/frequent/020-custom-apps.sh`).

The tag is the source of truth for the released version. The `Version:`
in the spec is only a default for local and CI builds.

## Roadmap

- Persist the preferred restart time (KConfig).
- Show and cancel an existing scheduled restart from logind state.
- Stretch: per package update management as a GitHub front end (open
  Renovate PRs as pending updates, merge = add to image). Keep the data
  layer abstract so updates can come from the staged diff or open PRs.
