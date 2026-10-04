# Install and configure this setup on a fresh Arch machine

The runbook for rebuilding the desktop in this repo on a **blank** computer,
without `archinstall`:

- **Part A** installs Arch Linux by hand from the live ISO: boot → partition →
  pacstrap → chroot → GRUB → reboot.
- **Parts B–F** install the packages and wire up everything `dotfiles/` encodes.
- **Part G** is what git cannot carry: secrets, extra repos, licensed apps.
- **Parts H–I** verify the result and list the known gaps.

It is written to be followed by a person *or* an agent, on hardware that is not
this laptop. Machine-specific values appear as `/dev/nvme1n1p5`-style literals in
the install commands (gathered into shell variables in A.2) and are catalogued
in [What you must adapt](#what-you-must-adapt); everything else is copy-paste.
The exact partition table, hostname, GPU and panel below belong to *this*
machine, so substitute yours — the parts that matter are structural.

[README.md](README.md) stays the software wish-list and day-to-day notes.

> **Part A erases disks.** The layout in A.2 is this machine's, not a
> requirement. Read A.2 and run `lsblk -f` before typing anything destructive.

## Target state

An Arch box that boots to a tty (no display manager) where logging in and
running `niri-session` brings up niri with waybar, mako, the swaybg/swayidle
user units, fcitx5 + Maple Mono, PipeWire audio, TLP, and the personal apps —
all driven by symlinks from `~/dotfiles` into `$HOME`.

### Reference machine

Read off the live system on 2026-10-04. Treat it as the worked example, not as
requirements.

| | |
|---|---|
| Hardware | Lenovo Legion Y9000P 2021H (82JD), i7-11800H (Intel), NVIDIA GA106M / RTX 3060 Mobile |
| Display | internal `eDP-1`, `2560x1600@165.004` |
| Disk | two NVMe drives. Arch on `nvme1n1`: `p3` 600M vfat ESP → `/boot`, `p4` 16 GiB swap, `p5` 428G ext4 → `/`, plus NTFS `p2` (`Work`). `nvme0n1` holds Windows (`Windows-SSD`, `SYSTEM_DRV`, `WINRE_DRV`). |
| Boot | UEFI, GRUB installed to `/boot`, `os-prober` enabled for the Windows drive |
| OS | Arch Linux (rolling), kernel `linux` (7.2.8-arch1-2) + `intel-ucode` |
| Locale / TZ | `en_US.UTF-8`, `Asia/Shanghai`, RTC in UTC, hostname `chunfeng` |
| User | `chun` (uid 1000), shell `/usr/bin/zsh`, groups `wheel input lp video docker` |
| Session | no display manager: log in on tty1 and run `niri-session` |

### What you must adapt

| Value | This machine | Where |
|---|---|---|
| Install disk, existing OSes | `nvme1n1` (Arch) + `nvme0n1` (Windows) | A.2 |
| ESP / swap / root | `nvme1n1p3` / `p4` / `p5` | A.2, A.3 |
| CPU microcode | `intel-ucode` (Intel) | A.4 |
| GPU driver set | NVIDIA → `nvidia-open-dkms`, `libva-nvidia-driver-git` | B.4, E.2 |
| Hostname | `chunfeng` | A.6 |
| Username / uid | `chun` / 1000 | A.6 |
| Timezone / locale | `Asia/Shanghai` / `en_US.UTF-8` | A.6 |
| Panel name + mode | `eDP-1`, `2560x1600@165.004` | `dotfiles/niri/config.kdl` (E.3) |
| Keyboard layout / options | `us`, `caps:swapescape` | `dotfiles/niri/config.kdl` |
| Touchpad preferences | tap, natural-scroll, dwt | `dotfiles/niri/config.kdl` |
| Personal apps, `solaar`, `usbip` | installed here | B.4 (drop what you don't need) |
| Repo location | `~/dotfiles` (required, see C) | C |
| Secrets, nvim repo, MCP servers | not in this repo | G |

## Part A — Install Arch Linux from the live ISO (manual)

No `archinstall`; this is the classic wiki path.

### A.1 Boot the ISO and get online

Get the current ISO from <https://archlinux.org/download/> and write it to a USB
stick (or use Ventoy). The boot menu on the Legion is `F12`.

```bash
dd bs=4M if=archlinux.iso of=/dev/sdX status=progress oflag=sync   # <-- your USB device
```

Confirm you booted in UEFI mode — this must print a number. If the file is
missing you are in BIOS/CSM mode: turn CSM off and reboot the stick.

```bash
cat /sys/firmware/efi/fw_platform_size     # 64 or 32
```

Console keyboard, if you are not on US: `loadkeys de-latin1`.

Wired networking gets DHCP automatically; Wi-Fi goes through `iwctl`:

```bash
ip a                        # find the interface name
iwctl                       # then:  device list / station wlan0 connect "YourSSID" / station wlan0 show
ping -c3 archlinux.org
```

Clock (TLS and package signatures need it) and a mirror list — this machine uses
the TUNA mirror first, as README describes:

```bash
timedatectl set-ntp true && timedatectl status
reflector --latest 20 --sort rate --save /etc/pacman.d/mirrorlist
# ...or, to match this machine:  reflector --country China --latest 20 --sort rate --save ...
```

### A.2 Partition

> **Destructive.** A partition table rewrite wipes the disk. Check the device
> name first with `lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT`, and never point
> these commands at a disk holding data you want to keep.

Set the device names once; every later command uses these variables:

```bash
DISK=/dev/nvme1n1     # <-- the disk Arch will live on
ESP=/dev/nvme1n1p3    # <-- <DISK>p3 on nvme/mmc, <DISK>3 on sata
SWAP=/dev/nvme1n1p4
ROOT=/dev/nvme1n1p5
```

The layout this machine uses (UEFI, no LUKS, no LVM):

| Partition | Size | gdisk type | Mount | Notes |
|---|---|---|---|---|
| `ESP` | 600M | `ef00` EFI System | `/boot` | vfat; GRUB, kernel, initramfs and microcode live here |
| `SWAP` | 16 GiB | `8200` Linux swap | `[SWAP]` | sized to RAM |
| `ROOT` | rest | `8300` Linux filesystem | `/` | ext4 |

Interactive: `cfdisk "$DISK"` → `New` 600M `EFI System` → `New` 16G `Linux swap`
→ `New` rest `Linux filesystem` → `Write` → `Quit`.

Scripted equivalent for a **blank** disk of that shape (the numbers start at 3
because the reference disk already carries `p1`/`p2` from the Windows install;
on a genuinely empty disk use `-n 1`/`-n 2`/`-n 3` and re-point the variables
above):

```bash
sgdisk --zap-all "$DISK"
sgdisk -n 3:0:+600M -t 3:ef00 -c 3:"ESP" \
       -n 4:0:+16G  -t 4:8200 -c 4:"swap" \
       -n 5:0:0     -t 5:8300 -c 5:"root" "$DISK"
partprobe "$DISK"
```

Installing **alongside Windows** (as here): zap nothing. Reuse the existing ESP
— create only swap and root, and never `mkfs.fat` a shared ESP. A vfat ESP must
be at least 300M; 600M leaves room for a second kernel and a fallback image.

### A.3 Format, mount, enable swap

```bash
mkfs.fat -F32 "$ESP"      # SKIP if you are reusing a shared ESP
mkswap "$SWAP"
mkfs.ext4 "$ROOT"
mount "$ROOT" /mnt
mount --mkdir "$ESP" /mnt/boot
swapon "$SWAP"
lsblk -f                  # sanity check before writing anything else
```

### A.4 pacstrap the base system

```bash
pacstrap -K /mnt \
  base base-devel linux linux-firmware intel-ucode \
  networkmanager sudo vim zsh git openssh man-db dialog \
  grub efibootmgr os-prober
```

- Swap `intel-ucode` for `amd-ucode` on AMD.
- `git` and `openssh` are only transitive dependencies on the reference machine
  (`git` of `paru`, `openssh` of `gcr`), so they are installed explicitly here:
  Part C clones this repo, and Part D enables `sshd.service`.
- `linux` is the rolling kernel; add `linux-lts` as a fallback if you want one
  (~100M more on the ESP). This machine has none.
- No GPU driver here on purpose — that choice is B.4, because it is the one that
  really depends on the hardware.

### A.5 fstab

```bash
genfstab -U /mnt >> /mnt/etc/fstab
cat /mnt/etc/fstab      # must list /, /boot and swap, and nothing stale
```

### A.6 Configure the chroot

```bash
arch-chroot /mnt
```

```bash
# time
ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime      # <-- your zone
hwclock --systohc                                            # RTC kept in UTC

# locale
sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen        # <-- your locale
locale-gen
echo 'LANG=en_US.UTF-8' > /etc/locale.conf                   # <-- your locale

# hostname and the local hosts entry this machine carries
echo chunfeng > /etc/hostname                               # <-- your hostname
cat > /etc/hosts <<'EOF'
127.0.0.1   localhost
::1         localhost
127.0.0.1   chunfeng.localdomain chunfeng
EOF

# console keymap
echo 'KEYMAP=us' > /etc/vconsole.conf

# root
passwd

# user -- docker's group does not exist yet, so it is added in B.2
useradd -m -G wheel,input,lp,video -s /usr/bin/zsh chun       # <-- your user
passwd chun
echo '%wheel ALL=(ALL:ALL) ALL' > /etc/sudoers.d/10-wheel
chmod 440 /etc/sudoers.d/10-wheel

# rebuild the initramfs (the linux install hook does this too; harmless)
mkinitcpio -P
exit
```

### A.7 Bootloader (GRUB on UEFI)

```bash
arch-chroot /mnt
```

Set the non-default options in `/etc/default/grub` (the rest of the file can stay
as shipped):

```ini
GRUB_DEFAULT=0
GRUB_TIMEOUT=5
GRUB_TIMEOUT_STYLE=menu
GRUB_DISTRIBUTOR="Arch"
GRUB_PRELOAD_MODULES="part_gpt part_msdos"
GRUB_TERMINAL_INPUT=console
GRUB_GFXMODE=auto
GRUB_GFXPAYLOAD_LINUX=keep
GRUB_DISABLE_RECOVERY=true
GRUB_DISABLE_OS_PROBER=false
# NVIDIA only. On Intel/AMD leave the shipped "loglevel=3 quiet" instead,
# and see E.2 for the matching mkinitcpio change.
GRUB_CMDLINE_LINUX_DEFAULT="nvidia_drm.modeset=1 nvidia.NVreg_PreserveVideoMemoryAllocations=1"
```

```bash
grub-install --target=x86_64-efi --efi-directory=/boot --bootloader-id=GRUB
grub-mkconfig -o /boot/grub/grub.cfg
exit
```

`grub-mkconfig` output must list your `vmlinuz-linux`, and on this machine also a
Windows entry (found by `os-prober` on the second NVMe). If the firmware refuses
the new entry later, re-run `grub-install` with `--removable`; this machine does
not need it (`efibootmgr -v` shows `Boot0004* grub`).

### A.8 First boot

```bash
umount -R /mnt
reboot        # remove the USB stick
```

Log in as `root`, then bring up networking and continue with Part B:

```bash
systemctl enable --now NetworkManager
nmcli device status
nmcli device wifi list && nmcli device wifi connect "YourSSID" password '…'
timedatectl set-ntp true
```

`systemd-timesyncd` is enabled by default. Optional extras this machine does
**not** use: `systemctl enable fstrim.timer` (SSD trim), and hibernation — swap
exists but there is no `resume=` kernel parameter, so suspend-to-disk would not
resume.

niri needs a polkit authentication agent; that is why `polkit-kde-agent` is in
Part B and `plasma-polkit-agent.service` in Part D.

## Part B — Repositories, packages, hardware choices

Everything from here on runs on the installed system (as your user, via `sudo`).

### B.1 Repositories and the AUR helper

`/etc/pacman.conf` — uncomment `[multilib]` (Steam), and add the `archlinuxcn`
repo from the TUNA mirror:

```ini
[multilib]
Include = /etc/pacman.d/mirrorlist

[archlinuxcn]
Server = https://mirrors.tuna.tsinghua.edu.cn/archlinuxcn/$arch
```

```bash
# If this fails with "invalid or corrupted package (PGP signature)", add
#   SigLevel = Optional TrustAll
# to the [archlinuxcn] section for this one command, install the keyring, then
# remove the override again.
sudo pacman -Syu                               # sync + full upgrade; never partial-upgrade
sudo pacman -S --needed archlinuxcn-keyring    # trust the repo's signing key
sudo pacman -S --needed paru
```

`paru` is used with `Devel` enabled (`/etc/paru.conf`) so `-git` packages
rebuild from upstream.

### B.2 Explicit packages from the configured repos (109)

Mostly `extra`; `core` = `base base-devel efibootmgr gcc-fortran grub linux
linux-firmware linux-headers man-db sudo`, `multilib` = `steam`, and
`archlinuxcn` = `ampcode archlinuxcn-keyring libtiff5 paru tofi-git
ttf-maplemono-nf-cn yazi-git zotero-bin zsh-theme-powerlevel10k-git`.

```bash
sudo pacman -S --needed \
  ampcode archlinuxcn-keyring base base-devel bluez bluez-utils cliphist dialog docker \
  efibootmgr fastfetch fcitx5 fcitx5-chinese-addons fcitx5-configtool fcitx5-gtk \
  fcitx5-pinyin-zhwiki fcitx5-qt fd ffmpegthumbnailer fzf gcc-fortran gnome-keyring \
  gpu-screen-recorder grub htop intel-ucode jq kitty libnotify libtiff5 libva linux \
  linux-firmware linux-headers lua-language-server luarocks mako man-db mesa-utils mpd \
  ncmpcpp neovim networkmanager niri noto-fonts-cjk noto-fonts-emoji npm ntp \
  nvidia-container-toolkit nvidia-open-dkms nvidia-settings obsidian octave openai-codex \
  os-prober otf-font-awesome paru pipewire pipewire-alsa pipewire-audio pipewire-jack \
  pipewire-pulse polkit-kde-agent poppler pyright qt5-wayland qt5ct qt6-wayland ripgrep \
  rustup shotcut socat solaar steam sudo swaybg swayidle swaylock timeshift tinymist tlp \
  tlp-rdw tlpui tofi-git tree-sitter-cli ttf-maplemono-nf-cn typst udiskie ueberzugpp \
  unarchiver unzip usbip uv vim waybar wireplumber xdg-desktop-portal-gnome \
  xdg-desktop-portal-gtk xdg-user-dirs xorg-xauth xorg-xev xorg-xhost \
  xwayland-satellite yazi-git zip zotero-bin zoxide zsh zsh-theme-powerlevel10k-git
```

Why the non-obvious ones are here: `cliphist` + `tofi-git` (niri clipboard and
launcher), `xwayland-satellite` (Steam/QQ and other X11 apps), `udiskie`
(spawned by niri), `qt5ct` + `qt{5,6}-wayland` (Qt apps under Wayland), `socat`
and `usbip` (USB/IP), `timeshift` (also pulls in `cronie`),
`gpu-screen-recorder` (screen recording), `gnome-keyring` +
`xdg-desktop-portal-{gtk,gnome}` (niri session portals), `ttf-maplemono-nf-cn`
(terminal font, Part F), `noto-fonts-*` (CJK/emoji), `zsh-*` plugins and p10k
(Part F).

The `docker` package creates the `docker` group, which is why A.6 left it out —
add yourself and re-login (then Part D enables `docker.service`):

```bash
sudo usermod -aG docker chun      # <-- your user
```

### B.3 Explicit AUR packages (25)

```bash
paru -S --needed \
  4kvideodownloaderplus baidunetdisk-bin brightnessctl-git cc-switch-bin claude-code \
  deepseek-harness-bin foxglove-bin google-chrome gpu-screen-recorder-gtk katex \
  lazygit-git libva-nvidia-driver-git linuxqq mathematica-light photoqt picgo-appimage \
  pwvucontrol splayer ttf-wps-fonts v2raya-bin wechat-universal-bwrap wps-office \
  zen-browser-bin zsh-autosuggestions-git zsh-fast-syntax-highlighting-git
```

The AUR also pulls a set of dependency-only packages
(`gtkmm js102 js115 lib32-* libtermkey libwireplumber-4.0-compat litehtml0.9
python-async-timeout python-pkg_resources bridge-utils`); `paru` resolves those.
Note `brightnessctl-git` (brightness keys in niri) is AUR despite replacing a
repo package, and `libva-nvidia-driver-git` is the AUR `-git` build of the
NVIDIA VA-API driver.

### B.4 What to swap or drop for your hardware

Two groups in the lists above exist only because of this laptop or its owner's
apps. Delete the lines you don't need — nothing else depends on them.

The GPU is the one choice that really matters:

| GPU | Install | Notes |
|---|---|---|
| NVIDIA, Turing or newer (this machine: GA106M) | `nvidia-open-dkms nvidia-settings nvidia-container-toolkit nvidia-utils` | `nvidia-utils` arrives as a dependency; add `libva-nvidia-driver-git` (AUR) and do E.2 |
| NVIDIA, pre-Turing | `nvidia-dkms` with the same userspace packages | same E.2 wiring |
| AMD | `mesa vulkan-radeon libva-mesa-driver` | skip E.2 entirely |
| Intel | `mesa vulkan-intel intel-media-driver` | skip E.2 entirely |

Do **not** install `nvidia-open-dkms`, the NVIDIA kernel parameters or the
mkinitcpio `MODULES` line on a machine without a supported NVIDIA card.

Hardware- or owner-specific packages elsewhere in B.2/B.3 — keep only what
applies to you:

- Laptop power/backlight: `tlp`, `tlp-rdw`, `tlpui`, `brightnessctl-git`
  (niri's brightness keys call it), `usbip` (only for the USB/IP setup here).
- Logitech peripherals: `solaar`.
- NVIDIA extras: `nvidia-*`, `libva-nvidia-driver-git`,
  `nvidia-container-toolkit` (Docker GPU access). `gpu-screen-recorder{,-gtk}`
  works on AMD and Intel too.
- Personal apps, safe to omit: `steam`, `mathematica-light`, `octave` +
  `gcc-fortran`, `shotcut`, `foxglove-bin`, `photoqt`, `4kvideodownloaderplus`,
  `baidunetdisk-bin`, `linuxqq`, `wechat-universal-bwrap`, `wps-office` +
  `ttf-wps-fonts`, `picgo-appimage`, `splayer`, `google-chrome`,
  `zen-browser-bin`, `v2raya-bin`, `cc-switch-bin`, `claude-code`,
  `deepseek-harness-bin`, `ampcode`, `openai-codex`, `katex`, `timeshift`
  (which is what drags in `cronie`).
- Development tooling: `neovim`, `pyright`, `lua-language-server`, `rustup`,
  `npm`, `uv`, `luarocks`, `tree-sitter-cli`, `typst`, `tinymist`, `lazygit-git`.

Printing is **not** configured on this machine: no `cups` daemon, no printer
drivers, no `system-config-printer`. `libcups` and `avahi` are present only as
dependencies of other packages, so do not read them as a working print setup.

## Part C — Dotfiles

```bash
git clone https://github.com/peakyet/dotfiles.git ~/dotfiles
cd ~/dotfiles
./install.sh          # skip anything that already exists
./install.sh force    # overwrite existing files/symlinks
```

How `install.sh` maps things:

| in `dotfiles/` | becomes |
|---|---|
| a directory (e.g. `kitty/`) | `~/.config/<name>` |
| a plain file (e.g. `bashrc`) | `~/.<name>` |
| `zsh/` contents | `~/.zshrc`, `~/.p10k.zsh`, … |
| `claude/` contents | individual files under `~/.claude/` |
| `agents/` | `~/.agents` (whole directory) |

Caveats worth knowing before you run it:

- The repo **must** be reachable as `~/dotfiles`, because the wallpapers and the
  systemd units reference `%h/dotfiles/wallpapers/...`. If you clone elsewhere,
  the script creates `~/dotfiles -> <repo>` for you (and only warns if that name
  is taken).
- `excludeConfigs` in `install.sh` skips `joshuto`, `tmux.conf`, `ueberzugpp`,
  `readme.md`, `README.md`. Those files stay in the repo but are not linked.
- Runtime junk is gitignored on purpose — `dotfiles/mpd/{database,log,sticker.sql}`,
  `dotfiles/ncmpcpp/error.log`, `dotfiles/helix/runtime/`. Do not commit them.

This repo deliberately manages only what lives under `dotfiles/`. It does not
touch `~/.config/nvim`, `~/.bash_profile`, `~/.env`, `~/.claude.json` or
`~/.codex/config.toml`; Part G covers those.

## Part D — Services and the session

### D.1 System units

On a fresh install nothing is enabled yet, so this command is required (on the
reference machine it is simply the current state):

```bash
sudo systemctl enable bluetooth cronie docker lm_sensors \
  NetworkManager NetworkManager-dispatcher NetworkManager-wait-online \
  sshd systemd-timesyncd tlp v2raya
```

Everything else under `/etc/systemd/system` is enabled by the packages
themselves: `remote-fs.target` and `getty@.service`, plus on the user side
`pipewire.socket` / `pipewire-pulse.socket`, `wireplumber`, `xdg-user-dirs`,
`gnome-keyring-daemon.socket` and `p11-kit-server.socket`.

`cronie` gets there as a dependency of `timeshift`, and enabling it is what runs
timeshift's hourly check (`/etc/cron.d/timeshift-hourly`). `sshd` is only worth
enabling if you actually want to log in remotely — drop it otherwise.

### D.2 User units and how the session starts

`dotfiles/niri/add_wants.sh` links the two custom units into
`~/.config/systemd/user/` and wires them into the niri session. `install.sh`
runs it automatically at the end; to run it by hand after cloning:

```bash
sh ~/dotfiles/dotfiles/niri/add_wants.sh
```

It links `swaybg.service` / `swayidle.service` and calls
`systemctl --user add-wants niri.service …` for `waybar mako mpd v2raya-lite
plasma-polkit-agent swaybg swayidle`. The two custom units must go through this
script because they do not exist in `/usr/lib/systemd/user` and have to be
linked into `~/.config/systemd/user` first. On this machine those same seven
units are additionally `systemctl --user enable`d, i.e. started via
`default.target` as well:

```bash
systemctl --user enable waybar mako mpd v2raya-lite plasma-polkit-agent \
  swaybg swayidle
```

There is **no display manager** (no gdm/greetd/sddm installed). Boot ends on a
tty; logging in and running `niri-session` starts `niri.service`, and
`graphical-session.target` then drags in the portals, `gnome-keyring`,
`pipewire`/`wireplumber`, `at-spi`, `gvfs` and the seven units above.

Nothing starts niri for you on this machine — `niri-session` is typed by hand.
To automate it, add this to `~/.zprofile` (which this repo does not manage):

```bash
# optional: start niri automatically on the first tty
[[ -z $DISPLAY && ${XDG_VTNR:-0} -eq 1 ]] && exec niri-session
```

### D.3 Audio, Bluetooth, power

Audio is plain PipeWire (`pipewire`, `pipewire-pulse`, `pipewire-alsa`,
`pipewire-jack`, `wireplumber`) with the sockets enabled by the packages. niri's
volume keys use `wpctl`; `pwvucontrol` is the GUI.

Bluetooth is `bluez` + `bluez-utils` with `bluetooth.service` enabled (no
blueman): `bluetoothctl` → `scan on` → `pair`/`connect`. The waybar Bluetooth
module toggles the radio through its own click action.

Power management is `tlp` + `tlp-rdw` with `tlp.service` enabled and `tlpui` as
the GUI. Before a long command, `awake <cmd>` (defined in both the zsh and bash
configs) inhibits sleep and lets niri keep the screen on through
`dotfiles/niri/awake.py`; `dotfiles/niri/swayidle.service` supplies the lock and
DPMS timeouts around it.

## Part E — Session environment, GPU, display

### E.1 Environment for niri's children

Set inside `dotfiles/niri/config.kdl` (`environment { … }`), so it applies to
everything niri spawns:

- `ELECTRON_OZONE_PLATFORM_HINT "auto"` — Electron apps (Obsidian) on Wayland.
- fcitx5: `GTK_IM_MODULE`, `QT_IM_MODULE`, `XMODIFIERS`, `SDL_IM_MODULE`,
  `INPUT_METHOD` = `fcitx`, `GLFW_IM_MODULE "ibus"`. `fcitx5` and `udiskie` are
  started with `spawn-at-startup`; waybar is not (it is a user unit, Part D).
- The `LIBVA_DRIVER_NAME` / `GBM_BACKEND` / `WLR_NO_HARDWARE_CURSORS` block is
  deliberately commented out here: this machine relies on the kernel parameters
  from A.7 plus the zsh `LD_LIBRARY_PATH=/usr/lib/nvidia` tweak (Part F)
  instead.

Input method configuration (the actual fcitx5 profile) lives in `~/.config/fcitx5`
and is **not** in this repo — set your pinyin input up once with
`fcitx5-configtool` after first login.

### E.2 NVIDIA wiring — skip entirely on AMD/Intel

Only for NVIDIA machines, and it has to match the B.4 package choice:

1. **Kernel parameters** — already set in A.7:
   `nvidia_drm.modeset=1 nvidia.NVreg_PreserveVideoMemoryAllocations=1`.
2. **Early module load** — this machine puts the modules in
   `/etc/mkinitcpio.conf`:

   ```ini
   MODULES=(nvidia nvidia_modeset nvidia_uvm nvidia_drm)
   HOOKS=(base udev autodetect modconf keyboard keymap consolefont block filesystems fsck)
   ```

   A fresh install ships
   `HOOKS=(base udev autodetect microcode modconf kms keyboard keymap consolefont block filesystems fsck)`.
   Prefer that shipped line and let the `kms` hook do the job, or keep the
   `MODULES` entry with the old hook list as here — do **not** do both. Then:

   ```bash
   sudo mkinitcpio -P && sudo reboot
   ```

   The reference `HOOKS` lines lack `microcode` and `kms` because the file
   predates those hooks; `intel-ucode` still loads early because
   `grub-mkconfig` prepends `intel-ucode.img` to the initrd.
3. **Userspace** — `nvidia-utils` (arrives as a dependency of the driver),
   `nvidia-settings`, and `libva-nvidia-driver-git` for VA-API video decode.
4. **Docker GPU access** — `nvidia-container-toolkit`.

Check it took effect with `nvidia-smi`, `cat /proc/cmdline` and
`journalctl -b -u niri | grep -i nvidia`. Nothing else in this repo is
NVIDIA-specific.

### E.3 Display and input — per machine

`dotfiles/niri/config.kdl` hard-codes this laptop's panel and the owner's input
preferences. On different hardware, edit before/after first login:

- `output "eDP-1" { mode "2560x1600@165.004" }` — run `niri msg outputs` in the
  session, then copy your connector name and mode; or delete the whole `output`
  node and let niri pick defaults.
- `input { keyboard { xkb { layout "us"; options "caps:swapescape" } } }` and the
  `touchpad { tap; natural-scroll; dwt }` block — personal, safe to relax.

Waybar, mako, kitty and the cursor theme are not hardware-specific; the wallpapers
referenced by `swaybg.service` / `swayidle.service` ship in `wallpapers/`.

## Part F — Shell and fonts

`zsh` is the login shell (set in A.6). `~/.zshrc` (from `dotfiles/zsh/zshrc`)
sources the repo-packaged plugins — `zsh-fast-syntax-highlighting-git`,
`zsh-autosuggestions-git`, `zsh-theme-powerlevel10k-git` — plus
`eval "$(zoxide init zsh)"`, `EDITOR=nvim`, `~/.env` (Part G), the
`clash_on`/`v2ray_on`/`proxy_off` helpers, and `ya`/`awake`. `p10k` reads
`~/.p10k.zsh`; `dotfiles/bashrc` keeps a stripped-down bash equivalent. The
oh-my-zsh variant was removed — do not install it.

`~/.zshrc` sources `~/.env` unconditionally (bash guards it with `[ -f ]`), so
create that file in Part G before you rely on this shell.

Fonts: `ttf-maplemono-nf-cn` (kitty's `font_family Maple Mono NF CN`, size 12)
plus `otf-font-awesome` and `noto-fonts-cjk`/`noto-fonts-emoji` (waybar, CJK,
emoji). `dotfiles/mako/config` asks for `font=Cantarell 13`, so install
`cantarell-fonts` as well — on this machine it is only an orphaned leftover
dependency, and without it mako silently falls back to the default font.

## Part G — Outside the repo (manual steps)

These are needed for the desktop to look and behave the same, but cannot be
cloned from this repo:

1. **Secrets** — `~/.env` sources `SENSENOVA_API_KEY`, `EXA_API_KEY`,
   `TAVILY_API_KEY` (gitignored, no copy in the repo). Create it yourself.
2. **Neovim** — `~/.config/nvim` is a separate repo:
   `git clone https://gitee.com/peakyet/nvim.git ~/.config/nvim`.
3. **uv tools** — `uv tool install arxiv-mcp-server` and
   `uv tool install zotero-mcp-server`.
4. **MCP servers** — configured separately for each agent:
   `~/.claude.json` and `~/.codex/config.toml` carry `tavily`, `exa`, `Wolfram`,
   `arxiv-mcp-server`, `paper-search-mcp`, `zotero-mcp-server`. Add the API keys
   in each file; nothing in this repo configures them.
5. **Agent skills** — `dotfiles/agents/skills/` is linked to `~/.agents` by
   `install.sh`. `dotfiles/agents/.skill-lock.json` records the upstream sources
   for the externally sourced ones (`tavily-ai/skills`, `exa-labs/agent-skills`,
   `vercel-labs/skills`, `tt-a1i/archify`); the rest (`exa-contents`,
   `exa-search`, `grilling`, `handoff`, `wolfram-mcp`) are local.
6. **Licensed / account-bound apps** — install and activate by hand: Steam
   (first launch needs a VPN to download Proton), `mathematica-light` (offline
   keygen; see README), `wps-office` + `ttf-wps-fonts`, `zotero-bin`
   (+ Better BibTeX plugin), `obsidian` (+ Image Auto Upload, Zotero
   Integration plugins), `picgo-appimage`, `baidunetdisk-bin`, the QQ/WeChat
   builds, `zen-browser-bin`/`google-chrome` (sign-in), `v2raya-bin`.

## Part H — Verify

### H.1 Install phase

Run these while you still have the ISO booted / right after first boot:

```bash
efibootmgr -v | grep -i grub          # the firmware entry points at your ESP
ls /boot                              # vmlinuz-linux initramfs-linux.img intel-ucode.img EFI/ grub/
findmnt / /boot && swapon --show      # mounts + swap as genfstab recorded them
cat /proc/cmdline                     # NVIDIA: nvidia_drm.modeset=1 present
systemctl is-system-running           # "running" or "degraded" (check the units)
```

### H.2 Configuration phase

```bash
# dotfiles actually linked
ls -la ~ | grep -- ' -> '
ls -la ~/.config | grep -- ' -> '

# session and its units
echo "$XDG_SESSION_TYPE"                     # wayland
systemctl --user is-active niri.service
systemctl --user list-dependencies graphical-session.target | grep -E 'waybar|mako|sway'

# compositor / bar / notifications / IME / audio
niri msg version && niri msg outputs
pgrep -a waybar; pgrep -a mako; pgrep -a fcitx5
wpctl status | head

# fonts the configs name
fc-list | grep -E 'Maple Mono NF CN|Font Awesome'

# system services
systemctl is-enabled bluetooth docker tlp sshd NetworkManager

# nvidia only
nvidia-smi
```

The composite acceptance test: `niri-session` comes up with the wallpaper and
waybar, `Mod+T` opens kitty with the Maple Mono prompt, `Mod+D` opens tofi,
notifications render as light rounded cards, the audio keys move the volume, and
`niri msg outputs` reports your real panel.

## Part I — Known gaps, and what to double-check

- `dotfiles/waybar/config` points its `mediaplayer` module at
  `~/.config/waybar/mediaplayer.py`, which exists in neither this repo nor the
  `waybar` package; that module cannot render. Add the script or drop the module.
- The same config has a click handler for `pavucontrol`, which is **not**
  installed here (`pwvucontrol` is). Repointing it is a one-line edit.
- `dotfiles/agents/.skill-lock.json` still lists upstream-sourced skills
  (`tavily-best-practices`, `tavily-dynamic-search`, `find-skills`,
  `build-with-exa`) that are no longer in `dotfiles/agents/skills/`; the lock is
  stale relative to the tree.
- `~/dotfiles` on this machine *is* the repo, so `install.sh` never exercised its
  compatibility-symlink branch — the units' `%h/dotfiles/...` paths resolve
  straight into the repo. On any other machine the symlink matters.
- `cantarell-fonts` is an orphan here, yet mako's font depends on it (Part F).
- No firewall is configured: `nftables`/`iptables` are present only as
  dependencies and `nftables.service` is disabled; there is no ufw/firewalld.
- Secure Boot is disabled and nothing here signs kernels (no `sbctl`), so do not
  turn it on without extra work.
- Not installed here, although `README.md` lists them as optional/todo: `cava`,
  `wluma`, `tmux`, `gdm`, `fuzzel`, `dunst`, and `cups`/printer drivers. Note
  `ghostscript` is installed only as a dependency of `octave`/`graphviz`, not as
  the PDF tool the README implies.