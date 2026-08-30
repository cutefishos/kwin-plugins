# CutefishOS KWin Plugins

Window decoration, animations, window switcher and KWin configuration for
CutefishOS. The plugins target KWin 6 (Plasma 6 / KDecoration3).

## Contents

| Path | Description |
| --- | --- |
| `plugins/decoration` | KDecoration3 window decoration (installed as `org.cutefish.decoration`) |
| `plugins/roundedwindow` | Rounded corners effect (`cutefish_roundedwindow`) |
| `scripts/` | JavaScript effects and scripts (`cutefish_scale`, `cutefish_squash`, `cutefish_popups`, `cutefishlauncher`) |
| `tabbox/` | `cutefish_thumbnail` window switcher layout |
| `config/` | System-wide KWin defaults installed into `/etc/xdg` |

## Dependencies

Debian/Ubuntu:

`sudo apt install cmake extra-cmake-modules qt6-base-dev libkf6coreaddons-dev libkdecorations3-dev kwin-dev kwin-common`

`kwin-common` is required at runtime: it provides the KPackage structures and
the `org.kde.kwin` QML module that KWin uses to load scripted effects, scripts
and window switchers. The window switcher additionally needs FishUI.

## Build

```
mkdir build
cd build
cmake ..
make
sudo make install
```

Build options: `CUTEFISH_BUILD_KWIN6_DECORATION` (ON) and `CUTEFISH_BUILD_KWIN6_EFFECT` (ON).

The rounded corners effect is a binary KWin plugin: KWin provides no stable effect
ABI, so it has to be rebuilt against the KWin version it runs on (KWin refuses to
load a plugin built against a different one).

## Enabling

The defaults in `config/kwinrc` select the decoration
(`[org.kde.kdecoration2] library=org.cutefish.decoration`), the
`cutefish_thumbnail` switcher and the CutefishOS effects. For an existing
user configuration, the same keys have to be set in `~/.config/kwinrc`.

## License

cutefish-kwin-plugins is licensed under GPLv3.
