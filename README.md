# dotfiles

my personal dotfiles

<div align="center">
	<img src="./arch.png" width="800" alt="display nvim" />
</div>

## Softwares

- [x] zsh
- [x] pipewire
- [x] wireplumber
- [x] xdg-desktop, 
- [x] docker
- [x] openssh
- [x] bluetooth, blueman, pulseaudio-bluetooth
- [x] pavucontrol
- [x] udiskie
- [x] kitty
- [x] waybar, otf-font-awesome, maple nerd font
- [x] baidunetdisk
- [x] v2raya
- [x] zip, unzip
- [x] fcitx5
- [x] zen-browser-bin: zen browser, a modern browser.
- notification
  - [x] dunst
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
  - [x] grim, slurp, feh
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
- [x] hyprland
  - [x] hypridle
  - [x] hyprlock
  - [x] xdg-desktop-portal-hyprland
- [x] niri
  - [x] xdg-desktop-portal-gtk, xdg-desktop-portal-gnome, gnome-keyring
  - [x] polkit-kde-agent: authentication agent (installed before niri)
  - [x] xwayland-satellite: run X11 apps like Steam or Discord
  - [x] swaylock, swayidle, swaybg
  - [x] gdm
  - [x] cliphist: clipboard history support
- app launcher
  - [x] fuzzel
  - [x] tofi
- printer
    - [ ] cpus-pdf
    - [ ] system-config-printer
    - [ ] hpuld for HP Laser 150 (printer)
- [x] cava (optional)
- [ ] wluma
- [ ] tmux and tqm


Note: In `./dotfiles/zsh`, `zshrc` is just configuration files for zsh with only three plugins, and `zhsrc_omz` is for `oh-my-zsh`, just pick one to use. Recommend to use `zshrc`.

## TroubleShooting

### obsidian

- [无法使用中文](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland#Chromium_.2F_Electron)

无法使用中文，需要使用下面命令打开：
```bash
obsidian --enable-features=UseOzonePlatform --ozone-platform=wayland --enable-wayland-ime
```

## Todo

- [ ] aliyunpan
- [x] limit the length of music bar of waybar

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
