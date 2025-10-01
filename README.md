# dotfiles

<!--toc:start-->
- [dotfiles](#dotfiles)
  - [Softwares](#softwares)
  - [Todo](#todo)
<!--toc:end-->

my personal dotfiles

<div align="center">
	<img src="./arch.png" width="800" alt="display nvim" />
</div>

## Softwares

- [x] dunst
- [x] hyprland
- [x] wluma
- [x] tmux and tqm
- [x] zsh
- [x] pipewire
- [x] wireplumber
- [x] grim, slurp, feh
- [x] xdg-desktop, xdg-desktop-portal-hyprland
- [x] tofi
- [x] docker
- [x] openssh
- [x] bluetooth, blueman, pulseaudio-bluetooth
- [x] cpus-pdf, system-config-printer, hpuld for HP Laser 150 (printer)
- [x] pavucontrol
- [x] wps-office, ttf-wps-fonts, libtiff5
- [x] udiskie
- [x] electron-netease-cloud-music, mpd, ncmpcpp
- [x] waybar, otf-font-awesome, maple nerd font
- [x] baidunetdisk
- [x] v2raya
- [x] neovim
- [x] cava (optional)
- [x] neovide (optional)
- [x] zotero
- [x] visual-studio-code
- [x] zip, unzip
- [x] yazi and Überzug++ (some bug with ueberzugpp in hyprland)
    - [x] ffmpegthumbnailer
    - [x] unar
    - [x] jq
    - [x] poppler
    - [x] fd
    - [x] rg
    - [x] fzf (optional)
    - [x] zoxide (optional)
- [x] helix
- [x] kitty
- [x] obsidian
- [x] fcitx5
- [x] hypridle
- [x] hyprlock
- [x] linuxqq, wechat
- [x] splayer: a music player

Note: In `./dotfiles/zsh`, `zshrc` is just configuration files for zsh with only three plugins, and `zhsrc_omz` is for `oh-my-zsh`, just pick one to use. Recommend to use `zshrc`.

## TroubleShooting

### obsidian

- https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland#Chromium_.2F_Electron

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
