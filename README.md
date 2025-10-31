# dotfiles

my personal dotfiles

<div align="center">
	<img src="./arch.png" width="800" alt="display nvim" />
</div>

## Softwares

- [x] zsh
- [x] xdg-desktop, 
- [x] docker
- [x] openssh
- [x] udiskie
- [x] kitty
- [x] waybar, otf-font-awesome, maple nerd font
- [x] baidunetdisk
- [x] v2raya
- [x] zip, unzip
- [x] fcitx5
- [x] zen-browser-bin: zen browser, a modern browser.
- [x] neofetch
- [x] foxglove
- [x] ghostscript: pdf 转换器
- [x] piliplus-bin: bilibili client
- [x] 4kvideodownloaderplus
- [ ] cava (optional)
- [ ] wluma
- [ ] tmux and tqm
- Audio (choose `pipewire` or `pulseaudio`)
  - [x] pipewire
    - [x] pipewire-audio: 关键组件
    - [x] pipewire-jack: 兼容层，支持需要 JACK 音频服务器
    - [x] wireplumber: 会话管理
    - [x] pwvucontrol: 音量控制工具
    - [x] pipewire-pulse: 代替 pulseaudio 和 pulseaudio-bluetooth
    - [x] pipewire-alsa
  - [ ] pulseaudio
    - [ ] alsa-utils
    - [ ] pavucontrol: PulseAudio Volume Control
    - [ ] pulseaudio-bluetooth
    - [ ] pulseaudio-alsa
    - [ ] pulseaudio
  - Bluetooth
    - [ ] blueman
    - [x] bluez
    - [x] bluez-utils
- notification
  - [ ] dunst
  - [x] mako: notification daemon (replace dunst)
- [x] yazi and Überzug++ (some bug with ueberzugpp in hyprland)
    - [x] ffmpegthumbnailer
    - [x] unar
    - [x] jq
    - [x] poppler
    - [x] fd
    - [x] rg
    - [x] fzf (optional)
    - [x] zoxide (optional)
- Pictures and video
  - [x] photoqt
  - [ ] grim, slurp, feh
  - [x] gpu-screen-recorder(-gtk): Screen recording functionality
- music player
  - [x] mpd, ncmpcpp
  - [x] splayer: a online music player
  - [ ] electron-netease-cloud-music
- work
  - [x] wps-office, ttf-wps-fonts, libtiff5
  - [x] zotero
  - editor
  	- [x] helix
  	- [x] neovim
  	- [x] visual-studio-code
  	- [x] Zed
  	- [ ] neovide (optional)
  	- [ ] lazyvim
  - markdown editor
  	- [x] obsidian
  	- [x] typora
  	- [x] picgo-appimage: A simple & beautiful tool for pictures uploading built by electron-vue
- social apps
  - [x] linuxqq
  - [x] wechat
- AI
  - [x] claude-code: ai coding tui
  - [x] gemini-cli: ai coding tui developed by Google
- [ ] hyprland
  - [ ] hypridle
  - [ ] hyprlock
  - [ ] xdg-desktop-portal-hyprland
- [x] niri
  - [x] xdg-desktop-portal-gtk, xdg-desktop-portal-gnome, gnome-keyring
  - [x] polkit-kde-agent: authentication agent (installed before niri)
  - [x] xwayland-satellite: run X11 apps like Steam or Discord
  - [x] swaylock, swayidle, swaybg
  - [ ] gdm: 登陆管理器
  - [x] cliphist: clipboard history support
- app launcher
  - [ ] fuzzel
  - [x] tofi
- printer
  - [ ] cpus-pdf
  - [ ] system-config-printer
  - [ ] hpuld for HP Laser 150 (printer)


Note: In `./dotfiles/zsh`, `zshrc` is just configuration files for zsh with only three plugins, and `zhsrc_omz` is for `oh-my-zsh`, just pick one to use. Recommend to use `zshrc`.

## Todo

- [x] 配置waybar,激活状态为光带形式。
- [x] limit the length of music bar of waybar

## Setup

### Install dotfiles

edit `install.sh` to include the dotfile you need and then run
```
./install.sh
```

### gdm

设置
```
systemctl enable gdm
```

## Common knownledge

### 连接网络

- 工具：nmcli

列出网络
```bash
nmcli dev wifi list
```

连接网络
```bash
nmcli dev wifi connect host password ****
```

### 更换国内源

打开[archlinux mirror](https://archlinux.org/mirrors/)，找到国内的源，按照下面的格式添加源到 `/etc/pacman.d/mirrorlist` 中：

```
Server = https://mirrors.tuna.tsinghua.edu.cn/archlinux/$repo/os/$arch
```

### 蓝牙连接

安装 `bluez, bluez-utils`.

开启蓝牙：
```bash
bluetooth on
```

进入控制界面：
```bash
bluetoothctl
```

用 `help` 命令查看，基本上就是
1. `scan on`
2. `connect [device]`

### Ghostscript

见[blog](https://www.cnblogs.com/springcoming/p/18796226).

## TroubleShooting

### wireplumber

打开时报错
```bash
wp-event-dispatcher: wp_event_dispatcher_unregister_hook: assertion 'already_registered_dispatcher == self' failed
```

貌似是正常现象。

### obsidian

> [!note]
> 使用 `niri` 按照配置好环境变量 `ELECTRON_OZONE_PLATFORM_HINT "auto"`，就可以了
> 不用下面的操作了

- [无法使用中文](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland#Chromium_.2F_Electron)

无法使用中文，需要使用下面命令打开：
```bash
obsidian --enable-features=UseOzonePlatform --ozone-platform=wayland --enable-wayland-ime
```
